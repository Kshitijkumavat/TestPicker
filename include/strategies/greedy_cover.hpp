#pragma once
#include "strategy.hpp"

class GreedySetCover : public Strategy {
public:
    std::string name() const override { return "Greedy Set Cover"; }
    Result select(const std::vector<Candidate>& cands,
                  const std::vector<int>& changed,
                  double budgetSec) override;
};
