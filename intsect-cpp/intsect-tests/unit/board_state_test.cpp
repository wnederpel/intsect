// Tests for Phase 2 board state and transitions.
#include "intsect/board.hpp"

#include <doctest/doctest.h>
#include <vector>

using namespace intsect;

namespace {

void check_state(const Board& board, uint16_t ply, Color current_color, int turn,
                 bool white_queen_placed, bool black_queen_placed, int just_moved_loc) {
    CHECK(board.ply == ply);
    CHECK(board.current_color == current_color);
    CHECK(board.turn == turn);
    CHECK(board.queen_placed[0] == white_queen_placed);
    CHECK(board.queen_placed[1] == black_queen_placed);
    CHECK(board.just_moved_loc == just_moved_loc);
}

} // namespace

TEST_CASE("Board initialization sets tile locations and sentinels") {
    const Board board{};

    CHECK(board.get_tile_on_board(MID) == EMPTY_TILE);
    CHECK(board.current_color == Color::White);
    CHECK(board.ply == 1);
    CHECK(board.turn == 1);
    CHECK(board.just_moved_loc == INVALID_LOC);

    for (int i = 0; i < 36; ++i) {
        const bool valid = detail::is_valid_shifted_tile(static_cast<uint8_t>(i));
        if (valid) {
            CHECK(board.tile_locs[static_cast<size_t>(i)] == NOT_PLACED);
        } else {
            CHECK(board.tile_locs[static_cast<size_t>(i)] == INVALID_LOC);
        }
    }
}

TEST_CASE("Placement and undo restore exact state") {
    Board board{};
    const Board initial = board;

    const uint8_t wq = tile_from_info(Color::White, Bug::QUEEN, 0);
    const uint8_t bq = tile_from_info(Color::Black, Bug::QUEEN, 0);

    CHECK(board.do_action(Action::make_placement(MID, wq)));
    CHECK(board.do_action(Action::make_placement(MID + 1, bq)));
    CHECK(board.undo());
    CHECK(board.undo());

    CHECK(board.equivalent_state(initial));
}

TEST_CASE("Climb updates underworld and tile locations") {
    Board board{};

    const uint8_t wb1 = tile_from_info(Color::White, Bug::BEETLE, 0);
    const uint8_t ba1 = tile_from_info(Color::Black, Bug::ANT, 0);

    CHECK(board.do_action(Action::make_placement(MID, wb1)));
    CHECK(board.do_action(Action::make_placement(MID + 1, ba1)));

    const Board before_climb = board;
    CHECK(board.do_action(Action::make_climb(MID, MID + 1)));

    CHECK(board.get_tile_on_board(MID) == EMPTY_TILE);
    CHECK(board.get_loc(ba1) == UNDERGROUND);
    CHECK(board.get_loc(wb1) == MID + 1);
    CHECK(board.underworld_sizes[static_cast<size_t>(MID + 1)] == 1);

    CHECK(board.undo());
    CHECK(board.equivalent_state(before_climb));
}

TEST_CASE("Hash and location hash are stable under apply-undo cycles") {
    Board board{};

    const uint8_t wa1 = tile_from_info(Color::White, Bug::ANT, 0);
    const uint8_t ba1 = tile_from_info(Color::Black, Bug::ANT, 0);

    CHECK(board.do_action(Action::make_placement(MID, wa1)));
    CHECK(board.do_action(Action::make_placement(MID + 1, ba1)));

    const uint64_t expected_hash      = board.hash;
    const uint64_t expected_loc_hash  = board.location_hash;
    const uint64_t expected_full_hash = board.full_hash();
    const Board expected_state        = board;

    for (int i = 0; i < 50; ++i) {
        CHECK(board.do_action(Action::make_move(MID, MID + 2)));
        CHECK(board.undo());
    }

    CHECK(board.hash == expected_hash);
    CHECK(board.location_hash == expected_loc_hash);
    CHECK(board.full_hash() == expected_full_hash);
    CHECK(board.equivalent_state(expected_state));
}

