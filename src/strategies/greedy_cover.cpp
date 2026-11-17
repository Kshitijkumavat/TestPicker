#include "strategies/greedy_cover.hpp"
#include <chrono>
#include <algorithm>
#include <unordered_set>

Result GreedySetCover::select(const std::vector<Candidate>& cands,
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

    // Determine all coverable changed files
    std::unordered_set<int> allCoverable;
    for (const auto& c : cands) {
        for (int f : c.covers) {
            allCoverable.insert(f);
        }
    }

    std::unordered_set<int> covered;
    std::vector<bool> used(cands.size(), false);
    std::vector<int> selected;
    double remBudget = budgetSec;

    // Phase 1: Set cover
    while (covered.size() < allCoverable.size()) {
        int bestIdx = -1;
        double bestScore = -1.0;
        double bestP = -1.0;

        for (size_t i = 0; i < cands.size(); ++i) {
            if (used[i]) continue;
            if (cands[i].timeSec > remBudget + 1e-9) continue;

            // Count newly covered files
            int newCover = 0;
            for (int f : cands[i].covers) {
                if (covered.find(f) == covered.end()) {
                    newCover++;
                }
            }
            if (newCover == 0) continue;

            double score = static_cast<double>(newCover) / std::max(cands[i].timeSec, 1e-9);
            if (score > bestScore || (std::abs(score - bestScore) < 1e-9 && cands[i].p > bestP)) {
                bestScore = score;
                bestP = cands[i].p;
                bestIdx = static_cast<int>(i);
            }
        }

        if (bestIdx == -1) break; // No candidate can cover new files within remaining budget

        used[bestIdx] = true;
        selected.push_back(bestIdx);
        remBudget -= cands[bestIdx].timeSec;
        for (int f : cands[bestIdx].covers) {
            covered.insert(f);
        }
    }

    // Phase 2: Fill remaining budget with Greedy Ratio
    std::vector<int> fillIdxs = greedyRatioFill(cands, used, remBudget);
    for (int idx : fillIdxs) {
        selected.push_back(idx);
    }

    auto t1 = std::chrono::high_resolution_clock::now();
    double ms = std::chrono::duration<double, std::milli>(t1 - t0).count();
    return buildResult(name(), selected, cands, ms, "ok");
}
