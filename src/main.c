#include <stdio.h>
#include <stdlib.h>
#include <stdlib.h>
#include <unistd.h>
// Make sure we have POSIX defines
#define _POSIX_C_SOURCE 200809L
#include <string.h>
#include <time.h>
#include "config.h"
#include "board.h"
#include "generator.h"
#include "solver.h"
#include "writer.h"

int main() {
    srand(time(NULL));

    game_config_t* config = load_config("game_forge.yaml");
    if (!config) {
        fprintf(stderr, "Error loading config\n");
        return 1;
    }

    const char* output_file = "minesweeper.csv";
    write_csv_header(output_file);

    for (size_t i = 0; i < config->difficulty_count; i++) {
        difficulty_config_t* diff = &config->difficulties[i];
        printf("Generating %d boards for difficulty: %s\n", diff->count, diff->name);

        int generated = 0;
        int attempts = 0;
        
        while (generated < diff->count) {
            attempts++;
            // Random mines count
            int min = diff->min_mines;
            int max = diff->max_mines;
            int mines = min + (rand() % (max - min + 1));
            
            board_t* board = create_board(diff->columns, diff->rows, mines);
            board->difficulty = strdup(diff->name);
            board->seed = rand(); // Seed used for what? Just storing
            
            generate_board(board);
            
            if (solve_board(board)) {
                write_csv_row(output_file, board);
                generated++;
                if (generated % 10 == 0) {
                    printf("  %d/%d\r", generated, diff->count);
                    fflush(stdout);
                }
            } else {
                // Failed, retry
            }
            
            free_board(board);
            
            if (attempts > diff->count * 1000) {
                 printf("Warning: High failure rate for hard difficulty. Aborting to prevent hang.\n");
                 break;
            }
        }
        printf("\nDone %s\n", diff->name);
    }

    free_config(config);
    return 0;
}
