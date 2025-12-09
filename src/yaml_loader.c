#include "config.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

// Simple line-based parser tailored for game_forge.yaml schema
// Assumptions:
// - Keys are "key: value"
// - Indentation denotes hierarchy (we track it simply)
// - We care about: game -> minesweeper -> create -> [difficulty] -> props

char* trim(char* str) {
    while(isspace((unsigned char)*str)) str++;
    if(*str == 0) return str;
    char* end = str + strlen(str) - 1;
    while(end > str && isspace((unsigned char)*end)) end--;
    end[1] = '\0';
    return str;
}

game_config_t* load_config(const char* path) {
    FILE* fh = fopen(path, "r");
    if (!fh) {
        perror("Failed to open config file");
        return NULL;
    }

    game_config_t* config = calloc(1, sizeof(game_config_t));
    int diff_cap = 4;
    config->difficulties = calloc(diff_cap, sizeof(difficulty_config_t));
    config->difficulty_count = 0;

    char line[256];
    // int current_indent = 0;
    
    // State
    // char* current_diff_name = NULL;
    difficulty_config_t current_diff = {0};
    
    // We strictly look for hierarchy
    // But since keys are unique enough in this small context:
    // game, minesweeper, create are strict parents
    // difficulty names are at indent level 6 (2+2+2) or so
    // properties are at indent level 8
    
    // Let's just track if we are in "create" block.
    int in_create = 0;
    
    while(fgets(line, sizeof(line), fh)) {
        // Calculate indent
        int indent = 0;
        char* ptr = line;
        while(*ptr == ' ') { indent++; ptr++; }
        
        char* trimmed = trim(ptr);
        if (strlen(trimmed) == 0 || trimmed[0] == '#') continue;

        // Check for "create:"
        if (strncmp(trimmed, "create:", 7) == 0) {
            in_create = 1;
            continue;
        }
        
        // If we are in create block logic
        if (in_create) {
            // Check for property line "key: value"
            char* colon = strchr(trimmed, ':');
            if (colon) {
                *colon = '\0';
                char* key = trim(trimmed);
                char* value_str = trim(colon + 1);
                
                // If value is empty, it's likely a block start (difficulty name)
                if (strlen(value_str) == 0) {
                    // Start of a difficulty or sub-block like "size:" or "mines:"
                    if (strcmp(key, "size") == 0 || strcmp(key, "mines") == 0) {
                        // Just ignore, properties follow in next lines
                    } else {
                        // Assume difficulty name
                        // Save previous difficulty if it exists
                        if (current_diff.name) {
                             if (config->difficulty_count >= (size_t)diff_cap) {
                                diff_cap *= 2;
                                config->difficulties = realloc(config->difficulties, diff_cap * sizeof(difficulty_config_t));
                            }
                            config->difficulties[config->difficulty_count++] = current_diff;
                            memset(&current_diff, 0, sizeof(difficulty_config_t));
                        }
                        
                        current_diff.name = strdup(key);
                    }
                } else {
                    // It's a property
                    if (strcmp(key, "count") == 0) current_diff.count = atoi(value_str);
                    else if (strcmp(key, "columns") == 0) current_diff.columns = atoi(value_str);
                    else if (strcmp(key, "rows") == 0) current_diff.rows = atoi(value_str);
                    else if (strcmp(key, "minimum") == 0) current_diff.min_mines = atoi(value_str);
                    else if (strcmp(key, "maximum") == 0) current_diff.max_mines = atoi(value_str);
                }
            }
        }
    }
    
    // Save last difficulty
    if (current_diff.name) {
         if (config->difficulty_count >= (size_t)diff_cap) {
            diff_cap *= 2;
            config->difficulties = realloc(config->difficulties, diff_cap * sizeof(difficulty_config_t));
        }
        config->difficulties[config->difficulty_count++] = current_diff;
    }

    fclose(fh);
    return config;
}

void free_config(game_config_t* config) {
    if (!config) return;
    for (size_t i = 0; i < config->difficulty_count; i++) {
        free(config->difficulties[i].name);
    }
    free(config->difficulties);
    free(config);
}
