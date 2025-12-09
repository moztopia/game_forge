#ifndef GAME_H
#define GAME_H

#include "config.h"
#include <stdbool.h>

// Result of a single game generation attempt
typedef struct {
    bool success;
    double score;
    char* csv_data; // Comma separated values for the row (excluding standard columns if main handles them, or full row?)
                    // Decision: Module returns part AFTER standard columns (difficulty, seed, score, time).
                    // Actually, main loop calculates time. 
                    // Main loop knows: Difficulty Name, Seed (it generated it), Score (from result), Time.
                    // So module should return: game-specific data string.
} game_result_t;

// Function pointer types for the module
typedef void* (*game_init_func)(difficulty_config_t* config);
typedef void (*game_cleanup_func)(void* ctx);
typedef game_result_t (*game_process_func)(void* ctx, unsigned int seed);

typedef struct {
    const char* game_name;
    const char* csv_header; // Game-specific CSV header part
    
    // Lifecycle
    game_init_func init;
    game_cleanup_func cleanup;
    
    // Execution
    game_process_func process;
} game_module_t;

void free_game_result(game_result_t* result);

#endif // GAME_H
