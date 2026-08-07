# Unit and parity tests. Phase 0 has a single smoke test that verifies the
# framework, build, and sanitizers are wired up correctly.

add_executable(intsect-tests
  unit/smoke_test.cpp
  unit/types_test.cpp
  unit/tile_test.cpp
  unit/hex_set_test.cpp
  unit/board_state_test.cpp)

target_link_libraries(intsect-tests PRIVATE
  intsect-engine
  intsect_options)

# Include doctest as a SYSTEM header so our warnings-as-errors do not fire on
# doctest's own code (e.g. its use of __COUNTER__ under -Wpedantic).
target_include_directories(intsect-tests SYSTEM PRIVATE ${doctest_SOURCE_DIR})

intsect_copy_runtime_deps(intsect-tests)

# Register each doctest TEST_CASE with CTest so `ctest` runs them individually.
include(${doctest_SOURCE_DIR}/scripts/cmake/doctest.cmake)
doctest_discover_tests(intsect-tests)
