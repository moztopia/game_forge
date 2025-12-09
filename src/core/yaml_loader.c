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
    int diff_cap = 4;
    config->difficulties = calloc(diff_cap, sizeof(difficulty_config_t));
    config->difficulty_count = 0;

    char line[256];
    
    // State
    difficulty_config_t current_diff = {0};
    
    int in_create = 0;
    int in_config = 0; 
    
    // We need to track context for nested keys (e.g. mines.minimum)
    // Simple state machine:
    // If we are in difficulty block, any 4-space indent is a property.
    // Any 6-space indent is a nested property.
    // Actually, based on previous file:
    // create: (level 0 or 2?)
    //   easy: (level 4)
    //     prop: val (level 6)
    //     mines: (level 6)
    //       min: 10 (level 8)
    
    // Let's track the last key to support one level of nesting (parent.child)
    char last_key[64] = {0};
    int last_indent = -1;

    int diff_indent = -1;

    while(fgets(line, sizeof(line), fh)) {
        // Calculate indent
        int indent = 0;
        char* ptr = line;
        while(*ptr == ' ') { indent++; ptr++; }
        
        char* trimmed = trim(ptr);
        if (strlen(trimmed) == 0 || trimmed[0] == '#') continue;

        if (strncmp(trimmed, "config:", 7) == 0) {
            in_config = 1;
            in_create = 0;
            continue;
        }

        if (strncmp(trimmed, "puzzles:", 8) == 0) {
            in_create = 1;
            in_config = 0;
            diff_indent = -1; // Reset for new create block
            continue;
        }
        
        if (in_config) {
             char* colon = strchr(trimmed, ':');
             if (colon) {
                *colon = '\0';
                char* key = trim(trimmed);
                char* value_str = trim(colon + 1);
                if (strcmp(key, "threads") == 0) config->threads = atoi(value_str);
                else if (strcmp(key, "output") == 0) config->output_file = strdup(value_str);
                else if (strcmp(key, "append") == 0) {
                    if (strcmp(value_str, "true") == 0) config->append = 1;
                    else config->append = 0;
                }
             }
        }
        
        if (in_create) {
            char* colon = strchr(trimmed, ':');
            if (colon) {
                *colon = '\0';
                char* key = trim(trimmed);
                char* value_str = trim(colon + 1);
                
                // Determine hierarchy level
                if (diff_indent == -1) {
                    // First item in create block, assume it defines the difficulty level indent
                    diff_indent = indent;
                }
                
                if (indent == diff_indent) {
                    // This is a difficulty definition (e.g. "easy:")
                    if (strlen(value_str) == 0) {
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
                        last_key[0] = '\0'; // Reset parent context
                    }
                } else if (indent > diff_indent) {
                    // Property of the current difficulty
                    if (strlen(value_str) == 0) {
                       // Nested block start (e.g. mines:)
                       strncpy(last_key, key, 63);
                       last_indent = indent;
                    } else {
                       // Leaf property
                        if (strcmp(key, "count") == 0) current_diff.count = atoi(value_str);
                        else {
                            // Construct key
                            char full_key[128];
                            // If this line is indented deeper than the last block start AND we have a last block
                            // And the indent matches the expected nested property indent (usually last_indent + 2)
                            // Let's be loose: if it's deeper than last_indent, it's a child of last_key
                            if (indent > last_indent && strlen(last_key) > 0) {
                                snprintf(full_key, sizeof(full_key), "%s.%s", last_key, key);
                            } else {
                                // If indent retreated (e.g. was 8, now 6), but still > diff_indent (4)
                                // It's a direct property of diff
                                strncpy(full_key, key, 127);
                            }
                            
                            if (strcmp(full_key, "size.columns") == 0) add_property(&current_diff, "columns", value_str);
                            else if (strcmp(full_key, "size.rows") == 0) add_property(&current_diff, "rows", value_str);
                            else {
                                add_property(&current_diff, full_key, value_str);
                            }
                        }
                    }
                }
            }
        }
    }
    
    // Save last
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
        for(size_t p=0; p<config->difficulties[i].property_count; p++) {
            free(config->difficulties[i].properties[p].key);
            free(config->difficulties[i].properties[p].value);
        }
        if(config->difficulties[i].properties) free(config->difficulties[i].properties);
    }
    free(config->difficulties);
    if (config->output_file) free(config->output_file);
    free(config);
}
