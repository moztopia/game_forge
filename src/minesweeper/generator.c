#include "generator.h"
#include <stdlib.h>
#include <time.h>
#include <string.h>

void generate_board(board_t* board) {
    if (!board || !board->grid) return;

    int size = board->width * board->height;
    // int placed = 0;

    // Clear grid
    memset(board->grid, 0, size * sizeof(int));
    
    // Simple random placement
    // Note: optimizing for large boards/many mines might require fisher-yates shuffle of indices
    // using reservoir sampling or just shuffling an array of indices.
    
    // For simplicity and uniformity:
    int* indices = malloc(size * sizeof(int));
    for(int i=0; i<size; i++) indices[i] = i;

    // Shuffle indices
    for (int i = size - 1; i > 0; i--) {
        int j = rand() % (i + 1);
        int temp = indices[i];
        indices[i] = indices[j];
        indices[j] = temp;
    }

    // Place mines
    for (int i = 0; i < board->mines; i++) {
        board->grid[indices[i]] = -1;
    }
    free(indices);

    // Calculate clues
    for (int y = 0; y < board->height; y++) {
        for (int x = 0; x < board->width; x++) {
            int idx = y * board->width + x;
            if (board->grid[idx] == -1) continue;

            int count = 0;
            // Check 8 neighbors
            for (int dy = -1; dy <= 1; dy++) {
                for (int dx = -1; dx <= 1; dx++) {
                    if (dx == 0 && dy == 0) continue;
                    int nx = x + dx;
                    int ny = y + dy;
                    if (nx >= 0 && nx < board->width && ny >= 0 && ny < board->height) {
                        if (board->grid[ny * board->width + nx] == -1) {
                            count++;
                        }
                    }
                }
            }
            board->grid[idx] = count;
        }
    }
}

board_t* create_board(int width, int height, int mines) {
    board_t* b = calloc(1, sizeof(board_t));
    b->width = width;
    b->height = height;
    b->mines = mines;
    b->grid = calloc(width * height, sizeof(int));
    b->revealed = calloc(width * height, sizeof(bool));
    b->flagged = calloc(width * height, sizeof(bool));
    return b;
}

void free_board(board_t* board) {
    if (!board) return;
    free(board->grid);
    free(board->revealed);
    free(board->flagged);
    if (board->difficulty) free(board->difficulty);
    if (board->tags) free(board->tags);
    free(board);
}
