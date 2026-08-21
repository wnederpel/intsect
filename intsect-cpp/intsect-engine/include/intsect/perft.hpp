#pragma once

#include "intsect/board.hpp"
#include "intsect/move_generation.hpp"

namespace intsect {

size_t perft(Board board, int depth);

} // namespace intsect