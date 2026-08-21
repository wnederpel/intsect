#include "intsect/game_string.hpp"
#include "intsect/move_generation.hpp"

#include <algorithm>
#include <doctest/doctest.h>

using namespace intsect;

namespace {

void place(Board& board, int loc, Color c, Bug b, uint8_t n = 0) {
    const uint8_t tile = tile_from_info(c, b, n);
    CHECK(board.do_action(Action::make_placement(loc, tile)));
}

void put(Board& board, int loc, Color c, Bug b, uint8_t n = 0, uint8_t extra_height = 0) {
    const uint8_t tile = static_cast<uint8_t>(tile_from_info(c, b, n) + extra_height);
    board.set_tile_on_board(loc, tile);
}

bool contains_action(const std::vector<Action>& actions, const Action& needle) {
    return std::find(actions.begin(), actions.end(), needle) != actions.end();
}

bool is_legal_move(const Board& board, const std::string& move_string) {
    const std::optional<Action> parsed = action_from_move_string(board, move_string);
    if (!parsed.has_value())
        return false;
    return contains_action(get_valid_actions(const_cast<Board&>(board)), *parsed);
}

Action parse_action_or_fail(const Board& board, const std::string& move_string) {
    const std::optional<Action> parsed = action_from_move_string(board, move_string);
    REQUIRE(parsed.has_value());
    return *parsed;
}

void play_legal_move(Board& board, const std::string& move_string) {
    const Action action = parse_action_or_fail(board, move_string);
    const std::vector<Action> actions = get_valid_actions(board);
    REQUIRE(contains_action(actions, action));
    CHECK(board.do_action(action));
}

} // namespace

TEST_CASE("Move generation first placement ordering matches Julia") {
    Board board{Variant::MLP};

    const std::vector<Action> actions = get_valid_actions(board);
    REQUIRE(actions.size() == 7);

    const std::array<Bug, 7> expected_bugs = {Bug::ANT,     Bug::GRASSHOPPER, Bug::BEETLE,
                                              Bug::SPIDER,  Bug::LADYBUG,     Bug::PILLBUG,
                                              Bug::MOSQUITO};

    for (size_t i = 0; i < actions.size(); ++i) {
        CHECK(actions[i].kind == ActionKind::Placement);
        CHECK(actions[i].to == MID);
        CHECK(get_tile_color(actions[i].tile) == Color::White);
        CHECK(get_tile_bug(actions[i].tile) == expected_bugs[i]);
    }
}

TEST_CASE("Move generation first move cannot place queen") {
    Board board{Variant::MLP};

    const std::vector<Action> actions = get_valid_actions(board);
    REQUIRE(actions.size() == 7);

    const uint8_t wq = tile_from_info(Color::White, Bug::QUEEN, 0);
    for (const Action& action : actions) {
        CHECK(action.kind == ActionKind::Placement);
        CHECK(action.to == MID);
        CHECK(action.tile != wq);
    }
}

TEST_CASE("Move generation second placement ordering matches Julia") {
    Board board{Variant::MLP};
    place(board, MID, Color::White, Bug::SPIDER, 0);

    const std::vector<Action> actions = get_valid_actions(board);
    REQUIRE(actions.size() == 42);

    const std::array<int, 6> neighs = all_neighs(MID);
    const std::array<Bug, 7> expected_bugs = {Bug::ANT,     Bug::GRASSHOPPER, Bug::BEETLE,
                                              Bug::SPIDER,  Bug::LADYBUG,     Bug::PILLBUG,
                                              Bug::MOSQUITO};

    for (size_t dir_i = 0; dir_i < 6; ++dir_i) {
        for (size_t tile_i = 0; tile_i < 7; ++tile_i) {
            const size_t idx = dir_i * 7 + tile_i;
            CHECK(actions[idx].kind == ActionKind::Placement);
            CHECK(actions[idx].to == neighs[dir_i]);
            CHECK(get_tile_color(actions[idx].tile) == Color::Black);
            CHECK(get_tile_bug(actions[idx].tile) == expected_bugs[tile_i]);
        }
    }
}

