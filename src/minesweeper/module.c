#include "generator.h"
#include "solver.h"
#include "../core/game.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Module state (none needed per instance for this simple generator, but we could store global config)
// Wait, we need to adapt generator to use the generic config object.
// But generator currently takes params. 
// Let's implement process() to read generic config, call generator, solve, and format output.

void* minesweeper_init(difficulty_config_t* config) {
    // Just pass through the config as context
    return config;
}

void minesweeper_cleanup(void* ctx) {
    (void)ctx;
}

game_result_t minesweeper_process(void* ctx, unsigned int seed) {
    difficulty_config_t* config = (difficulty_config_t*)ctx;
    
    // Read generic properties for mines
    int min_mines = get_int_property(config, "mines.minimum", 10);
    int max_mines = get_int_property(config, "mines.maximum", 10);
    
    // Use thread-safe rand
    unsigned int seed_copy = seed;
    int mines = min_mines + (rand_r(&seed_copy) % (max_mines - min_mines + 1));
    
    // Create Board
    int cols = get_int_property(config, "columns", 9);
    int rows = get_int_property(config, "rows", 9);
    board_t* board = create_board(cols, rows, mines);
    
    // Since board struct still has "difficulty" and "tags" fields which are duplicated in config
    // we can populate them if solver/generator needs them, OR we can remove them from board_t 
    // and let module handle it.
    // Solver updates board->score.
    // Generator uses board->mines.
    
    // Set seed
    board->seed = seed; // The board seed field is int, seed is uint. Cast fine.
    
    generate_board(board);
    
    bool success = solve_board(board);
    game_result_t result = {0};
    result.success = success;
    result.score = board->score;
    
    if (success) {
        // Format CSV data: width,height,mines,tags,board_string
        // Main loop writes: difficulty,seed,score
        // So we append: width,height,mines,tags,board_string
        
        // Calculate size needed
        // width(10) + height(10) + mines(10) + tags(len) + board(w*h) + commas + terminators
        const char* tags = get_string_property(config, "tags", "");
        int board_len = board->width * board->height;
        int buf_size = 50 + strlen(tags) + board_len + 10;
        
        result.csv_data = malloc(buf_size);
        int offset = sprintf(result.csv_data, "%d,%d,%d,%s,", 
            board->width, board->height, board->mines, tags);
            
        // Append grid
        char* ptr = result.csv_data + offset;
        for (int i = 0; i < board_len; i++) {
            if (board->grid[i] == -1) {
                *ptr++ = '*';
            } else {
                *ptr++ = '0' + board->grid[i];
            }
        }
        *ptr = '\0';
    }
    
    free_board(board);
    return result;
}

const game_module_t MINESWEEPER_MODULE = {
    .game_name = "Minesweeper",
    .csv_header = "width,height,mines,tags,board_string", // Part AFTER standard cols
    .init = minesweeper_init,
    .cleanup = minesweeper_cleanup,
    .process = minesweeper_process
};
