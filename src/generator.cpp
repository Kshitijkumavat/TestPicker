#include "generator.hpp"
#include <cmath>
#include <algorithm>
#include <unordered_set>

Generator::Generator(unsigned int seed) : rng_(seed) {}

Project Generator::generateProject(const GenConfig& cfg) {
    Project proj;
    int numModules = (cfg.numModules > 0) ? cfg.numModules : static_cast<int>(std::sqrt(cfg.numFiles));
    if (numModules < 1) numModules = 1;

    proj.files.resize(cfg.numFiles);
    // Assign files to modules evenly
    std::vector<int> fileModule(cfg.numFiles);
    std::vector<std::vector<int>> moduleFiles(numModules);
    for (int i = 0; i < cfg.numFiles; ++i) {
        int m = i % numModules;
        fileModule[i] = m;
        moduleFiles[m].push_back(i);
        proj.files[i].id = i;
        proj.files[i].name = "file_" + std::to_string(i) + ".cpp";
    }

    std::uniform_real_distribution<double> unif01(0.0, 1.0);
    std::uniform_int_distribution<int> depCountDist(0, 4);

    // Dependencies: DAG guarantee: only depend on files with index < i
    for (int i = 0; i < cfg.numFiles; ++i) {
        if (i == 0) continue;
        int m = fileModule[i];
        int numDeps = depCountDist(rng_);
        std::unordered_set<int> depsSet;

        // Utility files (first 5% of files) have high probability of being depended on
        int utilityThreshold = std::max(1, cfg.numFiles / 20);

        for (int d = 0; d < numDeps; ++d) {
            int target = -1;
            if (unif01(rng_) < 0.25 && utilityThreshold > 0) {
                // Pick utility file < i
                int maxU = std::min(i, utilityThreshold);
                std::uniform_int_distribution<int> uDist(0, maxU - 1);
                target = uDist(rng_);
            } else if (unif01(rng_) < cfg.crossModuleDepProb) {
                // Cross-module dependency < i
                std::uniform_int_distribution<int> anyDist(0, i - 1);
                target = anyDist(rng_);
            } else {
                // Intra-module dependency < i
                std::vector<int> validSameModule;
                for (int fid : moduleFiles[m]) {
                    if (fid < i) validSameModule.push_back(fid);
                }
                if (!validSameModule.empty()) {
                    std::uniform_int_distribution<int> sameDist(0, static_cast<int>(validSameModule.size() - 1));
                    target = validSameModule[sameDist(rng_)];
                } else {
                    std::uniform_int_distribution<int> anyDist(0, i - 1);
                    target = anyDist(rng_);
                }
            }
            if (target >= 0 && target < i) {
                depsSet.insert(target);
            }
        }
        proj.files[i].deps.assign(depsSet.begin(), depsSet.end());
        std::sort(proj.files[i].deps.begin(), proj.files[i].deps.end());
    }

    // Generate tests
    proj.tests.resize(cfg.numTests);
    // Log-normal distribution for test execution time (median ~ 5 sec)
    std::lognormal_distribution<double> timeDist(std::log(5.0), 0.7);
    std::uniform_int_distribution<int> fileCountDist(1, 5);

    for (int t = 0; t < cfg.numTests; ++t) {
        proj.tests[t].id = t;
        proj.tests[t].name = "test_" + std::to_string(t);
        double tsec = timeDist(rng_);
        if (tsec < 0.5) tsec = 0.5;
        if (tsec > 300.0) tsec = 300.0;
        proj.tests[t].timeSec = std::round(tsec * 10.0) / 10.0;

        // Choose primary module for the test
        int m = t % numModules;
        int numFilesForTest = fileCountDist(rng_);
        std::unordered_set<int> filesSet;

        for (int f = 0; f < numFilesForTest; ++f) {
            if (unif01(rng_) < 0.8 && !moduleFiles[m].empty()) {
                std::uniform_int_distribution<int> dist(0, static_cast<int>(moduleFiles[m].size() - 1));
                filesSet.insert(moduleFiles[m][dist(rng_)]);
            } else {
                std::uniform_int_distribution<int> dist(0, cfg.numFiles - 1);
                filesSet.insert(dist(rng_));
            }
        }
        proj.tests[t].files.assign(filesSet.begin(), filesSet.end());
        std::sort(proj.tests[t].files.begin(), proj.tests[t].files.end());

        // Prerequisite setup test: DAG guarantee: only pick test < t
        if (t > 0 && unif01(rng_) < cfg.prereqProb) {
            std::uniform_int_distribution<int> prereqDist(0, t - 1);
            proj.tests[t].prereqs.push_back(prereqDist(rng_));
        }
    }

    return proj;
}

GroundTruth Generator::generateGroundTruth(const Project& proj, const GenConfig& cfg) {
    GroundTruth truth;
    truth.trueFailRates.resize(proj.tests.size(), 0.01);
    truth.isFlaky.resize(proj.tests.size(), false);
    truth.flakyFailProb = 0.02;

    std::uniform_real_distribution<double> unif01(0.0, 1.0);
    std::uniform_real_distribution<double> lowRateDist(0.005, 0.03);
    std::uniform_real_distribution<double> highRateDist(0.10, 0.40);

    for (size_t t = 0; t < proj.tests.size(); ++t) {
        if (unif01(rng_) < 0.15) {
            truth.trueFailRates[t] = highRateDist(rng_);
        } else {
            truth.trueFailRates[t] = lowRateDist(rng_);
        }

        if (unif01(rng_) < cfg.flakyFraction) {
            truth.isFlaky[t] = true;
        }
    }
    return truth;
}

