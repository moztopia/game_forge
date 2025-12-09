#include "writer.h"
#include <stdio.h>
#include <unistd.h>

void write_csv_header(const char* filename) {
    // Only write header if file doesn't exist
    if (access(filename, F_OK) != -1) {
        return;
    }

    FILE* f = fopen(filename, "w");
    if (f) {
        fprintf(f, "difficulty,width,height,mines,seed,rating,board_string\n");
        fclose(f);
    }
}

void write_csv_row(const char* filename, board_t* board) {
    FILE* f = fopen(filename, "a");
    if (!f) return;

    fprintf(f, "%s,%d,%d,%d,%d,%.1f,", 
            board->difficulty, 
            board->width, 
            board->height, 
            board->mines, 
            board->seed, 
            board->rating);

    int size = board->width * board->height;
    for (int i = 0; i < size; i++) {
        if (board->grid[i] == -1) {
            fputc('*', f);
        } else {
            fprintf(f, "%d", board->grid[i]);
        }
    }
    fprintf(f, "\n");
    fclose(f);
}
