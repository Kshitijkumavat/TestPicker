#pragma once
// ─────────────────────────────────────────────────────────────────
// types.hpp – Core data structures for TestPicker
// ─────────────────────────────────────────────────────────────────
#include <string>
#include <vector>

// A source-code file node in the dependency graph.
struct SourceFile {
    int id = 0;
    std::string name;
    std::vector<int> deps;   // ids of files this file depends on (includes/imports)
};

// A test node with timing, exercised files and ordering prerequisites.
struct Test {
    int id = 0;
    std::string name;
    double timeSec = 0.0;              // estimated run time
    std::vector<int> files;            // source-file ids directly exercised
    std::vector<int> prereqs;          // test ids that must run before this one
};

// Per-test execution history for Laplace-smoothed probability estimation.
struct History {
    int runs     = 0;
    int failures = 0;
    double avgTime = 0.0;
};

// A developer commit – the set of changed source files.
struct Commit {
    std::vector<int> changedFiles;
};

// The full project model.
struct Project {
    std::vector<SourceFile> files;
    std::vector<Test>       tests;
};
