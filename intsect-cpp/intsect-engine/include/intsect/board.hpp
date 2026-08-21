#pragma once

#include "hex_set.hpp"
#include "tile.hpp"
#include "types.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>

namespace intsect {

enum class ActionKind : uint8_t { Move, Climb, Placement, Pass };

struct Action {
    ActionKind kind = ActionKind::Pass;
    int from = INVALID_LOC;
    int to = INVALID_LOC;
    uint8_t tile = 0;

    [[nodiscard]] static Action make_move(int from, int to) noexcept {
        return {ActionKind::Move, from, to, 0};
    }
    [[nodiscard]] static Action make_climb(int from, int to) noexcept {
        return {ActionKind::Climb, from, to, 0};
    }
    [[nodiscard]] static Action make_placement(int to, uint8_t tile) noexcept {
        return {ActionKind::Placement, INVALID_LOC, to, tile};
    }
    [[nodiscard]] static Action make_pass() noexcept {
        return {ActionKind::Pass, INVALID_LOC, INVALID_LOC, 0};
    }

    bool operator==(const Action&) const = default;
};

inline constexpr int HISTORY_BUFFER_SIZE = 600;
inline constexpr int MAX_UNDERWORLD_DEPTH =
    4; // max pieces that can be under a single tile (2B+M+L)
inline constexpr size_t MOVE_STORE_SIZE = 4096;
inline constexpr size_t PINNED_STORE_SIZE = 4096;
inline constexpr size_t MOVE_STORE_MASK = MOVE_STORE_SIZE - 1;
inline constexpr size_t PINNED_STORE_MASK = PINNED_STORE_SIZE - 1;

struct MoveStoreEntry {
    uint64_t location_hash = 0;
    HexSet ant_reachable_hs{};
};

struct PinnedStoreEntry {
    uint64_t location_hash = 0;
    HexSet pinned_pieces_hs{};
};

struct MoveGenWorkspaces {
    HexSet move_to_set{};
    HexSet no_placement_hs{};
    HexSet pillbug_throw_from{};
    HexSet pillbug_throw_to{};
    HexSet mosquito_throw_from{};
    HexSet mosquito_throw_to{};
    HexSet ladybug_visited_step_2{};
    HexSet ispinned_visited{};

    std::array<int, GRID_SIZE> ant_stack{};
    std::array<int, GRID_SIZE> depth_dict{};
    std::array<int, GRID_SIZE> low_dict{};
    std::array<int, GRID_SIZE> parent_dict{};
};

namespace detail {

inline constexpr uint64_t split_mix64(uint64_t x) noexcept {
    x += 0x9e3779b97f4a7c15ull;
    x = (x ^ (x >> 30)) * 0xbf58476d1ce4e5b9ull;
    x = (x ^ (x >> 27)) * 0x94d049bb133111ebull;
    return x ^ (x >> 31);
}

inline constexpr bool variant_has_mosquito(Variant variant) noexcept {
    return (static_cast<uint8_t>(variant) & static_cast<uint8_t>(Variant::M)) != 0u;
}

inline constexpr bool variant_has_ladybug(Variant variant) noexcept {
    return (static_cast<uint8_t>(variant) & static_cast<uint8_t>(Variant::L)) != 0u;
}

inline constexpr bool variant_has_pillbug(Variant variant) noexcept {
    return (static_cast<uint8_t>(variant) & static_cast<uint8_t>(Variant::P)) != 0u;
}

inline constexpr bool is_valid_shifted_tile(uint8_t shifted_tile) noexcept {
    const uint8_t tile = static_cast<uint8_t>(shifted_tile << INDEX_SHIFT);
    const Bug bug = get_tile_bug(tile);
    const uint8_t bug_no = get_tile_bug_num(tile);

    if (bug == Bug::QUEEN || bug == Bug::LADYBUG || bug == Bug::MOSQUITO || bug == Bug::PILLBUG)
        return bug_no == 0u;
    if (bug == Bug::BEETLE || bug == Bug::SPIDER)
        return bug_no <= 1u;
    if (bug == Bug::GRASSHOPPER || bug == Bug::ANT)
        return bug_no <= 2u;
    return false;
}

inline constexpr uint64_t piece_hash_value(uint8_t tile_without_height, int loc,
                                           int underworld_height) noexcept {
    // Julia indexes HASH_VALUES with [tile>>2 + height*36 + loc*36*7].
    const uint8_t shifted_tile = static_cast<uint8_t>(tile_without_height >> INDEX_SHIFT);
    const int safe_height = std::clamp(underworld_height, 0, 6);
    const uint64_t seed = static_cast<uint64_t>(shifted_tile) +
                          static_cast<uint64_t>(safe_height) * 36ull +
                          static_cast<uint64_t>(loc) * 36ull * 7ull;
    return split_mix64(seed);
}

inline constexpr uint64_t location_hash_value(int loc) noexcept {
    return split_mix64(0x123456789abcdef0ull ^ static_cast<uint64_t>(loc));
}

inline constexpr uint64_t color_hash_value() noexcept {
    return split_mix64(0xf00dcafe12345678ull);
}

inline constexpr uint64_t just_moved_hash_value(int loc) noexcept {
    return split_mix64(0x55aa00ff11223344ull ^ static_cast<uint64_t>(loc));
}

} // namespace detail

struct Board {
    std::array<uint8_t, GRID_SIZE> tiles{};
    std::array<int, 36> tile_locs{};