TEST_CASE("Move generation queen placement rule on turn 4") {
    Board board{Variant::MLP};

    place(board, MID, Color::White, Bug::LADYBUG, 0);
    place(board, apply_direction(MID, Direction::E), Color::Black, Bug::LADYBUG, 0);
    place(board, apply_direction(MID, Direction::W), Color::White, Bug::PILLBUG, 0);
    place(board, apply_direction(MID, Direction::SE), Color::Black, Bug::PILLBUG, 0);
    place(board, apply_direction(MID, Direction::NW), Color::White, Bug::ANT, 0);
    place(board, apply_direction(MID, Direction::NE), Color::Black, Bug::ANT, 0);

    REQUIRE(board.current_color == Color::White);
    REQUIRE(board.turn == 4);

    const uint8_t wq = tile_from_info(Color::White, Bug::QUEEN, 0);
    const std::vector<Action> actions = get_valid_actions(board);
    REQUIRE(!actions.empty());

    for (const Action& action : actions) {
        CHECK(action.kind == ActionKind::Placement);
        CHECK(action.tile == wq);
    }
}

TEST_CASE("Move generation black queen placement rule on turn 4") {
    Board board{Variant::MLP};

    place(board, MID, Color::White, Bug::LADYBUG, 0);
    place(board, apply_direction(MID, Direction::E), Color::Black, Bug::LADYBUG, 0);
    place(board, apply_direction(MID, Direction::W), Color::White, Bug::PILLBUG, 0);
    place(board, apply_direction(MID, Direction::SE), Color::Black, Bug::PILLBUG, 0);
    place(board, apply_direction(MID, Direction::NW), Color::White, Bug::ANT, 0);
    place(board, apply_direction(MID, Direction::NE), Color::Black, Bug::ANT, 0);
    place(board, apply_direction(MID, Direction::SW), Color::White, Bug::QUEEN, 0);

    REQUIRE(board.current_color == Color::Black);
    REQUIRE(board.turn == 4);

    const uint8_t bq = tile_from_info(Color::Black, Bug::QUEEN, 0);
    const std::vector<Action> actions = get_valid_actions(board);
    REQUIRE(!actions.empty());

    for (const Action& action : actions) {
        CHECK(action.kind == ActionKind::Placement);
        CHECK(action.tile == bq);
    }
}

TEST_CASE("Move generation second player's first move cannot place queen") {
    Board board{Variant::MLP};
    place(board, MID, Color::White, Bug::QUEEN, 0);

    const std::vector<Action> actions = get_valid_actions(board);
    REQUIRE(actions.size() == 42);

    const uint8_t bq = tile_from_info(Color::Black, Bug::QUEEN, 0);
    for (const Action& action : actions) {
        CHECK(action.kind == ActionKind::Placement);
        CHECK(action.tile != bq);
    }
}

TEST_CASE("Move generation ant cache stores and reuses reachable set") {
    Board board{Variant::MLP};

    const int bQ_loc = MID - 1;
    const int wG1_loc = apply_direction(bQ_loc, Direction::SW);
    const int wB1_loc = apply_direction(wG1_loc, Direction::SE);
    const int bA1_loc = apply_direction(wB1_loc, Direction::SE);
    const int bB1_loc = apply_direction(bA1_loc, Direction::NE);
    const int wQ_loc = apply_direction(bB1_loc, Direction::NE);

    place(board, wQ_loc, Color::White, Bug::QUEEN, 0);
    place(board, bQ_loc, Color::Black, Bug::QUEEN, 0);
    place(board, wG1_loc, Color::White, Bug::GRASSHOPPER, 0);
    place(board, bB1_loc, Color::Black, Bug::BEETLE, 0);
    place(board, wB1_loc, Color::White, Bug::BEETLE, 0);
    place(board, bA1_loc, Color::Black, Bug::ANT, 0);

    HexSet move_to_set_1{};
    ant_moves(board, bA1_loc, move_to_set_1);
    REQUIRE(move_to_set_1.count() > 10);

    const uint64_t key = board.location_hash ^ detail::location_hash_value(bA1_loc);
    const MoveStoreEntry& entry = board.move_store[static_cast<size_t>(key & MOVE_STORE_MASK)];
    CHECK(entry.location_hash == key);
    CHECK(entry.ant_reachable_hs.get(bA1_loc));

    HexSet move_to_set_2{};
    ant_moves(board, bA1_loc, move_to_set_2);
    CHECK(move_to_set_2 == move_to_set_1);
}

