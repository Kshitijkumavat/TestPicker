#include "graph.hpp"
#include <queue>
#include <algorithm>
#include <unordered_set>

void DependencyGraph::build(const Project& project) {
    numFiles_ = 0;
    for (const auto& f : project.files) {
        if (f.id >= numFiles_) numFiles_ = f.id + 1;
    }

    forward_.assign(numFiles_, {});
    reverse_.assign(numFiles_, {});
    fileToTests_.clear();

    for (const auto& f : project.files) {
        if (f.id < 0) continue;
        for (int dep : f.deps) {
            if (dep >= 0 && dep < numFiles_) {
                forward_[f.id].push_back(dep);
                reverse_[dep].push_back(f.id);
            }
        }
    }

    for (const auto& t : project.tests) {
        for (int fid : t.files) {
            if (fid >= 0 && fid < numFiles_) {
                auto* vec = fileToTests_.find(fid);
                if (vec) {
                    vec->push_back(t.id);
                } else {
                    fileToTests_.insert(fid, {t.id});
                }
            }
        }
    }
}

std::vector<bool> DependencyGraph::reverseReachable(const std::vector<int>& changedFiles) const {
    std::vector<bool> visited(numFiles_, false);
    std::queue<int> q;

    for (int c : changedFiles) {
        if (c >= 0 && c < numFiles_ && !visited[c]) {
            visited[c] = true;
            q.push(c);
        }
    }

    while (!q.empty()) {
        int u = q.front();
        q.pop();

        for (int v : reverse_[u]) {
            if (!visited[v]) {
                visited[v] = true;
                q.push(v);
            }
        }
    }
    return visited;
}

std::vector<Candidate> DependencyGraph::findAffectedTests(
    const Project& project,
    const std::vector<int>& changedFiles,
    const HashTable<int, History>& history,
    std::vector<int>& uncoveredChangedFiles) const
{
    uncoveredChangedFiles.clear();
    if (changedFiles.empty()) return {};

    // Map test id -> test index in project.tests
    int maxTestId = 0;
    for (const auto& t : project.tests) {
        if (t.id > maxTestId) maxTestId = t.id;
    }
    std::vector<int> testIdToIdx(maxTestId + 1, -1);
    for (size_t i = 0; i < project.tests.size(); ++i) {
        testIdToIdx[project.tests[i].id] = static_cast<int>(i);
    }

    // For each changed file, do reverse BFS to find all files it can reach
    // and collect all tests exercising those files.
    // changedFile -> list of test IDs that cover it
    std::vector<std::vector<int>> testsCoveringChange(changedFiles.size());
    std::vector<bool> isAffectedTest(maxTestId + 1, false);
    std::vector<int> affectedTestIds;

    for (size_t ci = 0; ci < changedFiles.size(); ++ci) {
        int c = changedFiles[ci];
        if (c < 0 || c >= numFiles_) continue;

        // BFS from c
        std::vector<bool> reachable(numFiles_, false);
        std::queue<int> q;
        reachable[c] = true;
        q.push(c);

        while (!q.empty()) {
            int u = q.front();
            q.pop();

            // Find tests exercising u
            const auto* testList = fileToTests_.find(u);
            if (testList) {
                for (int tid : *testList) {
                    if (std::find(testsCoveringChange[ci].begin(), testsCoveringChange[ci].end(), tid) == testsCoveringChange[ci].end()) {
                        testsCoveringChange[ci].push_back(tid);
                    }
                    if (tid <= maxTestId && !isAffectedTest[tid]) {
                        isAffectedTest[tid] = true;
                        affectedTestIds.push_back(tid);
                    }
                }
            }

            for (int v : reverse_[u]) {
                if (!reachable[v]) {
                    reachable[v] = true;
                    q.push(v);
                }
            }
        }

        if (testsCoveringChange[ci].empty()) {
            uncoveredChangedFiles.push_back(c);
        }
    }

    // Sort affected test ids for determinism
    std::sort(affectedTestIds.begin(), affectedTestIds.end());

    // Helper to compute inflated time (own time + transitive prereq times)
    auto computeTransitivePrereqTime = [&](int startTestId) -> double {
        std::vector<bool> visited(maxTestId + 1, false);
        std::queue<int> q;
        double totalTime = 0.0;

        int startIdx = testIdToIdx[startTestId];
        if (startIdx >= 0) {
            totalTime += project.tests[startIdx].timeSec;
            visited[startTestId] = true;
            for (int p : project.tests[startIdx].prereqs) {
                if (p <= maxTestId && !visited[p]) {
                    visited[p] = true;
                    q.push(p);
                }
            }
        }

        while (!q.empty()) {
            int curId = q.front();
            q.pop();
            int curIdx = (curId <= maxTestId) ? testIdToIdx[curId] : -1;
            if (curIdx >= 0) {
                totalTime += project.tests[curIdx].timeSec;
                for (int p : project.tests[curIdx].prereqs) {
                    if (p <= maxTestId && !visited[p]) {
                        visited[p] = true;
                        q.push(p);
                    }
                }
            }
        }
        return totalTime;
    };

    std::vector<Candidate> candidates;
    candidates.reserve(affectedTestIds.size());

    for (int tid : affectedTestIds) {
        int idx = testIdToIdx[tid];
        if (idx < 0) continue;
        const auto& testObj = project.tests[idx];

        Candidate cand;
        cand.testId = tid;
        cand.ownTimeSec = testObj.timeSec;
        cand.timeSec = computeTransitivePrereqTime(tid);

        // Laplace-smoothed failure probability
        const History* h = history.find(tid);
        if (h && h->runs > 0) {
            cand.p = static_cast<double>(h->failures + 1) / static_cast<double>(h->runs + 2);
        } else {
            cand.p = 1.0 / 2.0; // unobserved test default
        }

        // Covers list (subset of changedFiles that this test covers)
        for (size_t ci = 0; ci < changedFiles.size(); ++ci) {
            int c = changedFiles[ci];
            if (std::find(testsCoveringChange[ci].begin(), testsCoveringChange[ci].end(), tid) != testsCoveringChange[ci].end()) {
                cand.covers.push_back(c);
            }
        }

        candidates.push_back(std::move(cand));
    }

    return candidates;
}

