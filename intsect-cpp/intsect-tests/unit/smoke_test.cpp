// Smoke test: proves the test framework, build, and sanitizers all work.
// This single translation unit also provides doctest's main().

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include <cstring>

#include "intsect/version.hpp"

TEST_CASE("engine version is non-empty") {
  const char* v = intsect::version();
  REQUIRE(v != nullptr);
  CHECK(std::strlen(v) > 0);
}