TEST_CASE("State progression mirrors Julia board-state expectations") {
    Board board{};

    const int wS1_loc = MID;
    const int bS1_loc = apply_direction(wS1_loc, Direction::E);
    const int wQ_loc  = apply_direction(wS1_loc, Direction::W);
    const int bQ_loc  = apply_direction(bS1_loc, Direction::E);
    const int wA1_loc = apply_direction(wQ_loc, Direction::W);
    const int bA1_loc = apply_direction(bQ_loc, Direction::E);

    const uint8_t wS1 = tile_from_info(Color::White, Bug::SPIDER, 0);
    const uint8_t bS1 = tile_from_info(Color::Black, Bug::SPIDER, 0);
    const uint8_t wQ  = tile_from_info(Color::White, Bug::QUEEN, 0);
    const uint8_t bQ  = tile_from_info(Color::Black, Bug::QUEEN, 0);
    const uint8_t wA1 = tile_from_info(Color::White, Bug::ANT, 0);
    const uint8_t bA1 = tile_from_info(Color::Black, Bug::ANT, 0);

    CHECK(board.do_action(Action::make_placement(wS1_loc, wS1)));
    check_state(board, 2, Color::Black, 1, false, false, wS1_loc);

    CHECK(board.do_action(Action::make_placement(bS1_loc, bS1)));
    check_state(board, 3, Color::White, 2, false, false, bS1_loc);

    CHECK(board.do_action(Action::make_placement(wQ_loc, wQ)));
    check_state(board, 4, Color::Black, 2, true, false, wQ_loc);

    CHECK(board.do_action(Action::make_placement(bQ_loc, bQ)));
    check_state(board, 5, Color::White, 3, true, true, bQ_loc);

    CHECK(board.do_action(Action::make_placement(wA1_loc, wA1)));
    check_state(board, 6, Color::Black, 3, true, true, wA1_loc);

    CHECK(board.do_action(Action::make_placement(bA1_loc, bA1)));
    check_state(board, 7, Color::White, 4, true, true, bA1_loc);

    const int wA1_goal = apply_direction(wS1_loc, Direction::NW);
    CHECK(board.do_action(Action::make_move(wA1_loc, wA1_goal)));
    check_state(board, 8, Color::Black, 4, true, true, wA1_goal);
}

TEST_CASE("Undo restores prior hash and state checkpoints") {
    Board board{};

    const int wS1_loc = MID;
    const int bS1_loc = apply_direction(wS1_loc, Direction::E);
    const int wQ_loc  = apply_direction(wS1_loc, Direction::W);
    const int bQ_loc  = apply_direction(bS1_loc, Direction::E);

    const uint8_t wS1 = tile_from_info(Color::White, Bug::SPIDER, 0);
    const uint8_t bS1 = tile_from_info(Color::Black, Bug::SPIDER, 0);
    const uint8_t wQ  = tile_from_info(Color::White, Bug::QUEEN, 0);
    const uint8_t bQ  = tile_from_info(Color::Black, Bug::QUEEN, 0);

    CHECK(board.do_action(Action::make_placement(wS1_loc, wS1)));
    const uint64_t hash_after_1 = board.hash;

    CHECK(board.do_action(Action::make_placement(bS1_loc, bS1)));
    const uint64_t hash_after_2 = board.hash;

    CHECK(board.do_action(Action::make_placement(wQ_loc, wQ)));
    const uint64_t hash_after_3 = board.hash;

    CHECK(board.do_action(Action::make_placement(bQ_loc, bQ)));

    CHECK(board.undo());
    CHECK(board.hash == hash_after_3);
    check_state(board, 4, Color::Black, 2, true, false, wQ_loc);

    CHECK(board.undo());
    CHECK(board.hash == hash_after_2);
    check_state(board, 3, Color::White, 2, false, false, bS1_loc);

    CHECK(board.undo());
    CHECK(board.hash == hash_after_1);
    check_state(board, 2, Color::Black, 1, false, false, wS1_loc);
}

