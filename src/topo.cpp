#include "topo.hpp"
#include <queue>
#include <algorithm>
#include <unordered_map>
#include <unordered_set>

std::vector<int> gatherWithPrereqs(const Project& project,
                                   const std::vector<int>& selected)
{
    int maxTestId = 0;
    for (const auto& t : project.tests) {
        if (t.id > maxTestId) maxTestId = t.id;
    }
    std::vector<int> testIdToIdx(maxTestId + 1, -1);
    for (size_t i = 0; i < project.tests.size(); ++i) {
        testIdToIdx[project.tests[i].id] = static_cast<int>(i);
    }

    std::vector<bool> visited(maxTestId + 1, false);
    std::queue<int> q;
    std::vector<int> result;

    for (int tid : selected) {
        if (tid <= maxTestId && !visited[tid]) {
            visited[tid] = true;
            q.push(tid);
            result.push_back(tid);
        }
    }

    while (!q.empty()) {
        int u = q.front();
        q.pop();
        int uIdx = (u <= maxTestId) ? testIdToIdx[u] : -1;
        if (uIdx >= 0) {
            for (int p : project.tests[uIdx].prereqs) {
                if (p <= maxTestId && !visited[p]) {
                    visited[p] = true;
                    q.push(p);
                    result.push_back(p);
                }
            }
        }
    }

    return result;
}

std::vector<int> kahnTopologicalSort(const Project& project,
                                     const std::vector<int>& testIds)
{
    if (testIds.empty()) return {};

    int maxTestId = 0;
    for (const auto& t : project.tests) {
        if (t.id > maxTestId) maxTestId = t.id;
    }
    std::vector<int> testIdToIdx(maxTestId + 1, -1);
    for (size_t i = 0; i < project.tests.size(); ++i) {
        testIdToIdx[project.tests[i].id] = static_cast<int>(i);
    }

    // Set of active tests in the subgraph
    std::vector<bool> inSubgraph(maxTestId + 1, false);
    for (int tid : testIds) {
        if (tid <= maxTestId) {
            inSubgraph[tid] = true;
        }
    }

    // in-degree within the subgraph: if p is in prereqs of u, then p -> u, so u has in-degree
    // and p has out-edge to u
    std::vector<int> inDegree(maxTestId + 1, 0);
    std::vector<std::vector<int>> adj(maxTestId + 1);

    for (int tid : testIds) {
        if (tid > maxTestId) continue;
        int idx = testIdToIdx[tid];
        if (idx < 0) continue;

        for (int p : project.tests[idx].prereqs) {
            if (p <= maxTestId && inSubgraph[p]) {
                adj[p].push_back(tid);
                inDegree[tid]++;
            }
        }
    }

    std::queue<int> q;
    for (int tid : testIds) {
        if (tid <= maxTestId && inDegree[tid] == 0) {
            q.push(tid);
        }
    }

    std::vector<int> order;
    order.reserve(testIds.size());

    while (!q.empty()) {
        int u = q.front();
        q.pop();
        order.push_back(u);

        for (int v : adj[u]) {
            inDegree[v]--;
            if (inDegree[v] == 0) {
                q.push(v);
            }
        }
    }

    if (order.size() != testIds.size()) {
        // Cycle detected in subgraph
        return {};
    }

    return order;
}

double computeActualTime(const Project& project,
                         const std::vector<int>& selected)
{
    std::vector<int> fullSet = gatherWithPrereqs(project, selected);
    int maxTestId = 0;
    for (const auto& t : project.tests) {
        if (t.id > maxTestId) maxTestId = t.id;
    }
    std::vector<int> testIdToIdx(maxTestId + 1, -1);
    for (size_t i = 0; i < project.tests.size(); ++i) {
        testIdToIdx[project.tests[i].id] = static_cast<int>(i);
    }

    double total = 0.0;
    for (int tid : fullSet) {
        if (tid <= maxTestId) {
            int idx = testIdToIdx[tid];
            if (idx >= 0) {
                total += project.tests[idx].timeSec;
            }
        }
    }
    return total;
}
