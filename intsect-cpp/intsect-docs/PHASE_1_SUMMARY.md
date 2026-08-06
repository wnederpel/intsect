# Phase 1 summary — core types, tile encoding, and board location set

## What was built

Three headers and four test files. All logic is `constexpr` where possible (compile-time
checked) and `inline` (no separate translation unit needed). No heap allocation.

### `intsect-engine/include/intsect/types.hpp`

The primitive vocabulary: enums, grid constants, and direction helpers.

| Definition | Julia source | Notes |
|---|---|---|
| `enum class Bug : uint8_t` | `enums.jl` | Values 1–8 match Julia exactly; tile arithmetic depends on them. |
| `enum class Direction : uint8_t` | `enums.jl` | Values 0–5 from Julia's EnumX default ordering. |
| `enum class Color : uint8_t` | `constants.jl` WHITE/BLACK/DRAW/NO_COLOR | Modelled as an enum class for type safety; Julia stores these as plain constants. |
| `enum class Variant : uint8_t` | `enums.jl` Gametype structs | Julia uses empty structs for compile-time dispatch. Encoded as bit flags (M=1, L=2, P=4) so variant membership is testable with bitwise ops later. |
| `ROW_SIZE`, `GRID_SIZE`, `MID` | `constants.jl` | Inline constexpr ints: 16, 256, 136. |
| `other_color()` | `constants.jl` `other()` | Constexpr; returns the opposite player color. |
| `apply_direction()` | `game.jl` `apply_direction()` | Switch mirrors Julia's if-elseif exactly. Every case adds GRID_SIZE before % because C++'s signed % can return negative values; Julia's cannot. |

### `intsect-engine/include/intsect/tile.hpp`

Tile encoding and decoding. A tile is one `uint8_t` with four fields packed in:

```
  bits 7-6: bug_num (0-based index of which piece this is)
  bits 5-3: bug type – 1
  bit  2:   color – 1
  bits 1-0: height (0 stored = ground level 1; 0xFF = EMPTY_TILE sentinel)
```

| Definition | Julia source | Notes |
|---|---|---|
| `BUG_NUM_MASK/SHIFT` etc. | `constants.jl` | Identical bit layout. |
| `EMPTY_TILE`, `NOT_PLACED`, `INVALID_LOC`, `UNDERGROUND` | `constants.jl` | Sentinel values. |
| `MAX_BUG_NUMS` | `constants.jl` MAX_NUMS | `constexpr std::array<uint8_t,8>`. Access: `MAX_BUG_NUMS[static_cast<int>(bug)-1]`. |
| `tile_from_info()` | `game.jl` `tile_from_info()` | Packs Color+Bug+bug_num+height into one byte. Arithmetic done in `int` then cast back to `uint8_t` to avoid `-Wconversion` on narrow shift results. |
| `get_tile_color/bug/bug_num/height()` | `game.jl` getters | Each extracts one field using the mask/shift constants. `get_tile_height` returns 0 for EMPTY_TILE (Julia convention). |
| `tile_from_info_as_index()` | `game.jl` `tile_from_info_as_index()` | `tile_from_info(c,b,n,0) >> INDEX_SHIFT` — strips height bits. Used as 0-based index into the 36-entry tile_locs array (Phase 2). |
| `get_tile_unplaced()` | `game.jl` `get_tile_unplaced()` | Inverse of above: given a 1-based locs position, returns the ground-level tile byte. |

Note: Julia also has `tile_from_info_as_index_odd` which is byte-for-byte identical to
`tile_from_info_as_index` in its source. It is not ported (it is a duplicate).

### `intsect-engine/include/intsect/hex_set.hpp`

A dense bitset covering all 256 board locations, one bit per location.

| Definition | Julia source | Notes |
|---|---|---|
| `HEX_SET_NUM_WORDS = 4` | `structs.jl` | `GRID_SIZE / 64`. |
| `struct HexSet` | `structs.jl` | `std::array<uint64_t, 4>` — default construction zero-initialises. C++ copy/assign work correctly with no extra code. |
| `set / remove / toggle / get` | `hex_set.jl` | Location L → word `L >> 6`, bit `L & 63`. |
| `union_with()` | `hex_set.jl` `union!()` | Simple OR loop; the compiler auto-vectorises 4 × 64-bit iterations. Julia used `@simd` for the same reason. |
| `count()` | `hex_set.jl` `count_ones()` | Uses `std::popcount` (C++20). |
| `for_each_bit_set(f)` | `hex_set.jl` `for_each_bit_set()` | Template on the callable (allowed by dialect). Uses `std::countr_zero` (C++20) to isolate the lowest set bit without a branch, then clears it with `word &= word - 1`. |
| `operator==` | `hex_set.jl` `==` | Delegates to `std::array::operator==`. |

