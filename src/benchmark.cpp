#include "benchmark.hpp"
#include "graph.hpp"
#include "topo.hpp"
#include "report.hpp"
#include <iostream>
#include <algorithm>
#include <chrono>
#include <unordered_map>
#include <iomanip>

BenchmarkRunner::BenchmarkRunner(const BenchmarkConfig& cfg) : config_(cfg) {}

void BenchmarkRunner::runBenchmark(
    const Project& proj,
    const GroundTruth& truth,
    HistoryStore& history,
    std::vector<std::unique_ptr<Strategy>>& strategies,
    const std::string& resultsDir)
{
    Generator gen(config_.seed);
    DependencyGraph graph;
    graph.build(proj);

    double fullSuiteTime = 0.0;
    for (const auto& t : proj.tests) {
        fullSuiteTime += t.timeSec;
    }

    std::vector<CommitStrategyMetric> allMetrics;
    allMetrics.reserve(config_.numCommits * strategies.size());

    int commitsWithBugs = 0;
    std::unordered_map<std::string, int> bugsCaughtByStrat;
    std::unordered_map<std::string, double> sumTimeSaved;
    std::unordered_map<std::string, double> sumCoverage;
    std::unordered_map<std::string, std::vector<double>> algoTimes;
    std::unordered_map<std::string, int> timeouts;
    std::unordered_map<std::string, int> overBudget;
    std::unordered_map<std::string, int> totalRuns;

    for (const auto& s : strategies) {
        bugsCaughtByStrat[s->name()] = 0;
        sumTimeSaved[s->name()] = 0.0;
        sumCoverage[s->name()] = 0.0;
        algoTimes[s->name()] = {};
        timeouts[s->name()] = 0;
        overBudget[s->name()] = 0;
        totalRuns[s->name()] = 0;
    }

    std::cout << "Running benchmark with " << config_.numCommits << " commits...\n";

    for (int cIdx = 0; cIdx < config_.numCommits; ++cIdx) {
        Commit commit = gen.generateCommit(proj, config_.genConfig);
        PlantedBug bug = gen.plantBug(commit, config_.genConfig);
        if (bug.hasBug) commitsWithBugs++;

        std::vector<int> uncoveredFiles;
        std::vector<Candidate> cands = graph.findAffectedTests(proj, commit.changedFiles, history.table(), uncoveredFiles);
        if (cands.empty()) continue;

        double totalAffectedTime = 0.0;
        for (const auto& c : cands) {
            totalAffectedTime += c.ownTimeSec;
        }

        double budgetSec = (config_.budgetSec > 0.0)
            ? config_.budgetSec
            : std::max(5.0, totalAffectedTime * config_.budgetFraction);

        std::vector<bool> impacted = graph.reverseReachable(commit.changedFiles);
        std::vector<bool> detectionOutcomes = gen.sampleDetectionOutcomes(proj, truth, bug, impacted, config_.genConfig);
        std::vector<bool> normalFailures = gen.sampleNormalFailures(proj, truth);

        std::vector<Result> results;
        results.reserve(strategies.size());

        for (auto& s : strategies) {
            Result res = s->select(cands, commit.changedFiles, budgetSec);
            res.actualTime = computeActualTime(proj, res.selected);

            SimOutcome sim = simulateExecution(proj, res, truth, bug, detectionOutcomes, normalFailures);

            CommitStrategyMetric m;
            m.commitIndex = cIdx;
            m.strategyName = s->name();
            m.testsSelected = static_cast<int>(res.selected.size());
            m.totalRealTime = res.actualTime;
            m.timeSavedAffected = std::max(0.0, totalAffectedTime - res.actualTime);
            m.timeSavedSuite = std::max(0.0, fullSuiteTime - res.actualTime);
            m.coverageRatio = (res.totalCoverableFiles > 0)
                ? static_cast<double>(res.coveredFiles) / res.totalCoverableFiles
                : 1.0;
            m.detectProb = res.detectProb;
            m.hasPlantedBug = bug.hasBug;
            m.caughtBug = sim.bugCaught;
            m.algoMs = res.algoMs;
            m.nodesExplored = res.nodesExplored;
            m.dpCells = res.dpCells;
            m.status = res.status;
            m.exceededBudget = (res.actualTime > budgetSec + 1e-9);

            if (m.caughtBug) bugsCaughtByStrat[s->name()]++;
            sumTimeSaved[s->name()] += m.timeSavedAffected;
            sumCoverage[s->name()] += m.coverageRatio;
            algoTimes[s->name()].push_back(m.algoMs);
            if (res.status.find("timeout") != std::string::npos) timeouts[s->name()]++;
            if (m.exceededBudget) overBudget[s->name()]++;
            totalRuns[s->name()]++;

            allMetrics.push_back(m);
            results.push_back(std::move(res));
        }

        if (config_.learn) {
            updateHistoryFromSimulation(history, proj, results, truth, bug, detectionOutcomes, normalFailures);
        }
    }

    std::vector<StrategySummary> summaries;
    for (const auto& s : strategies) {
        std::string name = s->name();
        int runs = std::max(1, totalRuns[name]);
        StrategySummary sum;
        sum.strategyName = name;
        sum.meanCatchRate = (commitsWithBugs > 0) ? (static_cast<double>(bugsCaughtByStrat[name]) / commitsWithBugs) : 0.0;
        sum.meanTimeSaved = sumTimeSaved[name] / runs;
        sum.meanCoverage = sumCoverage[name] / runs;
        sum.timeoutCount = timeouts[name];
        sum.overBudgetCount = overBudget[name];

        auto& times = algoTimes[name];
        if (!times.empty()) {
            double totalMs = 0.0;
            for (double t : times) totalMs += t;
            sum.meanAlgoMs = totalMs / times.size();

            std::sort(times.begin(), times.end());
            size_t p95Idx = static_cast<size_t>(0.95 * (times.size() - 1));
            sum.p95AlgoMs = times[p95Idx];
        }
        summaries.push_back(sum);
    }

    Report::printBenchmarkSummary(summaries);

    Report::writeCommitsCSV(resultsDir + "/commits.csv", allMetrics);
    Report::writeSummaryCSV(resultsDir + "/summary.csv", summaries);
}

