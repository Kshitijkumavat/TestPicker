#pragma once
#include "strategy.hpp"

class GreedyRatio : public Strategy {
public:
    std::string name() const override { return "Greedy Ratio"; }
    Result select(const std::vector<Candidate>& cands,
                  const std::vector<int>& changed,
                  double budgetSec) override;
};
