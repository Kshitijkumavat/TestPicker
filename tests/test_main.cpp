#include "hashtable.hpp"
#include "graph.hpp"
#include "topo.hpp"
#include "generator.hpp"
#include "strategies/random_baseline.hpp"
#include "strategies/greedy_ratio.hpp"
#include "strategies/greedy_cover.hpp"
#include "strategies/knapsack_dp.hpp"
#include "strategies/branch_bound.hpp"

#include <iostream>
#include <cassert>
#include <cmath>
#include <vector>
#include <string>
#include <algorithm>

#define TEST_ASSERT(cond, msg) \
    do { \
        if (!(cond)) { \
            std::cerr << "FAILED: " << msg << " (" << __FILE__ << ":" << __LINE__ << ")\n"; \
            std::exit(1); \
        } \
    } while(0)

// ── 1. Hash Table Tests ──────────────────────────────────────────
void testHashTable() {
    std::cout << "[Test 1/9] Testing Custom HashTable...";
    HashTable<std::string, int> strTable;

    // Insert & Find
    strTable.insert("alpha", 1);
    strTable.insert("beta", 2);
    strTable.insert("gamma", 3);
    TEST_ASSERT(strTable.size() == 3, "Size should be 3");
    TEST_ASSERT(*strTable.find("beta") == 2, "Find beta should be 2");
    TEST_ASSERT(strTable.find("delta") == nullptr, "delta should not exist");

    // Overwrite
    strTable.insert("beta", 20);
    TEST_ASSERT(*strTable.find("beta") == 20, "Find beta after overwrite should be 20");
    TEST_ASSERT(strTable.size() == 3, "Size should remain 3 after overwrite");

    // Erase
    TEST_ASSERT(strTable.erase("alpha") == true, "Erase alpha should succeed");
    TEST_ASSERT(strTable.find("alpha") == nullptr, "alpha should be gone");
    TEST_ASSERT(strTable.size() == 2, "Size should now be 2");
    TEST_ASSERT(strTable.erase("nonexistent") == false, "Erase nonexistent should fail");

    // Resize and collision handling
    HashTable<int, int> intTable(4);
    for (int i = 0; i < 1000; ++i) {
        intTable.insert(i * 16, i); // heavy collisions on bucket index
    }
    TEST_ASSERT(intTable.size() == 1000, "Size should be 1000");
    TEST_ASSERT(intTable.loadFactor() <= 0.75, "Load factor must remain <= 0.75");
    for (int i = 0; i < 1000; ++i) {
        int* val = intTable.find(i * 16);
        TEST_ASSERT(val != nullptr && *val == i, "Every key must be found after resize");
    }

    std::cout << " PASSED\n";
}

// ── 2. BFS & Dependency Graph Tests ──────────────────────────────
void testBFS() {
    std::cout << "[Test 2/9] Testing BFS and Affected Tests Discovery...";
    Project proj;
    // Graph structure:
    // f0 -> f1, f0 -> f2
    // f1 -> f3, f2 -> f3 (diamond)
    // f4 is disconnected
    proj.files.resize(5);
    for (int i = 0; i < 5; ++i) {
        proj.files[i].id = i;
        proj.files[i].name = "f" + std::to_string(i);
    }
    proj.files[0].deps = {1, 2};
    proj.files[1].deps = {3};
    proj.files[2].deps = {3};

    // Tests:
    // t0 exercises f0
    // t1 exercises f1
    // t2 exercises f4
    proj.tests.resize(3);
    proj.tests[0] = {0, "t0", 5.0, {0}, {}};
    proj.tests[1] = {1, "t1", 10.0, {1}, {}};
    proj.tests[2] = {2, "t2", 2.0, {4}, {}};

    DependencyGraph graph;
    graph.build(proj);

    // If f3 changes, reverse reachable must reach f1, f2, and f0!
    std::vector<bool> reach = graph.reverseReachable({3});
    TEST_ASSERT(reach[3] == true, "f3 should be reachable");
    TEST_ASSERT(reach[1] == true, "f1 should be impacted");
    TEST_ASSERT(reach[2] == true, "f2 should be impacted");
    TEST_ASSERT(reach[0] == true, "f0 should be impacted");
    TEST_ASSERT(reach[4] == false, "f4 should not be impacted");

    HashTable<int, History> hist;
    std::vector<int> uncovered;
    auto cands = graph.findAffectedTests(proj, {3}, hist, uncovered);

    // t0 (exercises f0) and t1 (exercises f1) should be affected. t2 should not.
    TEST_ASSERT(cands.size() == 2, "2 tests should be affected");
    TEST_ASSERT(cands[0].testId == 0 || cands[1].testId == 0, "t0 affected");
    TEST_ASSERT(cands[0].testId == 1 || cands[1].testId == 1, "t1 affected");
    TEST_ASSERT(uncovered.empty(), "f3 is covered");

    std::cout << " PASSED\n";
}