TEST_CASE("Move generation beetle movement on top of hive") {
    Board board{Variant::MLP};

    const int bQ_loc = MID - 1;
    const int wQ_loc = apply_direction(bQ_loc, Direction::E);
    const int wB1_loc = apply_direction(bQ_loc, Direction::NE);
    const int bB1_loc = apply_direction(bQ_loc, Direction::SE);

    put(board, bQ_loc, Color::Black, Bug::QUEEN);
    put(board, wQ_loc, Color::White, Bug::QUEEN);
    put(board, wB1_loc, Color::White, Bug::BEETLE, 0, 1);
    put(board, bB1_loc, Color::Black, Bug::BEETLE, 0, 1);

    HexSet moves{};
    const uint8_t height = get_tile_height(board.get_tile_on_board(bB1_loc));
    beetle_moves(board, bB1_loc, height, moves);

    CHECK(moves.get(apply_direction(bB1_loc, Direction::NE)));
    CHECK(moves.get(apply_direction(bB1_loc, Direction::NW)));
    CHECK(moves.get(apply_direction(bB1_loc, Direction::E)));
    CHECK(moves.get(apply_direction(bB1_loc, Direction::SE)));
    CHECK(moves.get(apply_direction(bB1_loc, Direction::W)));
    CHECK(moves.get(apply_direction(bB1_loc, Direction::SW)));
    CHECK(moves.count() == 6);
}

TEST_CASE("Move generation sliding between stacked pieces") {
    Board board{Variant::MLP};

    const int wB1_loc = MID - 1;
    const int bB1_loc = apply_direction(wB1_loc, Direction::E);
    const int wB2_loc = apply_direction(wB1_loc, Direction::NE);
    const int bB2_loc = apply_direction(wB1_loc, Direction::SE);

    put(board, wB1_loc, Color::White, Bug::BEETLE);
    put(board, bB1_loc, Color::Black, Bug::BEETLE);
    put(board, wB2_loc, Color::White, Bug::BEETLE, 1, 1);
    put(board, bB2_loc, Color::Black, Bug::BEETLE, 1, 1);

    HexSet moves{};
    beetle_moves(board, wB1_loc, get_tile_height(board.get_tile_on_board(wB1_loc)), moves);

    CHECK(moves.get(apply_direction(wB1_loc, Direction::NE)));
    CHECK(moves.get(apply_direction(wB1_loc, Direction::SE)));
    CHECK(moves.get(apply_direction(wB1_loc, Direction::NW)));
    CHECK(moves.get(apply_direction(wB1_loc, Direction::SW)));
    CHECK(moves.count() == 4);
}

TEST_CASE("Move generation mosquito cannot move when only touching mosquito") {
    Board board{Variant::MLP};

    const int wQ_loc = MID - 1;
    const int bQ_loc = apply_direction(wQ_loc, Direction::E);
    const int wM_loc = apply_direction(bQ_loc, Direction::E);
    const int bM_loc = apply_direction(wM_loc, Direction::E);

    put(board, wQ_loc, Color::White, Bug::QUEEN);
    put(board, bQ_loc, Color::Black, Bug::QUEEN);
    put(board, wM_loc, Color::White, Bug::MOSQUITO);
    put(board, bM_loc, Color::Black, Bug::MOSQUITO);

    HexSet pinned{};
    HexSet moves{};
    mosquito_moves(board, bM_loc, get_tile_height(board.get_tile_on_board(bM_loc)), pinned, moves);
    CHECK(moves.count() == 0);
}

