#pragma once
// ─────────────────────────────────────────────────────────────────
// io.hpp – CSV loading / saving
// ─────────────────────────────────────────────────────────────────
#include "types.hpp"
#include "history.hpp"
#include <string>

/// Load project from files.csv and tests.csv.
/// Throws std::runtime_error on malformed data.
Project loadProject(const std::string& filesCSV, const std::string& testsCSV);

/// Load a commit from commit.csv.
Commit loadCommit(const std::string& filename);

/// Save project to CSV pair.
void saveProject(const Project& proj,
                 const std::string& filesCSV, const std::string& testsCSV);
