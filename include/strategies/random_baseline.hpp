#pragma once
#include "strategy.hpp"
#include <random>

class RandomBaseline : public Strategy {
public:
    explicit RandomBaseline(unsigned int seed) : rng_(seed) {}
    std::string name() const override { return "Random baseline"; }
    Result select(const std::vector<Candidate>& cands,
                  const std::vector<int>& changed,
                  double budgetSec) override;
private:
    std::mt19937 rng_;
};