TEST_CASE("Move generation spider cannot move to itself") {
    Board board{Variant::MLP};

    const int wQ_loc = MID - 2;
    const int bQ_loc = apply_direction(wQ_loc, Direction::E);
    const int wL_loc = apply_direction(bQ_loc, Direction::E);
    const int wA1_loc = apply_direction(wL_loc, Direction::NE);
    const int wA2_loc = apply_direction(wA1_loc, Direction::NW);
    const int wA3_loc = apply_direction(wA2_loc, Direction::NW);
    const int bA1_loc = apply_direction(wA3_loc, Direction::W);
    const int bA2_loc = apply_direction(bA1_loc, Direction::SW);
    const int bA3_loc = apply_direction(bA2_loc, Direction::SW);
    const int wS1_loc = apply_direction(bA3_loc, Direction::E);

    put(board, wQ_loc, Color::White, Bug::QUEEN);
    put(board, bQ_loc, Color::Black, Bug::QUEEN);
    put(board, wL_loc, Color::White, Bug::LADYBUG);
    put(board, wA1_loc, Color::White, Bug::ANT, 0);
    put(board, wA2_loc, Color::White, Bug::ANT, 1);
    put(board, wA3_loc, Color::White, Bug::ANT, 2);
    put(board, bA1_loc, Color::Black, Bug::ANT, 0);
    put(board, bA2_loc, Color::Black, Bug::ANT, 1);
    put(board, bA3_loc, Color::Black, Bug::ANT, 2);
    put(board, wS1_loc, Color::White, Bug::SPIDER);

    HexSet spider_moves{};
    spider_moves(board, wS1_loc, spider_moves);
    CHECK(spider_moves.count() == 0);

    HexSet ant_like_moves{};
    ant_moves(board, wS1_loc, ant_like_moves);
    CHECK(ant_like_moves.count() == 2);
}

TEST_CASE("Move generation pillbug special moves can fill elbows") {
    Board board{Variant::MLP};

    const int wP_loc = MID - 1;
    const int wQ_loc = apply_direction(wP_loc, Direction::SW);
    const int wM_loc = apply_direction(wQ_loc, Direction::NW);

    const int bP_loc = apply_direction(wP_loc, Direction::E);
    const int bQ_loc = apply_direction(bP_loc, Direction::NE);
    const int bM_loc = apply_direction(bQ_loc, Direction::NE);

    put(board, wP_loc, Color::White, Bug::PILLBUG);
    put(board, wQ_loc, Color::White, Bug::QUEEN);
    put(board, wM_loc, Color::White, Bug::MOSQUITO);
    put(board, bP_loc, Color::Black, Bug::PILLBUG);
    put(board, bQ_loc, Color::Black, Bug::QUEEN);
    put(board, bM_loc, Color::Black, Bug::MOSQUITO);

    HexSet ispinned{};
    ispinned.set(wP_loc);
    ispinned.set(bQ_loc);
    ispinned.set(bP_loc);

    HexSet move_to_locs{};
    HexSet throw_from_locs{};
    pillbug_moves_normal(board, wP_loc, ispinned, move_to_locs);
    pillbug_moves_throw(board, wP_loc, ispinned, throw_from_locs, move_to_locs);

    CHECK(move_to_locs.get(apply_direction(wP_loc, Direction::SE)));
    CHECK(move_to_locs.get(apply_direction(wP_loc, Direction::NE)));
    CHECK(move_to_locs.get(apply_direction(wP_loc, Direction::NW)));
    CHECK(throw_from_locs.get(wQ_loc));
    CHECK(throw_from_locs.get(wM_loc));
    CHECK(move_to_locs.count() == 3);
}

TEST_CASE("Move generation pillbug cannot special move through beetle gate") {
    Board board{Variant::MLP};

    const int wP_loc = MID - 1;
    const int wB1_loc = apply_direction(wP_loc, Direction::SW);
    const int wM_loc = apply_direction(wB1_loc, Direction::NW);

    const int bB1_loc = apply_direction(wP_loc, Direction::E);
    const int bQ_loc = apply_direction(bB1_loc, Direction::NE);
    const int bM_loc = apply_direction(bQ_loc, Direction::NE);

    put(board, wP_loc, Color::White, Bug::PILLBUG);
    put(board, wB1_loc, Color::White, Bug::BEETLE, 0, 1);
    put(board, wM_loc, Color::White, Bug::MOSQUITO);
    put(board, bB1_loc, Color::Black, Bug::BEETLE, 0, 1);
    put(board, bQ_loc, Color::Black, Bug::QUEEN);
    put(board, bM_loc, Color::Black, Bug::MOSQUITO);

    HexSet ispinned{};
    ispinned.set(wP_loc);
    ispinned.set(bQ_loc);
    ispinned.set(bB1_loc);

    HexSet move_to_locs{};
    HexSet throw_from_locs{};
    pillbug_moves_normal(board, wP_loc, ispinned, move_to_locs);
    pillbug_moves_throw(board, wP_loc, ispinned, throw_from_locs, move_to_locs);

    CHECK(move_to_locs.get(apply_direction(wP_loc, Direction::NE)));
    CHECK(move_to_locs.get(apply_direction(wP_loc, Direction::NW)));
    CHECK(throw_from_locs.get(wM_loc));
}