TEST_CASE("Climb transitions keep piece counts consistent like Julia bitboard tests") {
    Board board{};

    const uint8_t wB1 = tile_from_info(Color::White, Bug::BEETLE, 0);
    const uint8_t bB1 = tile_from_info(Color::Black, Bug::BEETLE, 0);
    const uint8_t wQ  = tile_from_info(Color::White, Bug::QUEEN, 0);
    const uint8_t bQ  = tile_from_info(Color::Black, Bug::QUEEN, 0);

    CHECK(board.do_action(Action::make_placement(136, wB1)));
    CHECK(board.do_action(Action::make_placement(120, bB1)));
    CHECK(board.do_action(Action::make_placement(135, wQ)));
    CHECK(board.do_action(Action::make_placement(103, bQ)));
    CHECK(board.do_action(Action::make_move(135, 119)));

    CHECK(board.pieces[0].count() == 2);
    CHECK(board.pieces[1].count() == 2);

    CHECK(board.do_action(Action::make_climb(120, 103)));
    CHECK(board.pieces[0].count() == 2);
    CHECK(board.pieces[1].count() == 1);

    CHECK(board.do_action(Action::make_climb(136, 119)));
    CHECK(board.pieces[0].count() == 1);
    CHECK(board.pieces[1].count() == 1);

    CHECK(board.do_action(Action::make_climb(103, 119)));
    CHECK(board.pieces[0].count() == 0);
    CHECK(board.pieces[1].count() == 2);

    CHECK(board.undo());
    CHECK(board.pieces[0].count() == 1);
    CHECK(board.pieces[1].count() == 1);
}

TEST_CASE("Undo climb sequence restores piece sets at every prior checkpoint") {
    Board board{};

    const uint8_t wB1 = tile_from_info(Color::White, Bug::BEETLE, 0);
    const uint8_t bB1 = tile_from_info(Color::Black, Bug::BEETLE, 0);
    const uint8_t wQ  = tile_from_info(Color::White, Bug::QUEEN, 0);
    const uint8_t bQ  = tile_from_info(Color::Black, Bug::QUEEN, 0);

    const std::array<Action, 8> actions = {
        Action::make_placement(136, wB1), Action::make_placement(120, bB1),
        Action::make_placement(135, wQ),  Action::make_placement(103, bQ),
        Action::make_move(135, 119),      Action::make_climb(120, 103),
        Action::make_climb(136, 119),     Action::make_climb(103, 119)};

    std::vector<std::pair<HexSet, HexSet>> checkpoints;
    checkpoints.reserve(actions.size());

    for (const Action& action : actions) {
        checkpoints.emplace_back(board.pieces[0], board.pieces[1]);
        CHECK(board.do_action(action));
    }

    for (int i = static_cast<int>(checkpoints.size()) - 1; i >= 0; --i) {
        CHECK(board.undo());
        CHECK(board.pieces[0] == checkpoints[static_cast<size_t>(i)].first);
        CHECK(board.pieces[1] == checkpoints[static_cast<size_t>(i)].second);
    }
}

TEST_CASE("Undo move sequence restores piece sets at every prior checkpoint") {
    Board board{};

    const uint8_t wA1 = tile_from_info(Color::White, Bug::ANT, 0);
    const uint8_t bM  = tile_from_info(Color::Black, Bug::MOSQUITO, 0);
    const uint8_t wQ  = tile_from_info(Color::White, Bug::QUEEN, 0);
    const uint8_t bQ  = tile_from_info(Color::Black, Bug::QUEEN, 0);

    // Mirrors Julia bb_tests move-only undo checkpoint pattern.
    const std::array<Action, 6> actions = {
        Action::make_placement(136, wA1), Action::make_placement(120, bM),
        Action::make_placement(135, wQ),  Action::make_placement(103, bQ),
        Action::make_move(135, 119),      Action::make_move(136, 104)};

    std::vector<std::pair<HexSet, HexSet>> checkpoints;
    checkpoints.reserve(actions.size());

    for (const Action& action : actions) {
        checkpoints.emplace_back(board.pieces[0], board.pieces[1]);
        CHECK(board.do_action(action));
    }

    for (int i = static_cast<int>(checkpoints.size()) - 1; i >= 0; --i) {
        CHECK(board.undo());
        CHECK(board.pieces[0] == checkpoints[static_cast<size_t>(i)].first);
        CHECK(board.pieces[1] == checkpoints[static_cast<size_t>(i)].second);
    }
}

