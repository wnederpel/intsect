# Phase 0 summary

## What was built

The empty project skeleton that everything else grows inside. No Hive logic yet — purely build system, compiler configuration, and a working test runner.

### The four targets

| Target | Kind | Purpose |
|---|---|---|
| `intsect-engine` | static library | All game/search/eval logic will live here (Phase 1+). Currently holds only `version()`. |
| `intsect-cli` | executable | User-facing entry point. Prints the version. |
| `intsect-tools` | executable | Developer utilities: perft drivers, fixture generators, parity helpers. Prints the version. |
| `intsect-tests` | executable | Test suite. Driven by doctest; registered with CTest. |

### Compiler configuration (`cmake/CompilerOptions.cmake`)

Every target links one shared INTERFACE target called `intsect_options` that carries:

- **C++23**, compiler extensions off, standard required.
- **Warnings-as-errors**: `-Wall -Wextra -Wpedantic -Wshadow -Wconversion -Werror`.
- **Debug sanitizers**: AddressSanitizer (`-fsanitize=address`) and UndefinedBehaviorSanitizer in trap mode (`-fsanitize=undefined -fsanitize-trap=undefined`). Trap mode means undefined behavior aborts immediately, no extra runtime library required.

One shared location means warning and sanitizer rules can never drift apart between targets.

### Test framework: doctest

Pulled in via CMake `FetchContent` (network required once, then cached). Chosen over Catch2 because it is a single header, compiles faster, and is simpler to read. The `smoke_test.cpp` file provides doctest's `main()` and one test that verifies the version string is non-empty.

### Build presets (`CMakePresets.json`)

| Preset | Compiler | Build type | Sanitizers |
|---|---|---|---|
| `debug` | `clang++` | Debug (`-O0 -g`) | ASan + UBSan |
| `release` | `clang++` | Release | None |

### Dialect rules (`intsect-docs/CPP_DIALECT.md`)

A summary of the allowed C++ subset lives in the C++ tree. The rules are **convention, not automated enforcement** — there is no scanner. Constructs to avoid: `shared_ptr`, `virtual`, `try`/`catch`.

---

## Windows/toolchain notes

Three Windows-specific issues arose and were solved; they are commented in the CMake so they are not mysterious later.

1. **UBSan requires Clang.** MSVC supports ASan but has no UBSan. The debug preset is wired to the LLVM Clang installed at `C:\Program Files\LLVM\bin\clang++.exe`. If configured with plain MSVC, the CMake falls back to ASan-only automatically.

2. **ASan requires the release CRT on Windows.** Linking the debug CRT (`msvcrtd`/`ucrtbased`) with ASan produces spurious bad-free errors during CRT startup. The fix: `CMAKE_MSVC_RUNTIME_LIBRARY = MultiThreadedDLL` for the sanitized Debug config. Compilation stays at `-O0 -g`; only the runtime library changes.

3. **ASan runtime DLL must travel with the executable.** The instrumented exes need `clang_rt.asan_dynamic-x86_64.dll`, which lives in the LLVM install tree, not on PATH. A post-build copy step places it next to each exe so they run without any PATH setup.

A fourth minor issue: doctest 2.4.11's bundled CMake predates CMake 4's minimum version requirement. Fixed with one compatibility variable (`CMAKE_POLICY_VERSION_MINIMUM`). Harmless.

---

## Proof command and observed result

```powershell
# Add LLVM to PATH if clang is not already visible.
$env:Path = "C:\Program Files\LLVM\bin;$env:Path"

cd intsect-cpp
cmake --preset debug
cmake --build --preset debug
ctest --preset debug
```

Result:

```
100% tests passed, 0 tests failed out of 1
Total Test time (real) = 0.88 sec
```

---

## Next: Phase 1 first slice

Port colors, bugs, directions, and game variants — the `enum class` types from `src/game/enums.jl` — with round-trip unit tests. Nothing else from Phase 1 until those are done.