TEST_CASE("Move generation pillbug cannot throw the tile that just moved") {
    Board board{Variant::MLP};

    const int bQ_loc = MID - 1;
    const int wP_loc = apply_direction(bQ_loc, Direction::SE);
    const int wQ_loc = apply_direction(wP_loc, Direction::SE);
    const int bA1_loc = apply_direction(wQ_loc, Direction::NE);
    const int wS1_loc = apply_direction(bA1_loc, Direction::NW);
    const int bB1_loc = apply_direction(wS1_loc, Direction::NE);

    put(board, bQ_loc, Color::Black, Bug::QUEEN);
    put(board, wQ_loc, Color::White, Bug::QUEEN);
    put(board, wP_loc, Color::White, Bug::PILLBUG);
    put(board, bA1_loc, Color::Black, Bug::ANT, 0);
    put(board, wS1_loc, Color::White, Bug::SPIDER, 0);
    put(board, bB1_loc, Color::Black, Bug::BEETLE, 0);

    board.just_moved_loc = bA1_loc;

    HexSet ispinned{};
    ispinned.set(wS1_loc);

    HexSet from_locs{};
    HexSet to_locs{};
    pillbug_moves_throw(board, wP_loc, ispinned, from_locs, to_locs);

    CHECK(!from_locs.get(bA1_loc));
    CHECK(from_locs.get(bQ_loc));
    CHECK(from_locs.get(wQ_loc));
}

TEST_CASE("Move generation pillbug cannot throw stacked pieces") {
    Board board{Variant::MLP};

    const int wP_loc = MID;
    const int wB1_loc = apply_direction(wP_loc, Direction::E);
    const int bQ_loc = apply_direction(wP_loc, Direction::W);

    put(board, wP_loc, Color::White, Bug::PILLBUG);
    put(board, bQ_loc, Color::Black, Bug::QUEEN);
    put(board, wB1_loc, Color::White, Bug::BEETLE, 0, 1);

    HexSet ispinned{};
    HexSet from_locs{};
    HexSet to_locs{};
    pillbug_moves_throw(board, wP_loc, ispinned, from_locs, to_locs);

    CHECK(!from_locs.get(wB1_loc));
}

TEST_CASE("Move generation no movement actions before queen is placed") {
    Board board{Variant::MLP};

    play_legal_move(board, "wL");
    play_legal_move(board, "bL wL-");

    const std::vector<Action> actions = get_valid_actions(board);
    REQUIRE(!actions.empty());
    for (const Action& action : actions)
        CHECK(action.kind == ActionKind::Placement);
}

TEST_CASE("Move generation ant movement wraps around board edges") {
    Board board{Variant::MLP};

    const int bQ_loc = 1;
    const int wG1_loc = apply_direction(bQ_loc, Direction::SW);
    const int wB1_loc = apply_direction(wG1_loc, Direction::SE);
    const int bA1_loc = apply_direction(wB1_loc, Direction::SE);
    const int bB1_loc = apply_direction(bA1_loc, Direction::NE);
    const int wQ_loc = apply_direction(bB1_loc, Direction::NE);

    put(board, bQ_loc, Color::Black, Bug::QUEEN);
    put(board, wQ_loc, Color::White, Bug::QUEEN);
    put(board, wG1_loc, Color::White, Bug::GRASSHOPPER);
    put(board, wB1_loc, Color::White, Bug::BEETLE);
    put(board, bB1_loc, Color::Black, Bug::BEETLE);
    put(board, bA1_loc, Color::Black, Bug::ANT);

    HexSet moves{};
    ant_moves(board, bA1_loc, moves);

    CHECK(moves.get(apply_direction(bB1_loc, Direction::E)));
    CHECK(moves.get(apply_direction(bB1_loc, Direction::SE)));
    CHECK(moves.get(apply_direction(wQ_loc, Direction::E)));
    CHECK(moves.get(apply_direction(wQ_loc, Direction::NE)));
    CHECK(moves.get(apply_direction(wQ_loc, Direction::NW)));
    CHECK(moves.get(apply_direction(bQ_loc, Direction::NE)));
    CHECK(moves.get(apply_direction(bQ_loc, Direction::NW)));
    CHECK(moves.get(apply_direction(bQ_loc, Direction::W)));
    CHECK(moves.get(apply_direction(wG1_loc, Direction::W)));
    CHECK(moves.get(apply_direction(wB1_loc, Direction::W)));
    CHECK(moves.get(apply_direction(bA1_loc, Direction::W)));
    CHECK(moves.count() == 11);
}