TEST_CASE("Stepwise undo and redo checkpoints mirror Julia undo-action test") {
    Board board{};

    const uint8_t wS1 = tile_from_info(Color::White, Bug::SPIDER, 0);
    const uint8_t bS1 = tile_from_info(Color::Black, Bug::SPIDER, 0);
    const uint8_t wQ  = tile_from_info(Color::White, Bug::QUEEN, 0);
    const uint8_t bQ  = tile_from_info(Color::Black, Bug::QUEEN, 0);
    const uint8_t wA1 = tile_from_info(Color::White, Bug::ANT, 0);
    const uint8_t bA1 = tile_from_info(Color::Black, Bug::ANT, 0);
    const uint8_t wS2 = tile_from_info(Color::White, Bug::SPIDER, 1);

    const int wS1_loc = MID;
    const int bS1_loc = apply_direction(wS1_loc, Direction::E);
    const int wQ_loc  = apply_direction(wS1_loc, Direction::W);
    const int bQ_loc  = apply_direction(bS1_loc, Direction::E);
    const int wA1_loc = apply_direction(wQ_loc, Direction::W);
    const int bA1_loc = apply_direction(bQ_loc, Direction::E);
    const int wS2_loc = apply_direction(wQ_loc, Direction::W);

    const Action action7 = Action::make_move(wA1_loc, apply_direction(wS1_loc, Direction::NW));
    const Action action8 = Action::make_move(bA1_loc, apply_direction(bS1_loc, Direction::NE));

    // action1
    CHECK(board.do_action(Action::make_placement(wS1_loc, wS1)));
    check_state(board, 2, Color::Black, 1, false, false, wS1_loc);

    // action2 with undo checkpoint
    const uint64_t hash_before_action2 = board.hash;
    CHECK(board.do_action(Action::make_placement(bS1_loc, bS1)));
    check_state(board, 3, Color::White, 2, false, false, bS1_loc);
    CHECK(board.undo());
    check_state(board, 2, Color::Black, 1, false, false, wS1_loc);
    CHECK(board.hash == hash_before_action2);
    CHECK(board.do_action(Action::make_placement(bS1_loc, bS1)));

    // action3 with undo checkpoint
    const uint64_t hash_before_action3 = board.hash;
    CHECK(board.do_action(Action::make_placement(wQ_loc, wQ)));
    check_state(board, 4, Color::Black, 2, true, false, wQ_loc);
    CHECK(board.undo());
    check_state(board, 3, Color::White, 2, false, false, bS1_loc);
    CHECK(board.hash == hash_before_action3);
    CHECK(board.do_action(Action::make_placement(wQ_loc, wQ)));

    // action4 with undo checkpoint
    const uint64_t hash_before_action4 = board.hash;
    CHECK(board.do_action(Action::make_placement(bQ_loc, bQ)));
    check_state(board, 5, Color::White, 3, true, true, bQ_loc);
    CHECK(board.undo());
    check_state(board, 4, Color::Black, 2, true, false, wQ_loc);
    CHECK(board.hash == hash_before_action4);
    CHECK(board.do_action(Action::make_placement(bQ_loc, bQ)));

    // action5 with undo checkpoint
    const uint64_t hash_before_action5 = board.hash;
    CHECK(board.do_action(Action::make_placement(wA1_loc, wA1)));
    check_state(board, 6, Color::Black, 3, true, true, wA1_loc);
    CHECK(board.undo());
    check_state(board, 5, Color::White, 3, true, true, bQ_loc);
    CHECK(board.hash == hash_before_action5);
    CHECK(board.do_action(Action::make_placement(wA1_loc, wA1)));

    // action6 with undo checkpoint
    const uint64_t hash_before_action6 = board.hash;
    CHECK(board.do_action(Action::make_placement(bA1_loc, bA1)));
    check_state(board, 7, Color::White, 4, true, true, bA1_loc);
    CHECK(board.undo());
    check_state(board, 6, Color::Black, 3, true, true, wA1_loc);
    CHECK(board.hash == hash_before_action6);
    CHECK(board.do_action(Action::make_placement(bA1_loc, bA1)));

    // action7 with undo checkpoint
    const uint64_t hash_before_action7 = board.hash;
    CHECK(board.do_action(action7));
    check_state(board, 8, Color::Black, 4, true, true, action7.to);
    CHECK(board.undo());
    check_state(board, 7, Color::White, 4, true, true, bA1_loc);
    CHECK(board.hash == hash_before_action7);
    CHECK(board.do_action(action7));

    // action8 with undo checkpoint
    const uint64_t hash_before_action8 = board.hash;
    CHECK(board.do_action(action8));
    check_state(board, 9, Color::White, 5, true, true, action8.to);
    CHECK(board.undo());
    check_state(board, 8, Color::Black, 4, true, true, action7.to);
    CHECK(board.hash == hash_before_action8);
    CHECK(board.do_action(action8));

    // action9 with undo checkpoint
    const uint64_t hash_before_action9 = board.hash;
    CHECK(board.do_action(Action::make_placement(wS2_loc, wS2)));
    check_state(board, 10, Color::Black, 5, true, true, wS2_loc);
    CHECK(board.undo());
    check_state(board, 9, Color::White, 5, true, true, action8.to);
    CHECK(board.hash == hash_before_action9);
    CHECK(board.do_action(Action::make_placement(wS2_loc, wS2)));
}

