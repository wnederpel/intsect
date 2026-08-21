#pragma once

#include "board.hpp"

#include <array>
#include <bit>
#include <vector>

namespace intsect {

inline constexpr int VALID_BUFFER_SIZE = 400;
inline constexpr std::array<uint8_t, BUGS_IN_PLAY> MAX_NUMS = {2, 2, 1, 1, 0, 0, 0, 0};

inline constexpr std::array<Direction, 6> JULIA_NEIGH_ORDER = {
    Direction::E, Direction::SE, Direction::SW, Direction::W, Direction::NW, Direction::NE};

inline constexpr std::array<Direction, 6> MOVEGEN_DIRECTION_ORDER = {
    Direction::NW, Direction::NE, Direction::E, Direction::SE, Direction::SW, Direction::W};

inline std::array<int, 6> all_neighs(int loc) noexcept {
    return {apply_direction(loc, Direction::E),  apply_direction(loc, Direction::SE),
            apply_direction(loc, Direction::SW), apply_direction(loc, Direction::W),
            apply_direction(loc, Direction::NW), apply_direction(loc, Direction::NE)};
}

[[nodiscard]] inline bool can_slide(int i, const Board& board,
                                    const std::array<int, 6>& neigh_locs) noexcept {
    const int left_i = (i == 0) ? 5 : i - 1;
    const int right_i = (i == 5) ? 0 : i + 1;

    const int left_neigh = neigh_locs[static_cast<size_t>(left_i)];
    const int right_neigh = neigh_locs[static_cast<size_t>(right_i)];
    const int goal = neigh_locs[static_cast<size_t>(i)];

    const bool goal_empty = board.get_tile_on_board(goal) == EMPTY_TILE;
    const bool left_empty = board.get_tile_on_board(left_neigh) == EMPTY_TILE;
    const bool right_empty = board.get_tile_on_board(right_neigh) == EMPTY_TILE;
    return goal_empty && (left_empty != right_empty);
}

[[nodiscard]] inline uint8_t get_slide_neighs(const Board& board,
                                              const std::array<int, 6>& all_neighbors) noexcept {
    int occupied = 0;
    for (int i = 5; i >= 0; --i) {
        occupied <<= 1;
        if (board.get_tile_on_board(all_neighbors[static_cast<size_t>(i)]) != EMPTY_TILE) {
            occupied |= 1;
            occupied |= 1 << 6;
            occupied |= 1 << 12;
        }
    }

    const int slidable = ((~occupied & ((occupied << 1) ^ (occupied >> 1))) >> 6) & 0x3f;
    return static_cast<uint8_t>(slidable);
}

inline void move_1(const Board& board, int startloc, HexSet& move_to_set) {
    const std::array<int, 6> neigh_locs = all_neighs(startloc);
    uint8_t slide_neighs = get_slide_neighs(board, neigh_locs);

    while (slide_neighs != 0) {
        const uint8_t bit_i = static_cast<uint8_t>(std::countr_zero(slide_neighs));
        slide_neighs = static_cast<uint8_t>(slide_neighs & static_cast<uint8_t>(slide_neighs - 1));
        move_to_set.set(neigh_locs[bit_i]);
    }
}

inline void queen_moves(const Board& board, int startloc, HexSet& move_to_set) {
    move_1(board, startloc, move_to_set);
}

inline void grasshopper_moves(const Board& board, int startloc, HexSet& move_to_set) {
    for (Direction dir : MOVEGEN_DIRECTION_ORDER) {
        if (board.get_tile_on_board(apply_direction(startloc, dir)) == EMPTY_TILE)
            continue;

        int loc = startloc;
        while (true) {
            loc = apply_direction(loc, dir);
            if (board.get_tile_on_board(loc) == EMPTY_TILE)
                break;
        }
        move_to_set.set(loc);
    }
}

inline void update_ispinned_general(Board& board) {
    board.ispinned.clear();

    PinnedStoreEntry& pinned_entry =
        board.pinned_store[static_cast<size_t>(board.location_hash & PINNED_STORE_MASK)];
    if (pinned_entry.location_hash == board.location_hash) {
        board.ispinned.union_with(pinned_entry.pinned_pieces_hs);
        return;
    }

    HexSet& visited = board.workspaces.ispinned_visited;
    visited.clear();

    std::array<int, GRID_SIZE>& depth_dict = board.workspaces.depth_dict;
    std::array<int, GRID_SIZE>& low_dict = board.workspaces.low_dict;
    std::array<int, GRID_SIZE>& parent_dict = board.workspaces.parent_dict;
    parent_dict.fill(INVALID_LOC);

    int start_loc = MID;
    if (board.queen_pos_white >= 0)
        start_loc = board.queen_pos_white;
    else if (board.queen_pos_black >= 0)
        start_loc = board.queen_pos_black;

    struct Rec {
        static void run(Board& b, HexSet& vis, std::array<int, GRID_SIZE>& depth,
                        std::array<int, GRID_SIZE>& low, std::array<int, GRID_SIZE>& parent,
                        int loc, int d) {
            vis.set(loc);
            depth[static_cast<size_t>(loc)] = d;
            low[static_cast<size_t>(loc)] = d;

            int child_count = 0;
            bool is_articulation = false;
            const auto neighbors = all_neighs(loc);
            for (int nloc : neighbors) {
                if (b.get_tile_on_board(nloc) == EMPTY_TILE)
                    continue;

                if (!vis.get(nloc)) {
                    parent[static_cast<size_t>(nloc)] = loc;
                    run(b, vis, depth, low, parent, nloc, d + 1);
                    ++child_count;
                    if (low[static_cast<size_t>(nloc)] >= depth[static_cast<size_t>(loc)])
                        is_articulation = true;
                    low[static_cast<size_t>(loc)] =
                        std::min(low[static_cast<size_t>(loc)], low[static_cast<size_t>(nloc)]);
                } else if (nloc != parent[static_cast<size_t>(loc)]) {
                    low[static_cast<size_t>(loc)] =
                        std::min(low[static_cast<size_t>(loc)], depth[static_cast<size_t>(nloc)]);
                }
            }

            if ((parent[static_cast<size_t>(loc)] != INVALID_LOC && is_articulation) ||
                (parent[static_cast<size_t>(loc)] == INVALID_LOC && child_count > 1)) {
                b.ispinned.set(loc);
            }
        }
    };

    Rec::run(board, visited, depth_dict, low_dict, parent_dict, start_loc, 0);

    pinned_entry.location_hash = board.location_hash;
    pinned_entry.pinned_pieces_hs.clear();
    pinned_entry.pinned_pieces_hs.union_with(board.ispinned);
}

inline void ant_moves(Board& board, int startloc, HexSet& move_to_set) {
    const uint8_t tmp_tile = board.get_tile_on_board(startloc);
    const uint64_t move_entry_hash = board.location_hash ^ detail::location_hash_value(startloc);

    MoveStoreEntry& move_entry =
        board.move_store[static_cast<size_t>(move_entry_hash & MOVE_STORE_MASK)];
    if (move_entry.location_hash == move_entry_hash && move_entry.ant_reachable_hs.get(startloc)) {
        move_to_set.union_with(move_entry.ant_reachable_hs);
        move_to_set.remove(startloc);
        return;
    }

    board.set_tile_on_board(startloc, EMPTY_TILE);

    std::array<int, GRID_SIZE>& stack_arr = board.workspaces.ant_stack;
    int stack_ptr = 1;
    stack_arr[0] = startloc;
    move_to_set.set(startloc);

    while (stack_ptr != 0) {
        const int loc = stack_arr[static_cast<size_t>(stack_ptr - 1)];
        --stack_ptr;

        const std::array<int, 6> neigh_locs = all_neighs(loc);
        uint8_t slide_neighs = get_slide_neighs(board, neigh_locs);

        while (slide_neighs != 0) {
            const uint8_t bit_i = static_cast<uint8_t>(std::countr_zero(slide_neighs));
            slide_neighs =
                static_cast<uint8_t>(slide_neighs & static_cast<uint8_t>(slide_neighs - 1));

            const int neigh_loc = neigh_locs[bit_i];
            if (!move_to_set.get(neigh_loc)) {
                move_to_set.set(neigh_loc);
                stack_arr[static_cast<size_t>(stack_ptr)] = neigh_loc;
                ++stack_ptr;
            }
        }
    }

    board.set_tile_on_board(startloc, tmp_tile);

    if (move_entry.location_hash != move_entry_hash && move_to_set.count() > 10) {
        move_entry.location_hash = move_entry_hash;
        move_entry.ant_reachable_hs.clear();
        move_entry.ant_reachable_hs.union_with(move_to_set);
    }

    move_to_set.remove(startloc);
}

inline void moves_to_depth(Board& board, int startloc, int depth, HexSet& move_to_set, int cur_loc,
                           int prev_loc) {
    if (depth == 0) {
        if (cur_loc != startloc)
            move_to_set.set(cur_loc);
        return;
    }

    const std::array<int, 6> neigh_locs = all_neighs(cur_loc);
    uint8_t slide_neighs = get_slide_neighs(board, neigh_locs);
    while (slide_neighs != 0) {
        const uint8_t bit_i = static_cast<uint8_t>(std::countr_zero(slide_neighs));
        slide_neighs = static_cast<uint8_t>(slide_neighs & static_cast<uint8_t>(slide_neighs - 1));

        const int next_loc = neigh_locs[bit_i];
        if (next_loc == prev_loc)
            continue;
        moves_to_depth(board, startloc, depth - 1, move_to_set, next_loc, cur_loc);
    }
}

inline void move_3(Board& board, int startloc, HexSet& move_to_set) {
    const uint8_t tmp_tile = board.get_tile_on_board(startloc);
    board.set_tile_on_board(startloc, EMPTY_TILE);
    moves_to_depth(board, startloc, 3, move_to_set, startloc, INVALID_LOC);
    board.set_tile_on_board(startloc, tmp_tile);
}

inline void spider_moves(Board& board, int startloc, HexSet& move_to_set) {
    move_3(board, startloc, move_to_set);
}

[[nodiscard]] inline bool can_slide_pillbug(int i, const Board& board,
                                            const std::array<int, 6>& neigh_locs) {
    const int left_i = (i == 0) ? 5 : i - 1;
    const int right_i = (i == 5) ? 0 : i + 1;
    const uint8_t left_tile = board.get_tile_on_board(neigh_locs[static_cast<size_t>(left_i)]);
    const uint8_t right_tile = board.get_tile_on_board(neigh_locs[static_cast<size_t>(right_i)]);
    return get_tile_height(left_tile) < 2 || get_tile_height(right_tile) < 2;
}

[[nodiscard]] inline bool can_slide_high(int i, const Board& board,
                                         const std::array<int, 6>& neigh_locs, uint8_t height) {
    const int left_i = (i == 0) ? 5 : i - 1;
    const int right_i = (i == 5) ? 0 : i + 1;

    const uint8_t left_tile = board.get_tile_on_board(neigh_locs[static_cast<size_t>(left_i)]);
    const uint8_t right_tile = board.get_tile_on_board(neigh_locs[static_cast<size_t>(right_i)]);
    const uint8_t goal_tile = board.get_tile_on_board(neigh_locs[static_cast<size_t>(i)]);

    const uint8_t left_h = get_tile_height(left_tile);
    const uint8_t right_h = get_tile_height(right_tile);
    const uint8_t goal_h = get_tile_height(goal_tile);
    const uint8_t needed = static_cast<uint8_t>(std::max<int>(goal_h + 1, height));

    return left_h < needed || right_h < needed;
}

inline void beetle_moves(Board& board, int startloc, uint8_t height, HexSet& move_to_set) {
    const std::array<int, 6> neigh_locs = all_neighs(startloc);
    if (height != 1) {
        for (int i = 0; i < 6; ++i) {
            if (can_slide_high(i, board, neigh_locs, height))
                move_to_set.set(neigh_locs[static_cast<size_t>(i)]);
        }
        return;
    }

    const uint8_t tmp_tile = board.get_tile_on_board(startloc);
    board.set_tile_on_board(startloc, EMPTY_TILE);

    for (int i = 0; i < 6; ++i) {
        const int goal_loc = neigh_locs[static_cast<size_t>(i)];
        const bool occupied = board.get_tile_on_board(goal_loc) != EMPTY_TILE;
        if ((occupied && can_slide_high(i, board, neigh_locs, 1)) ||
            (!occupied && can_slide(i, board, neigh_locs))) {
            move_to_set.set(goal_loc);
        }
    }

    board.set_tile_on_board(startloc, tmp_tile);
}

inline void ladybug_moves(Board& board, int startloc, HexSet& move_to_set) {
    const uint8_t tmp_tile = board.get_tile_on_board(startloc);
    board.set_tile_on_board(startloc, EMPTY_TILE);

    HexSet& visited_step_2 = board.workspaces.ladybug_visited_step_2;
    visited_step_2.clear();

    const std::array<int, 6> neigh_locs = all_neighs(startloc);

    for (int i = 0; i < 6; ++i) {
        const int step_1_loc = neigh_locs[static_cast<size_t>(i)];
        const uint8_t step_1_tile = board.get_tile_on_board(step_1_loc);
        if (step_1_tile == EMPTY_TILE)
            continue;

        const int step_1_left_i = (i == 0) ? 5 : i - 1;
        const int step_1_right_i = (i == 5) ? 0 : i + 1;
        const uint8_t step_1_left_tile =
            board.get_tile_on_board(neigh_locs[static_cast<size_t>(step_1_left_i)]);
        const uint8_t step_1_right_tile =
            board.get_tile_on_board(neigh_locs[static_cast<size_t>(step_1_right_i)]);

        const uint8_t step_1_height_raw = static_cast<uint8_t>(step_1_tile & 0x03u);
        const uint8_t step_1_left_h = step_1_left_tile == EMPTY_TILE
                                          ? 0
                                          : static_cast<uint8_t>((step_1_left_tile & 0x03u) + 1);
        const uint8_t step_1_right_h = step_1_right_tile == EMPTY_TILE
                                           ? 0
                                           : static_cast<uint8_t>((step_1_right_tile & 0x03u) + 1);
        const uint8_t step_1_height = static_cast<uint8_t>(step_1_height_raw + 1);
        const uint8_t max_height_1 = static_cast<uint8_t>(step_1_height + 1);

        if (!(step_1_left_h < max_height_1 || step_1_right_h < max_height_1))
            continue;

        const std::array<int, 6> step_2_locs = all_neighs(step_1_loc);
        for (int j = 0; j < 6; ++j) {
            const int step_2_loc = step_2_locs[static_cast<size_t>(j)];
            const uint8_t step_2_tile = board.get_tile_on_board(step_2_loc);
            if (step_2_tile == EMPTY_TILE || visited_step_2.get(step_2_loc))
                continue;

            const int step_2_left_i = (j == 0) ? 5 : j - 1;
            const int step_2_right_i = (j == 5) ? 0 : j + 1;
            const uint8_t step_2_left_tile =
                board.get_tile_on_board(step_2_locs[static_cast<size_t>(step_2_left_i)]);
            const uint8_t step_2_right_tile =
                board.get_tile_on_board(step_2_locs[static_cast<size_t>(step_2_right_i)]);

            const uint8_t step_2_height_raw = static_cast<uint8_t>(step_2_tile & 0x03u);
            const uint8_t step_2_left_h =
                step_2_left_tile == EMPTY_TILE
                    ? 0
                    : static_cast<uint8_t>((step_2_left_tile & 0x03u) + 1);
            const uint8_t step_2_right_h =
                step_2_right_tile == EMPTY_TILE
                    ? 0
                    : static_cast<uint8_t>((step_2_right_tile & 0x03u) + 1);
            const uint8_t step_2_height = static_cast<uint8_t>(step_2_height_raw + 1);
            const uint8_t h_1_to_2 = static_cast<uint8_t>(step_1_height + 1);
            const uint8_t max_height_2 =
                static_cast<uint8_t>(std::max<int>(step_2_height + 1, h_1_to_2));

            if (!(step_2_left_h < max_height_2 || step_2_right_h < max_height_2))
                continue;

            visited_step_2.set(step_2_loc);

            const std::array<int, 6> step_3_locs = all_neighs(step_2_loc);
            for (int k = 0; k < 6; ++k) {
                const int step_3_loc = step_3_locs[static_cast<size_t>(k)];
                if (board.get_tile_on_board(step_3_loc) != EMPTY_TILE ||
                    move_to_set.get(step_3_loc))
                    continue;

                const int step_3_left_i = (k == 0) ? 5 : k - 1;
                const int step_3_right_i = (k == 5) ? 0 : k + 1;
                const uint8_t step_3_left_tile =
                    board.get_tile_on_board(step_3_locs[static_cast<size_t>(step_3_left_i)]);
                const uint8_t step_3_right_tile =
                    board.get_tile_on_board(step_3_locs[static_cast<size_t>(step_3_right_i)]);

                const uint8_t step_3_left_h =
                    step_3_left_tile == EMPTY_TILE
                        ? 0
                        : static_cast<uint8_t>((step_3_left_tile & 0x03u) + 1);
                const uint8_t step_3_right_h =
                    step_3_right_tile == EMPTY_TILE
                        ? 0
                        : static_cast<uint8_t>((step_3_right_tile & 0x03u) + 1);
                const uint8_t max_height_3 = static_cast<uint8_t>(step_2_height + 1);

                if (step_3_left_h < max_height_3 || step_3_right_h < max_height_3)
                    move_to_set.set(step_3_loc);
            }
        }
    }

    move_to_set.remove(startloc);
    board.set_tile_on_board(startloc, tmp_tile);
}

inline void pillbug_moves_normal(const Board& board, int startloc, const HexSet& ispinned,
                                 HexSet& move_to_set) {
    if (!ispinned.get(startloc))
        move_1(board, startloc, move_to_set);
}

inline void pillbug_moves_throw(const Board& board, int startloc, const HexSet& ispinned,
                                HexSet& from_locs_hs, HexSet& to_locs_hs) {
    const std::array<int, 6> neigh_locs = all_neighs(startloc);

    for (int i = 0; i < 6; ++i) {
        if (can_slide_pillbug(i, board, neigh_locs)) {
            const int loc = neigh_locs[static_cast<size_t>(i)];
            if (board.get_tile_on_board(loc) == EMPTY_TILE)
                to_locs_hs.set(loc);
        }
    }

    for (int i = 0; i < 6; ++i) {
        const int loc = neigh_locs[static_cast<size_t>(i)];
        const uint8_t tile = board.get_tile_on_board(loc);
        if (tile != EMPTY_TILE && !ispinned.get(loc) && loc != board.just_moved_loc &&
            get_tile_height(tile) == 1 && can_slide_pillbug(i, board, neigh_locs)) {
            from_locs_hs.set(loc);
        }
    }
}

inline void mosquito_moves(Board& board, int loc, uint8_t height, const HexSet& ispinned,
                           HexSet& move_to_set) {
    if (height > 1) {
        beetle_moves(board, loc, height, move_to_set);
        return;
    }

    if (ispinned.get(loc))
        return;

    int bugs_touched = 0;
    for (int neigh : all_neighs(loc)) {
        const uint8_t tile = board.get_tile_on_board(neigh);
        if (tile == EMPTY_TILE)
            continue;
        const int bug = static_cast<int>(get_tile_bug(tile));
        bugs_touched |= 1 << bug;
    }

    if ((bugs_touched & (1 << static_cast<int>(Bug::ANT))) != 0) {
        ant_moves(board, loc, move_to_set);
    } else {
        const int queen_or_pillbug =
            (1 << static_cast<int>(Bug::QUEEN)) | (1 << static_cast<int>(Bug::PILLBUG));
        if ((bugs_touched & queen_or_pillbug) != 0)
            queen_moves(board, loc, move_to_set);
        if ((bugs_touched & (1 << static_cast<int>(Bug::SPIDER))) != 0)
            spider_moves(board, loc, move_to_set);
    }

    if ((bugs_touched & (1 << static_cast<int>(Bug::GRASSHOPPER))) != 0)
        grasshopper_moves(board, loc, move_to_set);
    if ((bugs_touched & (1 << static_cast<int>(Bug::LADYBUG))) != 0)
        ladybug_moves(board, loc, move_to_set);
    if ((bugs_touched & (1 << static_cast<int>(Bug::BEETLE))) != 0)
        beetle_moves(board, loc, height, move_to_set);
}

inline void bugmoves(Board& board, int loc, int bug, uint8_t height, const HexSet& ispinned,
                     HexSet& move_to_set) {
    if (bug == static_cast<int>(Bug::BEETLE) && (!ispinned.get(loc) || height != 1)) {
        beetle_moves(board, loc, height, move_to_set);
    } else if (bug == static_cast<int>(Bug::MOSQUITO) && (!ispinned.get(loc) || height != 1)) {
        mosquito_moves(board, loc, height, ispinned, move_to_set);
    } else if (!ispinned.get(loc)) {
        if (bug == static_cast<int>(Bug::PILLBUG))
            pillbug_moves_normal(board, loc, ispinned, move_to_set);
        else if (bug == static_cast<int>(Bug::ANT))
            ant_moves(board, loc, move_to_set);
        else if (bug == static_cast<int>(Bug::SPIDER))
            spider_moves(board, loc, move_to_set);
        else if (bug == static_cast<int>(Bug::QUEEN))
            queen_moves(board, loc, move_to_set);
        else if (bug == static_cast<int>(Bug::GRASSHOPPER))
            grasshopper_moves(board, loc, move_to_set);
        else if (bug == static_cast<int>(Bug::LADYBUG))
            ladybug_moves(board, loc, move_to_set);
    }
}

inline void add_moves(Board& board, const HexSet& ispinned, std::vector<Action>& out_actions,
                      Color current_color) {
    HexSet& move_to_set = board.workspaces.move_to_set;
    HexSet& pillbug_throw_from = board.workspaces.pillbug_throw_from;
    HexSet& pillbug_throw_to = board.workspaces.pillbug_throw_to;
    HexSet& mosquito_throw_from = board.workspaces.mosquito_throw_from;
    HexSet& mosquito_throw_to = board.workspaces.mosquito_throw_to;

    const uint8_t wP = tile_from_info(Color::White, Bug::PILLBUG, 0);
    const uint8_t bP = tile_from_info(Color::Black, Bug::PILLBUG, 0);
    const uint8_t wM = tile_from_info(Color::White, Bug::MOSQUITO, 0);
    const uint8_t bM = tile_from_info(Color::Black, Bug::MOSQUITO, 0);

    const int wP_loc = board.get_loc(wP);
    const int bP_loc = board.get_loc(bP);
    const int my_pillbug_loc = (current_color == Color::White) ? wP_loc : bP_loc;
    const int my_mosquito_loc = board.get_loc((current_color == Color::White) ? wM : bM);

    pillbug_throw_from.clear();
    pillbug_throw_to.clear();
    if (my_pillbug_loc >= 0 && my_pillbug_loc != board.just_moved_loc)
        pillbug_moves_throw(board, my_pillbug_loc, board.ispinned, pillbug_throw_from,
                            pillbug_throw_to);

    mosquito_throw_from.clear();
    mosquito_throw_to.clear();

    if (my_mosquito_loc >= 0 && my_mosquito_loc != board.just_moved_loc &&
        get_tile_height(board.get_tile_on_board(my_mosquito_loc)) == 1) {
        const std::array<int, 6> mosq_neighs = all_neighs(my_mosquito_loc);
        bool mosq_can_throw = false;
        for (int n : mosq_neighs) {
            if (n == wP_loc || n == bP_loc) {
                mosq_can_throw = true;
                break;
            }
        }
        if (mosq_can_throw)
            pillbug_moves_throw(board, my_mosquito_loc, board.ispinned, mosquito_throw_from,
                                mosquito_throw_to);
    }

    const size_t color_i = static_cast<size_t>(static_cast<uint8_t>(current_color) - 1u);
    for (int bug = 1; bug <= 8; ++bug) {
        if (get_tile_bug_num(board.placeable_tiles[color_i][static_cast<size_t>(bug - 1)]) == 0)
            continue;

        const uint8_t max_num = MAX_NUMS[static_cast<size_t>(bug - 1)];
        for (uint8_t num = 0; num <= max_num; ++num) {
            const uint8_t semi_tile =
                tile_from_info_as_index(current_color, static_cast<Bug>(bug), num);
            const int loc = board.tile_locs[semi_tile];

            if (loc == NOT_PLACED)
                break;
            if (loc == UNDERGROUND || loc == board.just_moved_loc || loc == INVALID_LOC)
                continue;

            const uint8_t tile = board.get_tile_on_board(loc);
            const uint8_t height = get_tile_height(tile);

            move_to_set.clear();
            bugmoves(board, loc, bug, height, ispinned, move_to_set);

            if (pillbug_throw_from.get(loc)) {
                pillbug_throw_from.remove(loc);
                pillbug_throw_to.for_each_bit_set([&](int goal_loc) { move_to_set.set(goal_loc); });
            }
            if (mosquito_throw_from.get(loc)) {
                mosquito_throw_from.remove(loc);
                mosquito_throw_to.for_each_bit_set(
                    [&](int goal_loc) { move_to_set.set(goal_loc); });
            }

            const bool moves_are_climbs = height > 1;
            move_to_set.for_each_bit_set([&](int goal_loc) {
                if (moves_are_climbs || board.get_tile_on_board(goal_loc) != EMPTY_TILE)
                    out_actions.push_back(Action::make_climb(loc, goal_loc));
                else
                    out_actions.push_back(Action::make_move(loc, goal_loc));
            });
        }
    }

    pillbug_throw_from.for_each_bit_set([&](int moving_loc) {
        pillbug_throw_to.for_each_bit_set(
            [&](int goal_loc) { out_actions.push_back(Action::make_move(moving_loc, goal_loc)); });
    });

    mosquito_throw_from.for_each_bit_set([&](int moving_loc) {
        mosquito_throw_to.for_each_bit_set([&](int goal_loc) {
            if (pillbug_throw_from.get(moving_loc) && pillbug_throw_to.get(goal_loc))
                return;
            out_actions.push_back(Action::make_move(moving_loc, goal_loc));
        });
    });
}

inline void for_placement_locs(Board& board, Color current_color, const auto& callback) {
    const size_t me = static_cast<size_t>(static_cast<uint8_t>(current_color) - 1u);
    const size_t them = static_cast<size_t>(static_cast<uint8_t>(other_color(current_color)) - 1u);

    const HexSet& my_pieces = board.pieces[me];
    const HexSet& their_pieces = board.pieces[them];
    HexSet& no_placement_hs = board.workspaces.no_placement_hs;

    no_placement_hs.clear();

    their_pieces.for_each_bit_set([&](int loc) {
        const auto neighs = all_neighs(loc);
        no_placement_hs.set(loc);
        no_placement_hs.set(neighs[0]);
        no_placement_hs.set(neighs[1]);
        no_placement_hs.set(neighs[2]);
        no_placement_hs.set(neighs[3]);
        no_placement_hs.set(neighs[4]);
        no_placement_hs.set(neighs[5]);
    });

    my_pieces.for_each_bit_set([&](int loc) { no_placement_hs.set(loc); });

    my_pieces.for_each_bit_set([&](int loc) {
        const auto neighs = all_neighs(loc);
        for (int i = 0; i < 6; ++i) {
            const int neigh_loc = neighs[static_cast<size_t>(i)];
            if (!no_placement_hs.get(neigh_loc)) {
                no_placement_hs.set(neigh_loc);
                callback(neigh_loc);
            }
        }
    });
}

inline void first_placements(const Board& board, std::vector<Action>& out_actions,
                             Color current_color) {
    const size_t color_i = static_cast<size_t>(static_cast<uint8_t>(current_color) - 1u);
    for (uint8_t tile : board.placeable_tiles[color_i]) {
        if (tile != EMPTY_TILE && get_tile_bug(tile) != Bug::QUEEN)
            out_actions.push_back(Action::make_placement(MID, tile));
    }
}

inline void second_placements(const Board& board, std::vector<Action>& out_actions,
                              Color current_color) {
    const size_t color_i = static_cast<size_t>(static_cast<uint8_t>(current_color) - 1u);
    for (int loc : all_neighs(MID)) {
        for (uint8_t tile : board.placeable_tiles[color_i]) {
            if (tile != EMPTY_TILE && get_tile_bug(tile) != Bug::QUEEN)
                out_actions.push_back(Action::make_placement(loc, tile));
        }
    }
}

inline void queen_placements(Board& board, std::vector<Action>& out_actions, Color current_color) {
    const uint8_t queen_tile = (current_color == Color::White)
                                   ? tile_from_info(Color::White, Bug::QUEEN, 0)
                                   : tile_from_info(Color::Black, Bug::QUEEN, 0);

    for_placement_locs(board, current_color, [&](int placement_loc) {
        out_actions.push_back(Action::make_placement(placement_loc, queen_tile));
    });
}

inline void add_placements(Board& board, std::vector<Action>& out_actions, Color current_color) {
    const size_t color_i = static_cast<size_t>(static_cast<uint8_t>(current_color) - 1u);
    for_placement_locs(board, current_color, [&](int placement_loc) {
        for (uint8_t tile : board.placeable_tiles[color_i]) {
            if (tile != EMPTY_TILE)
                out_actions.push_back(Action::make_placement(placement_loc, tile));
        }
    });
}

inline void valid_actions_general(Board& board, std::vector<Action>& out_actions,
                                  Color current_color) {
    if (board.queen_placed[static_cast<size_t>(static_cast<uint8_t>(current_color) - 1u)]) {
        update_ispinned_general(board);
        add_moves(board, board.ispinned, out_actions, current_color);
    }

    add_placements(board, out_actions, current_color);

    if (out_actions.empty())
        out_actions.push_back(Action::make_pass());
}

inline std::vector<Action> get_valid_actions(Board& board, Color current_color) {
    // TODO: There needs to be another version of the valid_actions method that also accepts the
    // out_actions as an input to avoid allocations
    std::vector<Action> out_actions;
    out_actions.reserve(VALID_BUFFER_SIZE);

    if (board.gameover)
        return out_actions;

    const size_t color_i = static_cast<size_t>(static_cast<uint8_t>(current_color) - 1u);
    const bool need_to_place_queen = !board.queen_placed[color_i] && board.turn == 4;
    const bool first_placement = board.ply == 1;
    const bool second_placement = board.ply == 2;

    if (need_to_place_queen)
        queen_placements(board, out_actions, current_color);
    else if (first_placement)
        first_placements(board, out_actions, current_color);
    else if (second_placement)
        second_placements(board, out_actions, current_color);
    else
        valid_actions_general(board, out_actions, current_color);

    return out_actions;
}

inline std::vector<Action> get_valid_actions(Board& board) {
    return get_valid_actions(board, board.current_color);
}

} // namespace intsect
