#pragma once

#include <cstdint>

namespace intsect {

// Grid layout.
// The board is a wrapping toroidal grid; every position is numbered 0..255.

inline constexpr int ROW_SIZE  = 16;
inline constexpr int GRID_SIZE = ROW_SIZE * ROW_SIZE;             // 256
inline constexpr int MID       = (ROW_SIZE + 1) * (ROW_SIZE / 2); // 136

// Enumerations.

enum class Bug : uint8_t {
    ANT         = 1,
    GRASSHOPPER = 2,
    BEETLE      = 3,
    SPIDER      = 4,
    QUEEN       = 5,
    LADYBUG     = 6,
    PILLBUG     = 7,
    MOSQUITO    = 8,
};

enum class Direction : uint8_t {
    NW = 0,
    NE = 1,
    E  = 2,
    SE = 3,
    SW = 4,
    W  = 5,
};

enum class Color : uint8_t {
    White = 1,
    Black = 2,
    Draw  = 3,
    None  = 4,
};

// Encoded as bit flags (M=1, L=2, P=4) so combinations are unambiguous.
enum class Variant : uint8_t {
    Base = 0,
    M    = 1,
    L    = 2,
    ML   = 1 + 2,
    P    = 4,
    MP   = 1 + 4,
    LP   = 2 + 4,
    MLP  = 1 + 2 + 4,
};

// Color helpers.

// Defined only for White/Black; behavior on Draw/None is undefined.
inline constexpr Color other_color(Color c) noexcept {
    return (c == Color::White) ? Color::Black : Color::White;
}

// Direction application.
// Each case adds GRID_SIZE before taking % so signed modulo stays non-negative.

inline constexpr int apply_direction(int loc, Direction d) noexcept {
    switch (d) {
    case Direction::E:
        return (loc + 1) % GRID_SIZE;
    case Direction::W:
        return (loc - 1 + GRID_SIZE) % GRID_SIZE;
    case Direction::NW:
        return (loc - 1 - ROW_SIZE + GRID_SIZE) % GRID_SIZE;
    case Direction::NE:
        return (loc - ROW_SIZE + GRID_SIZE) % GRID_SIZE;
    case Direction::SE:
        return (loc + 1 + ROW_SIZE) % GRID_SIZE;
    case Direction::SW:
        return (loc + ROW_SIZE) % GRID_SIZE;
    }
    return loc; // unreachable; silences compiler warnings
}

} // namespace intsect
