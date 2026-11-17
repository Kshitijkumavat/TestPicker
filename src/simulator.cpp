#include "simulator.hpp"
#include <unordered_set>

SimOutcome simulateExecution(
    const Project& proj,
    const Result& result,
    const GroundTruth& truth,
    const PlantedBug& bug,
    const std::vector<bool>& detectionOutcomes,
    const std::vector<bool>& normalFailures)
{
    (void)proj;
    (void)truth;
    SimOutcome outcome;
    outcome.strategyName = result.name;
    outcome.bugApplicable = bug.hasBug;
    outcome.testsRun = static_cast<int>(result.selected.size());

    for (int tid : result.selected) {
        bool failed = false;
        if (tid >= 0 && tid < static_cast<int>(detectionOutcomes.size()) && detectionOutcomes[tid]) {
            outcome.bugCaught = true;
            failed = true;
        }
        if (tid >= 0 && tid < static_cast<int>(normalFailures.size()) && normalFailures[tid]) {
            failed = true;
        }
        if (failed) {
            outcome.testsFailed++;
        }
    }

    return outcome;
}

void updateHistoryFromSimulation(
    HistoryStore& history,
    const Project& proj,
    const std::vector<Result>& results,
    const GroundTruth& truth,
    const PlantedBug& bug,
    const std::vector<bool>& detectionOutcomes,
    const std::vector<bool>& normalFailures)
{
    (void)truth;
    (void)bug;
    std::unordered_set<int> unionTests;
    for (const auto& res : results) {
        for (int tid : res.selected) {
            unionTests.insert(tid);
        }
    }

    int maxTestId = 0;
    for (const auto& t : proj.tests) {
        if (t.id > maxTestId) maxTestId = t.id;
    }
    std::vector<double> testTimes(maxTestId + 1, 1.0);
    for (const auto& t : proj.tests) {
        if (t.id <= maxTestId) {
            testTimes[t.id] = t.timeSec;
        }
    }

    for (int tid : unionTests) {
        bool failed = false;
        if (tid >= 0 && tid < static_cast<int>(detectionOutcomes.size()) && detectionOutcomes[tid]) {
            failed = true;
        }
        if (tid >= 0 && tid < static_cast<int>(normalFailures.size()) && normalFailures[tid]) {
            failed = true;
        }
        double actualTime = (tid <= maxTestId) ? testTimes[tid] : 1.0;
        history.update(tid, failed, actualTime);
    }
}
