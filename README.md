# TestPicker

An Advanced Data Structures and Algorithms (ADSA) C++17 command-line system that solves the regression test selection problem: **Given changed source files and a time budget, which tests should run, and in what order?**

TestPicker implements custom data structures and algorithms without external third-party dependencies, running and comparing five distinct algorithmic selection strategies side by side on every commit.

---

## 1. Key Features & Syllabus Mapping

- **Custom Hash Table (`HashTable<K, V>`)**:
  - Separate chaining with collision resolution.
  - Dynamic rehashing at load factor $\ge 0.75$.
  - Probe tracking for rigorous complexity analysis against `std::unordered_map`.
- **Graph Algorithms (`DependencyGraph`)**:
  - Multi-source reverse **BFS** for transitive impact discovery from changed files.
  - 3-color (White/Gray/Black) **DFS** for prerequisite cycle detection with path reconstruction.
- **Topological Sorting (`topo.hpp`)**:
  - **Kahn's algorithm** with in-degree queues to generate a valid test execution sequence with shared prerequisite deduplication.
- **Five Algorithmic Selection Strategies**:
  1. **Random Baseline**: Uniform shuffle bounded by budget constraint.
  2. **Greedy Ratio**: Sort by $p_i / t_i$ to greedily capture high-probability failures.
  3. **Greedy Set Cover**: Approximation for weighted set cover prioritizing unexercised code, filling residual budget with greedy ratio.
  4. **Knapsack DP**: Dynamic programming with additive log-survival value transformation ($v_i = -\ln(1 - p_i)$) for mathematically exact detection probability maximization. Includes both 2D table reconstruction and 1D rolling array.
  5. **Branch and Bound**: Pruned backtracking search with admissible lower bounding, dominance preprocessing, and timeout guards.
- **Benchmark & Simulation Engine**:
  - Multi-commit replay with hidden ground truth failure rates, flaky tests, and planted bugs.
  - Automated CSV export for commits, aggregate summaries, budget sweeps, scaling tests, and hash performance.

---

## 2. Project Layout

```
├── CMakeLists.txt              # Standard CMake build configuration
├── README.md                   # System documentation & usage guide
├── DECISIONS.md                # Architectural design choices & rationales
├── REPORT.md                   # Algorithmic complexity table & empirical evaluation
├── include/                    # Header interfaces
│   ├── types.hpp               # Core data structures (SourceFile, Test, History, Commit)
│   ├── hashtable.hpp           # Custom templated separate-chaining HashTable
│   ├── graph.hpp               # Reverse BFS and DFS cycle detection
│   ├── topo.hpp                # Kahn topological sorting & actual runtime computation
│   ├── history.hpp             # Test execution history backed by custom HashTable
│   ├── generator.hpp           # Synthetic DAG project, commit & bug generator
│   ├── simulator.hpp           # Simulation against hidden ground truth
│   ├── strategy.hpp            # Abstract Strategy interface & Result structures
│   ├── strategies/
│   │   ├── random_baseline.hpp # Strategy 1: Random baseline
│   │   ├── greedy_ratio.hpp    # Strategy 2: Greedy ratio
│   │   ├── greedy_cover.hpp    # Strategy 3: Greedy set cover
│   │   ├── knapsack_dp.hpp     # Strategy 4: Knapsack DP (2D & 1D)
│   │   └── branch_bound.hpp    # Strategy 5: Branch and Bound
│   ├── benchmark.hpp           # Multi-commit replay, budget sweeps & scaling
│   ├── report.hpp              # Console tables and CSV formatters
│   └── io.hpp                  # CSV loading and saving
├── src/                        # Implementation files & CLI main driver
├── tests/                      # Unit test harness (9 test suites)
├── data/                       # CSV project datasets (files.csv, tests.csv, history.csv)
└── results/                    # Generated experimental benchmark CSV files
```

---

## 3. Building the Project

### Using CMake
```bash
mkdir build
cd build
cmake ..
cmake --build . --config Release
```

