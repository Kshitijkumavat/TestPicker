#include "strategies/knapsack_dp.hpp"
#include <chrono>
#include <cmath>
#include <algorithm>

long long KnapsackDP::memory2D(int n, int bUnits) const {
    // Value table (double) + keep table (char)
    return static_cast<long long>(n + 1) * (bUnits + 1) * (sizeof(double) + sizeof(char));
}

long long KnapsackDP::memory1D(int bUnits) const {
    return static_cast<long long>(bUnits + 1) * sizeof(double);
}

double KnapsackDP::solve1D(const std::vector<Candidate>& cands, double budgetSec) const {
    if (cands.empty() || budgetSec <= 0) return 0.0;
    int bUnits = static_cast<int>(std::floor(budgetSec / resolution_));
    if (bUnits <= 0) return 0.0;

    std::vector<double> dp(bUnits + 1, 0.0);
    for (const auto& c : cands) {
        int wi = std::max(1, static_cast<int>(std::round(c.timeSec / resolution_)));
        double vi = -std::log(1.0 - std::min(c.p, 0.999));
        for (int w = bUnits; w >= wi; --w) {
            dp[w] = std::max(dp[w], dp[w - wi] + vi);
        }
    }
    return 1.0 - std::exp(-dp[bUnits]);
}

Result KnapsackDP::select(const std::vector<Candidate>& cands,
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

    int n = static_cast<int>(cands.size());
    int bUnits = static_cast<int>(std::floor(budgetSec / resolution_));
    if (bUnits <= 0) {
        auto t1 = std::chrono::high_resolution_clock::now();
        double ms = std::chrono::duration<double, std::milli>(t1 - t0).count();
        return buildResult(name(), {}, cands, ms, "budget too small");
    }

    long long totalCells = static_cast<long long>(n) * (bUnits + 1);
    if (totalCells > cellLimit_) {
        auto t1 = std::chrono::high_resolution_clock::now();
        double ms = std::chrono::duration<double, std::milli>(t1 - t0).count();
        Result res = buildResult(name(), {}, cands, ms, "skipped: table too large");
        res.dpCells = totalCells;
        return res;
    }

    // Prepare item weights and values
    std::vector<int> weights(n);
    std::vector<double> values(n);
    for (int i = 0; i < n; ++i) {
        weights[i] = std::max(1, static_cast<int>(std::round(cands[i].timeSec / resolution_)));
        values[i] = -std::log(1.0 - std::min(cands[i].p, 0.999));
    }

    // 2D DP with reconstruction:
    // We maintain previous and current row of dp values for memory efficiency,
    // plus a 1-byte keep table of size n x (bUnits + 1).
    std::vector<double> prevRow(bUnits + 1, 0.0);
    std::vector<double> currRow(bUnits + 1, 0.0);
    std::vector<std::vector<char>> keep(n, std::vector<char>(bUnits + 1, 0));

    for (int i = 0; i < n; ++i) {
        int wi = weights[i];
        double vi = values[i];
        for (int w = 0; w <= bUnits; ++w) {
            currRow[w] = prevRow[w];
            if (w >= wi) {
                double takeVal = prevRow[w - wi] + vi;
                if (takeVal > currRow[w]) {
                    currRow[w] = takeVal;
                    keep[i][w] = 1;
                }
            }
        }
        prevRow = currRow;
    }

    // Reconstruct selected items
    std::vector<int> selected;
    int currW = bUnits;
    for (int i = n - 1; i >= 0; --i) {
        if (keep[i][currW]) {
            selected.push_back(i);
            currW -= weights[i];
        }
    }
    std::reverse(selected.begin(), selected.end());

    auto t1 = std::chrono::high_resolution_clock::now();
    double ms = std::chrono::duration<double, std::milli>(t1 - t0).count();
    return buildResult(name(), selected, cands, ms, "ok", 0, totalCells);
}
