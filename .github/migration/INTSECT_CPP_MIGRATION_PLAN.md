# Intsect C++ Migration Plan

This file is the concrete migration plan for this repository.

It is not a general note on C++ style. That already exists in `HIVE_CPP_MIGRATION.md`.
This file says what to build, in what order, and what counts as done.

## Goal

Rewrite the engine in a new `intsect-cpp` folder.

- Keep the Julia source in `src/` untouched.
- Move quickly with AI.
- Keep the C++ dialect simple, even though the project targets C++23.
- Port all expansion bugs from day one: Ladybug, Pillbug, and Mosquito.
- Defer the cross-engine perft comparator until C++ move generation and perft are stable.

## High-level rules

1. Julia stays as the reference implementation during migration.
2. The new engine lives in `intsect-cpp/` and is self-contained.
3. Validation should move into C++ as soon as the C++ engine is mature enough to support it.
4. Do not port search before move generation, make/undo, and perft are stable.
5. Do not loosen the C++ dialect by accident. Use simple constructs unless there is a clear reason not to.

## Folder structure during migration

Use this top-level layout:

```text
Intsect.jl/
  src/                  # Julia engine, left untouched
  test/
  scripts/
  engines/
  data/

  intsect-cpp/
    CMakeLists.txt
    cmake/

    intsect-engine/
      include/intsect/
      src/

    intsect-cli/
      src/

    intsect-tools/
      src/

    intsect-tests/
      unit/
      parity/
      fixtures/

    intsect-data/
      positions/
      regressions/

    intsect-docs/
      MIGRATION_PLAN.md
      CPP_DIALECT.md
      PARITY_GATES.md
```

Why this structure:

- It keeps the old engine and new engine separate.
- It keeps engine code, CLI code, tools, tests, and data separate.
- It makes it easier to port one Julia module family at a time.
- It makes CI and local builds simpler.

## Phase 0: Project foundation

Target: create a clean C++ project that is ready for fast iteration.

Build this first:

1. `intsect-cpp/` with CMake.
2. C++23 enabled.
3. Warnings enabled.
4. AddressSanitizer and UndefinedBehaviorSanitizer in debug builds.
5. A lightweight test framework.
6. A small `intsect-cli` executable.
7. A short C++ dialect note that mirrors `HIVE_CPP_MIGRATION.md`.

Definition of done:

- The project configures and builds cleanly.
- Tests run from the command line.
- Debug builds include sanitizers.

## Phase 1: Core types and encodings

Port the basic types before any move logic.

Start from these Julia areas:

- `src/game/enums.jl`
- `src/game/constants.jl`
- `src/game/game.jl`
- `src/game/hex_set.jl`

Port these concepts first:

1. Colors, bugs, directions, and game variants.
2. Tile encoding and decoding.
3. Neighbor lookup and direction application.
4. Action indexing.
5. Basic set type for board locations.

Validation in this phase:

- Unit tests for tile packing and unpacking.
- Unit tests for direction and neighbor lookup.
- Unit tests for action index round-trips.

Definition of done:

- All low-level encodings are deterministic.
- Core helper tests pass.

## Phase 2: Board state and state transitions

Port the board representation next.

Start from these Julia areas:

- `src/game/structs.jl`
- `src/game/game.jl`
- `src/game/hash_values.jl`

Port these parts:

1. Board storage.
2. Piece locations.
3. Underworld and stack handling.
4. Turn state.
5. Queen tracking.
6. History needed for make/undo and perft.
7. Hash state needed by core board operations.

Validation in this phase:

- Board initialization tests.
- Apply/undo round-trip tests.
- Stack and buried-piece tests.
- Hash stability checks after apply/undo cycles.

Definition of done:

- Applying and undoing a move restores the exact prior state.
- No sanitizer errors occur in repeated apply/undo loops.

## Phase 2.5: faster compilation

Add a 'fast' preset that does uses a variable optimization level (or the default) and really just does the minimal amount of work otherwise to get something runnable as fast as possible for the fastest iteration cycle.

## Phase 3: Move generation

This is the key migration phase.

Start from these Julia areas:

- `src/game/move_generation.jl`
- `src/game/game.jl`

Port in this order:

