# intsect-cpp

Small C++23 rewrite of the Intsect engine.

## Prerequisites

- CMake 3.24+
- Clang available on PATH (`clang++`)

If `clang++` is not on PATH, either add it or set `CMAKE_CXX_COMPILER` explicitly.

## Build (Debug)

From `intsect-cpp/`:

```powershell
cmake --preset debug
cmake --build --preset debug
```

This writes build output to `build/debug/`.

## Run the CLI

After building debug:

```powershell
./build/debug/intsect-cli/intsect-cli.exe
```

Current behavior: prints the engine version string.

## Run Tests

Run all registered CTest tests in debug build:

```powershell
ctest --preset debug
```

Or run from the build folder with failure output enabled:

```powershell
ctest --test-dir build/debug --output-on-failure
```

## Release Build (optional)

```powershell
cmake --preset release
cmake --build --preset release
```
