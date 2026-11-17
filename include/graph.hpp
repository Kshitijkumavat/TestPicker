#pragma once
// ─────────────────────────────────────────────────────────────────
// graph.hpp – Dependency graph, reverse BFS, DFS cycle detection
// ─────────────────────────────────────────────────────────────────
#include "types.hpp"
#include "hashtable.hpp"
#include "strategy.hpp"
#include <vector>
#include <string>

class DependencyGraph {
public:
    /// Build forward / reverse adjacency lists and file→tests index.
    void build(const Project& project);

    /// Multi-source BFS from changed files over the reverse graph.
    /// Returns a visited vector (true = impacted file).
    std::vector<bool> reverseReachable(const std::vector<int>& changedFiles) const;

    /// Discover affected tests, build Candidate list with covers and
    /// inflated timeSec (own + transitive prereqs).
    /// Also populates `uncoveredChangedFiles`.
    std::vector<Candidate> findAffectedTests(
        const Project& project,
        const std::vector<int>& changedFiles,
        const HashTable<int, History>& history,
        std::vector<int>& uncoveredChangedFiles) const;

    /// DFS cycle detection in the test-prerequisite graph.
    /// Returns true if a cycle exists; `cyclePath` contains the cycle names.
    static bool detectPrereqCycle(const Project& project,
                                  std::vector<std::string>& cyclePath);

    int numFiles() const { return numFiles_; }

private:
    int numFiles_ = 0;
    std::vector<std::vector<int>> forward_;   // forward_[a] = deps of a
    std::vector<std::vector<int>> reverse_;   // reverse_[b] = files depending on b
    HashTable<int, std::vector<int>> fileToTests_;  // file id → test ids
};