TEST_CASE("Move generation sometimes only pass is legal") {
    Board board{Variant::MLP};

    CHECK(
        board.do_action(Action::make_placement(136, tile_from_info(Color::White, Bug::BEETLE, 0))));
    CHECK(
        board.do_action(Action::make_placement(120, tile_from_info(Color::Black, Bug::BEETLE, 0))));
    CHECK(
        board.do_action(Action::make_placement(135, tile_from_info(Color::White, Bug::QUEEN, 0))));
    CHECK(
        board.do_action(Action::make_placement(103, tile_from_info(Color::Black, Bug::QUEEN, 0))));
    CHECK(board.do_action(Action::make_move(135, 119)));
    CHECK(board.do_action(Action::make_climb(120, 103)));
    CHECK(board.do_action(Action::make_climb(136, 119)));
    CHECK(board.do_action(Action::make_climb(103, 119)));

    const std::vector<Action> actions = get_valid_actions(board);
    REQUIRE(actions.size() == 1);
    CHECK(actions.front() == Action::make_pass());
}

TEST_CASE("Move generation mosquito on top only has beetle-like moves") {
    Board board{Variant::MLP};

    const std::vector<std::string> seq = {"wB1",     "bA1 wB1-", "wQ /wB1", "bQ bA1\\",
                                          "wM \\wQ", "bL bA1/",  "wM wB1",  "bL -wM"};

    for (const std::string& move : seq)
        play_legal_move(board, move);

    const std::vector<Action> actions = get_valid_actions(board);

    CHECK(!contains_action(actions, parse_action_or_fail(board, "wM bA1-")));
    CHECK(!contains_action(actions, parse_action_or_fail(board, "wM bQ")));

    const int wM_loc = board.get_loc(tile_from_info(Color::White, Bug::MOSQUITO, 0));
    const std::array<int, 6> neighs = all_neighs(wM_loc);

    int mosquito_action_count = 0;
    for (const Action& action : actions) {
        if (action.from != wM_loc)
            continue;
        ++mosquito_action_count;
        const bool is_move = action.kind == ActionKind::Move;
        const bool is_climb = action.kind == ActionKind::Climb;
        if (!is_move)
            CHECK(is_climb);
        CHECK(std::find(neighs.begin(), neighs.end(), action.to) != neighs.end());
    }
    CHECK(mosquito_action_count == 6);
}

TEST_CASE("Move generation handles repeated piece climbs for beetles") {
    Board board{Variant::MLP};

    const std::vector<std::string> seq = {"wB1",       "bB1 wB1-", "wQ -wB1", "bQ bB1-",
                                          "wB2 \\wB1", "bB2 bB1/", "wB2 wB1", "bB2 bB1"};

    for (const std::string& move : seq)
        play_legal_move(board, move);

    CHECK(true);
}

TEST_CASE("Move generation handles repeated piece climbs for mosquitoes") {
    Board board{Variant::MLP};

    const std::vector<std::string> seq = {"wB1",      "bB1 wB1-", "wQ -wB1", "bQ bB1-",
                                          "wM \\wB1", "bM bB1/",  "wM wB1",  "bM bB1"};

    for (const std::string& move : seq)
        play_legal_move(board, move);

    CHECK(true);
}

TEST_CASE("Move generation valid-actions: only the first bug of a kind can be placed") {
    Board board{Variant::MLP};
    play_legal_move(board, "wS1");
    const std::vector<Action> actions = get_valid_actions(board);
    CHECK(actions.size() == 42);
    CHECK(std::count_if(actions.begin(), actions.end(),
                        [](const Action& a) { return a.kind == ActionKind::Placement; }) == 42);
}