void BenchmarkRunner::runBudgetSweep(
    const Project& proj,
    const GroundTruth& truth,
    HistoryStore& history,
    std::vector<std::unique_ptr<Strategy>>& strategies,
    const std::vector<double>& budgetPercents,
    int commitsPerSweep,
    const std::string& resultsDir)
{
    Generator gen(config_.seed + 100);
    DependencyGraph graph;
    graph.build(proj);

    std::vector<BudgetSweepRow> sweepRows;

    for (double pct : budgetPercents) {
        std::cout << "Sweeping budget: " << (pct * 100.0) << "% of affected time (" << commitsPerSweep << " commits)...\n";
        std::unordered_map<std::string, int> bugCatches;
        std::unordered_map<std::string, double> coverages;
        std::unordered_map<std::string, double> times;
        std::unordered_map<std::string, double> algoms;
        std::unordered_map<std::string, int> runs;
        int bugCommits = 0;

        for (const auto& s : strategies) {
            bugCatches[s->name()] = 0;
            coverages[s->name()] = 0.0;
            times[s->name()] = 0.0;
            algoms[s->name()] = 0.0;
            runs[s->name()] = 0;
        }

        for (int c = 0; c < commitsPerSweep; ++c) {
            Commit commit = gen.generateCommit(proj, config_.genConfig);
            PlantedBug bug = gen.plantBug(commit, config_.genConfig);
            if (bug.hasBug) bugCommits++;

            std::vector<int> uncoveredFiles;
            std::vector<Candidate> cands = graph.findAffectedTests(proj, commit.changedFiles, history.table(), uncoveredFiles);
            if (cands.empty()) continue;

            double totalAffectedTime = 0.0;
            for (const auto& cand : cands) totalAffectedTime += cand.ownTimeSec;
            double budgetSec = std::max(2.0, totalAffectedTime * pct);

            std::vector<bool> impacted = graph.reverseReachable(commit.changedFiles);
            std::vector<bool> detectionOutcomes = gen.sampleDetectionOutcomes(proj, truth, bug, impacted, config_.genConfig);
            std::vector<bool> normalFailures = gen.sampleNormalFailures(proj, truth);

            for (auto& s : strategies) {
                Result res = s->select(cands, commit.changedFiles, budgetSec);
                res.actualTime = computeActualTime(proj, res.selected);
                SimOutcome sim = simulateExecution(proj, res, truth, bug, detectionOutcomes, normalFailures);

                if (sim.bugCaught) bugCatches[s->name()]++;
                double cov = (res.totalCoverableFiles > 0)
                    ? static_cast<double>(res.coveredFiles) / res.totalCoverableFiles
                    : 1.0;
                coverages[s->name()] += cov;
                times[s->name()] += res.actualTime;
                algoms[s->name()] += res.algoMs;
                runs[s->name()]++;
            }
        }

        for (const auto& s : strategies) {
            std::string name = s->name();
            int rCount = std::max(1, runs[name]);
            BudgetSweepRow row;
            row.budgetPct = pct;
            row.strategyName = name;
            row.meanCatchRate = (bugCommits > 0) ? (static_cast<double>(bugCatches[name]) / bugCommits) : 0.0;
            row.meanCoverage = coverages[name] / rCount;
            row.meanTime = times[name] / rCount;
            row.meanAlgoMs = algoms[name] / rCount;
            sweepRows.push_back(row);
        }
    }

    Report::writeBudgetSweepCSV(resultsDir + "/budget_sweep.csv", sweepRows);
}

