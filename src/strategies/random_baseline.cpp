#include "strategies/random_baseline.hpp"
#include <chrono>
#include <algorithm>
#include <numeric>

Result RandomBaseline::select(const std::vector<Candidate>& cands,
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

    std::vector<int> indices(cands.size());
    std::iota(indices.begin(), indices.end(), 0);
    std::shuffle(indices.begin(), indices.end(), rng_);

    double remBudget = budgetSec;
    std::vector<int> selected;

    for (int idx : indices) {
        if (cands[idx].timeSec <= remBudget + 1e-9) {
            selected.push_back(idx);
            remBudget -= cands[idx].timeSec;
        }
    }

    auto t1 = std::chrono::high_resolution_clock::now();
    double ms = std::chrono::duration<double, std::milli>(t1 - t0).count();
    return buildResult(name(), selected, cands, ms, "ok");
}
