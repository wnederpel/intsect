# Phase 2 summary - board state and state transitions

## Scope delivered

Phase 2 targeted board representation and deterministic state transitions that are stable under do/undo cycles.

Implemented areas:

- Board storage and tile-location tracking.
- Stack and underworld handling for climbs.
- Turn, ply, color-to-move, and just-moved tracking.
- Queen placement flags and queen position tracking.
- Piece-set tracking through `HexSet`.
- Hash and location-hash recomputation and full-hash composition.
- Per-action do/undo implementation for `Placement`, `Move`, `Climb`, and `Pass`.

## Key implementation notes

### Board and actions

- `Board` and action types live in `intsect-engine/include/intsect/board.hpp`.
- Actions are represented as a variant of:
  - `Placement`
  - `Move`
  - `Climb`
  - `Pass`

### Undo architecture

- Undo is now explicit per-action inverse logic, not whole-board snapshot restore.
- A compact `UndoRecord` keeps pre-action metadata needed for exact rollback:
  - previous `just_moved_loc`
  - previous `gameover`
  - previous `victor`
  - previous queen position caches

### Hashing

- Deterministic hash mixing uses `splitmix64`-based key generation from piece/location/height seeds.
- `full_hash()` combines board hash with side-to-move and just-moved components.

### Game termination checks

- Win detection: queen fully surrounded.
- Draw detection: repeated same-side full hash count.

## Test coverage added in C++

Board-state tests now include:

- Board initialization and sentinel validity.
- Placement and undo exact-state round-trip.
- Climb and underworld behavior.
- Hash and location-hash stability over repeated do/undo cycles.
- State progression parity checks (ply/turn/color/queen flags/just moved).
- Stepwise undo/redo checkpoint parity (Julia-style action-by-action).
- Piece-set checkpoint parity across reverse undo for move and climb sequences.
- Gameover detection by queen surround.
- Repetition draw detection.

Current result:

- `44/44` tests passing in the debug C++ suite.

## CLI interaction – Universal Hive Protocol

The CLI implements the [Universal Hive Protocol](https://github.com/jonthysell/Mzinga/wiki/UniversalHiveProtocol) (UHP).
On startup the engine prints its `info` block and is then ready for commands.

Run:

```powershell
./build/debug/intsect-cli/intsect-cli.exe
```

### Supported UHP commands

| Command | Description |
|---|---|
| `info` | Print engine identifier and capability flags |
| `newgame [GameTypeString\|GameString]` | Start (or load) a game; returns a GameString |
| `play MoveString` | Play the given move; returns updated GameString |
| `pass` | Shorthand for `play pass` |
| `undo [N]` | Undo the last N moves (default 1); returns updated GameString |
| `validmoves` | *(not yet implemented)* |
| `bestmove time HH:MM:SS \| depth N` | *(not yet implemented)* |
| `options` | No options; returns `ok` |
| `quit` | Exit the engine |

The non-standard extension command `show` prints a hexagonal ASCII board and a piece-location table (mirrors the Julia `show(board)` output).

### Quick smoke script (copy/paste line by line)

```text
newgame
play wQ
play bQ wQ-
show
undo
quit
```

Expected output (abbreviated):

```text
id intsect-cpp 0.0.0 (phase 0)
Mosquito;Ladybug;Pillbug
ok
Base;NotStarted;White[1]
ok
Base;InProgress;Black[1];wQ
ok
Base;InProgress;White[2];wQ;bQ wQ-
ok
Base;InProgress;White[2];wQ;bQ wQ-
-----------------
  (hex grid with wQ at 136, bQ at 137)
wQ : 136            bQ : 137
-----------------
ok
Base;InProgress;Black[1];wQ
ok
```

- `play bQ wQ-` places bQ to the right (east) of wQ, returning the updated GameString.
- `show` renders the hexagonal grid and piece table.
- `undo` returns the GameString rolled back by one move.

### GameString round-trip

```text
newgame Base;InProgress;White[2];wQ;bQ wQ-
show
quit
```

Expected checks:

- `newgame` with a full GameString replays both moves and returns the same GameString.
- `show` shows wQ at 136 and bQ at 137.

### Gameover test

```text
newgame
play wQ
play bA1 wQ/
play wA1 -wQ
play bA2 wQ\
play wA2 wQ-
play bA3 \wQ
play bB1 /wQ
quit
```

Expected checks:

- After the last `play` the returned GameString contains `BlackWins` in the state field because white queen is fully surrounded (NE bA1, W wA1, SE bA2, E wA2, NW bA3, SW bB1).

### Draw test

```text
newgame
pass
pass
pass
pass
pass
quit
```

Expected checks:

- After five passes the GameString state transitions to `Draw` by repetition.

## Known limits at end of Phase 2

- `validmoves` and `bestmove` return `err … not yet implemented`.
- Full legal move validation is not in place; the engine applies moves without checking legality.
- Move generation is the next major migration phase.
