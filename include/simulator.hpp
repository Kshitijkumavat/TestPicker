#pragma once
// ─────────────────────────────────────────────────────────────────
// simulator.hpp – Execute selected tests against ground truth
// ─────────────────────────────────────────────────────────────────
#include "types.hpp"
#include "history.hpp"
#include "generator.hpp"
#include "strategy.hpp"
#include <vector>

/// Per-commit simulation outcome for one strategy.
struct SimOutcome {
    std::string strategyName;
    bool bugCaught    = false;   // did any selected test detect the planted bug?
    bool bugApplicable = false;  // was there a planted bug at all?
    int  testsRun     = 0;
    int  testsFailed  = 0;
};

/// Simulate execution of selected tests against hidden ground truth.
/// `detectionOutcomes[testId]` = true means that test detects the bug.
/// `normalFailures[testId]` = true means the test fails independent of bugs.
SimOutcome simulateExecution(
    const Project& proj,
    const Result& result,
    const GroundTruth& truth,
    const PlantedBug& bug,
    const std::vector<bool>& detectionOutcomes,
    const std::vector<bool>& normalFailures);

/// Update history for the union of all strategies' selected tests.
void updateHistoryFromSimulation(
    HistoryStore& history,
    const Project& proj,
    const std::vector<Result>& results,
    const GroundTruth& truth,
    const PlantedBug& bug,
    const std::vector<bool>& detectionOutcomes,
    const std::vector<bool>& normalFailures);
