#include "intsect/board.hpp"
#include "intsect/move_generation.hpp"

#include <iostream>

namespace intsect {

size_t perft(Board board, int depth) {
    if (depth <= 0) {
        return 1;
    }
    if (depth == 1) {
        return get_valid_actions(board).size();
    }
    std::vector<Action> valid_actions = get_valid_actions(board);
    size_t tot = 0;
    for (Action action : valid_actions) {
        bool s = board.do_action(action);
        tot += perft(board, depth - 1);
        s &= board.undo();

        if (!s) {
            std::cout << "Some action was invalid";
            return (size_t)-1;
        }
    }
    return tot;
}

} // namespace intsect