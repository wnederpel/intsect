// Developer tools (perft drivers, fixture generators, parity helpers) will live
// here later. Phase 0 placeholder: prints the version so the target builds and runs.

#include "intsect/version.hpp"

#include <cstdio>


int main() {
    std::printf("intsect-tools: %s\n", intsect::version());
    return 0;
}
