cmake_minimum_required(VERSION 3.15)
project(LOBEngine VERSION 1.0.0 LANGUAGES CXX)

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

# Main Executable (Verification)
add_executable(lob_sim src/main.cpp)
target_link_libraries(lob_sim PRIVATE lob_core)

# --- Python Bindings ---
include(FetchContent)
FetchContent_Declare(
  pybind11
  GIT_REPOSITORY https://github.com/pybind/pybind11.git
  GIT_TAG        v2.11.1 
)
FetchContent_MakeAvailable(pybind11)

pybind11_add_module(lob_py pybind/PyBindings.cpp)
target_link_libraries(lob_py PRIVATE lob_core)