    int just_moved_loc = INVALID_LOC;
    Color current_color = Color::White;
    std::array<bool, 2> queen_placed{false, false};
    uint16_t ply = 1;
    int turn = 1;
    bool gameover = false;
    Color victor = Color::None;

    std::array<Action, HISTORY_BUFFER_SIZE> history{};
    std::array<uint64_t, HISTORY_BUFFER_SIZE> hash_history{};

    std::array<std::array<uint8_t, MAX_UNDERWORLD_DEPTH>, GRID_SIZE> underworld{};
    std::array<uint8_t, GRID_SIZE> underworld_sizes{};

    std::array<std::array<uint8_t, BUGS_IN_PLAY>, 2> placeable_tiles{};

    HexSet ispinned{};
    std::array<HexSet, 2> pieces{};

    int queen_pos_white = NOT_PLACED;
    int queen_pos_black = NOT_PLACED;

    uint64_t hash = 0;
    uint64_t location_hash = 0;

    std::array<MoveStoreEntry, MOVE_STORE_SIZE> move_store{};
    std::array<PinnedStoreEntry, PINNED_STORE_SIZE> pinned_store{};
    MoveGenWorkspaces workspaces{};

    Variant variant = Variant::MLP;

    Board() {
        tiles.fill(EMPTY_TILE);
        tile_locs.fill(NOT_PLACED);
        for (int i = 0; i < 36; ++i) {
            if (!detail::is_valid_shifted_tile(static_cast<uint8_t>(i)))
                tile_locs[static_cast<size_t>(i)] = INVALID_LOC;
        }

        placeable_tiles[0] = {tile_from_info(Color::White, Bug::ANT, 0),
                              tile_from_info(Color::White, Bug::GRASSHOPPER, 0),
                              tile_from_info(Color::White, Bug::BEETLE, 0),
                              tile_from_info(Color::White, Bug::SPIDER, 0),
                              tile_from_info(Color::White, Bug::QUEEN, 0),
                              tile_from_info(Color::White, Bug::LADYBUG, 0),
                              tile_from_info(Color::White, Bug::PILLBUG, 0),
                              tile_from_info(Color::White, Bug::MOSQUITO, 0)};
        placeable_tiles[1] = {tile_from_info(Color::Black, Bug::ANT, 0),
                              tile_from_info(Color::Black, Bug::GRASSHOPPER, 0),
                              tile_from_info(Color::Black, Bug::BEETLE, 0),
                              tile_from_info(Color::Black, Bug::SPIDER, 0),
                              tile_from_info(Color::Black, Bug::QUEEN, 0),
                              tile_from_info(Color::Black, Bug::LADYBUG, 0),
                              tile_from_info(Color::Black, Bug::PILLBUG, 0),
                              tile_from_info(Color::Black, Bug::MOSQUITO, 0)};

        apply_variant_filter();
        recompute_piece_sets();
        recompute_hashes();
    }

    explicit Board(Variant v) : Board() {
        variant = v;
        apply_variant_filter();
    }

    [[nodiscard]] uint8_t get_tile_on_board(int loc) const noexcept {
        return tiles[static_cast<size_t>(loc)];
    }

