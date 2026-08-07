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

The CLI implements the [Universal Hive Protocol](https://github.com/jonthysell/Mzinga/wiki/UniversalHiveProtocol) (UHP).
On startup it prints its `info` block and waits for commands.
A game must be started with `newgame` before playing moves.

Supported commands:

| Command | Description |
|---|---|
| `info` | Print engine id and capability flags |
| `newgame [GameTypeString\|GameString]` | Start or load a game |
| `play MoveString` | Play a move (e.g. `play wQ`, `play bQ wQ-`) |
| `pass` | Shorthand for `play pass` |
| `undo [N]` | Undo the last N moves (default 1) |
| `validmoves` | *(not yet implemented)* |
| `bestmove time HH:MM:SS \| depth N` | *(not yet implemented)* |
| `options` | List engine options (none currently) |
| `show` | Non-standard: display hexagonal board + piece table |
| `quit` | Exit |

Notes:

- Legal move validation is not yet enforced; the engine applies moves without checking Hive legality.
- `validmoves` and `bestmove` require move generation, which is the next migration phase.

## Phase summaries

- Phase 0: `intsect-docs/PHASE_0_SUMMARY.md`
- Phase 1: `intsect-docs/PHASE_1_SUMMARY.md`
- Phase 2: `intsect-docs/PHASE_2_SUMMARY.md`

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
