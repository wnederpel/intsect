# C++ dialect for the Intsect rewrite

This is the working dialect for all code under `intsect-cpp/`. It is a summary of
`.github/migration/HIVE_CPP_MIGRATION.md`, kept inside the C++ tree so the rules
travel with the code. If this file and `HIVE_CPP_MIGRATION.md` ever disagree, the
migration doc wins — fix this file to match.

The point of the restriction: the resulting C++ must stay something a human can read,
debug, and extend without AI. Smaller and clearer beats larger and clever.

## Allowed

- Plain structs/classes: data plus methods. No inheritance hierarchies.
- `std::array`, `std::vector`, `std::string`. Avoid allocation in hot loops — reserve
  capacity or use fixed-size arrays there.
- `std::unique_ptr` for ownership. Raw pointers or references for non-owning access.
- Simple function/class templates for compile-time specialization
  (e.g. `template<bool IsWhite> ...`). One version per case, nothing clever.
- `constexpr` functions and variables, including compile-time lookup tables.
- `enum class` for type safety (colors, bugs, directions, variants).
- Namespaces for organization.
- Minimal operator overloading for value types only (e.g. `+`, `-`, `==` on
  coordinates/bitboards) where it mirrors math notation.

## Avoid (revisit later, deliberately, one rule at a time)

- Template metaprogramming, SFINAE, concepts.
- Multiple or virtual inheritance and runtime polymorphism, especially in hot paths.
- Exceptions for control flow. Use return codes/asserts in engine code; reserve
  exceptions for setup/IO only.
- `shared_ptr` — default to `unique_ptr` or plain references.
- Long STL algorithm/iterator/ranges chains in hot loops.
- Operator overloading abuse (implicit conversions, exotic operators).
- Macros beyond simple constants/include guards — prefer `constexpr`/`inline`.

## Enforced automatically

Warnings are errors (`-Wall -Wextra -Wpedantic -Wshadow -Wconversion -Werror` on
Clang). Debug builds run with AddressSanitizer and UndefinedBehaviorSanitizer.

The banned constructs below are a matter of discipline, not tooling — there is no
automated scanner. Keep them out by convention:

| Avoid | Why |
|---|---|
| `shared_ptr` | No shared ownership; use `unique_ptr` or references. |
| `virtual`    | No virtual dispatch. |
| `try` / `catch` | No exception control flow in engine code. |

## When you need something outside this dialect

Stop and justify it in one paragraph before using it. Do not slip it in. Loosen the
rules deliberately and one at a time, never several at once.
