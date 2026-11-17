#include "strategies/greedy_ratio.hpp"
#include <chrono>
#include <algorithm>

Result GreedyRatio::select(const std::vector<Candidate>& cands,
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

    struct Item {
        int idx;
        double ratio;
    };
    std::vector<Item> items(cands.size());
    for (size_t i = 0; i < cands.size(); ++i) {
        items[i] = { static_cast<int>(i), cands[i].p / std::max(cands[i].timeSec, 1e-9) };
    }

    std::sort(items.begin(), items.end(), [](const Item& a, const Item& b) {
        return a.ratio > b.ratio;
    });

    double remBudget = budgetSec;
    std::vector<int> selected;

    for (const auto& it : items) {
        if (cands[it.idx].timeSec <= remBudget + 1e-9) {
            selected.push_back(it.idx);
            remBudget -= cands[it.idx].timeSec;
        }
    }

    auto t1 = std::chrono::high_resolution_clock::now();
    double ms = std::chrono::duration<double, std::milli>(t1 - t0).count();
    return buildResult(name(), selected, cands, ms, "ok");
}