bool DependencyGraph::detectPrereqCycle(const Project& project,
                                        std::vector<std::string>& cyclePath)
{
    cyclePath.clear();
    int maxTestId = 0;
    for (const auto& t : project.tests) {
        if (t.id > maxTestId) maxTestId = t.id;
    }

    std::vector<int> testIdToIdx(maxTestId + 1, -1);
    for (size_t i = 0; i < project.tests.size(); ++i) {
        testIdToIdx[project.tests[i].id] = static_cast<int>(i);
    }

    // 0: White, 1: Gray, 2: Black
    std::vector<int> color(maxTestId + 1, 0);
    std::vector<int> parent(maxTestId + 1, -1);
    int cycleStart = -1;
    int cycleEnd = -1;

    auto dfs = [&](auto& self, int u) -> bool {
        color[u] = 1; // Gray
        int uIdx = testIdToIdx[u];
        if (uIdx >= 0) {
            for (int v : project.tests[uIdx].prereqs) {
                if (v > maxTestId || testIdToIdx[v] < 0) continue;
                if (color[v] == 1) { // Back edge!
                    cycleStart = v;
                    cycleEnd = u;
                    return true;
                }
                if (color[v] == 0) {
                    parent[v] = u;
                    if (self(self, v)) return true;
                }
            }
        }
        color[u] = 2; // Black
        return false;
    };

    for (const auto& t : project.tests) {
        if (color[t.id] == 0) {
            if (dfs(dfs, t.id)) {
                // Reconstruct cycle path
                std::vector<int> cycleNodeIds;
                cycleNodeIds.push_back(cycleStart);
                for (int cur = cycleEnd; cur != cycleStart && cur != -1; cur = parent[cur]) {
                    cycleNodeIds.push_back(cur);
                }
                cycleNodeIds.push_back(cycleStart);
                std::reverse(cycleNodeIds.begin(), cycleNodeIds.end());

                for (int cid : cycleNodeIds) {
                    int cidx = (cid <= maxTestId) ? testIdToIdx[cid] : -1;
                    if (cidx >= 0) {
                        cyclePath.push_back(project.tests[cidx].name);
                    } else {
                        cyclePath.push_back(std::to_string(cid));
                    }
                }
                return true;
            }
        }
    }
    return false;
}
