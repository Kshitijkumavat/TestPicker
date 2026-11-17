#pragma once
// ─────────────────────────────────────────────────────────────────
// topo.hpp – Kahn's topological sort for test execution ordering
// ─────────────────────────────────────────────────────────────────
#include "types.hpp"
#include <vector>

/// Collect the union of `selected` and all their transitive prerequisites.
std::vector<int> gatherWithPrereqs(const Project& project,
                                   const std::vector<int>& selected);

/// Kahn's topological sort on the subgraph of `testIds`.
/// Returns the ordered list; empty if a cycle is detected (should not
/// happen if detectPrereqCycle passed).
std::vector<int> kahnTopologicalSort(const Project& project,
                                     const std::vector<int>& testIds);

/// Compute actual total time for a selected set, counting shared
/// prerequisites exactly once.
double computeActualTime(const Project& project,
                         const std::vector<int>& selected);
