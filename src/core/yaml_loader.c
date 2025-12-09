#include "config.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

char* trim(char* str) {
    while(isspace((unsigned char)*str)) str++;
    if(*str == 0) return str;
    char* end = str + strlen(str) - 1;
    while(end > str && isspace((unsigned char)*end)) end--;
    end[1] = '\0';
    return str;
}

// Add property to config
void add_property(difficulty_config_t* diff, const char* key, const char* value) {
    diff->property_count++;
    diff->properties = realloc(diff->properties, diff->property_count * sizeof(property_t));
    diff->properties[diff->property_count - 1].key = strdup(key);
    diff->properties[diff->property_count - 1].value = strdup(value);
}

// Property getters
const char* get_property(difficulty_config_t* config, const char* key) {
    if (!config) return NULL;
    for (size_t i = 0; i < config->property_count; i++) {
        if (strcmp(config->properties[i].key, key) == 0) {
            return config->properties[i].value;
        }
    }
    return NULL;
}

int get_int_property(difficulty_config_t* config, const char* key, int default_val) {
    const char* val = get_property(config, key);
    if (val) return atoi(val);
    return default_val;
}

const char* get_string_property(difficulty_config_t* config, const char* key, const char* default_val) {
    const char* val = get_property(config, key);
    if (val) return val;
    return default_val;
}

