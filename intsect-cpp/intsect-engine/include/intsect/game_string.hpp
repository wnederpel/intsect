#pragma once

// Move string / game string generation and parsing following the Universal Hive Protocol.
// https://github.com/jonthysell/Mzinga/wiki/UniversalHiveProtocol

#include "intsect/board.hpp"
#include "intsect/tile.hpp"
#include "intsect/types.hpp"

#include <array>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace intsect {

// ---- Tile name ----

inline std::string tile_name(uint8_t tile) {
    if (tile == EMPTY_TILE)
        return "empty";

    const Color color     = get_tile_color(tile);
    const Bug bug         = get_tile_bug(tile);
    const uint8_t bug_num = get_tile_bug_num(tile);

    std::string name;
    name += (color == Color::White) ? 'w' : 'b';

    switch (bug) {
    case Bug::ANT:
        name += 'A';
        break;
    case Bug::GRASSHOPPER:
        name += 'G';
        break;
    case Bug::BEETLE:
        name += 'B';
        break;
    case Bug::SPIDER:
        name += 'S';
        break;
    case Bug::QUEEN:
        name += 'Q';
        break;
    case Bug::LADYBUG:
        name += 'L';
        break;
    case Bug::PILLBUG:
        name += 'P';
        break;
    case Bug::MOSQUITO:
        name += 'M';
        break;
    }

    // Multi-instance bugs (Ant, Grasshopper, Beetle, Spider) get a 1-based number.
    if (bug == Bug::ANT || bug == Bug::GRASSHOPPER || bug == Bug::BEETLE || bug == Bug::SPIDER) {
        name += static_cast<char>('1' + bug_num);
    }

    return name;
}

// Look up a tile by UHP short name ("wQ", "bA1", etc.).
// Returns EMPTY_TILE if the name is unrecognised.
inline uint8_t tile_by_name(std::string_view name) {
    if (name.size() < 2 || name.size() > 3)
        return EMPTY_TILE;

    Color color{};
    if (name[0] == 'w')
        color = Color::White;
    else if (name[0] == 'b')
        color = Color::Black;
    else
        return EMPTY_TILE;

    Bug bug{};
    switch (name[1]) {
    case 'A':
        bug = Bug::ANT;
        break;
    case 'G':
        bug = Bug::GRASSHOPPER;
        break;
    case 'B':
        bug = Bug::BEETLE;
        break;
    case 'S':
        bug = Bug::SPIDER;
        break;
    case 'Q':
        bug = Bug::QUEEN;
        break;
    case 'L':
        bug = Bug::LADYBUG;
        break;
    case 'P':
        bug = Bug::PILLBUG;
        break;
    case 'M':
        bug = Bug::MOSQUITO;
        break;
    default:
        return EMPTY_TILE;
    }

    uint8_t bug_num = 0;
    if (name.size() == 3) {
        if (name[2] < '1' || name[2] > '3')
            return EMPTY_TILE;
        bug_num = static_cast<uint8_t>(name[2] - '1');
    }

    return tile_from_info(color, bug, bug_num);
}

// ---- Move string generation ----

// Direction iteration order matching Julia's `instances(Direction.T)`.
inline constexpr std::array<Direction, 6> DIRECTION_ORDER = {
    Direction::NW, Direction::NE, Direction::E, Direction::SE, Direction::SW, Direction::W};

// Generate the position descriptor (" wQ/", " -bA1", etc.) for a target location.
// moving_loc is INVALID_LOC for placements.
inline std::string move_string_goal(const Board& board, int goal_loc,
                                    int moving_loc = INVALID_LOC) {
    for (Direction dir : DIRECTION_ORDER) {
        const int neigh_loc = apply_direction(goal_loc, dir);
        uint8_t neigh_tile  = board.get_tile_on_board(neigh_loc);

        if (neigh_tile == EMPTY_TILE)
            continue;

        // Skip the moving piece unless it is elevated (beetle stacked).
        if (neigh_loc == moving_loc) {
            if (get_tile_height(neigh_tile) <= 1)
                continue;
            // Use the tile directly beneath the moving piece.
            const auto& stack = board.underworld[static_cast<size_t>(moving_loc)];
            if (stack.empty())
                continue;
            neigh_tile = stack.back();
        }

        const std::string ref = tile_name(neigh_tile);

        // dir is FROM goal_loc TO the reference piece; map to UHP notation.
        switch (dir) {
        case Direction::SE:
            return " \\" + ref; // ref is SE → goal is NW of ref → \ref
        case Direction::E:
            return " -" + ref; // ref is E  → goal is W  of ref → -ref
        case Direction::NE:
            return " /" + ref; // ref is NE → goal is SW of ref → /ref
        case Direction::NW:
            return " " + ref + "\\"; // ref is NW → goal is SE of ref (ref + backslash)
        case Direction::W:
            return " " + ref + "-"; // ref is W  → goal is E  → ref-
        case Direction::SW:
            return " " + ref + "/"; // ref is SW → goal is NE → ref/
        default:
            break;
        }
    }
    return ""; // first move of the game; no reference piece needed
}

inline std::string move_string_from_action(const Board& board, const Action& action) {
    switch (action.kind) {
    case ActionKind::Placement:
        return tile_name(action.tile) + move_string_goal(board, action.to);
    case ActionKind::Move: {
        const uint8_t moving_tile = board.get_tile_on_board(action.from);
        return tile_name(moving_tile) + move_string_goal(board, action.to, action.from);
    }
    case ActionKind::Climb: {
        const uint8_t moving_tile = board.get_tile_on_board(action.from);
        const uint8_t goal_tile   = board.get_tile_on_board(action.to);
        if (goal_tile != EMPTY_TILE) {
            // Climbing onto another piece: just name the target piece.
            return tile_name(moving_tile) + " " + tile_name(goal_tile);
        }
        return tile_name(moving_tile) + move_string_goal(board, action.to, action.from);
    }
    case ActionKind::Pass:
        return "pass";
    }
    return "pass"; // unreachable
}

