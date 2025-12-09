#include "writer.h"
#include <stdio.h>
#include <unistd.h>

void write_csv_header(const char* filename, const char* game_header, int append) {
    if (append) {
        // If appending, check if file exists. If so, do not write header.
        if (access(filename, F_OK) != -1) {
            return;
        }
    }
    
    // If not appending (overwriting) OR file didn't exist, open with "w" to create/truncate
    FILE* f = fopen(filename, "w");
    if (f) {
        fprintf(f, "difficulty,seed,score,%s\n", game_header);
        fclose(f);
    }
}

void write_csv_row(const char* filename, 
                   const char* difficulty, 
                   unsigned int seed, 
                   double score, 
                   const char* game_data) {
    FILE* f = fopen(filename, "a");
    if (!f) return;

    fprintf(f, "%s,%u,%.1f,%s\n", 
            difficulty, 
            seed,
            score, 
            game_data ? game_data : "");

    fclose(f);
}