    void set_tile_on_board(int loc, uint8_t tile) noexcept {
        tiles[static_cast<size_t>(loc)] = tile;
    }

    [[nodiscard]] int get_loc(uint8_t tile_without_height) const noexcept {
        return tile_locs[static_cast<size_t>(tile_without_height >> INDEX_SHIFT)];
    }

    void set_loc(uint8_t tile_without_height, int loc) noexcept {
        tile_locs[static_cast<size_t>(tile_without_height >> INDEX_SHIFT)] = loc;
    }

    [[nodiscard]] uint64_t full_hash() const noexcept {
        uint64_t out = hash;
        if (current_color == Color::Black)
            out ^= detail::color_hash_value();

        if (just_moved_loc >= 0) {
            const uint8_t tile = get_tile_on_board(just_moved_loc);
            if (tile != EMPTY_TILE && get_tile_color(tile) == current_color)
                out ^= detail::just_moved_hash_value(just_moved_loc);
        }
        return out;
    }

    // Excludes variant (config).
    [[nodiscard]] bool equivalent_state(const Board& other) const noexcept {
        return tiles == other.tiles && tile_locs == other.tile_locs &&
               just_moved_loc == other.just_moved_loc && current_color == other.current_color &&
               queen_placed == other.queen_placed && ply == other.ply && turn == other.turn &&
               gameover == other.gameover && victor == other.victor &&
               underworld == other.underworld && underworld_sizes == other.underworld_sizes &&
               placeable_tiles == other.placeable_tiles && ispinned == other.ispinned &&
               pieces == other.pieces && queen_pos_white == other.queen_pos_white &&
               queen_pos_black == other.queen_pos_black && hash == other.hash &&
               location_hash == other.location_hash && history == other.history &&
               hash_history == other.hash_history;
    }

    [[nodiscard]] bool do_action(const Action& action) {
        bool applied = false;
        switch (action.kind) {
        case ActionKind::Placement:
            applied = do_placement(action);
            break;
        case ActionKind::Move:
            applied = do_move(action);
            break;
        case ActionKind::Climb:
            applied = do_climb(action);
            break;
        case ActionKind::Pass:
            applied = do_pass(action);
            break;
        }

        if (!applied)
            return false;

        history[static_cast<size_t>(ply) - 1u] = action;

        post_action_update(action);
        return true;
    }

    [[nodiscard]] bool undo() {
        if (ply == 1)
            return false;

        const Action action = history[static_cast<size_t>(ply) - 2u];
        history[static_cast<size_t>(ply) - 2u] = Action{};
        hash_history[static_cast<size_t>(ply) - 2u] = 0;

        switch (action.kind) {
        case ActionKind::Placement:
            undo_placement(action);
            break;
        case ActionKind::Move:
            undo_move(action);
            break;
        case ActionKind::Climb:
            undo_climb(action);
            break;
        case ActionKind::Pass:
            undo_pass(action);
            break;
        }

        reverse_post_action_general_update();

        just_moved_loc = (ply == 1) ? INVALID_LOC : history[static_cast<size_t>(ply) - 2u].to;

        // check_gameover recomputes queen_pos_white/black from restored tile locations.
        gameover = false;
        victor = Color::None;
        check_gameover(true);

        recompute_piece_sets();
        recompute_hashes();
        return true;
    }

  private:
    [[nodiscard]] static bool is_valid_loc(int loc) noexcept {
        return loc >= 0 && loc < GRID_SIZE;
    }

    [[nodiscard]] static size_t color_index(Color c) noexcept {
        return static_cast<size_t>(static_cast<uint8_t>(c) - 1u);
    }

    void apply_variant_filter() noexcept {
        if (!detail::variant_has_ladybug(variant)) {
            placeable_tiles[0][5] = EMPTY_TILE;
            placeable_tiles[1][5] = EMPTY_TILE;
        }
        if (!detail::variant_has_pillbug(variant)) {
            placeable_tiles[0][6] = EMPTY_TILE;
            placeable_tiles[1][6] = EMPTY_TILE;
        }
        if (!detail::variant_has_mosquito(variant)) {
            placeable_tiles[0][7] = EMPTY_TILE;
            placeable_tiles[1][7] = EMPTY_TILE;
        }
    }

