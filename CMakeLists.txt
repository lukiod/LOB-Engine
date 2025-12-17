cmake_minimum_required(VERSION 3.15)
project(LOBEngine VERSION 1.0.0 LANGUAGES CXX)

include(FetchContent)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Optimization flags
if(MSVC)
    add_compile_options(/O2 /Oi /Ot /Oy /GL)
else()
    add_compile_options(-O3 -march=native -flto)
endif()

# Include directories
include_directories(include)

# Library
add_library(lob_core INTERFACE)
target_include_directories(lob_core INTERFACE include)

# Google Benchmark
set(BENCHMARK_ENABLE_TESTING OFF CACHE BOOL "Disable benchmark testing" FORCE)
FetchContent_Declare(
  googlebenchmark
  GIT_REPOSITORY https://github.com/google/benchmark.git
  GIT_TAG v1.8.3
)
FetchContent_MakeAvailable(googlebenchmark)

# Google Test
FetchContent_Declare(
  googletest
  GIT_REPOSITORY https://github.com/google/googletest.git
  GIT_TAG v1.14.0
)
# Prevent GTest from overriding parent compile options
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

# --- Executables ---

# 1. Main Simulation
add_executable(lob_sim src/main.cpp)
target_link_libraries(lob_sim PRIVATE lob_core)

# 2. Benchmarks
add_executable(lob_bench src/benchmarks.cpp)
target_link_libraries(lob_bench PRIVATE lob_core benchmark::benchmark)

# 3. Unit Tests
enable_testing()
add_executable(lob_test tests/test_orderbook.cpp)
target_link_libraries(lob_test PRIVATE lob_core GTest::gtest_main)

# --- Python Bindings ---
FetchContent_Declare(
  pybind11
  GIT_REPOSITORY https://github.com/pybind/pybind11.git
  GIT_TAG        v2.11.1 
)
FetchContent_MakeAvailable(pybind11)

pybind11_add_module(lob_py pybind/PyBindings.cpp)
target_link_libraries(lob_py PRIVATE lob_core)

