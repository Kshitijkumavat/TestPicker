#pragma once
#include "strategy.hpp"

class KnapsackDP : public Strategy {
public:
    /// `resolution` = seconds per integer weight unit (default 1).
    /// `cellLimit`  = max n*B_units before skipping (default 200 M).
    explicit KnapsackDP(double resolution = 1.0, long long cellLimit = 200'000'000)
        : resolution_(resolution), cellLimit_(cellLimit) {}

    std::string name() const override { return "Knapsack DP"; }
    Result select(const std::vector<Candidate>& cands,
                  const std::vector<int>& changed,
                  double budgetSec) override;

    /// 1-D rolling-array variant (value only, no reconstruction).
    /// Returns the optimal detection value; used for benchmark comparison.
    double solve1D(const std::vector<Candidate>& cands, double budgetSec) const;

    /// Analytical memory usage of the 2-D table (bytes).
    long long memory2D(int n, int bUnits) const;
    /// Analytical memory usage of the 1-D table (bytes).
    long long memory1D(int bUnits) const;

private:
    double    resolution_;
    long long cellLimit_;
};