// ---- Move string parsing ----

// Parse a UHP MoveString into an Action for the current board state.
// Returns nullopt if the string cannot be parsed.
inline std::optional<Action> action_from_move_string(const Board& board, const std::string& s) {
    if (s == "pass")
        return Action::make_pass();

    // Split "piece_name [pos_token]"
    const auto space             = s.find(' ');
    const std::string piece_name = (space == std::string::npos) ? s : s.substr(0, space);
    const std::string pos_token  = (space == std::string::npos) ? "" : s.substr(space + 1);

    const uint8_t piece_tile = tile_by_name(piece_name);
    if (piece_tile == EMPTY_TILE)
        return std::nullopt;

    // No position token → first move of the game, place at the centre.
    if (pos_token.empty()) {
        return Action::make_placement(MID, piece_tile);
    }

    // Determine the reference piece name and the direction from reference to goal.
    Direction goal_dir{};
    std::string ref_name;

    if (pos_token[0] == '\\') {
        goal_dir = Direction::NW; // \ref → goal is NW of ref
        ref_name = pos_token.substr(1);
    } else if (pos_token[0] == '-') {
        goal_dir = Direction::W; // -ref → goal is W of ref
        ref_name = pos_token.substr(1);
    } else if (pos_token[0] == '/') {
        goal_dir = Direction::SW; // /ref → goal is SW of ref
        ref_name = pos_token.substr(1);
    } else if (pos_token.back() == '\\') {
        goal_dir = Direction::SE; // ref\ → goal is SE of ref
        ref_name = pos_token.substr(0, pos_token.size() - 1);
    } else if (pos_token.back() == '-') {
        goal_dir = Direction::E; // ref- → goal is E of ref
        ref_name = pos_token.substr(0, pos_token.size() - 1);
    } else if (pos_token.back() == '/') {
        goal_dir = Direction::NE; // ref/ → goal is NE of ref
        ref_name = pos_token.substr(0, pos_token.size() - 1);
    } else {
        // No direction indicator → beetle/mosquito climbing on top of ref piece.
        ref_name               = pos_token;
        const uint8_t ref_tile = tile_by_name(ref_name);
        if (ref_tile == EMPTY_TILE)
            return std::nullopt;
        const int ref_loc = board.get_loc(ref_tile);
        if (ref_loc < 0)
            return std::nullopt;
        const int moving_loc = board.get_loc(piece_tile);
        if (moving_loc == NOT_PLACED)
            return Action::make_placement(ref_loc, piece_tile);
        return Action::make_climb(moving_loc, ref_loc);
    }

    // Locate the reference piece and compute the goal location.
    const uint8_t ref_tile = tile_by_name(ref_name);
    if (ref_tile == EMPTY_TILE)
        return std::nullopt;
    const int ref_loc = board.get_loc(ref_tile);
    if (ref_loc < 0)
        return std::nullopt;

    const int goal_loc   = apply_direction(ref_loc, goal_dir);
    const int moving_loc = board.get_loc(piece_tile);

    if (moving_loc == NOT_PLACED)
        return Action::make_placement(goal_loc, piece_tile);

    const uint8_t goal_tile = board.get_tile_on_board(goal_loc);
    if (goal_tile != EMPTY_TILE)
        return Action::make_climb(moving_loc, goal_loc);
    return Action::make_move(moving_loc, goal_loc);
}

// ---- Game string components ----

inline std::string game_type_string(Variant variant) {
    const bool m = (static_cast<uint8_t>(variant) & static_cast<uint8_t>(Variant::M)) != 0u;
    const bool l = (static_cast<uint8_t>(variant) & static_cast<uint8_t>(Variant::L)) != 0u;
    const bool p = (static_cast<uint8_t>(variant) & static_cast<uint8_t>(Variant::P)) != 0u;

    std::string s = "Base";
    if (m || l || p) {
        s += '+';
        if (m)
            s += 'M';
        if (l)
            s += 'L';
        if (p)
            s += 'P';
    }
    return s;
}

inline Variant parse_game_type_string(const std::string& s) {
    // Expected: "Base" or "Base+[M][L][P]"
    uint8_t flags = 0;
    for (size_t i = 5; i < s.size(); ++i) { // characters after "Base+"
        if (s[i] == 'M')
            flags |= static_cast<uint8_t>(Variant::M);
        else if (s[i] == 'L')
            flags |= static_cast<uint8_t>(Variant::L);
        else if (s[i] == 'P')
            flags |= static_cast<uint8_t>(Variant::P);
    }
    return static_cast<Variant>(flags);
}

inline std::string game_state_string(const Board& board) {
    if (board.gameover) {
        switch (board.victor) {
        case Color::White:
            return "WhiteWins";
        case Color::Black:
            return "BlackWins";
        default:
            return "Draw";
        }
    }
    if (board.history.empty())
        return "NotStarted";
    return "InProgress";
}

inline std::string turn_string(const Board& board) {
    const std::string color = (board.current_color == Color::White) ? "White" : "Black";
    return color + "[" + std::to_string(board.turn) + "]";
}

// Build a full UHP GameString.
// move_history contains the UHP MoveStrings played so far (in order).
inline std::string build_game_string(const Board& board,
                                     const std::vector<std::string>& move_history,
                                     Variant variant) {
    std::string gs =
        game_type_string(variant) + ";" + game_state_string(board) + ";" + turn_string(board);
    for (const auto& ms : move_history)
        gs += ";" + ms;
    return gs;
}

} // namespace intsect
