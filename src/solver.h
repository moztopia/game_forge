#ifndef SOLVER_H
#define SOLVER_H

#include "board.h"

// Returns true if the board is solvable without guessing.
// Updates board->rating based on difficulty.
bool solve_board(board_t* board);

#endif // SOLVER_H
