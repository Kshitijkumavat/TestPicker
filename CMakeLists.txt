cmake_minimum_required(VERSION 3.14)
project(TestPicker VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Compiler-specific warning and optimization flags
if(MSVC)
    add_compile_options(/W4 /O2 /EHsc)
else()
    add_compile_options(-Wall -Wextra -O2)
endif()

# ── Main executable ──────────────────────────────────────────────
set(LIB_SOURCES
    src/graph.cpp
    src/topo.cpp
    src/history.cpp
    src/io.cpp
    src/generator.cpp
    src/simulator.cpp
    src/benchmark.cpp
    src/report.cpp
    src/strategies/random_baseline.cpp
    src/strategies/greedy_ratio.cpp
    src/strategies/greedy_cover.cpp
    src/strategies/knapsack_dp.cpp
    src/strategies/branch_bound.cpp
)

add_executable(testpicker src/main.cpp ${LIB_SOURCES})
target_include_directories(testpicker PRIVATE include)

# ── Unit-test executable ─────────────────────────────────────────
add_executable(testpicker_tests tests/test_main.cpp ${LIB_SOURCES})
target_include_directories(testpicker_tests PRIVATE include)

# ── Ensure output directories exist ─────────────────────────────
file(MAKE_DIRECTORY ${CMAKE_SOURCE_DIR}/results)
file(MAKE_DIRECTORY ${CMAKE_SOURCE_DIR}/data)