### Direct GCC / MinGW Compilation
```bash
# Build unit test runner
g++ -std=c++17 -O2 -Iinclude src/graph.cpp src/topo.cpp src/history.cpp src/io.cpp src/generator.cpp src/simulator.cpp src/benchmark.cpp src/report.cpp src/strategies/random_baseline.cpp src/strategies/greedy_ratio.cpp src/strategies/greedy_cover.cpp src/strategies/knapsack_dp.cpp src/strategies/branch_bound.cpp tests/test_main.cpp -o testpicker_tests.exe

# Build interactive application
g++ -std=c++17 -O2 -Iinclude src/graph.cpp src/topo.cpp src/history.cpp src/io.cpp src/generator.cpp src/simulator.cpp src/benchmark.cpp src/report.cpp src/strategies/random_baseline.cpp src/strategies/greedy_ratio.cpp src/strategies/greedy_cover.cpp src/strategies/knapsack_dp.cpp src/strategies/branch_bound.cpp src/main.cpp -o testpicker.exe
```

---

## 4. Running the Tests & Benchmark

### Run All 9 Unit Test Suites
```bash
./testpicker_tests.exe
```
Output:
```
=================================================
       TestPicker Comprehensive Unit Tests       
=================================================
[Test 1/9] Testing Custom HashTable... PASSED
[Test 2/9] Testing BFS and Affected Tests Discovery... PASSED
[Test 3/9] Testing Prerequisite Cycle Detection... PASSED
[Test 4/9] Testing Kahn's Topological Sort & Prereq Cost... PASSED
[Test 5/9] Testing Knapsack DP vs Brute Force (n=12)... PASSED
[Test 6/9] Testing Branch & Bound vs Brute Force (n=10, |C|=5)... PASSED
[Test 7/9] Testing Greedy Set Cover Guarantees... PASSED
[Test 8/9] Testing Budget Invariant Across Strategies... PASSED
[Test 9/9] Testing Synthetic Generator Determinism & DAG Guarantees... PASSED
=================================================
   ALL 9 TEST SUITES PASSED CLEANLY & SAFELY!   
=================================================
```

### Automated Benchmark Mode
```bash
./testpicker.exe --benchmark 500
```
This runs 500 simulated commits, budget sweeps (5%, 10%, 25%, 50%), scaling tests (sizes 20 to 2560), and hash table benchmarks, writing full results to `results/`.

---

## 5. Interactive Session Walkthrough

```
=== TestPicker Menu ===
1. Generate synthetic project
2. Load project from CSV files
3. Analyze a commit (runs ALL strategies)
4. Benchmark mode (multi-commit replay & sweeps)
5. Scaling mode (growing candidate sizes)
6. Show project summary
7. Compare custom HashTable vs std::unordered_map
8. Exit
> 3

[Validate] Prerequisite graph OK (no cycles).
Changed files (space separated, or 'random'): random
Generated commit with 3 changed files: file_4.cpp file_14.cpp file_24.cpp 
Time budget (seconds) [e.g. 60]: 60
[BFS] 100 tests total, 23 affected. Changed files with no tests: none.

Strategy               Tests   Time(s)     Covered   Detect%    AlgoTime   Status
--------------------------------------------------------------------------------
Random baseline            7      54.8         3/3     61.2%     0.00 ms   ok
Greedy Ratio               9      59.1         3/3     79.4%     0.00 ms   ok
Greedy Set Cover           9      58.4         3/3     81.2%     0.01 ms   ok
Knapsack DP                9      59.8         3/3     82.5%     0.02 ms   ok
Branch & Bound             3      18.2         3/3     64.1%     0.00 ms   optimal cover

Best detection within budget: Knapsack DP (82.5%)
Branch & Bound result: 3 tests, 18.2 s (optimal cover)

Execution order for Knapsack DP (with prerequisites):
  test_12 -> test_35 -> test_18 -> test_77 -> test_84 -> test_91
```