    [[nodiscard]] bool do_placement(const Action& action) {
        if (!is_valid_loc(action.to))
            return false;
        if (get_tile_on_board(action.to) != EMPTY_TILE)
            return false;

        set_tile_on_board(action.to, action.tile);
        set_loc(action.tile, action.to);
        if (get_tile_bug(action.tile) == Bug::QUEEN)
            queen_placed[color_index(current_color)] = true;

        const size_t color_i = color_index(current_color);
        const size_t bug_i =
            static_cast<size_t>(static_cast<uint8_t>(get_tile_bug(action.tile)) - 1u);
        placeable_tiles[color_i][bug_i] = next_bug_num(action.tile);
        return true;
    }

    [[nodiscard]] bool do_move(const Action& action) {
        if (!is_valid_loc(action.from) || !is_valid_loc(action.to))
            return false;

        const uint8_t moving_tile = get_tile_on_board(action.from);
        if (moving_tile == EMPTY_TILE)
            return false;

        set_tile_on_board(action.to, moving_tile);
        set_tile_on_board(action.from, EMPTY_TILE);
        set_loc(moving_tile, action.to);
        return true;
    }

    [[nodiscard]] bool do_climb(const Action& action) {
        if (!is_valid_loc(action.from) || !is_valid_loc(action.to))
            return false;

        const size_t idx_from = static_cast<size_t>(action.from);
        const size_t idx_to = static_cast<size_t>(action.to);
        const uint8_t burrowed_tile = get_tile_on_board(action.to);
        uint8_t moving_tile = get_tile_on_board(action.from);
        if (moving_tile == EMPTY_TILE)
            return false;

        if (burrowed_tile != EMPTY_TILE) {
            underworld[idx_to][underworld_sizes[idx_to]++] = burrowed_tile;
            set_loc(burrowed_tile, UNDERGROUND);
        }

        if (get_tile_height(moving_tile) > 1u) {
            if (underworld_sizes[idx_from] == 0)
                return false;
            const uint8_t released_tile = underworld[idx_from][--underworld_sizes[idx_from]];
            underworld[idx_from][underworld_sizes[idx_from]] = 0;
            set_tile_on_board(action.from, released_tile);
            set_loc(released_tile, action.from);
        } else {
            set_tile_on_board(action.from, EMPTY_TILE);
        }

        const uint8_t old_height = static_cast<uint8_t>(get_tile_height(moving_tile) - 1u);
        const uint8_t new_height =
            static_cast<uint8_t>(std::min<size_t>(underworld_sizes[idx_to], 3u));
        moving_tile = static_cast<uint8_t>(moving_tile + new_height - old_height);

        set_tile_on_board(action.to, moving_tile);
        set_loc(moving_tile, action.to);
        return true;
    }

    [[nodiscard]] bool do_pass(const Action&) noexcept {
        return true;
    }

    void undo_placement(const Action& action) {
        set_tile_on_board(action.to, EMPTY_TILE);
        set_loc(action.tile, NOT_PLACED);

        if (get_tile_bug(action.tile) == Bug::QUEEN) {
            const Color tile_color = get_tile_color(action.tile);
            queen_placed[color_index(tile_color)] = false;
        }

        const size_t color_i = color_index(get_tile_color(action.tile));
        const size_t bug_i =
            static_cast<size_t>(static_cast<uint8_t>(get_tile_bug(action.tile)) - 1u);
        placeable_tiles[color_i][bug_i] = action.tile;
    }

    void undo_move(const Action& action) {
        const uint8_t moving_tile = get_tile_on_board(action.to);
        set_tile_on_board(action.to, EMPTY_TILE);
        set_tile_on_board(action.from, moving_tile);
        set_loc(moving_tile, action.from);
    }