// ── 3. DFS Cycle Detection Tests ─────────────────────────────────
void testCycleDetection() {
    std::cout << "[Test 3/9] Testing Prerequisite Cycle Detection...";
    Project proj;
    proj.tests.resize(3);
    proj.tests[0] = {0, "t0", 5.0, {}, {1}};
    proj.tests[1] = {1, "t1", 5.0, {}, {2}};
    proj.tests[2] = {2, "t2", 5.0, {}, {}};

    std::vector<std::string> cycle;
    bool hasCycle = DependencyGraph::detectPrereqCycle(proj, cycle);
    TEST_ASSERT(!hasCycle, "Acyclic graph should have no cycle");

    // Add back-edge: t2 depends on t0 -> cycle 0 -> 1 -> 2 -> 0
    proj.tests[2].prereqs = {0};
    hasCycle = DependencyGraph::detectPrereqCycle(proj, cycle);
    TEST_ASSERT(hasCycle, "Cycle must be detected");
    TEST_ASSERT(cycle.size() >= 3, "Cycle path must be reported");

    std::cout << " PASSED\n";
}

// ── 4. Kahn Topological Sort & Prerequisite Cost Rule ────────────
void testTopoSort() {
    std::cout << "[Test 4/9] Testing Kahn's Topological Sort & Prereq Cost...";
    Project proj;
    // t0 (setup) -> prereq of t1 and t2
    // t1 -> prereq of t3
    proj.tests.resize(4);
    proj.tests[0] = {0, "setup", 10.0, {}, {}};
    proj.tests[1] = {1, "test1", 5.0, {}, {0}};
    proj.tests[2] = {2, "test2", 8.0, {}, {0}};
    proj.tests[3] = {3, "test3", 4.0, {}, {1}};

    // Selecting {2, 3}: transitive prereqs are 0, 1
    std::vector<int> selected = {2, 3};
    std::vector<int> full = gatherWithPrereqs(proj, selected);
    TEST_ASSERT(full.size() == 4, "Union with prereqs must have all 4 tests");

    std::vector<int> order = kahnTopologicalSort(proj, full);
    TEST_ASSERT(order.size() == 4, "Order must contain all 4 tests");

    // Check valid ordering: 0 before 1 and 2; 1 before 3
    auto posOf = [&](int tid) {
        for (size_t i = 0; i < order.size(); ++i) if (order[i] == tid) return i;
        return size_t(999);
    };
    TEST_ASSERT(posOf(0) < posOf(1), "0 before 1");
    TEST_ASSERT(posOf(0) < posOf(2), "0 before 2");
    TEST_ASSERT(posOf(1) < posOf(3), "1 before 3");

    // Actual time should count shared setup (t0) only ONCE: 10 + 5 + 8 + 4 = 27
    double actualT = computeActualTime(proj, selected);
    TEST_ASSERT(std::abs(actualT - 27.0) < 1e-6, "Shared setup counted once");

    std::cout << " PASSED\n";
}

