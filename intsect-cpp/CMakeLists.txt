# Top-level build for the C++ rewrite of the Intsect Hive engine.
#
# Phase 0 goal: a clean, self-contained C++23 project that configures, builds,
# and runs an (empty) test suite, with sanitizers in debug and a guardrail that
# rejects banned language constructs. See intsect-docs/CPP_DIALECT.md.

cmake_minimum_required(VERSION 3.24)

project(Intsect
  VERSION 0.0.0
  DESCRIPTION "C++ rewrite of the Intsect Hive engine"
  LANGUAGES CXX)

# Keep the language strict: C++23, no compiler extensions, standard required.
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Default to Debug when no build type is given (single-config generators).
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE Debug CACHE STRING "Build type" FORCE)
endif()

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

# Shared compiler options: warnings-as-errors, C++23, and debug sanitizers.
# Everything links this INTERFACE target so the rules apply uniformly.
include(CompilerOptions)

enable_testing()

# Lightweight test framework, fetched at configure time (network needed once).
# doctest 2.4.11 ships an old cmake_minimum_required that CMake 4 rejects; this
# variable tells CMake to treat that subproject as if it targeted policy 3.5.
include(FetchContent)
set(CMAKE_POLICY_VERSION_MINIMUM 3.5)
FetchContent_Declare(doctest
  GIT_REPOSITORY https://github.com/doctest/doctest.git
  GIT_TAG v2.4.11)
FetchContent_MakeAvailable(doctest)

add_subdirectory(intsect-engine)
add_subdirectory(intsect-cli)
add_subdirectory(intsect-tools)
add_subdirectory(intsect-tests)
