#pragma once
// ─────────────────────────────────────────────────────────────────
// report.hpp – Console tables and CSV writers
// ─────────────────────────────────────────────────────────────────
#include "types.hpp"
#include "strategy.hpp"
#include "benchmark.hpp"
#include <vector>
#include <string>

namespace Report {

/// Print formatted table comparing all strategies on a commit.
void printComparisonTable(const std::vector<Result>& results, double budgetSec);

/// Print summary callouts: best detection and smallest covering set.
void printBestSummary(const std::vector<Result>& results);

/// Print Kahn topological execution order for a strategy's selected tests.
void printExecutionOrder(const Project& proj,
                         const std::string& stratName,
                         const std::vector<int>& orderedTestIds);

/// Print structural project overview.
void printProjectSummary(const Project& proj);

/// Print benchmark summary table to console.
void printBenchmarkSummary(const std::vector<StrategySummary>& summaries);

// CSV Writers
void writeCommitsCSV(const std::string& path, const std::vector<CommitStrategyMetric>& rows);
void writeSummaryCSV(const std::string& path, const std::vector<StrategySummary>& summaries);
void writeBudgetSweepCSV(const std::string& path, const std::vector<BudgetSweepRow>& rows);
void writeScalingCSV(const std::string& path, const std::vector<ScalingRow>& rows);
void writeHashStatsCSV(const std::string& path, const HashComparisonStats& stats);

} // namespace Report