---

## Test results

```
100% tests passed, 0 tests failed out of 32
```

32 tests across 4 test files covering all of Phase 1.

---

## Open questions / divergences

- **Variant bit-flag encoding** — no Julia code that uses variants has been ported yet,
  so this design is untested against the oracle. Revisit in Phase 3 when game-type
  dispatch is needed.
- **`tile_from_info_as_index_odd` not ported** — its Julia body is byte-for-byte
  identical to `tile_from_info_as_index`; likely a copy left in by accident. If it turns
  out callers pass different argument types, revisit.
- **apply_direction fallthrough** — all six enum cases are covered; `return loc` after
  the switch is unreachable but silences the compiler warning without runtime cost.

---

## Next: Phase 2

Port board storage and state transitions: `Board` struct, tile placement tracking,
stack/underworld handling, turn state, queen tracking, and Zobrist hash state.


## What was built

One header and one test file. No executable logic yet — this slice defines the primitive
vocabulary the rest of the engine will be written in.

### `intsect-engine/include/intsect/types.hpp`

The single source of truth for the basic types.

| Definition | Julia source | Notes |
|---|---|---|
| `enum class Bug : uint8_t` | `enums.jl` | Values 1–8 copied exactly from Julia. Explicit values matter: tile encoding arithmetic uses them. |
| `enum class Direction : uint8_t` | `enums.jl` | Values 0–5 from Julia's EnumX default ordering. |
| `enum class Color : uint8_t` | `constants.jl` WHITE/BLACK/DRAW/NO_COLOR | Julia stores these as plain constants, not an enum. Modelled as an enum class here for type safety. |
| `enum class Variant : uint8_t` | `enums.jl` Gametype structs | Julia uses empty structs for compile-time dispatch. Encoded as bit flags (M=1, L=2, P=4) so variant membership is testable with bitwise ops. |
| `ROW_SIZE`, `GRID_SIZE`, `MID` | `constants.jl` | Inline constexpr ints: 16, 256, 136. |
| `other_color()` | `constants.jl` `other()` | Constexpr; returns the opposite player color. |
| `apply_direction()` | `game.jl` `apply_direction()` | Switch mirrors Julia's if-elseif exactly. Every case adds GRID_SIZE before % because C++'s signed % can return negative values for negative left-hand operands; Julia's cannot. |

### `intsect-tests/unit/types_test.cpp`

Eight test cases, all expected values derived analytically from the Julia formulas (no Julia run required):

| Test | What it checks |
|---|---|
| Bug enum values | Static values 1–8 match Julia |
| Direction enum values | Static values 0–5 match Julia |
| Color enum values | Static values 1–4 match Julia |
| Grid constants | ROW_SIZE=16, GRID_SIZE=256, MID=136 |
| other_color round-trips | White↔Black, double-flip returns original |
| apply_direction exact values from MID | E=137, W=135, NW=119, NE=120, SE=153, SW=152 |
| apply_direction opposite round-trips | All six direction pairs return to start |
| apply_direction grid wrapping | Steps off last cell wrap to first cell and vice versa |

---

## Result

```
100% tests passed, 0 tests failed out of 9
```

---

## Open questions / divergences

- **Variant bit-flag encoding** — no Julia code that uses variants has been ported yet, so
  this design is untested against the oracle. If it turns out Julia dispatch on game type
  needs a different shape, this is the place to revisit.
- **apply_direction fallthrough** — the switch covers all six Direction enum values, but
  the compiler cannot prove this. A `return loc` after the switch silences the warning
  without runtime cost.

---

## Next: Phase 1 slice 2

Port tile encoding and decoding: `tile_from_info()`, `get_tile_color/bug/bug_num/height()`,
and the index variants from `game.jl`, with round-trip unit tests against Julia-derived
oracle values.
