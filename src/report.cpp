#include "report.hpp"
#include <iostream>
#include <fstream>
#include <iomanip>
#include <numeric>

namespace Report {

void printComparisonTable(const std::vector<Result>& results, double budgetSec) {
    (void)budgetSec;
    std::cout << "\n";
    std::cout << std::left << std::setw(20) << "Strategy"
              << std::right << std::setw(8) << "Tests"
              << std::right << std::setw(10) << "Time(s)"
              << std::right << std::setw(12) << "Covered"
              << std::right << std::setw(10) << "Detect%"
              << std::right << std::setw(12) << "AlgoTime"
              << "   " << std::left << "Status\n";
    std::cout << std::string(80, '-') << "\n";

    for (const auto& r : results) {
        std::string covStr = std::to_string(r.coveredFiles) + "/" + std::to_string(r.totalCoverableFiles);
        std::stringstream msSs;
        msSs << std::fixed << std::setprecision(2) << r.algoMs << " ms";

        std::cout << std::left << std::setw(20) << r.name
                  << std::right << std::setw(8) << r.selected.size()
                  << std::right << std::setw(10) << std::fixed << std::setprecision(1) << r.actualTime
                  << std::right << std::setw(12) << covStr
                  << std::right << std::setw(9) << std::fixed << std::setprecision(1) << (r.detectProb * 100.0) << "%"
                  << std::right << std::setw(12) << msSs.str()
                  << "   " << std::left << r.status << "\n";
    }
    std::cout << "\n";
}

void printBestSummary(const std::vector<Result>& results) {
    if (results.empty()) return;

    const Result* bestDetect = nullptr;
    const Result* minCover = nullptr;

    for (const auto& r : results) {
        if (!bestDetect || r.detectProb > bestDetect->detectProb) {
            bestDetect = &r;
        }
        if (r.name == "Branch & Bound") {
            minCover = &r;
        }
    }

    if (bestDetect) {
        std::cout << "Best detection within budget: " << bestDetect->name
                  << " (" << std::fixed << std::setprecision(1) << (bestDetect->detectProb * 100.0) << "%)\n";
    }
    if (minCover && !minCover->selected.empty()) {
        std::cout << "Branch & Bound result: " << minCover->selected.size()
                  << " tests, " << std::fixed << std::setprecision(1) << minCover->actualTime
                  << " s (" << minCover->status << ")\n";
    }
}

void printExecutionOrder(const Project& proj,
                         const std::string& stratName,
                         const std::vector<int>& orderedTestIds)
{
    std::cout << "\nExecution order for " << stratName << " (with prerequisites):\n";
    if (orderedTestIds.empty()) {
        std::cout << "  (none)\n";
        return;
    }

    int maxTestId = 0;
    for (const auto& t : proj.tests) {
        if (t.id > maxTestId) maxTestId = t.id;
    }
    std::vector<std::string> names(maxTestId + 1);
    for (const auto& t : proj.tests) {
        if (t.id <= maxTestId) names[t.id] = t.name;
    }

    std::cout << "  ";
    for (size_t i = 0; i < orderedTestIds.size(); ++i) {
        if (i > 0) std::cout << " -> ";
        int tid = orderedTestIds[i];
        if (tid <= maxTestId && !names[tid].empty()) {
            std::cout << names[tid];
        } else {
            std::cout << "test_" << tid;
        }
    }
    std::cout << "\n";
}

void printProjectSummary(const Project& proj) {
    std::cout << "\n=== Project Summary ===\n";
    std::cout << "Source files : " << proj.files.size() << "\n";
    std::cout << "Tests        : " << proj.tests.size() << "\n";

    double totalTime = 0.0;
    int totalDeps = 0;
    int totalPrereqs = 0;
    for (const auto& f : proj.files) {
        totalDeps += static_cast<int>(f.deps.size());
    }
    for (const auto& t : proj.tests) {
        totalTime += t.timeSec;
        totalPrereqs += static_cast<int>(t.prereqs.size());
    }

    std::cout << "Total test suite time : " << std::fixed << std::setprecision(1) << totalTime << " s\n";
    if (!proj.files.empty()) {
        std::cout << "Avg dependencies/file : " << std::fixed << std::setprecision(2)
                  << (static_cast<double>(totalDeps) / proj.files.size()) << "\n";
    }
    if (!proj.tests.empty()) {
        std::cout << "Avg test time         : " << std::fixed << std::setprecision(2)
                  << (totalTime / proj.tests.size()) << " s\n";
        std::cout << "Avg prerequisites/test: " << std::fixed << std::setprecision(2)
                  << (static_cast<double>(totalPrereqs) / proj.tests.size()) << "\n";
    }
    std::cout << "=======================\n\n";
}

void printBenchmarkSummary(const std::vector<StrategySummary>& summaries) {
    std::cout << "\n=== Benchmark Summary ===\n";
    std::cout << std::left << std::setw(20) << "Strategy"
              << std::right << std::setw(12) << "CatchRate%"
              << std::right << std::setw(14) << "TimeSaved(s)"
              << std::right << std::setw(12) << "Coverage%"
              << std::right << std::setw(12) << "MeanAlgoMs"
              << std::right << std::setw(12) << "P95AlgoMs"
              << std::right << std::setw(10) << "Timeouts"
              << std::right << std::setw(12) << "OverBudget\n";
    std::cout << std::string(104, '-') << "\n";

    for (const auto& s : summaries) {
        std::cout << std::left << std::setw(20) << s.strategyName
                  << std::right << std::setw(11) << std::fixed << std::setprecision(1) << (s.meanCatchRate * 100.0) << "%"
                  << std::right << std::setw(14) << std::fixed << std::setprecision(1) << s.meanTimeSaved
                  << std::right << std::setw(11) << std::fixed << std::setprecision(1) << (s.meanCoverage * 100.0) << "%"
                  << std::right << std::setw(12) << std::fixed << std::setprecision(2) << s.meanAlgoMs
                  << std::right << std::setw(12) << std::fixed << std::setprecision(2) << s.p95AlgoMs
                  << std::right << std::setw(10) << s.timeoutCount
                  << std::right << std::setw(12) << s.overBudgetCount << "\n";
    }
    std::cout << std::string(104, '-') << "\n\n";
}

void writeCommitsCSV(const std::string& path, const std::vector<CommitStrategyMetric>& rows) {
    std::ofstream out(path);
    if (!out.is_open()) return;

    out << "commit,strategy,tests_selected,real_time_sec,time_saved_affected,time_saved_suite,coverage_ratio,detect_prob,planted_bug,caught_bug,algo_ms,nodes_explored,dp_cells,status\n";
    for (const auto& r : rows) {
        out << r.commitIndex << ","
            << "\"" << r.strategyName << "\","
            << r.testsSelected << ","
            << r.totalRealTime << ","
            << r.timeSavedAffected << ","
            << r.timeSavedSuite << ","
            << r.coverageRatio << ","
            << r.detectProb << ","
            << (r.hasPlantedBug ? 1 : 0) << ","
            << (r.caughtBug ? 1 : 0) << ","
            << r.algoMs << ","
            << r.nodesExplored << ","
            << r.dpCells << ","
            << "\"" << r.status << "\"\n";
    }
}

void writeSummaryCSV(const std::string& path, const std::vector<StrategySummary>& summaries) {
    std::ofstream out(path);
    if (!out.is_open()) return;

    out << "strategy,mean_catch_rate,mean_time_saved,mean_coverage,mean_algo_ms,p95_algo_ms,timeout_count,over_budget_count\n";
    for (const auto& s : summaries) {
        out << "\"" << s.strategyName << "\","
            << s.meanCatchRate << ","
            << s.meanTimeSaved << ","
            << s.meanCoverage << ","
            << s.meanAlgoMs << ","
            << s.p95AlgoMs << ","
            << s.timeoutCount << ","
            << s.overBudgetCount << "\n";
    }
}

void writeBudgetSweepCSV(const std::string& path, const std::vector<BudgetSweepRow>& rows) {
    std::ofstream out(path);
    if (!out.is_open()) return;

    out << "budget_pct,strategy,mean_catch_rate,mean_coverage,mean_time_sec,mean_algo_ms\n";
    for (const auto& r : rows) {
        out << r.budgetPct << ","
            << "\"" << r.strategyName << "\","
            << r.meanCatchRate << ","
            << r.meanCoverage << ","
            << r.meanTime << ","
            << r.meanAlgoMs << "\n";
    }
}

void writeScalingCSV(const std::string& path, const std::vector<ScalingRow>& rows) {
    std::ofstream out(path);
    if (!out.is_open()) return;

    out << "candidates,strategy,mean_algo_ms,max_algo_ms,status\n";
    for (const auto& r : rows) {
        out << r.candidateCount << ","
            << "\"" << r.strategyName << "\","
            << r.meanAlgoMs << ","
            << r.maxAlgoMs << ","
            << "\"" << r.status << "\"\n";
    }
}

void writeHashStatsCSV(const std::string& path, const HashComparisonStats& stats) {
    std::ofstream out(path);
    if (!out.is_open()) return;

    out << "total_ops,custom_probes_per_lookup,custom_load_factor,custom_time_ms,std_time_ms\n";
    out << stats.totalOps << ","
        << stats.customProbesPerLookup << ","
        << stats.customLoadFactor << ","
        << stats.customTotalTimeMs << ","
        << stats.stdTotalTimeMs << "\n";
}

} // namespace Report