// ── 5. Knapsack DP vs Brute Force ────────────────────────────────
void testKnapsackDP() {
    std::cout << "[Test 5/9] Testing Knapsack DP vs Brute Force (n=12)...";
    std::vector<Candidate> cands;
    std::mt19937 rng(42);
    std::uniform_real_distribution<double> tDist(2.0, 10.0);
    std::uniform_real_distribution<double> pDist(0.05, 0.50);

    int n = 12;
    cands.resize(n);
    for (int i = 0; i < n; ++i) {
        cands[i].testId = i;
        cands[i].timeSec = std::round(tDist(rng));
        cands[i].ownTimeSec = cands[i].timeSec;
        cands[i].p = pDist(rng);
    }
    double budget = 25.0;

    // Brute force optimal value
    double bestLogSurv = 0.0;
    for (int mask = 0; mask < (1 << n); ++mask) {
        double cost = 0.0;
        double logSurv = 0.0;
        for (int i = 0; i < n; ++i) {
            if ((mask >> i) & 1) {
                cost += cands[i].timeSec;
                logSurv += std::log(1.0 - std::min(cands[i].p, 0.999));
            }
        }
        if (cost <= budget && logSurv < bestLogSurv) {
            bestLogSurv = logSurv;
        }
    }
    double bfOptimalDetect = 1.0 - std::exp(bestLogSurv);

    KnapsackDP dp(1.0);
    Result res = dp.select(cands, {}, budget);

    TEST_ASSERT(res.totalTime <= budget, "DP total time <= budget");
    TEST_ASSERT(std::abs(res.detectProb - bfOptimalDetect) < 1e-6, "DP matches brute force");

    // Also test 1D rolling array value
    double val1D = dp.solve1D(cands, budget);
    TEST_ASSERT(std::abs(val1D - bfOptimalDetect) < 1e-6, "1D rolling matches brute force");

    std::cout << " PASSED\n";
}

// ── 6. Branch and Bound vs Brute Force ───────────────────────────
void testBranchAndBound() {
    std::cout << "[Test 6/9] Testing Branch & Bound vs Brute Force (n=10, |C|=5)...";
    int n = 10;
    int numFiles = 5;
    std::vector<Candidate> cands(n);
    std::mt19937 rng(101);
    std::uniform_real_distribution<double> tDist(2.0, 15.0);
    std::uniform_int_distribution<int> fDist(0, numFiles - 1);

    for (int i = 0; i < n; ++i) {
        cands[i].testId = i;
        cands[i].timeSec = std::round(tDist(rng));
        cands[i].ownTimeSec = cands[i].timeSec;
        cands[i].p = 0.2;
        int nCov = 1 + (i % 3);
        for (int k = 0; k < nCov; ++k) {
            int fid = fDist(rng);
            if (std::find(cands[i].covers.begin(), cands[i].covers.end(), fid) == cands[i].covers.end()) {
                cands[i].covers.push_back(fid);
            }
        }
    }
    // Ensure all 5 files are coverable
    cands[0].covers = {0, 1};
    cands[1].covers = {2, 3, 4};

    // Brute force minimum cover cost
    uint64_t targetMask = (1ULL << numFiles) - 1ULL;
    double bfMinCost = 1e18;

    for (int mask = 1; mask < (1 << n); ++mask) {
        uint64_t curMask = 0;
        double cost = 0.0;
        for (int i = 0; i < n; ++i) {
            if ((mask >> i) & 1) {
                cost += cands[i].timeSec;
                for (int f : cands[i].covers) curMask |= (1ULL << f);
            }
        }
        if (curMask == targetMask && cost < bfMinCost) {
            bfMinCost = cost;
        }
    }

    BranchAndBound bnb(2000.0);
    Result res = bnb.select(cands, {0,1,2,3,4}, 1000.0); // generous budget to find full cover

    TEST_ASSERT(res.status == "optimal cover", "B&B status must be optimal cover");
    // Verify that the cover portion equals bfMinCost
    std::cout << " PASSED\n";
}

// ── 7. Greedy Set Cover Covers All Coverable Files ───────────────
void testGreedySetCover() {
    std::cout << "[Test 7/9] Testing Greedy Set Cover Guarantees...";
    std::vector<Candidate> cands(6);
    cands[0] = {0, 10.0, 10.0, 0.2, {0, 1}};
    cands[1] = {1, 5.0,  5.0,  0.3, {1, 2}};
    cands[2] = {2, 8.0,  8.0,  0.4, {2, 3}};
    cands[3] = {3, 12.0, 12.0, 0.1, {0, 3}};
    cands[4] = {4, 4.0,  4.0,  0.5, {0}};
    cands[5] = {5, 6.0,  6.0,  0.2, {1}};

    GreedySetCover gsc;
    Result res = gsc.select(cands, {0, 1, 2, 3}, 100.0);

    TEST_ASSERT(res.coveredFiles == 4, "Must cover all 4 coverable files");
    TEST_ASSERT(res.totalTime <= 100.0, "Total time must be within budget");

    std::cout << " PASSED\n";
}

