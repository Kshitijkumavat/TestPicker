#pragma once
// ─────────────────────────────────────────────────────────────────
// generator.hpp – Synthetic project, ground-truth, commit & bug gen
// ─────────────────────────────────────────────────────────────────
#include "types.hpp"
#include "history.hpp"
#include <random>
#include <vector>

/// Parameters for synthetic project generation.
struct GenConfig {
    int    numFiles           = 200;
    int    numTests           = 100;
    int    numModules         = 0;     // 0 → sqrt(numFiles)
    unsigned int seed         = 42;
    double crossModuleDepProb = 0.05;
    double prereqProb         = 0.05;
    int    numHistoryRuns     = 200;
    bool   coldStart          = false;
    double flakyFraction      = 0.03;
    int    commitMinFiles     = 1;
    int    commitMaxFiles     = 5;
    double bugProb            = 0.3;
    double directDetectProb   = 0.6;
    double transitiveDetectProb = 0.3;
};

/// Hidden ground truth that strategies never see directly.
struct GroundTruth {
    std::vector<double> trueFailRates;   // indexed by test id
    std::vector<bool>   isFlaky;
    double flakyFailProb = 0.02;
};

/// A planted bug in a commit.
struct PlantedBug {
    bool hasBug      = false;
    int  buggyFileId = -1;
};

class Generator {
public:
    explicit Generator(unsigned int seed);

    Project      generateProject(const GenConfig& cfg);
    GroundTruth  generateGroundTruth(const Project& proj, const GenConfig& cfg);
    HistoryStore generateHistory(const Project& proj, const GroundTruth& truth,
                                 const GenConfig& cfg);
    Commit       generateCommit(const Project& proj, const GenConfig& cfg);

    /// Plant a bug; returns which file is buggy.
    PlantedBug plantBug(const Commit& commit, const GenConfig& cfg);

    /// Pre-sample per-test detection outcomes for this commit.
    /// Result[testId] = true means that test detects the planted bug.
    std::vector<bool> sampleDetectionOutcomes(
        const Project& proj,
        const GroundTruth& truth,
        const PlantedBug& bug,
        const std::vector<bool>& impactedFiles,
        const GenConfig& cfg);

    /// Pre-sample per-test normal failure (independent of bugs).
    std::vector<bool> sampleNormalFailures(
        const Project& proj,
        const GroundTruth& truth);

    std::mt19937& rng() { return rng_; }

private:
    std::mt19937 rng_;
};
