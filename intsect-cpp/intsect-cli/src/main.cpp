// Universal Hive Protocol engine front-end.
// https://github.com/jonthysell/Mzinga/wiki/UniversalHiveProtocol

#include "intsect/board.hpp"
#include "intsect/game_string.hpp"
#include "intsect/tile.hpp"
#include "intsect/types.hpp"
#include "intsect/version.hpp"

#include <array>
#include <iomanip>
#include <iostream>
#include <optional>
#include <sstream>
#include <string>
#include <vector>

namespace {

using intsect::Board;
using intsect::Bug;
using intsect::Color;
using intsect::EMPTY_TILE;
using intsect::get_tile_height;
using intsect::GRID_SIZE;
using intsect::ROW_SIZE;
using intsect::tile_name;
using intsect::Variant;

// ---- Engine state ----

struct EngineState {
    std::optional<Board> board; // nullopt until newgame is called
    Variant variant = Variant::MLP;
    std::vector<std::string> move_history;

    void reset(Variant v = Variant::MLP) {
        variant = v;
        board.emplace(v);
        move_history.clear();
    }

    [[nodiscard]] bool has_game() const noexcept {
        return board.has_value();
    }

    [[nodiscard]] std::string game_string() const {
        return intsect::build_game_string(*board, move_history, variant);
    }
};

// ---- Board visualization ----

std::string lpad(const std::string& s, int width) {
    if (static_cast<int>(s.size()) >= width)
        return s.substr(0, static_cast<size_t>(width));
    return std::string(static_cast<size_t>(width - static_cast<int>(s.size())), ' ') + s;
}

void print_board_visual(const Board& board) {
    // Hex grid.  Row 0 has the most indent (2*(ROW_SIZE-1) spaces); row ROW_SIZE-1 has none.
    for (int row = 0; row < ROW_SIZE; ++row) {
        const int indent = 2 * (ROW_SIZE - 1 - row);
        for (int i = 0; i < indent; ++i)
            std::cout << ' ';

        for (int col = 0; col < ROW_SIZE; ++col) {
            const int loc      = row * ROW_SIZE + col;
            const uint8_t tile = board.get_tile_on_board(loc);

            std::string cell;
            if (tile != EMPTY_TILE) {
                cell = tile_name(tile);
                if (cell.size() == 2)
                    cell += ' '; // pad 2-char names ("wQ ") to 3 chars
            } else {
                cell = "\033[2m" + lpad(std::to_string(loc), 3) + "\033[0m";
            }
            std::cout << cell << ' ';
        }
        std::cout << '\n';
    }

    // Piece location table.
    std::cout << '\n';
    constexpr std::array<Bug, 8> BUG_ORDER    = {Bug::ANT,     Bug::GRASSHOPPER, Bug::BEETLE,
                                                 Bug::SPIDER,  Bug::QUEEN,       Bug::LADYBUG,
                                                 Bug::PILLBUG, Bug::MOSQUITO};
    constexpr std::array<uint8_t, 8> MAX_NUMS = {2, 2, 1, 1, 0, 0, 0, 0};

    for (size_t bi = 0; bi < BUG_ORDER.size(); ++bi) {
        const Bug bug         = BUG_ORDER[bi];
        const uint8_t max_num = MAX_NUMS[bi];

        for (uint8_t num = 0; num <= max_num; ++num) {
            const uint8_t wtile = intsect::tile_from_info(Color::White, bug, num);
            const uint8_t btile = intsect::tile_from_info(Color::Black, bug, num);
            const int wloc      = board.get_loc(wtile);
            const int bloc      = board.get_loc(btile);

            auto format_loc = [&](int loc, uint8_t base_tile) -> std::string {
                if (loc == intsect::NOT_PLACED || loc == intsect::INVALID_LOC)
                    return "";
                if (loc == intsect::UNDERGROUND) {
                    for (int l = 0; l < GRID_SIZE; ++l) {
                        for (const uint8_t t : board.underworld[static_cast<size_t>(l)]) {
                            if ((t & ~intsect::HEIGHT_MASK) == (base_tile & ~intsect::HEIGHT_MASK))
                                return std::to_string(l) + "(under)";
                        }
                    }
                    return "underground";
                }
                const uint8_t h = get_tile_height(board.get_tile_on_board(loc));
                return (h > 1) ? std::to_string(loc) + "^" + std::to_string(h)
                               : std::to_string(loc);
            };

            const bool w_placed = wloc > intsect::NOT_PLACED || wloc == intsect::UNDERGROUND;
            const bool b_placed = bloc > intsect::NOT_PLACED || bloc == intsect::UNDERGROUND;

            if (!w_placed && !b_placed)
                continue;

            const std::string wpart =
                w_placed ? (tile_name(wtile) + " : " + format_loc(wloc, wtile)) : "";
            const std::string bpart =
                b_placed ? (tile_name(btile) + " : " + format_loc(bloc, btile)) : "";

            if (!wpart.empty() && !bpart.empty())
                std::cout << std::left << std::setw(20) << wpart << bpart << '\n';
            else if (!wpart.empty())
                std::cout << wpart << '\n';
            else
                std::cout << std::string(20, ' ') << bpart << '\n';
        }
    }
}

void show_board(const EngineState& state) {
    std::cout << "----------------------------------\n";
    std::cout << state.game_string() << '\n';
    std::cout << "----------------------------------\n";
    print_board_visual(*state.board);
    std::cout << "----------------------------------\n";
}

// ---- UHP command handlers ----

void cmd_info() {
    std::cout << "id " << intsect::version() << '\n';
    std::cout << "Mosquito;Ladybug;Pillbug\n";
    std::cout << "ok\n";
}

void cmd_newgame(EngineState& state, const std::string& param) {
    if (param.empty()) {
        state.reset(Variant::Base);
        std::cout << state.game_string() << '\n';
        std::cout << "ok\n";
        return;
    }

    if (param.find(';') == std::string::npos) {
        // GameTypeString only (e.g. "Base+MLP").
        state.reset(intsect::parse_game_type_string(param));
        std::cout << state.game_string() << '\n';
        std::cout << "ok\n";
        return;
    }

    // Full GameString: split on ';', parse variant, skip state/turn, replay moves.
    std::vector<std::string> parts;
    {
        std::istringstream iss(param);
        std::string token;
        while (std::getline(iss, token, ';'))
            parts.push_back(token);
    }

    if (parts.size() < 3) {
        std::cout << "err Invalid GameString\n";
        std::cout << "ok\n";
        return;
    }

    state.reset(intsect::parse_game_type_string(parts[0]));

    for (size_t i = 3; i < parts.size(); ++i) {
        const auto action = intsect::action_from_move_string(*state.board, parts[i]);
        if (!action.has_value()) {
            std::cout << "err Cannot parse move: " << parts[i] << '\n';
            std::cout << "ok\n";
            return;
        }
        if (!state.board->do_action(*action)) {
            std::cout << "err Cannot apply move: " << parts[i] << '\n';
            std::cout << "ok\n";
            return;
        }
        state.move_history.push_back(parts[i]);
    }

    std::cout << state.game_string() << '\n';
    std::cout << "ok\n";
}

void cmd_play(EngineState& state, const std::string& move_str) {
    if (!state.has_game()) {
        std::cout << "err No game in progress. Use newgame first.\n";
        std::cout << "ok\n";
        return;
    }
    if (move_str.empty()) {
        std::cout << "err Missing MoveString\n";
        std::cout << "ok\n";
        return;
    }

    const auto action = intsect::action_from_move_string(*state.board, move_str);
    if (!action.has_value()) {
        std::cout << "invalidmove Cannot parse: " << move_str << '\n';
        std::cout << "ok\n";
        return;
    }
    if (!state.board->do_action(*action)) {
        std::cout << "invalidmove Illegal move: " << move_str << '\n';
        std::cout << "ok\n";
        return;
    }
    state.move_history.push_back(move_str);
    std::cout << state.game_string() << '\n';
    std::cout << "ok\n";
}

void cmd_undo(EngineState& state, int count) {
    if (!state.has_game()) {
        std::cout << "err No game in progress. Use newgame first.\n";
        std::cout << "ok\n";
        return;
    }
    for (int i = 0; i < count; ++i) {
        if (!state.board->undo()) {
            std::cout << "err Nothing left to undo\n";
            std::cout << "ok\n";
            return;
        }
        if (!state.move_history.empty())
            state.move_history.pop_back();
    }
    std::cout << state.game_string() << '\n';
    std::cout << "ok\n";
}

bool require_game(const EngineState& state) {
    if (!state.has_game()) {
        std::cout << "err No game in progress. Use newgame first.\n";
        std::cout << "ok\n";
        return false;
    }
    return true;
}

} // namespace

