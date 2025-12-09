#ifndef BOARD_H
#define BOARD_H

#include <stddef.h>
#include <stdbool.h>

typedef struct {
    char* difficulty;
    int width;
    int height;
    int mines;
    int seed;
    double rating;
    int* grid; // Flattened array: -1 for mine, 0-8 for clues
    bool* revealed; // For solver use
    bool* flagged;  // For solver use
} board_t;

// Function prototypes
board_t* create_board(int width, int height, int mines);
void free_board(board_t* board);
void print_board(board_t* board);

#endif // BOARD_H
