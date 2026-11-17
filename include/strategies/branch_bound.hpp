#pragma once
#include "strategy.hpp"

class BranchAndBound : public Strategy {
public:
    /// `timeLimitMs` = wall-clock cap on B&B search (default 2000 ms).
    explicit BranchAndBound(double timeLimitMs = 2000.0)
        : timeLimitMs_(timeLimitMs) {}

    std::string name() const override { return "Branch & Bound"; }
    Result select(const std::vector<Candidate>& cands,
                  const std::vector<int>& changed,
                  double budgetSec) override;

private:
    double timeLimitMs_;
};