TEST_CASE(
    "Move generation valid-actions: the tile moved by the pillbug cannot be moved the next turn") {
    Board board{Variant::MLP};

    const int bQ_loc = MID - 1;
    const int wG1_loc = apply_direction(bQ_loc, Direction::SW);
    const int wB1_loc = apply_direction(wG1_loc, Direction::SE);
    const int bA1_loc = apply_direction(wB1_loc, Direction::SE);
    const int bB1_loc = apply_direction(bA1_loc, Direction::NE);
    const int wQ_loc = apply_direction(bB1_loc, Direction::NE);

    put(board, bQ_loc, Color::Black, Bug::QUEEN);
    put(board, wQ_loc, Color::White, Bug::QUEEN);
    put(board, wG1_loc, Color::White, Bug::GRASSHOPPER);
    put(board, wB1_loc, Color::White, Bug::BEETLE);
    put(board, bB1_loc, Color::Black, Bug::BEETLE);
    put(board, bA1_loc, Color::Black, Bug::ANT);

    board.just_moved_loc = bA1_loc;
    const std::vector<Action> actions = get_valid_actions(board);
    const size_t moved_piece_moves =
        static_cast<size_t>(std::count_if(actions.begin(), actions.end(), [&](const Action& a) {
            return (a.kind == ActionKind::Move || a.kind == ActionKind::Climb) && a.from == bA1_loc;
        }));
    CHECK(moved_piece_moves == 0);
}

TEST_CASE("Move generation ladybug: moving through gates on top of hive") {
    Board board{Variant::MLP};
    const std::vector<std::string> seq = {"wG1",      "bG1 wG1-", "wB1 /wG1", "bB1 bG1-",
                                          "wQ \\wB1", "bQ \\bB1", "wL \\wG1", "bB1 bQ",
                                          "wB1 wG1",  "bM bB1\\", "wL \\bG1", "bM bG1\\"};

    for (const std::string& move : seq)
        play_legal_move(board, move);

    CHECK(!is_legal_move(board, "wL bM-"));
    CHECK(!is_legal_move(board, "wL bM\\"));
    CHECK(!is_legal_move(board, "wL /bM"));
    CHECK(!is_legal_move(board, "wL bB1/"));
    CHECK(!is_legal_move(board, "wL bB1-"));
    CHECK(!is_legal_move(board, "wL \\bB1"));
    CHECK(!is_legal_move(board, "wL -bB1"));
    CHECK(is_legal_move(board, "wL bM/"));
    CHECK(is_legal_move(board, "wL -wQ"));
    CHECK(is_legal_move(board, "wL wQ/"));
    CHECK(is_legal_move(board, "wL wQ\\"));
    CHECK(is_legal_move(board, "wL wB1\\"));
    CHECK(is_legal_move(board, "wL bG1-"));
}

TEST_CASE("Move generation pillbug: cannot move piece if it splits hive") {
    Board board{Variant::MLP};
    const std::vector<std::string> seq = {"wP",      "bP wP-",  "wQ \\wP", "bQ bP\\",
                                          "wB1 /wP", "bB1 bQ-", "wB1 wP",  "bM bB1-",
                                          "wQ \\bP", "bS1 bM-", "wM wQ/"};

    for (const std::string& move : seq)
        play_legal_move(board, move);

    CHECK(!is_legal_move(board, "wQ bP/"));
}

TEST_CASE("Move generation pillbug: too-narrow gap restrictions") {
    Board board{Variant::MLP};
    const std::vector<std::string> seq = {"wP",     "bP wP-",   "wQ /wP",  "bQ bP\\", "wM \\wP",
                                          "bM /bQ", "wB1 \\wQ", "bB1 bQ/", "wM \\bP", "bM wP\\",
                                          "wB1 wP", "bB1 bP",   "wB1 wM",  "bB1 bM"};

    for (const std::string& move : seq)
        play_legal_move(board, move);

    CHECK(is_legal_move(board, "wQ \\wP"));
    CHECK(!is_legal_move(board, "bP -wP"));
    CHECK(!is_legal_move(board, "bP \\wP"));
}

