#ifndef CONFIG_H
#define CONFIG_H

#include <stddef.h>

typedef struct {
    char* name;
    int count;
    int columns;
    int rows;
    int min_mines;
    int max_mines;
} difficulty_config_t;

typedef struct {
    difficulty_config_t* difficulties;
    size_t difficulty_count;
} game_config_t;

// Function prototypes
game_config_t* load_config(const char* path);
void free_config(game_config_t* config);

#endif // CONFIG_H
