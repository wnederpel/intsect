// Tests for Phase 1 slice 1: enums, grid constants, color helpers, direction application.
// Do NOT define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN here; smoke_test.cpp provides main().
#include "intsect/types.hpp"

#include <cstdint>
#include <doctest/doctest.h>

using namespace intsect;

// Bug enum values.

TEST_CASE("Bug enum values") {
    CHECK(static_cast<uint8_t>(Bug::ANT) == 1);
    CHECK(static_cast<uint8_t>(Bug::GRASSHOPPER) == 2);
    CHECK(static_cast<uint8_t>(Bug::BEETLE) == 3);
    CHECK(static_cast<uint8_t>(Bug::SPIDER) == 4);
    CHECK(static_cast<uint8_t>(Bug::QUEEN) == 5);
    CHECK(static_cast<uint8_t>(Bug::LADYBUG) == 6);
    CHECK(static_cast<uint8_t>(Bug::PILLBUG) == 7);
    CHECK(static_cast<uint8_t>(Bug::MOSQUITO) == 8);
}

// Direction enum values.

TEST_CASE("Direction enum values") {
    CHECK(static_cast<uint8_t>(Direction::NW) == 0);
    CHECK(static_cast<uint8_t>(Direction::NE) == 1);
    CHECK(static_cast<uint8_t>(Direction::E) == 2);
    CHECK(static_cast<uint8_t>(Direction::SE) == 3);
    CHECK(static_cast<uint8_t>(Direction::SW) == 4);
    CHECK(static_cast<uint8_t>(Direction::W) == 5);
}

// Color enum values.

TEST_CASE("Color enum values") {
    CHECK(static_cast<uint8_t>(Color::White) == 1);
    CHECK(static_cast<uint8_t>(Color::Black) == 2);
    CHECK(static_cast<uint8_t>(Color::Draw) == 3);
    CHECK(static_cast<uint8_t>(Color::None) == 4);
}

// Grid constants.

TEST_CASE("Grid constants") {
    CHECK(ROW_SIZE == 16);
    CHECK(GRID_SIZE == 256);
    CHECK(MID == 136);
}

// other_color.

TEST_CASE("other_color round-trips") {
    CHECK(other_color(Color::White) == Color::Black);
    CHECK(other_color(Color::Black) == Color::White);
    CHECK(other_color(other_color(Color::White)) == Color::White);
    CHECK(other_color(other_color(Color::Black)) == Color::Black);
}

// apply_direction exact values.
// Expected values at loc=MID=136:
//   E : (136+1)%256           = 137
//   W : (136-1+256)%256       = 135
//   NW: (136-1-16+256)%256    = 119
//   NE: (136-16+256)%256      = 120
//   SE: (136+1+16)%256        = 153
//   SW: (136+16)%256          = 152

TEST_CASE("apply_direction exact values from MID") {
    CHECK(apply_direction(MID, Direction::E) == 137);
    CHECK(apply_direction(MID, Direction::W) == 135);
    CHECK(apply_direction(MID, Direction::NW) == 119);
    CHECK(apply_direction(MID, Direction::NE) == 120);
    CHECK(apply_direction(MID, Direction::SE) == 153);
    CHECK(apply_direction(MID, Direction::SW) == 152);
}

// apply_direction round-trips.
// Applying a direction then its opposite must return to the start.

TEST_CASE("apply_direction opposite-direction round-trips from MID") {
    CHECK(apply_direction(apply_direction(MID, Direction::E), Direction::W) == MID);
    CHECK(apply_direction(apply_direction(MID, Direction::W), Direction::E) == MID);
    CHECK(apply_direction(apply_direction(MID, Direction::NW), Direction::SE) == MID);
    CHECK(apply_direction(apply_direction(MID, Direction::SE), Direction::NW) == MID);
    CHECK(apply_direction(apply_direction(MID, Direction::NE), Direction::SW) == MID);
    CHECK(apply_direction(apply_direction(MID, Direction::SW), Direction::NE) == MID);
}

// apply_direction grid wrapping.
// The grid is toroidal: stepping off one edge arrives at the opposite edge.

TEST_CASE("apply_direction wraps at grid boundary") {
    CHECK(apply_direction(GRID_SIZE - 1, Direction::E) == 0);
    CHECK(apply_direction(0, Direction::W) == GRID_SIZE - 1);
}