TEST_CASE("Move generation pillbug: moved piece cannot move use or be moved next turn") {
    Board board{Variant::MLP};
    const std::vector<std::string> seq = {"wP",     "bP wP-",   "wQ /wP",  "bQ bP\\", "wM \\wP",
                                          "bM /bQ", "wB1 \\wQ", "bB1 bQ/", "wM \\bP", "bM wP\\"};

    for (const std::string& move : seq)
        play_legal_move(board, move);

    CHECK(is_legal_move(board, "bP -wM"));
    play_legal_move(board, "bP -wM");

    CHECK(!is_legal_move(board, "bP \\wB"));
    CHECK(!is_legal_move(board, "wB -wP"));
}

TEST_CASE("Move generation pillbug: pillbug used ability can be moved by opposing pillbug") {
    Board board{Variant::MLP};
    const std::vector<std::string> seq = {"wP",      "bP wP-",  "wQ /wP",
                                          "bQ bP\\", "wQ wP\\", "bQ \\bP"};

    for (const std::string& move : seq)
        play_legal_move(board, move);

    CHECK(is_legal_move(board, "bP -wP"));
}

TEST_CASE("Move generation pillbug: mosquito mimics pillbug even if hive-pinned") {
    Board board{Variant::MLP};
    const std::vector<std::string> seq = {"wP",     "bP wP-",   "wQ /wP",   "bQ bP\\", "wM \\wP",
                                          "bM bP/", "wA1 \\wQ", "bA1 bM\\", "wA1 -wQ", "bM wM/"};

    for (const std::string& move : seq)
        play_legal_move(board, move);

    play_legal_move(board, "wA1 /wQ");
    play_legal_move(board, "bA1 bQ-");

    CHECK(is_legal_move(board, "bM -wM"));
    CHECK(is_legal_move(board, "bM wM-"));
    CHECK(is_legal_move(board, "bM \\wM"));
    CHECK(is_legal_move(board, "bM /wM"));
}

TEST_CASE(
    "Move generation pillbug: mosquito mimics pillbug when rendered immobile by opposing pillbug") {
    Board board{Variant::MLP};
    const std::vector<std::string> seq = {"wP",       "bB1 wP-", "wQ /wP",   "bP bB1\\",
                                          "wL /wQ",   "bQ bP/",  "wG1 /wL",  "bP wP\\",
                                          "wG2 /wG1", "bM \\bQ", "bP \\bB1", "bB1 bQ/"};

    for (const std::string& move : seq)
        play_legal_move(board, move);

    const std::vector<Action> actions = get_valid_actions(board);
    CHECK(!actions.empty());
}

TEST_CASE("Move generation pillbug: pillbug under beetle cannot move or use ability") {
    Board board{Variant::MLP};
    const std::vector<std::string> seq = {"wP",      "bP wP-",  "wQ /wP",   "bQ bP\\",
                                          "wM \\wQ", "bB1 bQ/", "wB1 \\wP", "bB1 bP",
                                          "wM wP",   "bQ wM\\", "wB1 \\wQ"};

    for (const std::string& move : seq)
        play_legal_move(board, move);

    CHECK(!is_legal_move(board, "bQ bB1-"));

    play_legal_move(board, "bA1 bB1-");

    CHECK(!is_legal_move(board, "wQ bM/"));
    CHECK(!is_legal_move(board, "bQ bM/"));
}

TEST_CASE("Move generation pillbug: mosquito can throw pillbug") {
    Board board{Variant::MLP};
    const std::vector<std::string> seq = {
        "wM",      "bP \\wM",  "wS1 /wM",   "bB1 bP/", "wB1 -wS1", "bM \\bB1", "wQ wM\\",
        "bQ /bM",  "wG1 /wB1", "bG1 -bM",   "wP -wG1", "bM bB1",   "wQ /bP",   "bG2 bG1/",
        "wP wP\\", "bB2 -bG1", "wB2 wB1\\", "wQ bM\\", "wA1 -wB1", "bA1 bM/",  "bP wM-"};

    for (const std::string& move : seq)
        play_legal_move(board, move);

    CHECK(board.undo());
    play_legal_move(board, "wQ wM\\");
    CHECK(true);
}

TEST_CASE("Move generation special: pillbug extra-down-slide placeholder parity") {
    CHECK(true);
}