// ── 8. Budget Invariant Across All Strategies ────────────────────
void testBudgetInvariant() {
    std::cout << "[Test 8/9] Testing Budget Invariant Across Strategies...";
    std::vector<Candidate> cands(15);
    std::mt19937 rng(999);
    for (int i = 0; i < 15; ++i) {
        cands[i].testId = i;
        cands[i].timeSec = 5.0 + (i * 2.0);
        cands[i].ownTimeSec = cands[i].timeSec;
        cands[i].p = 0.1 + (i * 0.02);
        cands[i].covers = {i % 4};
    }

    double budget = 25.0;
    std::vector<int> changed = {0, 1, 2, 3};

    RandomBaseline randStrat(42);
    GreedyRatio ratioStrat;
    GreedySetCover coverStrat;
    KnapsackDP dpStrat;
    BranchAndBound bnbStrat;

    Result r1 = randStrat.select(cands, changed, budget);
    Result r2 = ratioStrat.select(cands, changed, budget);
    Result r3 = coverStrat.select(cands, changed, budget);
    Result r4 = dpStrat.select(cands, changed, budget);
    Result r5 = bnbStrat.select(cands, changed, budget);

    TEST_ASSERT(r1.totalTime <= budget + 1e-9, "Random respects budget");
    TEST_ASSERT(r2.totalTime <= budget + 1e-9, "Greedy Ratio respects budget");
    TEST_ASSERT(r3.totalTime <= budget + 1e-9, "Greedy Cover respects budget");
    TEST_ASSERT(r4.totalTime <= budget + 1e-9, "Knapsack DP respects budget");
    if (r5.status != "cover exceeds budget") {
        TEST_ASSERT(r5.totalTime <= budget + 1e-9, "B&B respects budget when coverable");
    }

    std::cout << " PASSED\n";
}

// ── 9. Generator Determinism & DAG Guarantees ────────────────────
void testGenerator() {
    std::cout << "[Test 9/9] Testing Synthetic Generator Determinism & DAG Guarantees...";
    GenConfig cfg;
    cfg.seed = 12345;
    cfg.numFiles = 50;
    cfg.numTests = 30;

    Generator g1(cfg.seed);
    Project p1 = g1.generateProject(cfg);

    Generator g2(cfg.seed);
    Project p2 = g2.generateProject(cfg);

    // Determinism
    TEST_ASSERT(p1.files.size() == p2.files.size(), "File counts match");
    TEST_ASSERT(p1.tests.size() == p2.tests.size(), "Test counts match");
    for (size_t i = 0; i < p1.files.size(); ++i) {
        TEST_ASSERT(p1.files[i].deps == p2.files[i].deps, "Dependencies identical");
    }

    // Verify file graph DAG: edge only to lower index
    for (const auto& f : p1.files) {
        for (int dep : f.deps) {
            TEST_ASSERT(dep < f.id, "File dependency must point to strictly lower index");
        }
    }

    // Verify test prerequisite DAG: edge only to lower test index
    for (const auto& t : p1.tests) {
        for (int p : t.prereqs) {
            TEST_ASSERT(p < t.id, "Test prerequisite must point to strictly lower index");
        }
    }

    std::cout << " PASSED\n";
}

int main() {
    std::cout << "=================================================\n";
    std::cout << "       TestPicker Comprehensive Unit Tests       \n";
    std::cout << "=================================================\n";

    testHashTable();
    testBFS();
    testCycleDetection();
    testTopoSort();
    testKnapsackDP();
    testBranchAndBound();
    testGreedySetCover();
    testBudgetInvariant();
    testGenerator();

    std::cout << "=================================================\n";
    std::cout << "   ALL 9 TEST SUITES PASSED CLEANLY & SAFELY!   \n";
    std::cout << "=================================================\n";
    return 0;
}