int main() {
    EngineState state; // no active game until newgame is called

    // UHP: print info automatically on startup so the viewer knows the engine is ready.
    cmd_info();

    std::string line;
    while (std::getline(std::cin, line)) {
        // Strip trailing carriage return (Windows line endings).
        if (!line.empty() && line.back() == '\r')
            line.pop_back();
        if (line.empty())
            continue;

        std::istringstream iss(line);
        std::string cmd;
        iss >> cmd;

        std::string rest;
        std::getline(iss >> std::ws, rest);

        if (cmd == "info") {
            cmd_info();
        } else if (cmd == "newgame") {
            cmd_newgame(state, rest);
        } else if (cmd == "play") {
            cmd_play(state, rest);
        } else if (cmd == "pass") {
            // "pass" is shorthand for "play pass".
            cmd_play(state, "pass");
        } else if (cmd == "validmoves") {
            if (require_game(state)) {
                std::cout << "err validmoves not yet implemented\n";
                std::cout << "ok\n";
            }
        } else if (cmd == "bestmove") {
            if (require_game(state)) {
                std::cout << "err bestmove not yet implemented\n";
                std::cout << "ok\n";
            }
        } else if (cmd == "undo") {
            int count = 1;
            if (!rest.empty()) {
                std::istringstream n_iss(rest);
                if (!(n_iss >> count) || count < 1)
                    count = 1;
            }
            cmd_undo(state, count);
        } else if (cmd == "options") {
            // No configurable options yet.
            std::cout << "ok\n";
        } else if (cmd == "show") {
            if (require_game(state)) {
                show_board(state);
                std::cout << "ok\n";
            }
        } else if (cmd == "quit" || cmd == "exit") {
            break;
        } else {
            std::cout
                << "err Invalid command: " << cmd
                << ". Valid: info newgame play pass validmoves bestmove undo options show quit\n";
            std::cout << "ok\n";
        }
    }

    return 0;
}
