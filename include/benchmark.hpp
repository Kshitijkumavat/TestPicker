#pragma once
// ─────────────────────────────────────────────────────────────────
// benchmark.hpp – Multi-commit replay, budget sweeps, scaling tests
// ─────────────────────────────────────────────────────────────────
#include "types.hpp"
#include "strategy.hpp"
#include "history.hpp"
#include "generator.hpp"
#include "simulator.hpp"
#include <vector>
#include <string>
#include <memory>

struct CommitStrategyMetric {
    int commitIndex = 0;
    std::string strategyName;
    int testsSelected = 0;
    double totalRealTime = 0.0;
    double timeSavedAffected = 0.0;
    double timeSavedSuite = 0.0;
    double coverageRatio = 0.0;
    double detectProb = 0.0;
    bool hasPlantedBug = false;
    bool caughtBug = false;
    double algoMs = 0.0;
    long long nodesExplored = 0;
    long long dpCells = 0;
    std::string status;
    bool exceededBudget = false;
};

struct StrategySummary {
    std::string strategyName;
    double meanCatchRate = 0.0;
    double meanTimeSaved = 0.0;
    double meanCoverage = 0.0;
    double meanAlgoMs = 0.0;
    double p95AlgoMs = 0.0;
    int timeoutCount = 0;
    int overBudgetCount = 0;
};

struct BudgetSweepRow {
    double budgetPct = 0.0;
    std::string strategyName;
    double meanCatchRate = 0.0;
    double meanCoverage = 0.0;
    double meanTime = 0.0;
    double meanAlgoMs = 0.0;
};

struct ScalingRow {
    int candidateCount = 0;
    std::string strategyName;
    double meanAlgoMs = 0.0;
    double maxAlgoMs = 0.0;
    std::string status;
};

struct HashComparisonStats {
    int totalOps = 0;
    double customProbesPerLookup = 0.0;
    double customLoadFactor = 0.0;
    double customTotalTimeMs = 0.0;
    double stdTotalTimeMs = 0.0;
};

struct BenchmarkConfig {
    int numCommits = 500;
    double budgetFraction = 0.25;
    double budgetSec = 0.0;
    unsigned int seed = 42;
    bool learn = true;
    GenConfig genConfig;
};

class BenchmarkRunner {
public:
    explicit BenchmarkRunner(const BenchmarkConfig& cfg);

    void runBenchmark(const Project& proj,
                      const GroundTruth& truth,
                      HistoryStore& history,
                      std::vector<std::unique_ptr<Strategy>>& strategies,
                      const std::string& resultsDir = "results");

    void runBudgetSweep(const Project& proj,
                        const GroundTruth& truth,
                        HistoryStore& history,
                        std::vector<std::unique_ptr<Strategy>>& strategies,
                        const std::vector<double>& budgetPercents = {0.05, 0.10, 0.25, 0.50},
                        int commitsPerSweep = 50,
                        const std::string& resultsDir = "results");

    void runScaling(std::vector<std::unique_ptr<Strategy>>& strategies,
                    const std::vector<int>& sizes = {20, 40, 80, 160, 320, 640, 1280, 2560},
                    int instancesPerSize = 5,
                    double timeCapMs = 5000.0,
                    const std::string& resultsDir = "results");

    static HashComparisonStats compareHashTables(int numOperations = 50000);

private:
    BenchmarkConfig config_;
};