HistoryStore Generator::generateHistory(const Project& proj,
                                        const GroundTruth& truth,
                                        const GenConfig& cfg)
{
    HistoryStore store;
    if (cfg.coldStart || cfg.numHistoryRuns <= 0) {
        return store;
    }

    std::uniform_real_distribution<double> unif01(0.0, 1.0);
    std::normal_distribution<double> noiseDist(1.0, 0.08);

    for (const auto& t : proj.tests) {
        double failRate = (t.id < static_cast<int>(truth.trueFailRates.size())) ? truth.trueFailRates[t.id] : 0.01;
        bool flaky = (t.id < static_cast<int>(truth.isFlaky.size())) ? truth.isFlaky[t.id] : false;

        for (int r = 0; r < cfg.numHistoryRuns; ++r) {
            bool failed = (unif01(rng_) < failRate);
            if (!failed && flaky && (unif01(rng_) < truth.flakyFailProb)) {
                failed = true;
            }
            double runTime = t.timeSec * std::max(0.5, noiseDist(rng_));
            store.update(t.id, failed, runTime);
        }
    }

    return store;
}

Commit Generator::generateCommit(const Project& proj, const GenConfig& cfg) {
    Commit c;
    if (proj.files.empty()) return c;

    int minF = std::max(1, cfg.commitMinFiles);
    int maxF = std::min(static_cast<int>(proj.files.size()), cfg.commitMaxFiles);
    std::uniform_int_distribution<int> countDist(minF, maxF);
    int count = countDist(rng_);

    int numModules = (cfg.numModules > 0) ? cfg.numModules : static_cast<int>(std::sqrt(proj.files.size()));
    if (numModules < 1) numModules = 1;

    std::uniform_int_distribution<int> modDist(0, numModules - 1);
    int chosenMod = modDist(rng_);

    std::vector<int> modFiles;
    for (const auto& f : proj.files) {
        if (f.id % numModules == chosenMod) {
            modFiles.push_back(f.id);
        }
    }

    std::unordered_set<int> picked;
    std::uniform_real_distribution<double> unif01(0.0, 1.0);

    for (int i = 0; i < count; ++i) {
        int fid = -1;
        if (unif01(rng_) < 0.75 && !modFiles.empty()) {
            std::uniform_int_distribution<int> d(0, static_cast<int>(modFiles.size() - 1));
            fid = modFiles[d(rng_)];
        } else {
            std::uniform_int_distribution<int> d(0, static_cast<int>(proj.files.size() - 1));
            fid = proj.files[d(rng_)].id;
        }
        picked.insert(fid);
    }

    c.changedFiles.assign(picked.begin(), picked.end());
    std::sort(c.changedFiles.begin(), c.changedFiles.end());
    return c;
}

PlantedBug Generator::plantBug(const Commit& commit, const GenConfig& cfg) {
    PlantedBug pb;
    if (commit.changedFiles.empty()) return pb;

    std::uniform_real_distribution<double> unif01(0.0, 1.0);
    if (unif01(rng_) < cfg.bugProb) {
        pb.hasBug = true;
        std::uniform_int_distribution<int> d(0, static_cast<int>(commit.changedFiles.size() - 1));
        pb.buggyFileId = commit.changedFiles[d(rng_)];
    }
    return pb;
}

std::vector<bool> Generator::sampleDetectionOutcomes(
    const Project& proj,
    const GroundTruth& truth,
    const PlantedBug& bug,
    const std::vector<bool>& impactedFiles,
    const GenConfig& cfg)
{
    (void)truth;
    std::vector<bool> detected(proj.tests.size(), false);
    if (!bug.hasBug || bug.buggyFileId < 0) {
        return detected;
    }

    std::uniform_real_distribution<double> unif01(0.0, 1.0);

    for (size_t t = 0; t < proj.tests.size(); ++t) {
        const auto& testObj = proj.tests[t];
        bool exercisesDirectly = false;
        bool exercisesImpacted = false;

        for (int fid : testObj.files) {
            if (fid == bug.buggyFileId) {
                exercisesDirectly = true;
                break;
            }
            if (fid >= 0 && fid < static_cast<int>(impactedFiles.size()) && impactedFiles[fid]) {
                exercisesImpacted = true;
            }
        }

        if (exercisesDirectly) {
            if (unif01(rng_) < cfg.directDetectProb) {
                detected[t] = true;
            }
        } else if (exercisesImpacted) {
            if (unif01(rng_) < cfg.transitiveDetectProb) {
                detected[t] = true;
            }
        }
    }
    return detected;
}

std::vector<bool> Generator::sampleNormalFailures(
    const Project& proj,
    const GroundTruth& truth)
{
    std::vector<bool> fails(proj.tests.size(), false);
    std::uniform_real_distribution<double> unif01(0.0, 1.0);

    for (size_t t = 0; t < proj.tests.size(); ++t) {
        double rate = (t < truth.trueFailRates.size()) ? truth.trueFailRates[t] : 0.01;
        bool flaky = (t < truth.isFlaky.size()) ? truth.isFlaky[t] : false;

        if (unif01(rng_) < rate) {
            fails[t] = true;
        } else if (flaky && unif01(rng_) < truth.flakyFailProb) {
            fails[t] = true;
        }
    }
    return fails;
}
