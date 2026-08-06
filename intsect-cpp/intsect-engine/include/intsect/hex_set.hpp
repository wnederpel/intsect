#pragma once

#include "intsect/types.hpp"

#include <array>
#include <bit>
#include <cstdint>

namespace intsect {

// Number of 64-bit words needed to represent one bit per board location.
inline constexpr int HEX_SET_NUM_WORDS = GRID_SIZE / 64;

// A dense bitset covering all GRID_SIZE board locations (one bit per location).
//
// Bit layout: location L lives in word[L >> 6], bit (L & 63).
// Default construction zero-initialises all bits.
struct HexSet {
    std::array<uint64_t, HEX_SET_NUM_WORDS> table{};

    void set(int loc) noexcept {
        table[static_cast<size_t>(loc >> 6)] |= uint64_t{1} << (loc & 63);
    }

    void remove(int loc) noexcept {
        table[static_cast<size_t>(loc >> 6)] &= ~(uint64_t{1} << (loc & 63));
    }

    void toggle(int loc) noexcept {
        table[static_cast<size_t>(loc >> 6)] ^= uint64_t{1} << (loc & 63);
    }

    [[nodiscard]] bool get(int loc) const noexcept {
        return (table[static_cast<size_t>(loc >> 6)] & (uint64_t{1} << (loc & 63))) != 0;
    }

    void clear() noexcept {
        table.fill(0);
    }

    // Bitwise OR with another set.
    void union_with(const HexSet& other) noexcept {
        for (int i = 0; i < HEX_SET_NUM_WORDS; ++i)
            table[static_cast<size_t>(i)] |= other.table[static_cast<size_t>(i)];
    }

    // Return the number of set bits.
    [[nodiscard]] int count() const noexcept {
        int n = 0;
        for (uint64_t w : table)
            n += std::popcount(w);
        return n;
    }

    [[nodiscard]] bool operator==(const HexSet& other) const noexcept {
        return table == other.table;
    }

    // Call f(loc) for each set bit in ascending location order.
    // Uses std::countr_zero (C++20) to find the lowest set bit efficiently.
    template <typename F> void for_each_bit_set(F f) const noexcept {
        for (int i = 0; i < HEX_SET_NUM_WORDS; ++i) {
            uint64_t word = table[static_cast<size_t>(i)];
            while (word != 0) {
                int b = std::countr_zero(word);
                word &= word - uint64_t{1}; // clear lowest set bit
                f(i * 64 + b);
            }
        }
    }
};

} // namespace intsect
