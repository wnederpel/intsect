// Tests for tile encoding and decoding (tile.hpp).
// Do NOT define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN here; smoke_test.cpp provides main().
#include "intsect/tile.hpp"

#include <cstdint>
#include <doctest/doctest.h>

using namespace intsect;

// tile_from_info encoding.
// Oracle: tile = (color-1)<<2 | (bug-1)<<3 | bug_num<<6 | height<<0

TEST_CASE("tile_from_info encoding") {
    // White Queen (color=1, bug=5, bug_num=0, height=0): (0)<<2|(4)<<3|(0)<<6|0 = 32 = 0x20
    CHECK(tile_from_info(Color::White, Bug::QUEEN, 0) == uint8_t{0x20});
    // Black Queen: (1)<<2|(4)<<3 = 4|32 = 36 = 0x24
    CHECK(tile_from_info(Color::Black, Bug::QUEEN, 0) == uint8_t{0x24});
    // White Ant#3 (bug_num=2): (0)<<2|(0)<<3|(2)<<6 = 128 = 0x80
    CHECK(tile_from_info(Color::White, Bug::ANT, 2) == uint8_t{0x80});
    // White Ant#1 at height 2 (stored as height=1): 1 = 0x01
    CHECK(tile_from_info(Color::White, Bug::ANT, 0, 1) == uint8_t{0x01});
    // Black Mosquito (color=2, bug=8): (1)<<2|(7)<<3 = 4|56 = 60 = 0x3C
    CHECK(tile_from_info(Color::Black, Bug::MOSQUITO, 0) == uint8_t{0x3C});
    // White Ant#1 at ground: 0x00
    CHECK(tile_from_info(Color::White, Bug::ANT, 0) == uint8_t{0x00});
}

// get_tile_color.

TEST_CASE("get_tile_color decodes color correctly") {
    CHECK(get_tile_color(uint8_t{0x20}) == Color::White);
    CHECK(get_tile_color(uint8_t{0x24}) == Color::Black);
    CHECK(get_tile_color(uint8_t{0x00}) == Color::White);
    CHECK(get_tile_color(uint8_t{0x04}) == Color::Black);
}

// get_tile_bug.

TEST_CASE("get_tile_bug decodes bug type correctly") {
    CHECK(get_tile_bug(uint8_t{0x20}) == Bug::QUEEN);    // (0x20 & 0x38) >> 3 + 1 = 5
    CHECK(get_tile_bug(uint8_t{0x00}) == Bug::ANT);      // 0 >> 3 + 1 = 1
    CHECK(get_tile_bug(uint8_t{0x3C}) == Bug::MOSQUITO); // (0x3C & 0x38) >> 3 + 1 = 8
}

// get_tile_bug_num.

TEST_CASE("get_tile_bug_num decodes bug number correctly") {
    CHECK(get_tile_bug_num(uint8_t{0x20}) == 0); // 0x20 & 0xC0 = 0
    CHECK(get_tile_bug_num(uint8_t{0x80}) == 2); // 0x80 & 0xC0 = 0x80 >> 6 = 2
    CHECK(get_tile_bug_num(uint8_t{0x40}) == 1); // 0x40 & 0xC0 = 0x40 >> 6 = 1
}

// get_tile_height.

TEST_CASE("get_tile_height decodes height correctly") {
    CHECK(get_tile_height(EMPTY_TILE) == 0);    // special sentinel
    CHECK(get_tile_height(uint8_t{0x20}) == 1); // height bits = 00 → stored 0 → level 1
    CHECK(get_tile_height(uint8_t{0x01}) == 2); // height bits = 01 → stored 1 → level 2
    CHECK(get_tile_height(uint8_t{0x02}) == 3); // height bits = 10 → stored 2 → level 3
    CHECK(get_tile_height(uint8_t{0x00}) == 1); // ground level
}

// round-trip encode/decode.

TEST_CASE("tile_from_info and getters are inverse operations") {
    const uint8_t t = tile_from_info(Color::Black, Bug::SPIDER, 1, 2);
    CHECK(get_tile_color(t) == Color::Black);
    CHECK(get_tile_bug(t) == Bug::SPIDER);
    CHECK(get_tile_bug_num(t) == 1);
    CHECK(get_tile_height(t) == 3); // stored height 2 → level 3
}

// tile_from_info_as_index.
// Oracle: (color-1)<<0 | (bug-1)<<1 | bug_num<<4
// Equivalently: tile_from_info(c,b,n,0) >> INDEX_SHIFT

TEST_CASE("tile_from_info_as_index") {
    // White Queen: 0|(4<<1)|0 = 8
    CHECK(tile_from_info_as_index(Color::White, Bug::QUEEN, 0) == uint8_t{8});
    // Black Ant#3: 1|(0<<1)|(2<<4) = 1|0|32 = 33
    CHECK(tile_from_info_as_index(Color::Black, Bug::ANT, 2) == uint8_t{33});
    // White Mosquito: 0|(7<<1)|0 = 14
    CHECK(tile_from_info_as_index(Color::White, Bug::MOSQUITO, 0) == uint8_t{14});
}

TEST_CASE("tile_from_info_as_index equals tile >> INDEX_SHIFT") {
    for (uint8_t bug_num = 0; bug_num <= 2; ++bug_num) {
        const uint8_t t = tile_from_info(Color::White, Bug::ANT, bug_num, 0);
        CHECK(tile_from_info_as_index(Color::White, Bug::ANT, bug_num) ==
              static_cast<uint8_t>(t >> INDEX_SHIFT));
    }
}

// get_tile_unplaced.
// Oracle: (semi_tile - 1) << INDEX_SHIFT  where semi_tile = tile_as_index + 1

TEST_CASE("get_tile_unplaced") {
    // White Queen: index=8, semi_tile=9 → (8)<<2 = 32 = 0x20
    CHECK(get_tile_unplaced(9) == uint8_t{0x20});
    // White Ant#1: index=0, semi_tile=1 → (0)<<2 = 0
    CHECK(get_tile_unplaced(1) == uint8_t{0x00});
}

TEST_CASE("get_tile_unplaced is inverse of tile_from_info_as_index (via semi_tile)") {
    const uint8_t idx       = tile_from_info_as_index(Color::White, Bug::QUEEN, 0); // 8
    const uint8_t semi_tile = static_cast<uint8_t>(idx + 1u);                       // 9
    CHECK(get_tile_unplaced(semi_tile) == tile_from_info(Color::White, Bug::QUEEN, 0, 0));
}
