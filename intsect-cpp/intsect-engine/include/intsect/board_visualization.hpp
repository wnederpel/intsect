#pragma once

#include "intsect/board.hpp"
#include "intsect/game_string.hpp"
#include "intsect/tile.hpp"
#include "intsect/types.hpp"

#include <array>
#include <iomanip>
#include <iostream>
#include <string>

namespace intsect {

namespace detail {

inline std::string left_pad(const std::string& s, int width) {
    if (static_cast<int>(s.size()) >= width)
        return s.substr(0, static_cast<size_t>(width));
    return std::string(static_cast<size_t>(width - static_cast<int>(s.size())), ' ') + s;
}

inline std::string format_piece_location(const Board& board, int loc, uint8_t base_tile) {
    if (loc == NOT_PLACED || loc == INVALID_LOC)
        return "";
    if (loc == UNDERGROUND) {
        for (int l = 0; l < GRID_SIZE; ++l) {
            for (const uint8_t t : board.underworld[static_cast<size_t>(l)]) {
                if ((t & ~HEIGHT_MASK) == (base_tile & ~HEIGHT_MASK))
                    return std::to_string(l) + "(under)";
            }
        }
        return "underground";
    }
    const uint8_t h = get_tile_height(board.get_tile_on_board(loc));
    return (h > 1) ? std::to_string(loc) + "^" + std::to_string(h) : std::to_string(loc);
}

} // namespace detail

inline void print_board_visual(const Board& board, std::ostream& out = std::cout,
                               bool use_ansi_for_empty = true) {
    // Hex grid. Row 0 has the most indent (2*(ROW_SIZE-1) spaces); row ROW_SIZE-1 has none.
    for (int row = 0; row < ROW_SIZE; ++row) {
        const int indent = 2 * (ROW_SIZE - 1 - row);
        for (int i = 0; i < indent; ++i)
            out << ' ';

        for (int col = 0; col < ROW_SIZE; ++col) {
            const int loc = row * ROW_SIZE + col;
            const uint8_t tile = board.get_tile_on_board(loc);

            std::string cell;
            if (tile != EMPTY_TILE) {
                cell = tile_name(tile);
                if (cell.size() == 2)
                    cell += ' '; // pad 2-char names ("wQ ") to 3 chars
            } else if (use_ansi_for_empty) {
                cell = "\033[2m" + detail::left_pad(std::to_string(loc), 3) + "\033[0m";
            } else {
                cell = detail::left_pad(std::to_string(loc), 3);
            }
            out << cell << ' ';
        }
        out << '\n';
    }

    // Piece location table.
    out << '\n';
    constexpr std::array<Bug, 8> BUG_ORDER = {Bug::ANT,     Bug::GRASSHOPPER, Bug::BEETLE,
                                              Bug::SPIDER,  Bug::QUEEN,       Bug::LADYBUG,
                                              Bug::PILLBUG, Bug::MOSQUITO};
    constexpr std::array<uint8_t, 8> MAX_NUMS = {2, 2, 1, 1, 0, 0, 0, 0};

    for (size_t bi = 0; bi < BUG_ORDER.size(); ++bi) {
        const Bug bug = BUG_ORDER[bi];
        const uint8_t max_num = MAX_NUMS[bi];

        for (uint8_t num = 0; num <= max_num; ++num) {
            const uint8_t wtile = tile_from_info(Color::White, bug, num);
            const uint8_t btile = tile_from_info(Color::Black, bug, num);
            const int wloc = board.get_loc(wtile);
            const int bloc = board.get_loc(btile);

            const bool w_placed = wloc > NOT_PLACED || wloc == UNDERGROUND;
            const bool b_placed = bloc > NOT_PLACED || bloc == UNDERGROUND;

            if (!w_placed && !b_placed)
                continue;

            const std::string wpart =
                w_placed
                    ? (tile_name(wtile) + " : " + detail::format_piece_location(board, wloc, wtile))
                    : "";
            const std::string bpart =
                b_placed
                    ? (tile_name(btile) + " : " + detail::format_piece_location(board, bloc, btile))
                    : "";

            if (!wpart.empty() && !bpart.empty())
                out << std::left << std::setw(20) << wpart << bpart << '\n';
            else if (!wpart.empty())
                out << wpart << '\n';
            else
                out << std::string(20, ' ') << bpart << '\n';
        }
    }
}

} // namespace intsect