#include "types.hpp"
#include "graph.hpp"
#include "topo.hpp"
#include "history.hpp"
#include "generator.hpp"
#include "simulator.hpp"
#include "benchmark.hpp"
#include "report.hpp"
#include "io.hpp"
#include "strategies/random_baseline.hpp"
#include "strategies/greedy_ratio.hpp"
#include "strategies/greedy_cover.hpp"
#include "strategies/knapsack_dp.hpp"
#include "strategies/branch_bound.hpp"

#include <iostream>
#include <memory>
#include <string>
#include <sstream>
#include <algorithm>
#include <iomanip>

static std::vector<std::unique_ptr<Strategy>> makeStrategies(unsigned int seed = 42) {
    std::vector<std::unique_ptr<Strategy>> strats;
    strats.push_back(std::make_unique<RandomBaseline>(seed));
    strats.push_back(std::make_unique<GreedyRatio>());
    strats.push_back(std::make_unique<GreedySetCover>());
    strats.push_back(std::make_unique<KnapsackDP>(1.0, 200'000'000));
    strats.push_back(std::make_unique<BranchAndBound>(2000.0));
    return strats;
}

int main(int argc, char** argv) {
    GenConfig cfg;
    Generator gen(cfg.seed);

    // JSON API Flags for Direct Frontend C++ Integration
    if (argc > 1) {
        std::string arg = argv[1];
        if (arg == "--json-summary") {
            try {
                Project p = loadProject("data/files.csv", "data/tests.csv");
                std::cout << "{\"files\":[";
                for (size_t i = 0; i < p.files.size(); ++i) {
                    if (i > 0) std::cout << ",";
                    std::cout << "{\"id\":" << p.files[i].id << ",\"name\":\"" << p.files[i].name << "\"}";
                }
                std::cout << "],\"totalTests\":" << p.tests.size() << "}\n";
            } catch (const std::exception& e) {
                std::cout << "{\"error\":\"" << e.what() << "\"}\n";
            }
            return 0;
        }

        if (arg == "--json-analyze") {
            double budgetSec = (argc > 2) ? std::stod(argv[2]) : 60.0;
            std::vector<int> changed;
            for (int i = 3; i < argc; ++i) {
                try { changed.push_back(std::stoi(argv[i])); } catch (...) {}
            }
            try {
                Project p = loadProject("data/files.csv", "data/tests.csv");
                HistoryStore hStore;
                try { hStore.loadFromCSV("data/history.csv"); } catch (...) {}
                DependencyGraph g;
                g.build(p);
                std::vector<int> uncovered;
                auto cands = g.findAffectedTests(p, changed, hStore.table(), uncovered);
                auto strats = makeStrategies(cfg.seed);
                std::vector<Result> results;
                for (auto& s : strats) {
                    Result r = s->select(cands, changed, budgetSec);
                    r.actualTime = computeActualTime(p, r.selected);
                    results.push_back(r);
                }

                // Kahn schedule for Knapsack DP
                const Result* topRes = &results[3];
                auto allSched = gatherWithPrereqs(p, topRes->selected);
                auto ord = kahnTopologicalSort(p, allSched);

                std::cout << "{\"budgetSec\":" << budgetSec
                          << ",\"candsCount\":" << cands.size()
                          << ",\"uncoveredCount\":" << uncovered.size()
                          << ",\"results\":[";
                for (size_t i = 0; i < results.size(); ++i) {
                    if (i > 0) std::cout << ",";
                    std::cout << "{\"name\":\"" << results[i].name << "\""
                              << ",\"selectedCount\":" << results[i].selected.size()
                              << ",\"actualTime\":" << results[i].actualTime
                              << ",\"coveredFiles\":" << results[i].coveredFiles
                              << ",\"totalCoverableFiles\":" << results[i].totalCoverableFiles
                              << ",\"detectProb\":" << results[i].detectProb
                              << ",\"algoMs\":" << results[i].algoMs
                              << ",\"status\":\"" << results[i].status << "\"}";
                }
                std::cout << "],\"kahnSchedule\":[";
                for (size_t i = 0; i < ord.size(); ++i) {
                    if (i > 0) std::cout << ",";
                    int tid = ord[i];
                    bool isSetup = (std::find(topRes->selected.begin(), topRes->selected.end(), tid) == topRes->selected.end());
                    std::string tname = (tid < static_cast<int>(p.tests.size())) ? p.tests[tid].name : ("test_" + std::to_string(tid));
                    double ttime = (tid < static_cast<int>(p.tests.size())) ? p.tests[tid].timeSec : 0.0;
                    std::cout << "{\"id\":" << tid
                              << ",\"name\":\"" << tname << "\""
                              << ",\"timeSec\":" << ttime
                              << ",\"isSetup\":" << (isSetup ? "true" : "false") << "}";
                }
                std::cout << "]}\n";
            } catch (const std::exception& e) {
                std::cout << "{\"error\":\"" << e.what() << "\"}\n";
            }
            return 0;
        }

        if (arg == "--json-simulate") {
            int stratIdx = (argc > 2) ? std::stoi(argv[2]) : 3;
            double budgetSec = (argc > 3) ? std::stod(argv[3]) : 60.0;
            std::vector<int> changed;
            for (int i = 4; i < argc; ++i) {
                try { changed.push_back(std::stoi(argv[i])); } catch (...) {}
            }
            try {
                Project p = loadProject("data/files.csv", "data/tests.csv");
                HistoryStore hStore;
                try { hStore.loadFromCSV("data/history.csv"); } catch (...) {}
                GroundTruth t = gen.generateGroundTruth(p, cfg);
                Commit commit; commit.changedFiles = changed;
                PlantedBug bug = gen.plantBug(commit, cfg);
                if (!bug.hasBug && !changed.empty()) {
                    bug.hasBug = true;
                    bug.buggyFileId = changed[0];
                }
                DependencyGraph g; g.build(p);
                std::vector<int> uncovered;
                auto cands = g.findAffectedTests(p, changed, hStore.table(), uncovered);
                auto strats = makeStrategies(cfg.seed);
                if (stratIdx < 0 || stratIdx >= static_cast<int>(strats.size())) stratIdx = 3;
                Result r = strats[stratIdx]->select(cands, changed, budgetSec);
                auto impacted = g.reverseReachable(changed);
                auto detOutcomes = gen.sampleDetectionOutcomes(p, t, bug, impacted, cfg);
                auto normFails = gen.sampleNormalFailures(p, t);
                SimOutcome outcome = simulateExecution(p, r, t, bug, detOutcomes, normFails);
                std::cout << "{\"strategyName\":\"" << r.name << "\""
                          << ",\"bugApplicable\":" << (outcome.bugApplicable ? "true" : "false")
                          << ",\"buggyFileId\":" << bug.buggyFileId
                          << ",\"bugCaught\":" << (outcome.bugCaught ? "true" : "false")
                          << ",\"testsRun\":" << outcome.testsRun
                          << ",\"testsFailed\":" << outcome.testsFailed << "}\n";
            } catch (const std::exception& e) {
                std::cout << "{\"error\":\"" << e.what() << "\"}\n";
            }
            return 0;
        }

        if (arg == "--benchmark" || arg == "-b") {
            Project project = gen.generateProject(cfg);
            GroundTruth truth = gen.generateGroundTruth(project, cfg);
            HistoryStore history = gen.generateHistory(project, truth, cfg);
            int nCommits = (argc > 2) ? std::stoi(argv[2]) : 500;
            BenchmarkConfig bcfg;
            bcfg.numCommits = nCommits;
            bcfg.seed = cfg.seed;
            bcfg.genConfig = cfg;
            BenchmarkRunner runner(bcfg);
            auto strats = makeStrategies(cfg.seed);
            runner.runBenchmark(project, truth, history, strats);
            runner.runBudgetSweep(project, truth, history, strats);
            runner.runScaling(strats);
            auto hStats = BenchmarkRunner::compareHashTables();
            Report::writeHashStatsCSV("results/hash_stats.csv", hStats);
            std::cout << "Automated benchmark complete. Results saved in results/\n";
            return 0;
        }
    }

    std::cout << "=========================================\n";
    std::cout << "   TestPicker: Smart Test Selection CI   \n";
    std::cout << "=========================================\n";

    Project project = gen.generateProject(cfg);
    GroundTruth truth = gen.generateGroundTruth(project, cfg);
    HistoryStore history = gen.generateHistory(project, truth, cfg);

    try {
        saveProject(project, "data/files.csv", "data/tests.csv");
        history.saveToCSV("data/history.csv");
    } catch (...) {}

    while (true) {
        std::cout << "\n=== TestPicker Menu ===\n";
        std::cout << "1. Generate synthetic project\n";
        std::cout << "2. Load project from CSV files\n";
        std::cout << "3. Analyze a commit (runs ALL strategies)\n";
        std::cout << "4. Benchmark mode (multi-commit replay & sweeps)\n";
        std::cout << "5. Scaling mode (growing candidate sizes)\n";
        std::cout << "6. Show project summary\n";
        std::cout << "7. Compare custom HashTable vs std::unordered_map\n";
        std::cout << "8. Exit\n";
        std::cout << "> ";

        std::string choiceStr;
        if (!std::getline(std::cin, choiceStr)) break;
        if (choiceStr.empty()) continue;
        int choice = 0;
        try { choice = std::stoi(choiceStr); } catch (...) { continue; }

        if (choice == 8) {
            std::cout << "Exiting TestPicker. Goodbye!\n";
            break;
        }

        if (choice == 1) {
            std::cout << "Enter number of files [default 200]: ";
            std::string s;
            std::getline(std::cin, s);
            if (!s.empty()) cfg.numFiles = std::stoi(s);

            std::cout << "Enter number of tests [default 100]: ";
            std::getline(std::cin, s);
            if (!s.empty()) cfg.numTests = std::stoi(s);

            std::cout << "Enter seed [default 42]: ";
            std::getline(std::cin, s);
            if (!s.empty()) cfg.seed = static_cast<unsigned int>(std::stoul(s));

            gen = Generator(cfg.seed);
            project = gen.generateProject(cfg);
            truth = gen.generateGroundTruth(project, cfg);
            history = gen.generateHistory(project, truth, cfg);

            try {
                saveProject(project, "data/files.csv", "data/tests.csv");
                history.saveToCSV("data/history.csv");
            } catch (...) {}

            std::cout << "Successfully generated synthetic project and baseline history!\n";
            Report::printProjectSummary(project);
        }
        else if (choice == 2) {
            std::cout << "Files CSV path [default: data/files.csv]: ";
            std::string filesPath;
            std::getline(std::cin, filesPath);
            if (filesPath.empty()) filesPath = "data/files.csv";

            std::cout << "Tests CSV path [default: data/tests.csv]: ";
            std::string testsPath;
            std::getline(std::cin, testsPath);
            if (testsPath.empty()) testsPath = "data/tests.csv";

            try {
                project = loadProject(filesPath, testsPath);
                try {
                    history.loadFromCSV("data/history.csv");
                } catch (...) {}
                truth = gen.generateGroundTruth(project, cfg);
                std::cout << "Project loaded successfully!\n";
                Report::printProjectSummary(project);
            } catch (const std::exception& e) {
                std::cerr << "Error loading project: " << e.what() << "\n";
            }
        }
        else if (choice == 3) {
            if (project.files.empty() || project.tests.empty()) {
                std::cout << "Project is empty. Please generate or load a project first.\n";
                continue;
            }

            // Step 3: Validate prerequisites (DFS cycle check)
            std::vector<std::string> cyclePath;
            if (DependencyGraph::detectPrereqCycle(project, cyclePath)) {
                std::cout << "\n[Error] Prerequisite cycle detected!\nPath: ";
                for (size_t i = 0; i < cyclePath.size(); ++i) {
                    if (i > 0) std::cout << " -> ";
                    std::cout << cyclePath[i];
                }
                std::cout << "\nAborting commit analysis.\n";
                continue;
            }
            std::cout << "[Validate] Prerequisite graph OK (no cycles).\n";

            std::cout << "Changed files (space separated, or 'random'): ";
            std::string line;
            std::getline(std::cin, line);
            if (line.empty()) line = "random";

            Commit commit;
            PlantedBug bug;

            if (line == "random") {
                commit = gen.generateCommit(project, cfg);
                bug = gen.plantBug(commit, cfg);
                std::cout << "Generated commit with " << commit.changedFiles.size() << " changed files: ";
                for (int fid : commit.changedFiles) std::cout << "file_" << fid << ".cpp ";
                std::cout << "\n";
            } else {
                std::stringstream ss(line);
                std::string fname;
                while (ss >> fname) {
                    // Try parsing as ID or finding by name
                    int fid = -1;
                    for (const auto& f : project.files) {
                        if (f.name == fname || ("file_" + std::to_string(f.id) + ".cpp") == fname) {
                            fid = f.id;
                            break;
                        }
                    }
                    if (fid == -1) {
                        try { fid = std::stoi(fname); } catch (...) {}
                    }
                    if (fid >= 0 && fid < static_cast<int>(project.files.size())) {
                        commit.changedFiles.push_back(fid);
                    } else {
                        std::cout << "[Warning] Changed file '" << fname << "' not in project, ignoring.\n";
                    }
                }
                if (commit.changedFiles.empty()) {
                    std::cout << "No valid changed files. Nothing to analyze.\n";
                    continue;
                }
                bug = gen.plantBug(commit, cfg);
            }

            std::cout << "Time budget (seconds) [e.g. 60]: ";
            std::string bStr;
            std::getline(std::cin, bStr);
            double budgetSec = 60.0;
            if (!bStr.empty()) {
                try { budgetSec = std::stod(bStr); } catch (...) {}
            }

            DependencyGraph graph;
            graph.build(project);

            std::vector<int> uncoveredFiles;
            std::vector<Candidate> cands = graph.findAffectedTests(project, commit.changedFiles, history.table(), uncoveredFiles);

            std::cout << "[BFS] " << project.tests.size() << " tests total, "
                      << cands.size() << " affected. ";
            if (uncoveredFiles.empty()) {
                std::cout << "Changed files with no tests: none.\n";
            } else {
                std::cout << "Changed files with no tests: ";
                for (int uf : uncoveredFiles) std::cout << "file_" << uf << ".cpp ";
                std::cout << "\n";
            }

            if (cands.empty()) {
                std::cout << "No tests affected, nothing to run.\n";
                continue;
            }

            // Run ALL strategies on identical input
            auto strategies = makeStrategies(cfg.seed);
            std::vector<Result> results;
            results.reserve(strategies.size());

            for (auto& s : strategies) {
                Result res = s->select(cands, commit.changedFiles, budgetSec);
                res.actualTime = computeActualTime(project, res.selected);
                results.push_back(std::move(res));
            }

            Report::printComparisonTable(results, budgetSec);
            Report::printBestSummary(results);

            // Print Kahn execution order for the top detection strategy (Knapsack DP or Greedy Ratio)
            const Result* displayRes = &results[1]; // default Greedy Ratio
            for (const auto& r : results) {
                if (r.name == "Knapsack DP") { displayRes = &r; break; }
            }
            std::vector<int> allScheduled = gatherWithPrereqs(project, displayRes->selected);
            std::vector<int> ordered = kahnTopologicalSort(project, allScheduled);
            Report::printExecutionOrder(project, displayRes->name, ordered);

            // Optional simulation step
            std::cout << "\nSimulate execution of one strategy's set? Enter strategy number (1..5) or 0 to skip: ";
            std::string simChoiceStr;
            std::getline(std::cin, simChoiceStr);
            int simChoice = 0;
            try { simChoice = std::stoi(simChoiceStr); } catch (...) {}

            if (simChoice >= 1 && simChoice <= static_cast<int>(results.size())) {
                const auto& chosenRes = results[simChoice - 1];
                std::vector<bool> impacted = graph.reverseReachable(commit.changedFiles);
                std::vector<bool> detectionOutcomes = gen.sampleDetectionOutcomes(project, truth, bug, impacted, cfg);
                std::vector<bool> normalFailures = gen.sampleNormalFailures(project, truth);

                SimOutcome sim = simulateExecution(project, chosenRes, truth, bug, detectionOutcomes, normalFailures);
                std::cout << "\n[Simulation Result for " << chosenRes.name << "]\n";
                std::cout << "  Tests executed: " << sim.testsRun << "\n";
                std::cout << "  Tests failed  : " << sim.testsFailed << "\n";
                if (sim.bugApplicable) {
                    std::cout << "  Planted bug   : PRESENT in file_" << bug.buggyFileId << ".cpp\n";
                    std::cout << "  Bug caught    : " << (sim.bugCaught ? "YES (DETECTED!)" : "NO (MISSED!)") << "\n";
                } else {
                    std::cout << "  Planted bug   : None planted in this commit.\n";
                }

                std::cout << "Update history with simulation outcomes? (y/n) [default: y]: ";
                std::string upStr;
                std::getline(std::cin, upStr);
                if (upStr.empty() || upStr == "y" || upStr == "Y") {
                    updateHistoryFromSimulation(history, project, results, truth, bug, detectionOutcomes, normalFailures);
                    std::cout << "History updated.\n";
                }
            }
        }
        else if (choice == 4) {
            std::cout << "Number of commits to simulate [default 500]: ";
            std::string s;
            std::getline(std::cin, s);
            int nCommits = 500;
            if (!s.empty()) {
                try { nCommits = std::stoi(s); } catch (...) {}
            }

            std::cout << "Budget fraction of affected test time [default 0.25]: ";
            std::getline(std::cin, s);
            double bFraction = 0.25;
            if (!s.empty()) {
                try { bFraction = std::stod(s); } catch (...) {}
            }

            BenchmarkConfig bcfg;
            bcfg.numCommits = nCommits;
            bcfg.budgetFraction = bFraction;
            bcfg.seed = cfg.seed;
            bcfg.genConfig = cfg;

            BenchmarkRunner runner(bcfg);
            auto strategies = makeStrategies(cfg.seed);
            runner.runBenchmark(project, truth, history, strategies);
            runner.runBudgetSweep(project, truth, history, strategies);

            auto hStats = BenchmarkRunner::compareHashTables();
            Report::writeHashStatsCSV("results/hash_stats.csv", hStats);

            std::cout << "Benchmark CSV outputs written to results/ directory.\n";
        }
        else if (choice == 5) {
            BenchmarkConfig bcfg;
            bcfg.seed = cfg.seed;
            bcfg.genConfig = cfg;
            BenchmarkRunner runner(bcfg);
            auto strategies = makeStrategies(cfg.seed);
            runner.runScaling(strategies);
            std::cout << "Scaling results written to results/scaling.csv\n";
        }
        else if (choice == 6) {
            Report::printProjectSummary(project);
        }
        else if (choice == 7) {
            std::cout << "Running comparison between custom HashTable and std::unordered_map (50,000 ops)...\n";
            auto stats = BenchmarkRunner::compareHashTables(50000);
            std::cout << "Custom HashTable time       : " << stats.customTotalTimeMs << " ms\n";
            std::cout << "std::unordered_map time      : " << stats.stdTotalTimeMs << " ms\n";
            std::cout << "Custom table avg probes/op   : " << stats.customProbesPerLookup << "\n";
            std::cout << "Custom table final load factor: " << stats.customLoadFactor << "\n";
            Report::writeHashStatsCSV("results/hash_stats.csv", stats);
            std::cout << "Saved to results/hash_stats.csv\n";
        }
    }

    return 0;
}