game_config_t* load_config(const char* path) {
    FILE* fh = fopen(path, "r");
    if (!fh) {
        perror("Failed to open config file");
        return NULL;
    }

    game_config_t* config = calloc(1, sizeof(game_config_t));
    
    // Game list dynamic array
    int game_cap = 2;
    config->games = calloc(game_cap, sizeof(local_game_config_t));
    config->game_count = 0;

    char line[256];
    
    // State
    local_game_config_t* current_game = NULL;
    difficulty_config_t current_diff = {0};
    
    int state_config = 0; // 1 if inside game.config
    // We determine context by indent and top-level keys
    
    // Diff list dynamic handling for current game
    int diff_cap = 0; 
    
    // Nested property tracking
    char last_key[64] = {0};
    int last_indent = -1;
    
    // Indent levels identification
    int indent_game_root = 2; // "minesweeper:"
    int indent_puzzles = -1;  // "puzzles:" inside game
    int indent_diff_name = -1; // "easy:" inside puzzles

    while(fgets(line, sizeof(line), fh)) {
        int indent = 0;
        char* ptr = line;
        while(*ptr == ' ') { indent++; ptr++; }
        
        char* trimmed = trim(ptr);
        if (strlen(trimmed) == 0 || trimmed[0] == '#') continue;

        // Level 0: "game:" - ignore, just context
        if (strcmp(trimmed, "game:") == 0) continue;

        char* colon = strchr(trimmed, ':');
        char key[128] = {0};
        char value[128] = {0};
        
        if (colon) {
            *colon = '\0';
            strncpy(key, trimmed, 127);
            strncpy(value, trim(colon + 1), 127);
        } else {
             // Should not happen for valid yaml lines we care about
             continue;
        }

        // Level 2: Top level blocks under game
        if (indent == indent_game_root) {
            // Save previous difficulty if open
            if (current_game && current_diff.name) {
                if (current_game->difficulty_count >= (size_t)diff_cap) {
                    diff_cap *= 2;
                    current_game->difficulties = realloc(current_game->difficulties, diff_cap * sizeof(difficulty_config_t));
                }
                current_game->difficulties[current_game->difficulty_count++] = current_diff;
                memset(&current_diff, 0, sizeof(difficulty_config_t));
            }
            
            if (strcmp(key, "config") == 0) {
                state_config = 1;
                current_game = NULL; // Exit any game block
            } else {
                state_config = 0;
                // Start new game
                if (config->game_count >= (size_t)game_cap) {
                    game_cap *= 2;
                    config->games = realloc(config->games, game_cap * sizeof(local_game_config_t));
                }
                current_game = &config->games[config->game_count++];
                // Initialize new game struct
                memset(current_game, 0, sizeof(local_game_config_t));
                current_game->game_name = strdup(key);
                
                // alloc difficultes
                diff_cap = 4;
                current_game->difficulties = calloc(diff_cap, sizeof(difficulty_config_t));
            }
            // Reset context
            indent_puzzles = -1;
            indent_diff_name = -1;
            continue;
        }
        
        // Inside Config Block
        if (state_config) {
             if (strcmp(key, "threads") == 0) config->threads = atoi(value);
             continue;
        }
        
        // Inside Game Block
        if (current_game) {
            // Check for "puzzles:"
            if (strcmp(key, "puzzles") == 0 && strlen(value) == 0) {
                indent_puzzles = indent;
                continue;
            }
            
            // Game properties (output, append) - usually indent 4 (same as puzzles)
            if (indent_puzzles == -1 || indent == indent_puzzles) {
                 if (strcmp(key, "output") == 0) current_game->output_file = strdup(value);
                 else if (strcmp(key, "append") == 0) current_game->append = (strcmp(value, "true") == 0);
                 continue;
            }
            
            // Inside Puzzles
            if (indent_puzzles != -1 && indent > indent_puzzles) {
                // Difficulty Name (e.g. "easy:")
                if (indent_diff_name == -1) indent_diff_name = indent;
                
                if (indent == indent_diff_name) {
                     // Save previous difficulty
                    if (current_diff.name) {
                        if (current_game->difficulty_count >= (size_t)diff_cap) {
                            diff_cap *= 2;
                            current_game->difficulties = realloc(current_game->difficulties, diff_cap * sizeof(difficulty_config_t));
                        }
                        current_game->difficulties[current_game->difficulty_count++] = current_diff;
                        memset(&current_diff, 0, sizeof(difficulty_config_t));
                    }
                    current_diff.name = strdup(key);
                    last_key[0] = '\0';
                } 
                // Difficulty Properties
                else if (indent > indent_diff_name) {
                     if (strlen(value) == 0) {
                         // Nested start
                         strncpy(last_key, key, 63);
                         last_indent = indent;
                     } else {
                         // Leaf
                         if (strcmp(key, "count") == 0) current_diff.count = atoi(value);
                         else {
                            char full_key[128];
                             if (indent > last_indent && strlen(last_key) > 0) {
                                snprintf(full_key, sizeof(full_key), "%s.%s", last_key, key);
                            } else {
                                strncpy(full_key, key, 127);
                            }
                            
                            if (strcmp(full_key, "size.columns") == 0) add_property(&current_diff, "columns", value);
                            else if (strcmp(full_key, "size.rows") == 0) add_property(&current_diff, "rows", value);
                            else add_property(&current_diff, full_key, value);
                         }
                     }
                }
            }
        }
    }
    
    // Save last diff
    if (current_game && current_diff.name) {
         if (current_game->difficulty_count >= (size_t)diff_cap) {
            diff_cap *= 2;
            current_game->difficulties = realloc(current_game->difficulties, diff_cap * sizeof(difficulty_config_t));
        }
        current_game->difficulties[current_game->difficulty_count++] = current_diff;
    }

    fclose(fh);
    return config;
}

void free_config(game_config_t* config) {
    if (!config) return;
    
    for(size_t g=0; g<config->game_count; g++) {
        local_game_config_t* game = &config->games[g];
        if (game->game_name) free(game->game_name);
        if (game->output_file) free(game->output_file);
        
        for (size_t i = 0; i < game->difficulty_count; i++) {
            free(game->difficulties[i].name);
            for(size_t p=0; p<game->difficulties[i].property_count; p++) {
                free(game->difficulties[i].properties[p].key);
                free(game->difficulties[i].properties[p].value);
            }
            if(game->difficulties[i].properties) free(game->difficulties[i].properties);
        }
        free(game->difficulties);
    }
    free(config->games);
    free(config);
}
