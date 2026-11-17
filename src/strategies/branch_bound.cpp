#include "strategies/branch_bound.hpp"
#include <chrono>
#include <algorithm>
#include <cstdint>
#include <unordered_map>
#include <vector>

Result BranchAndBound::select(const std::vector<Candidate>& cands,
                              const std::vector<int>& changed,
                              double budgetSec)
{
    (void)changed;
    auto t0 = std::chrono::high_resolution_clock::now();

    if (cands.empty()) {
        return buildResult(name(), {}, cands, 0.0, "ok");
    }

    double minTime = 1e18;
    for (const auto& c : cands) {
        if (c.timeSec < minTime) minTime = c.timeSec;
    }
    if (budgetSec < minTime) {
        auto t1 = std::chrono::high_resolution_clock::now();
        double ms = std::chrono::duration<double, std::milli>(t1 - t0).count();
        return buildResult(name(), {}, cands, ms, "budget too small");
    }

    // 1. Map coverable files to bit indices [0..M-1]
    std::vector<int> coverableFiles;
    for (const auto& c : cands) {
        for (int f : c.covers) {
            if (std::find(coverableFiles.begin(), coverableFiles.end(), f) == coverableFiles.end()) {
                coverableFiles.push_back(f);
            }
        }
    }
    std::sort(coverableFiles.begin(), coverableFiles.end());
    int M = static_cast<int>(coverableFiles.size());

    if (M == 0) {
        // Nothing to cover, simply fill with greedy ratio
        std::vector<bool> used(cands.size(), false);
        std::vector<int> sel = greedyRatioFill(cands, used, budgetSec);
        auto t1 = std::chrono::high_resolution_clock::now();
        double ms = std::chrono::duration<double, std::milli>(t1 - t0).count();
        return buildResult(name(), sel, cands, ms, "optimal cover");
    }

    if (M > 64) {
        // Exceeds 64-bit mask limit
        auto t1 = std::chrono::high_resolution_clock::now();
        double ms = std::chrono::duration<double, std::milli>(t1 - t0).count();
        return buildResult(name(), {}, cands, ms, "unsupported: >64 changed files");
    }

    std::unordered_map<int, int> fileToBit;
    for (int i = 0; i < M; ++i) {
        fileToBit[coverableFiles[i]] = i;
    }

    // 2. Build candidate masks
    struct CandInfo {
        int origIdx;
        uint64_t mask;
        double timeSec;
        double p;
    };
    std::vector<CandInfo> rawItems;
    rawItems.reserve(cands.size());
    for (size_t i = 0; i < cands.size(); ++i) {
        uint64_t m = 0;
        for (int f : cands[i].covers) {
            auto it = fileToBit.find(f);
            if (it != fileToBit.end()) {
                m |= (1ULL << it->second);
            }
        }
        if (m > 0) {
            rawItems.push_back({ static_cast<int>(i), m, cands[i].timeSec, cands[i].p });
        }
    }

    // 3. Dominated-test preprocessing:
    // Remove test i if another test j has (mask_i subset of mask_j) and (time_i >= time_j)
    std::vector<CandInfo> items;
    for (size_t i = 0; i < rawItems.size(); ++i) {
        bool dominated = false;
        for (size_t j = 0; j < rawItems.size(); ++j) {
            if (i == j) continue;
            if ((rawItems[i].mask & rawItems[j].mask) == rawItems[i].mask &&
                rawItems[i].timeSec >= rawItems[j].timeSec) {
                // If identical mask and identical time, break ties by index
                if (rawItems[i].mask == rawItems[j].mask && rawItems[i].timeSec == rawItems[j].timeSec) {
                    if (i > j) dominated = true;
                } else {
                    dominated = true;
                }
                if (dominated) break;
            }
        }
        if (!dominated) {
            items.push_back(rawItems[i]);
        }
    }

    uint64_t targetMask = (M == 64) ? ~0ULL : ((1ULL << M) - 1ULL);

    // 4. Initial upper bound: greedy set cover on these items
    double bestCost = 1e18;
    std::vector<int> bestChosen; // items indices
    {
        uint64_t curMask = 0;
        double curCost = 0.0;
        std::vector<bool> gUsed(items.size(), false);
        std::vector<int> gChosen;

        while (curMask != targetMask) {
            int bestIdx = -1;
            double bestScore = -1.0;
            for (size_t i = 0; i < items.size(); ++i) {
                if (gUsed[i]) continue;
                uint64_t newly = items[i].mask & (~curMask);
                if (newly == 0) continue;
                int count = 0;
                for (int b = 0; b < M; ++b) if ((newly >> b) & 1ULL) count++;
                double score = static_cast<double>(count) / std::max(items[i].timeSec, 1e-9);
                if (score > bestScore) {
                    bestScore = score;
                    bestIdx = static_cast<int>(i);
                }
            }
            if (bestIdx == -1) break;
            gUsed[bestIdx] = true;
            gChosen.push_back(bestIdx);
            curMask |= items[bestIdx].mask;
            curCost += items[bestIdx].timeSec;
        }

        if (curMask == targetMask) {
            bestCost = curCost;
            bestChosen = gChosen;
        }
    }

    // 5. Branch and Bound recursive search
    long long nodesExplored = 0;
    bool timedOut = false;
    std::vector<int> currentChosen;

    // Precompute tests covering each bit
    std::vector<std::vector<int>> bitCoveringTests(M);
    for (size_t i = 0; i < items.size(); ++i) {
        for (int b = 0; b < M; ++b) {
            if ((items[i].mask >> b) & 1ULL) {
                bitCoveringTests[b].push_back(static_cast<int>(i));
            }
        }
    }

    auto search = [&](auto& self, uint64_t coveredMask, double currentCost) -> void {
        nodesExplored++;
        if ((nodesExplored & 1023) == 0) {
            auto now = std::chrono::high_resolution_clock::now();
            double elapsed = std::chrono::duration<double, std::milli>(now - t0).count();
            if (elapsed >= timeLimitMs_) {
                timedOut = true;
                return;
            }
        }

        if (coveredMask == targetMask) {
            if (currentCost < bestCost) {
                bestCost = currentCost;
                bestChosen = currentChosen;
            }
            return;
        }

        // Lower bound calculation:
        // cost + sum over uncovered file f of min over tests t covering f of (t.time / |t.covers ∩ uncovered|)
        double lb = currentCost;
        uint64_t uncovered = targetMask & (~coveredMask);

        for (int b = 0; b < M; ++b) {
            if ((uncovered >> b) & 1ULL) {
                double minUnitCost = 1e18;
                for (int itemIdx : bitCoveringTests[b]) {
                    uint64_t inter = items[itemIdx].mask & uncovered;
                    int interCount = 0;
                    for (int k = 0; k < M; ++k) if ((inter >> k) & 1ULL) interCount++;
                    if (interCount > 0) {
                        double uCost = items[itemIdx].timeSec / interCount;
                        if (uCost < minUnitCost) minUnitCost = uCost;
                    }
                }
                if (minUnitCost < 1e17) {
                    lb += minUnitCost;
                }
            }
        }

        if (lb >= bestCost) {
            return; // Prune
        }

        // Branching rule: pick uncovered bit with fewest covering candidates
        int bestBit = -1;
        int minCandidates = 1e9;
        for (int b = 0; b < M; ++b) {
            if ((uncovered >> b) & 1ULL) {
                int cCount = static_cast<int>(bitCoveringTests[b].size());
                if (cCount < minCandidates) {
                    minCandidates = cCount;
                    bestBit = b;
                }
            }
        }

        if (bestBit == -1) return;

        // Branch on tests covering bestBit, sorted by increasing cost per newly covered file
        struct BranchOption {
            int itemIdx;
            double costPerNew;
        };
        std::vector<BranchOption> branchList;
        for (int itemIdx : bitCoveringTests[bestBit]) {
            uint64_t newly = items[itemIdx].mask & uncovered;
            int count = 0;
            for (int k = 0; k < M; ++k) if ((newly >> k) & 1ULL) count++;
            if (count > 0) {
                branchList.push_back({ itemIdx, items[itemIdx].timeSec / count });
            }
        }

        std::sort(branchList.begin(), branchList.end(), [](const BranchOption& a, const BranchOption& b) {
            return a.costPerNew < b.costPerNew;
        });

        for (const auto& opt : branchList) {
            if (timedOut) return;
            int itemIdx = opt.itemIdx;
            if (currentCost + items[itemIdx].timeSec >= bestCost) continue;

            currentChosen.push_back(itemIdx);
            self(self, coveredMask | items[itemIdx].mask, currentCost + items[itemIdx].timeSec);
            currentChosen.pop_back();
        }
    };

    search(search, 0ULL, 0.0);

    // Collect candidate indices chosen for the minimum cover
    std::vector<int> finalSelected;
    std::vector<bool> usedCand(cands.size(), false);
    double coverTime = 0.0;
    for (int itIdx : bestChosen) {
        int orig = items[itIdx].origIdx;
        finalSelected.push_back(orig);
        usedCand[orig] = true;
        coverTime += cands[orig].timeSec;
    }

    std::string status;
    if (coverTime > budgetSec + 1e-9) {
        status = "cover exceeds budget";
    } else {
        // Fill remaining budget with greedy ratio
        double remBudget = budgetSec - coverTime;
        std::vector<int> fillIdxs = greedyRatioFill(cands, usedCand, remBudget);
        for (int idx : fillIdxs) {
            finalSelected.push_back(idx);
        }
        status = timedOut ? "timeout: best so far" : "optimal cover";
    }

    auto t1 = std::chrono::high_resolution_clock::now();
    double ms = std::chrono::duration<double, std::milli>(t1 - t0).count();
    return buildResult(name(), finalSelected, cands, ms, status, nodesExplored, 0);
}
