// Tests for HexSet (hex_set.hpp).
// Do NOT define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN here; smoke_test.cpp provides main().
#include "intsect/hex_set.hpp"

#include <doctest/doctest.h>
#include <vector>

using namespace intsect;

// Default construction.

TEST_CASE("HexSet default-constructs to all-zeros") {
    HexSet hs;
    for (int i = 0; i < GRID_SIZE; ++i)
        CHECK(!hs.get(i));
    CHECK(hs.count() == 0);
}

// set / get / remove.

TEST_CASE("HexSet set and get") {
    HexSet hs;
    hs.set(0);
    CHECK(hs.get(0));
    CHECK(!hs.get(1));
}

TEST_CASE("HexSet remove clears a set bit") {
    HexSet hs;
    hs.set(42);
    CHECK(hs.get(42));
    hs.remove(42);
    CHECK(!hs.get(42));
}

TEST_CASE("HexSet toggle flips a bit") {
    HexSet hs;
    hs.toggle(10);
    CHECK(hs.get(10));
    hs.toggle(10);
    CHECK(!hs.get(10));
}

// Boundary locations.
// The grid has 256 locations (0..255). Test the word boundaries at 0, 63, 64, 255.

TEST_CASE("HexSet handles word boundary locations") {
    HexSet hs;
    hs.set(0);
    hs.set(63);  // last bit of word 0
    hs.set(64);  // first bit of word 1
    hs.set(255); // last bit of word 3
    CHECK(hs.get(0));
    CHECK(hs.get(63));
    CHECK(hs.get(64));
    CHECK(hs.get(255));
    CHECK(!hs.get(1));
    CHECK(!hs.get(62));
    CHECK(!hs.get(65));
    CHECK(!hs.get(254));
    CHECK(hs.count() == 4);
}

// clear.

TEST_CASE("HexSet clear resets all bits") {
    HexSet hs;
    hs.set(0);
    hs.set(128);
    hs.clear();
    CHECK(hs.count() == 0);
    CHECK(!hs.get(0));
    CHECK(!hs.get(128));
}

// count.

TEST_CASE("HexSet count returns number of set bits") {
    HexSet hs;
    hs.set(1);
    hs.set(100);
    hs.set(200);
    CHECK(hs.count() == 3);
}

// union_with.

TEST_CASE("HexSet union_with ORs two sets") {
    HexSet a, b;
    a.set(0);
    b.set(1);
    a.union_with(b);
    CHECK(a.get(0));
    CHECK(a.get(1));
    CHECK(!a.get(2));
}

// operator== and copy.

TEST_CASE("HexSet operator== compares correctly") {
    HexSet a, b;
    CHECK(a == b);
    a.set(5);
    CHECK(!(a == b));
    b.set(5);
    CHECK(a == b);
}

TEST_CASE("HexSet copy-assignment produces an equal independent copy") {
    HexSet a;
    a.set(10);
    a.set(200);
    HexSet b = a;
    CHECK(a == b);
    b.set(50);
    CHECK(!(a == b)); // modifying b does not affect a
}

// for_each_bit_set.

TEST_CASE("HexSet for_each_bit_set visits exactly the set locations") {
    HexSet hs;
    hs.set(0);
    hs.set(64);
    hs.set(128);
    hs.set(192);

    std::vector<int> visited;
    hs.for_each_bit_set([&](int loc) { visited.push_back(loc); });

    REQUIRE(visited.size() == 4);
    CHECK(visited[0] == 0);
    CHECK(visited[1] == 64);
    CHECK(visited[2] == 128);
    CHECK(visited[3] == 192);
}

TEST_CASE("HexSet for_each_bit_set on empty set calls f zero times") {
    HexSet hs;
    int calls = 0;
    hs.for_each_bit_set([&](int) { ++calls; });
    CHECK(calls == 0);
}

TEST_CASE("HexSet for_each_bit_set count matches count()") {
    HexSet hs;
    hs.set(7);
    hs.set(63);
    hs.set(100);

    int calls = 0;
    hs.for_each_bit_set([&](int) { ++calls; });
    CHECK(calls == hs.count());
}
