// Command-line front end for the engine.
// Phase 0 placeholder: prints the version so the executable target builds and runs.

#include <cstdio>

#include "intsect/version.hpp"

int main() {
  std::printf("%s\n", intsect::version());
  return 0;
}