1. Placement rules.
2. Queen.
3. Beetle.
4. Grasshopper.
5. Spider.
6. Ant.
7. Ladybug.
8. Pillbug.
9. Mosquito.

Reason for this order:

- Placement and simpler movement rules give early traction.
- Beetle and stack behavior must be trusted early.
- Pillbug and Mosquito are the trickiest and should come after the base machinery is solid.

Validation in this phase:

- Unit tests per bug type.
- Edge-case tests for pinned pieces, stacking, and throws.
- Duplicate-move detection.
- State-integrity checks after generating and applying legal moves.

Definition of done:

- Move generation runs correctly for all bug types.
- Expansion bugs are included from day one.
- No state corruption appears in deep randomized legal-move walks.

## Phase 4: C++ perft

Once move generation and make/undo are stable, add perft in C++.

Start from this Julia area:

- `src/game/perft.jl`

Build:

1. A C++ perft routine.
2. A small CLI or tool target that runs perft over a game string and depth range.
3. A test corpus of positions in `intsect-tests/fixtures/perft_fixtures`.

Validation in this phase:

- Internal consistency checks.
- Duplicate-move checks at shallow depth.
- Stability under repeated runs.
- Regression positions added whenever a bug is found.

Definition of done:

- C++ perft is stable on the agreed position corpus.
- Perft can be used as the first serious migration gate.


## Phase 4.5: full uhp compliance

Implement all uhp commands
Switch to assertion in essentially all parts of the code instead of passing flags around to indicate success. 
Implement near error handling around user commands so that it both looks and feels good and it is uhp compliant.

Ask for a resource on what uhp compliance exactly means. I will provide a wiki and an engine that tests it. 


## Phase 5: Cross-engine perft comparison

This topic is deferred until Phase 4 is stable.

When it starts, implement it in C++, not Julia.

The C++ comparison tool should:

1. Compare the C++ engine against external executables.
2. Use the Julia executable as one oracle.
3. Also compare against Nokamute and Mzinga.
4. Search back to the first position where results diverge, in the same spirit as the Julia perft comparison flow.

This is intentionally deferred. Do not spend time on it before C++ move generation and C++ perft are credible.

## Phase 6: Search and evaluation

Only start this after move generation and perft are trusted.

Start from these Julia areas:

- `src/ai/search.jl`
- `src/ai/evaluate.jl`
- `src/ai/suggested_actions.jl`

Port search features in this order:

1. Iterative deepening.
2. Alpha-beta or negamax search core.
3. Move ordering.
4. Transposition table.
5. Principal variation handling.
6. Killer moves and related heuristics.

Then port evaluation terms one group at a time.

Validation in this phase:

- Fixed-depth best-move tests.
- Score sanity checks.
- Node-count tracking.
- Arena smoke tests after search becomes usable.

Definition of done:

- Search is stable enough to play complete games.
- Evaluation is ported without breaking basic engine behavior.

## Phase 7: Match integration and cutover

After search and evaluation are working:

1. Expose the C++ engine through the CLI and protocol layer.
2. Run matches against the Julia engine and other engines.
3. Use the C++ engine as the default implementation only after the core gates are green.

Definition of done:

- C++ can replace Julia as the main engine binary.
- Julia can remain in the repository as a reference during the transition period.

## Hard gates before moving on

Do not move from one major phase to the next unless the current phase passes its checks.

The important gates are:

1. Core types are tested.
2. Apply/undo is exact.
3. Move generation is stable for all bugs, including expansions.
4. C++ perft is stable before cross-engine perft comparison begins.
5. Cross-engine perft comparison is in place before search work becomes the main focus.

## What not to do

1. Do not rewrite the Julia source during migration.
2. Do not start with search.
3. Do not add advanced C++ features without a clear reason.
4. Do not treat arena results as a substitute for move-generation correctness.
5. Do not spend time on the cross-engine comparator before C++ move generation and perft are ready.

## Immediate next implementation step

The next useful work is Phase 0.

Create `intsect-cpp/` with:

1. CMake.
2. C++23.
3. Warnings.
4. Sanitizers.
5. Test framework.
6. Empty targets for `intsect-engine`, `intsect-cli`, and `intsect-tools`.

After that, begin Phase 1 with the low-level game types and encodings.