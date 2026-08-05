# Role and mission

You are helping me migrate a Hive engine from Julia to C++. The authoritative plans are:

- `.github/migration/INTSECT_CPP_MIGRATION_PLAN.md` — the concrete phase order, folder
  layout, and definition-of-done per phase. This governs WHAT to build and in WHAT order.
- `.github/migration/HIVE_CPP_MIGRATION.md` — the C++ dialect, allowed/disallowed
  features, incremental-porting rules, and performance guidance. This governs HOW code
  may be written.

Read both fully before doing anything. If they ever conflict, ask me; do not guess.

# My overriding goal

I must be able to read, debug, and extend the resulting C++ WITHOUT AI afterwards.
Correctness and my comprehension outrank speed of delivery. A smaller, clearer,
fully-understood increment always beats a larger clever one.

# Hard rules

1. NEVER modify anything under `src/`, `test/`, `scripts/`, or the existing Julia code.
   Julia is the frozen reference oracle. All new work goes under `intsect-cpp/`.
2. Use ONLY the restricted C++ dialect from HIVE_CPP_MIGRATION.md. If you believe a
   construct outside that dialect is genuinely needed, STOP and ask me first with a
   one-paragraph justification. Do not slip it in.
3. Work in ONE sub-phase per session — the smallest unit named in the plan (e.g. a
   single bug type in Phase 3, not all of movegen). Do not proceed to the next
   sub-phase in the same session.
4. Write or port the tests for a module BEFORE or ALONGSIDE the module — never leave
   untested surface area.
5. Verify against the Julia oracle wherever the plan calls for parity. Do not invent
   expected values; derive them from Julia output or the frozen fixtures.
6. Keep every change revertible and scoped to the current sub-phase.

# Required workflow for EVERY session

At the start, restate in 2-3 sentences:
  - which single sub-phase you are doing,
  - its definition of done from the plan,
  - the exact command(s) that will prove it done.

Then implement the minimal code + tests for that sub-phase only.

At the end of the sub-phase, STOP and give me:
  a. A plain-language explanation of what you built and why (no C++ jargon dumps).
  b. A mapping table: each non-trivial C++ construct -> the Julia source lines it was
     ported from, and one sentence on why it looks the way it does.
  c. The exact command to build and run the tests, and the observed result.
  d. Anything you were unsure about or that diverged from the Julia semantics.
  e. A one-line statement of the next sub-phase — then WAIT for me. Do not continue.

If at any point you cannot explain a piece of code in plain language, simplify it until
you can, before showing it to me.

# First session scope: Phase 0 only

Do Phase 0 from INTSECT_CPP_MIGRATION_PLAN.md and nothing else:

- Create `intsect-cpp/` with CMake, C++23, warnings-as-errors, ASan+UBSan in debug.
- Add a lightweight test framework (doctest or Catch2 — pick one, tell me which and why).
- Create empty targets for `intsect-engine`, `intsect-cli`, `intsect-tools`,
  `intsect-tests` per the plan's folder layout.
- Add a dialect guardrail: a simple pre-build check (script or CMake step) that greps for
  banned tokens (`shared_ptr`, `virtual`, `try`, `catch`) under `intsect-cpp/` and fails
  the build if found. List the banned set explicitly so I can adjust it.
- Add `intsect-cpp/intsect-docs/CPP_DIALECT.md` summarizing the allowed dialect, derived
  from HIVE_CPP_MIGRATION.md, so the rule set lives in the C++ tree too.
- Provide a single documented command that configures, builds, and runs the (empty)
  test suite cleanly.

Do NOT start Phase 1 (types/encodings). Stop at the Phase 0 definition of done and give
me the end-of-session report described above.