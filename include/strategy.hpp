#pragma once
// ─────────────────────────────────────────────────────────────────
// strategy.hpp – Strategy interface, Candidate, Result, helpers
// ─────────────────────────────────────────────────────────────────
#include "types.hpp"
#include <string>
#include <vector>
#include <cmath>
#include <algorithm>
#include <memory>
#include <chrono>
#include <numeric>

// ── Candidate: one test the strategies consider ─────────────────
struct Candidate {
    int    testId;
    double timeSec;     // inflated time (own + transitive prereq upper-bound)
    double ownTimeSec;  // test's own estimated time
    double p;           // Laplace-smoothed failure probability
    std::vector<int> covers; // changed-file ids this test covers
};

// ── Result: what a strategy returns ─────────────────────────────
struct Result {
    std::string      name;
    std::vector<int> selected;          // test ids
    double totalTime        = 0;        // sum of inflated times
    double actualTime       = 0;        // recomputed with shared prereqs once (filled by driver)
    int    coveredFiles     = 0;
    int    totalCoverableFiles = 0;
    double detectProb       = 0;
    double algoMs           = 0;
    std::string status;                 // 'ok', 'optimal cover', 'timeout: best so far', …
    long long nodesExplored = 0;
    long long dpCells       = 0;
};

// ── Abstract strategy ───────────────────────────────────────────
class Strategy {
public:
    virtual ~Strategy() = default;
    virtual std::string name() const = 0;
    virtual Result select(const std::vector<Candidate>& cands,
                          const std::vector<int>& changed,
                          double budgetSec) = 0;
};

// ── Helpers used by multiple strategies ─────────────────────────

/// Build a Result from a set of selected candidate-indices.
inline Result buildResult(const std::string& stratName,
                          const std::vector<int>& selIdx,
                          const std::vector<Candidate>& cands,
                          double algoMs,
                          const std::string& status,
                          long long nodesExplored = 0,
                          long long dpCells = 0)
{
    Result r;
    r.name          = stratName;
    r.algoMs        = algoMs;
    r.status        = status;
    r.nodesExplored = nodesExplored;
    r.dpCells       = dpCells;

    double logSurv = 0.0;
    std::vector<int> covSet;

    for (int i : selIdx) {
        const auto& c = cands[i];
        r.selected.push_back(c.testId);
        r.totalTime += c.timeSec;
        double pi = std::min(c.p, 0.999);
        logSurv += std::log(1.0 - pi);
        for (int f : c.covers)
            if (std::find(covSet.begin(), covSet.end(), f) == covSet.end())
                covSet.push_back(f);
    }
    r.detectProb    = selIdx.empty() ? 0.0 : 1.0 - std::exp(logSurv);
    r.coveredFiles  = static_cast<int>(covSet.size());

    // total coverable files across all candidates
    std::vector<int> allCov;
    for (const auto& c : cands)
        for (int f : c.covers)
            if (std::find(allCov.begin(), allCov.end(), f) == allCov.end())
                allCov.push_back(f);
    r.totalCoverableFiles = static_cast<int>(allCov.size());

    return r;
}

/// Greedy-ratio fill: pick unused candidates by descending p/t while they fit.
/// Returns additional candidate indices to add.
inline std::vector<int> greedyRatioFill(const std::vector<Candidate>& cands,
                                        const std::vector<bool>& used,
                                        double remainingBudget)
{
    // Build (index, score) pairs for unused candidates
    struct IS { int idx; double score; };
    std::vector<IS> items;
    for (int i = 0; i < static_cast<int>(cands.size()); ++i)
        if (!used[i])
            items.push_back({i, cands[i].p / std::max(cands[i].timeSec, 1e-9)});

    std::sort(items.begin(), items.end(),
              [](const IS& a, const IS& b){ return a.score > b.score; });

    std::vector<int> added;
    for (const auto& it : items) {
        if (cands[it.idx].timeSec <= remainingBudget + 1e-9) {
            added.push_back(it.idx);
            remainingBudget -= cands[it.idx].timeSec;
        }
    }
    return added;
}
