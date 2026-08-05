# Migrating the Hive Engine from Julia to C++

A guide for translating the codebase with AI assistance while keeping it something
you can personally read, debug, and extend — not just a black box that happens to work.

## Core principle

Restrict the C++ *dialect* you accept, and treat AI translation as an incremental,
test-verified process rather than a one-shot dump. This does not cost you performance:
the features that make engines fast (cache-friendly layout, branch elimination, avoiding
heap allocation/indirection in hot loops, SIMD) are largely orthogonal to — or even in
tension with — the "fancy" modern C++ features you're excluding.

## 1. Performance vs. the restricted dialect

- Excluded features (virtual dispatch, exceptions in hot paths, `shared_ptr`, heavy STL
  abstraction/ranges) don't just fail to help performance — they can actively hurt it
  (vtable indirection, refcounting overhead, hidden allocations). Cutting them is free
  or even beneficial.
- Two exceptions worth allowing because they're simple *and* used by real engines for
  speed:
  - Simple function/class templates for compile-time specialization, e.g.
    `template<bool IsWhite> Bitboard generateMoves(...)` to eliminate runtime branches.
    This is exactly what Stockfish does; once explained ("the compiler generates one
    version per case") it's not hard to follow.
  - `constexpr` functions/tables for compile-time-computed data (e.g. precomputed
    adjacency/attack tables).

## 2. Allowed / disallowed feature checklist

**Allowed (this is your working dialect):**
- Plain structs/classes: data + methods, no inheritance hierarchies.
- `std::array`, `std::vector`, `std::string` (avoid allocation in hot loops — reserve
  capacity or use fixed-size arrays there).
- `std::unique_ptr` for ownership; raw pointers or references for non-owning access.
- Simple function templates for compile-time specialization (see above).
- `constexpr` functions/variables.
- `enum class` for type safety (piece types, directions, etc.).
- Namespaces for organization.
- Minimal operator overloading for value types only (e.g. hex-coordinate/bitboard
  arithmetic: `+`, `-`, `==`) where it mirrors math notation.

**Avoid for now (revisit deliberately, later, one at a time):**
- Template metaprogramming, SFINAE, concepts.
- Multiple/virtual inheritance and polymorphism in hot paths.
- Exceptions in the search/movegen hot loop (use return codes/asserts; reserve
  exceptions for setup/IO code only).
- `shared_ptr` (default to `unique_ptr` or plain references instead).
- Long STL algorithm/iterator/ranges chains in hot loops.
- Operator overloading abuse (implicit conversions, exotic operators).
- Macros beyond simple constants/include guards — prefer `constexpr`/`inline` functions.

## 3. Incremental porting order

1. **Tooling first:** CMake build, a lightweight test framework (Catch2 or doctest),
   AddressSanitizer/UndefinedBehaviorSanitizer enabled in debug builds from day one —
   this catches the memory-safety bugs C++ makes easy that Julia doesn't.
2. **State representation + move generation:** port first, verify against the Julia
   implementation with parity tests (same positions → same move lists / move counts).
   Keep the Julia version around purely as a test oracle until parity is proven.
3. **Search** (alpha-beta, iterative deepening, transposition table): verify with
   move-choice and node-count comparisons on fixed test positions/depths.
4. **Evaluation** (handcrafted first): verify against reference scores from Julia.
5. **NNUE migration:** only after 1–4 are solid and tested. The offline training
   pipeline (data generation, network training) commonly stays in Python/PyTorch
   regardless of the engine's implementation language — that's normal practice, not
   a compromise.
6. Don't delete the Julia reference for a module until its C++ counterpart passes
   parity tests.

## 4. Working with AI on the translation itself

- Translate one file/module at a time — never accept a whole-codebase rewrite in a
  single pass.
- For any construct beyond plain "C with classes" level, require a plain-language
  explanation before accepting it. If you can't restate why it's written that way,
  simplify it first.
- Write or port tests for a module *before* moving to the next one — don't let
  untested surface area accumulate.
- Keep commits small (one logical module each) and easy to revert.
- Periodically explain the accumulated code back in your own words as a comprehension
  check — if you can't, that's a signal to pause and simplify before continuing.

## 5. Guardrails to set up early

- CMake + sanitizers (ASan/UBSan) in debug builds; consider `-Wall -Wextra -Werror`.
- clang-tidy (`cppcoreguidelines-*`, `modernize-*` checks) for general hygiene, plus a
  simple pre-commit grep for banned tokens (`shared_ptr`, `virtual`, `try`/`catch`) as
  an easy guardrail while you're calibrating what you're comfortable with.
- Basic CI (e.g. GitHub Actions) running the test suite (and perft benchmarks) on
  every commit.
- Keep this file (or a trimmed version of it) in the repo as `CODING_STYLE.md` so the
  dialect stays consistent across future sessions — including future AI-assisted ones.

## 6. Writing performant C++ and keeping it that way

### Principles while writing

- **Data-oriented design:** prefer flat, contiguous arrays over pointer-chasing
  structures (linked lists, maps of objects) for anything touched every node —
  Structure-of-Arrays for piece lists/bitboards over Array-of-Structures where it
  improves cache locality.
- **Avoid heap allocation in the hot loop:** preallocate move lists, search-stack
  frames, and the transposition table once at startup; reuse buffers instead of
  allocating per node. Use `std::vector::reserve` up front, or fixed-size stack
  arrays (e.g. `std::array<Move, 256>` for a move list) where the bound is known.
- **Minimize indirection:** avoid virtual calls, `std::function`, and unnecessary
  pointer chasing anywhere in the search loop.
- **Mind branch prediction:** order hot conditionals by likelihood; consider
  `[[likely]]`/`[[unlikely]]` (C++20) once profiling shows it matters — don't guess
  at this before measuring.
- **Inline small, hot functions**, or rely on the compiler with LTO/whole-program
  optimization rather than hand-forcing every function.
- **Prefer `constexpr`/compile-time tables** over runtime computation for fixed
  lookup data (attack patterns, Zobrist keys, adjacency tables).
- **Beware false sharing** in multi-threaded search (shared TT entries, per-thread
  node counters) — align/pad per-thread data to cache-line boundaries.

### Visibility: measure before (and after) optimizing

A restrained C++ dialect makes code *readable*, but readability doesn't tell you
where the time goes — profile first, never guess.

- **Sampling profilers:** Linux `perf record`/`perf report` (and `perf stat` for
  cache-miss/branch-misprediction counters); Windows: Visual Studio's built-in
  profiler or Windows Performance Recorder/Analyzer (WPR/WPA); cross-platform:
  [Tracy Profiler](https://github.com/wolfpld/tracy) — frame-based, low overhead,
  free/open-source, and a good fit for search-tree-style workloads.
- **Allocation visibility:** catch hot-loop allocations before they become habits —
  `heaptrack` (Linux), Visual Studio's Heap Profiler (Windows), or a simple
  counting/logging allocator wrapped around `new`/`delete` in debug builds that
  asserts zero allocations occur during a search call.
- **Cache/hotspot analysis:** `perf stat` for L1/L2 miss rates and branch
  mispredictions; `valgrind --tool=cachegrind` for detailed (slower) cache
  simulation; Intel VTune if available for a deeper hardware-counter view.
- **Micro-benchmarks:** Google Benchmark (or a small hand-rolled harness) for
  isolated hot functions (movegen, eval, make/unmake move) so regressions in a
  single function are caught without running a full search.
- **Macro-benchmarks / regression tracking:** keep a fixed `bench` command (à la
  Stockfish's `bench`) that runs search to a fixed depth/node budget over a standard
  suite of positions and reports nodes/sec. Log this number over time (a simple
  CSV or commit log) so a "clean" refactor that silently regresses speed doesn't go
  unnoticed — treat performance as a tracked, tested property of the codebase, not
  a one-time pass you do at the end and hope holds.

### A minimal workflow

1. Write the straightforward, correct version first (readable beats clever).
2. Run the bench suite to get a baseline.
3. Profile *only* where the bench says it's slow.
4. Optimize the specific hotspot the profiler points to — not what you assume is slow.
5. Re-run the bench to confirm the change actually helped; keep it only if it did.
6. Re-check allocation counts if you touched hot-loop code — an "obviously correct"
   change can silently introduce a `std::vector` reallocation per node.

## 7. Graduating beyond the restricted dialect

Loosen the rules deliberately and one at a time as you gain comfort — e.g. allow
`concepts` once you're confident reading constraint error messages, or a new template
pattern once you understand why it's there. Never relax multiple rules at once; that's
how comprehension quietly erodes.

## 8. Cross-engine perft parity gate for this repo

During migration, correctness is defined by parity across engines on the same positions.

- Keep Julia code as the in-repo oracle.
- Add the migration gate script at `scripts/verify_cross_engine_perft.jl`.
- Run it against four engines:
  - C++ rewrite executable (required)
  - Julia executable wrapper (optional but recommended)
  - `engines/nokamute.exe`
  - `engines/MzingaEngine.exe`

The validator compares perft counts depth-by-depth for each position and fails fast on
any mismatch. This gives immediate feedback when AI-generated C++ diverges from known
behavior.

### Expected command shape

```powershell
julia --project=. scripts/verify_cross_engine_perft.jl --depth 4 --positions-file ./starting_positions.txt --cpp-exe ./cpp/build/intsect_cpp.exe --julia-exe ./engines/intsect.bat --nokamute-exe ./engines/nokamute.exe --mzinga-exe ./engines/MzingaEngine.exe
```

Optionally compile C++ first:

```powershell
julia --project=. scripts/verify_cross_engine_perft.jl --cpp-build-cmd "cmake -S cpp -B cpp/build; cmake --build cpp/build --config Release" --depth 4
```

### Merge rule

Do not merge move generation, make/undo, search, or evaluation changes in C++ unless
this parity check passes for the agreed depth and position corpus.