    void undo_climb(const Action& action) {
        const size_t idx_from = static_cast<size_t>(action.from);
        const size_t idx_to = static_cast<size_t>(action.to);
        const uint8_t burrowed_tile = get_tile_on_board(action.from);
        uint8_t moving_tile = get_tile_on_board(action.to);

        if (burrowed_tile != EMPTY_TILE) {
            underworld[idx_from][underworld_sizes[idx_from]++] = burrowed_tile;
            set_loc(burrowed_tile, UNDERGROUND);
        }

        if (get_tile_height(moving_tile) > 1u) {
            if (underworld_sizes[idx_to] > 0) {
                const uint8_t released_tile = underworld[idx_to][--underworld_sizes[idx_to]];
                underworld[idx_to][underworld_sizes[idx_to]] = 0;
                set_tile_on_board(action.to, released_tile);
                set_loc(released_tile, action.to);
            } else {
                set_tile_on_board(action.to, EMPTY_TILE);
            }
        } else {
            set_tile_on_board(action.to, EMPTY_TILE);
        }

        const uint8_t old_height = static_cast<uint8_t>(get_tile_height(moving_tile) - 1u);
        const uint8_t new_height =
            static_cast<uint8_t>(std::min<size_t>(underworld_sizes[idx_from], 3u));
        moving_tile = static_cast<uint8_t>(moving_tile + new_height - old_height);

        set_tile_on_board(action.from, moving_tile);
        set_loc(moving_tile, action.from);
    }

    void undo_pass(const Action&) noexcept {}

    void reverse_post_action_general_update() {
        if (current_color == Color::White) {
            current_color = Color::Black;
            --turn;
        } else {
            current_color = Color::White;
        }
        --ply;
    }

    void post_action_update(const Action& action) {
        just_moved_loc = action.to;

        ++ply;
        if (current_color == Color::White) {
            current_color = Color::Black;
        } else {
            current_color = Color::White;
            ++turn;
        }

        check_gameover(false);
        recompute_piece_sets();
        recompute_hashes();
        hash_history[static_cast<size_t>(ply) - 2u] = full_hash();
        check_draw();
    }

    void check_draw() {
        const uint64_t h = full_hash();
        int count = 0;
        for (int i = static_cast<int>(ply) - 2; i >= 0; i -= 2) {
            if (hash_history[static_cast<size_t>(i)] == h)
                ++count;
        }
        if (count >= 3) {
            gameover = true;
            victor = Color::Draw;
        }
    }

    void check_gameover(bool undoing) {
        const uint8_t white_q = tile_from_info(Color::White, Bug::QUEEN, 0);
        const uint8_t black_q = tile_from_info(Color::Black, Bug::QUEEN, 0);

        int wq_loc = get_loc(white_q);
        int bq_loc = get_loc(black_q);
        if (wq_loc == UNDERGROUND) {
            wq_loc = queen_pos_white;
        } else {
            queen_pos_white = wq_loc;
        }
        if (bq_loc == UNDERGROUND) {
            bq_loc = queen_pos_black;
        } else {
            queen_pos_black = bq_loc;
        }
        if (undoing) {
            // We only need to update the queen locs when undoing, no need to check for gameover
            return;
        }
        if (wq_loc >= 0 && surrounded(wq_loc)) {
            gameover = true;
            victor = Color::Black;
        }
        if (bq_loc >= 0 && surrounded(bq_loc)) {
            gameover = true;
            victor = Color::White;
        }
    }

    [[nodiscard]] bool surrounded(int loc) const {
        constexpr std::array<Direction, 6> dirs = {Direction::E, Direction::SE, Direction::SW,
                                                   Direction::W, Direction::NW, Direction::NE};
        for (Direction d : dirs) {
            const int n = apply_direction(loc, d);
            if (get_tile_on_board(n) == EMPTY_TILE)
                return false;
        }
        return true;
    }

    void recompute_piece_sets() noexcept {
        pieces[0].clear();
        pieces[1].clear();
        for (int loc = 0; loc < GRID_SIZE; ++loc) {
            const uint8_t tile = get_tile_on_board(loc);
            if (tile == EMPTY_TILE)
                continue;
            const Color color = get_tile_color(tile);
            pieces[color_index(color)].set(loc);
        }
    }

    void recompute_hashes() noexcept {
        hash = 0;
        location_hash = 0;
        for (int loc = 0; loc < GRID_SIZE; ++loc) {
            const uint8_t tile = get_tile_on_board(loc);
            if (tile == EMPTY_TILE)
                continue;

            const int under_height = static_cast<int>(underworld_sizes[static_cast<size_t>(loc)]);
            const uint8_t tile_without_height =
                static_cast<uint8_t>(tile & static_cast<uint8_t>(~HEIGHT_MASK));
            hash ^= detail::piece_hash_value(tile_without_height, loc, under_height);
            location_hash ^= detail::location_hash_value(loc);
        }
    }
};

} // namespace intsect