void BenchmarkRunner::runScaling(
    std::vector<std::unique_ptr<Strategy>>& strategies,
    const std::vector<int>& sizes,
    int instancesPerSize,
    double timeCapMs,
    const std::string& resultsDir)
{
    std::cout << "\nRunning scaling benchmarks (time cap: " << timeCapMs << " ms)...\n";
    std::vector<ScalingRow> scalingRows;
    std::unordered_map<std::string, bool> isStopped;
    for (const auto& s : strategies) isStopped[s->name()] = false;

    std::mt19937 rng(config_.seed + 500);
    std::uniform_real_distribution<double> timeDist(1.0, 30.0);
    std::uniform_real_distribution<double> pDist(0.01, 0.40);

    for (int n : sizes) {
        std::cout << "  Testing candidate size N = " << n << "...\n";
        for (auto& s : strategies) {
            std::string name = s->name();
            if (isStopped[name]) {
                scalingRows.push_back({ n, name, 0.0, 0.0, "stopped (> 5000 ms)" });
                continue;
            }

            double totalMs = 0.0;
            double maxMs = 0.0;
            bool exceeded = false;

            for (int inst = 0; inst < instancesPerSize; ++inst) {
                // Generate n synthetic candidates covering up to 20 changed files
                std::vector<Candidate> cands(n);
                int numFiles = std::min(20, std::max(5, n / 10));
                std::uniform_int_distribution<int> fDist(0, numFiles - 1);
                std::uniform_int_distribution<int> numCovDist(1, 4);

                double totalTime = 0.0;
                for (int i = 0; i < n; ++i) {
                    cands[i].testId = i;
                    cands[i].ownTimeSec = timeDist(rng);
                    cands[i].timeSec = cands[i].ownTimeSec;
                    cands[i].p = pDist(rng);
                    totalTime += cands[i].timeSec;

                    int nCov = numCovDist(rng);
                    for (int k = 0; k < nCov; ++k) {
                        int fid = fDist(rng);
                        if (std::find(cands[i].covers.begin(), cands[i].covers.end(), fid) == cands[i].covers.end()) {
                            cands[i].covers.push_back(fid);
                        }
                    }
                }

                double budget = totalTime * 0.25;
                std::vector<int> changed(numFiles);
                std::iota(changed.begin(), changed.end(), 0);

                Result res = s->select(cands, changed, budget);
                totalMs += res.algoMs;
                if (res.algoMs > maxMs) maxMs = res.algoMs;
                if (res.algoMs > timeCapMs) {
                    exceeded = true;
                    break;
                }
            }

            if (exceeded) {
                isStopped[name] = true;
                scalingRows.push_back({ n, name, maxMs, maxMs, "stopped (> " + std::to_string(static_cast<int>(timeCapMs)) + " ms)" });
            } else {
                double meanMs = totalMs / instancesPerSize;
                scalingRows.push_back({ n, name, meanMs, maxMs, "ok" });
            }
        }
    }

    Report::writeScalingCSV(resultsDir + "/scaling.csv", scalingRows);
}

HashComparisonStats BenchmarkRunner::compareHashTables(int numOperations) {
    HashComparisonStats stats;
    stats.totalOps = numOperations;

    HashTable<int, int> customTable;
    std::unordered_map<int, int> stdMap;

    std::mt19937 rng(12345);
    std::uniform_int_distribution<int> keyDist(0, numOperations / 2);
    std::uniform_int_distribution<int> valDist(0, 100000);

    std::vector<int> testKeys(numOperations);
    std::vector<int> testVals(numOperations);
    for (int i = 0; i < numOperations; ++i) {
        testKeys[i] = keyDist(rng);
        testVals[i] = valDist(rng);
    }

    // Benchmark Custom HashTable
    customTable.resetProbeCount();
    auto t0 = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < numOperations / 2; ++i) {
        customTable.insert(testKeys[i], testVals[i]);
    }
    for (int i = 0; i < numOperations; ++i) {
        customTable.find(testKeys[i]);
    }
    for (int i = 0; i < numOperations / 10; ++i) {
        customTable.erase(testKeys[i]);
    }
    auto t1 = std::chrono::high_resolution_clock::now();
    stats.customTotalTimeMs = std::chrono::duration<double, std::milli>(t1 - t0).count();
    stats.customLoadFactor = customTable.loadFactor();
    stats.customProbesPerLookup = static_cast<double>(customTable.getProbeCount()) / (numOperations + numOperations / 2 + numOperations / 10);

    // Benchmark std::unordered_map
    auto t2 = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < numOperations / 2; ++i) {
        stdMap[testKeys[i]] = testVals[i];
    }
    for (int i = 0; i < numOperations; ++i) {
        stdMap.find(testKeys[i]);
    }
    for (int i = 0; i < numOperations / 10; ++i) {
        stdMap.erase(testKeys[i]);
    }
    auto t3 = std::chrono::high_resolution_clock::now();
    stats.stdTotalTimeMs = std::chrono::duration<double, std::milli>(t3 - t2).count();

    return stats;
}
