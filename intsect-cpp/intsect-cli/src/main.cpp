// Command-line front end for the engine.
// Phase 0 placeholder: prints the version so the executable target builds and runs.

#include "intsect/version.hpp"

#include <cstdio>


int main() {
    std::printf("%s\n", intsect::version());
    return 0;
}
