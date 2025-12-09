#ifndef CONFIG_H
#define CONFIG_H

#include <stddef.h>

// Simple Key-Value pair
typedef struct {
    char* key;
    char* value;
} property_t;

typedef struct {
    char* name;
    int count;
    
    // Generic properties (e.g. mines.min, size.rows, tags, etc.)
    // We flatten the structure: "mines: minimum: 10" -> key="mines.minimum", value="10"
    // Top level fields like "count" are special and kept struct fields for main loop convenience.
    property_t* properties;
    size_t property_count;
} difficulty_config_t;

typedef struct {
    char* game_name;
    char* output_file;
    int append;
    
    difficulty_config_t* difficulties;
    size_t difficulty_count;
} local_game_config_t;

typedef struct {
    int threads;
    
    local_game_config_t* games;
    size_t game_count;
} game_config_t;

// Function prototypes
game_config_t* load_config(const char* path);
void free_config(game_config_t* config);

// Property helpers
const char* get_property(difficulty_config_t* config, const char* key);
int get_int_property(difficulty_config_t* config, const char* key, int default_val);
const char* get_string_property(difficulty_config_t* config, const char* key, const char* default_val);

#endif // CONFIG_H