TEST_CASE("Gameover is detected when white queen is fully surrounded") {
    Board board{};

    const int q  = MID;
    const int n1 = apply_direction(q, Direction::E);
    const int n2 = apply_direction(q, Direction::SE);
    const int n3 = apply_direction(q, Direction::SW);
    const int n4 = apply_direction(q, Direction::W);
    const int n5 = apply_direction(q, Direction::NW);
    const int n6 = apply_direction(q, Direction::NE);

    // White queen at center.
    CHECK(board.do_action(Action::make_placement(q, tile_from_info(Color::White, Bug::QUEEN, 0))));

    // Black surrounds while white places filler pieces elsewhere.
    CHECK(board.do_action(Action::make_placement(n1, tile_from_info(Color::Black, Bug::ANT, 0))));
    CHECK(board.do_action(Action::make_placement(0, tile_from_info(Color::White, Bug::ANT, 0))));

    CHECK(board.do_action(Action::make_placement(n2, tile_from_info(Color::Black, Bug::ANT, 1))));
    CHECK(board.do_action(Action::make_placement(1, tile_from_info(Color::White, Bug::ANT, 1))));

    CHECK(board.do_action(Action::make_placement(n3, tile_from_info(Color::Black, Bug::ANT, 2))));
    CHECK(board.do_action(Action::make_placement(2, tile_from_info(Color::White, Bug::ANT, 2))));

    CHECK(
        board.do_action(Action::make_placement(n4, tile_from_info(Color::Black, Bug::BEETLE, 0))));
    CHECK(board.do_action(Action::make_placement(3, tile_from_info(Color::White, Bug::BEETLE, 0))));

    CHECK(
        board.do_action(Action::make_placement(n5, tile_from_info(Color::Black, Bug::BEETLE, 1))));
    CHECK(board.do_action(Action::make_placement(4, tile_from_info(Color::White, Bug::BEETLE, 1))));

    CHECK(board.do_action(
        Action::make_placement(n6, tile_from_info(Color::Black, Bug::GRASSHOPPER, 0))));

    CHECK(board.gameover);
    CHECK(board.victor == Color::Black);
}

TEST_CASE("Repetition draw is detected from repeated same-side hashes") {
    Board board{};

    // With current manual-action scaffolding, pass is always accepted.
    // Five passes create three occurrences of the same full hash on the same side to move.
    CHECK(board.do_action(Action::make_pass()));
    CHECK(!board.gameover);
    CHECK(board.do_action(Action::make_pass()));
    CHECK(!board.gameover);
    CHECK(board.do_action(Action::make_pass()));
    CHECK(!board.gameover);
    CHECK(board.do_action(Action::make_pass()));
    CHECK(!board.gameover);
    CHECK(board.do_action(Action::make_pass()));

    CHECK(board.gameover);
    CHECK(board.victor == Color::Draw);
}