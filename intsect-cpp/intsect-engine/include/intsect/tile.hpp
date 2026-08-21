#pragma once

#include "types.hpp"

#include <array>
#include <cstdint>

namespace intsect {

// Tile byte layout:
//   bits 7-6: bug_num (0-based count of which piece this is)
//   bits 5-3: bug type - 1
//   bit  2:   color - 1
//   bits 1-0: height (0 stored = ground level 1; EMPTY_TILE = 0xFF)

inline constexpr uint8_t BUG_NUM_MASK = 0b11000000u;
inline constexpr uint8_t BUG_NUM_SHIFT = 6;
inline constexpr uint8_t BUG_MASK = 0b00111000u;
inline constexpr uint8_t BUG_SHIFT = 3;
inline constexpr uint8_t COLOR_MASK = 0b00000100u;
inline constexpr uint8_t COLOR_SHIFT = 2;
inline constexpr uint8_t HEIGHT_MASK = 0b00000011u;
inline constexpr uint8_t HEIGHT_SHIFT = 0;
// Stripping the 2 height bits gives the 0-based index into the tile_locs table.
inline constexpr uint8_t INDEX_SHIFT = 2;

inline constexpr uint8_t EMPTY_TILE = 0xFFu;

// Sentinel values for a tile's location on the board.
inline constexpr int NOT_PLACED = -1;
inline constexpr int INVALID_LOC = -2;
inline constexpr int UNDERGROUND = -3;

inline constexpr int BUGS_IN_PLAY = 8;
inline constexpr int TOTAL_NUM_BUGS = 14;

// Maximum bug_num per bug type (0-based).
// Access: MAX_BUG_NUMS[static_cast<int>(bug) - 1].
inline constexpr std::array<uint8_t, 8> MAX_BUG_NUMS = {2, 2, 1, 1, 0, 0, 0, 0};

// Encoding.

inline constexpr uint8_t tile_from_info(Color color, Bug bug, uint8_t bug_num,
                                        uint8_t height = 0) noexcept {
    return static_cast<uint8_t>(((static_cast<int>(color) - 1) << COLOR_SHIFT) |
                                ((static_cast<int>(bug) - 1) << BUG_SHIFT) |
                                (static_cast<int>(bug_num) << BUG_NUM_SHIFT) |
                                (static_cast<int>(height) << HEIGHT_SHIFT));
}

// Decoding.

inline constexpr Color get_tile_color(uint8_t tile) noexcept {
    return static_cast<Color>(((tile & COLOR_MASK) >> COLOR_SHIFT) + 1u);
}

inline constexpr Bug get_tile_bug(uint8_t tile) noexcept {
    return static_cast<Bug>(((tile & BUG_MASK) >> BUG_SHIFT) + 1u);
}

inline constexpr uint8_t get_tile_bug_num(uint8_t tile) noexcept {
    return static_cast<uint8_t>((tile & BUG_NUM_MASK) >> BUG_NUM_SHIFT);
}

// Returns 0 for EMPTY_TILE; otherwise 1-based height (1 = ground level).
inline constexpr uint8_t get_tile_height(uint8_t tile) noexcept {
    if (tile == EMPTY_TILE)
        return 0;
    return static_cast<uint8_t>((tile & HEIGHT_MASK) + 1u); // HEIGHT_SHIFT is 0
}

// Index helpers.
// tile_from_info_as_index builds a tile byte with height stripped, then shifts by INDEX_SHIFT.
// Used as 0-based index into the tile_locs location table (size 36).

inline constexpr uint8_t tile_from_info_as_index(Color color, Bug bug, uint8_t bug_num) noexcept {
    return static_cast<uint8_t>(((static_cast<int>(color) - 1) << (COLOR_SHIFT - INDEX_SHIFT)) |
                                ((static_cast<int>(bug) - 1) << (BUG_SHIFT - INDEX_SHIFT)) |
                                (static_cast<int>(bug_num) << (BUG_NUM_SHIFT - INDEX_SHIFT)));
}

// Inverse of tile_from_info_as_index.
// semi_tile is 1-based (tile_from_info_as_index + 1). Returns ground-level tile byte.
inline constexpr uint8_t get_tile_unplaced(uint8_t semi_tile) noexcept {
    return static_cast<uint8_t>((semi_tile - 1u) << INDEX_SHIFT);
}

inline constexpr uint8_t next_bug_num(uint8_t tile) noexcept {
    if (tile == EMPTY_TILE)
        return EMPTY_TILE;

    const Bug bug = get_tile_bug(tile);
    const uint8_t bug_num = get_tile_bug_num(tile);
    const uint8_t max_num = MAX_BUG_NUMS[static_cast<size_t>(static_cast<uint8_t>(bug) - 1u)];
    if (bug_num == max_num)
        return EMPTY_TILE;

    const uint8_t raw_height = static_cast<uint8_t>(tile & HEIGHT_MASK);
    return tile_from_info(get_tile_color(tile), bug, static_cast<uint8_t>(bug_num + 1u),
                          raw_height);
}

} // namespace intsect
