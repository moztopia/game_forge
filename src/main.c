#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <pthread.h>
// Make sure we have POSIX defines
#define _POSIX_C_SOURCE 200809L
#include <string.h>
#include <time.h>
#include "core/config.h"
#include "core/writer.h"
#include "core/game.h"
#include "minesweeper/module.h"

// terminal control
#define CLEAR_SCREEN "\033[2J\033[H"
#define MOVE_TOP "\033[H"
#define HIDE_CURSOR "\033[?25l"
#define SHOW_CURSOR "\033[?25h"
#define COLOR_RED "\033[0;31m"
#define COLOR_RESET "\033[0m"

volatile sig_atomic_t keep_running = 1;

void handle_sigint(int sig) {
    (void)sig;
    keep_running = 0;
}

typedef struct {
    char game_name[20];
    char name[50];
    int target;
    int generated;
    long long attempts;
    long long failures;
    struct timespec start_time;
    struct timespec end_time;
    int status; // 0: pending, 1: running, 2: done
    volatile int stop_signal;
} diff_stats_t;

// Helper for time difference in seconds
double get_elapsed_seconds(struct timespec start, struct timespec end) {
    return (end.tv_sec - start.tv_sec) + (end.tv_nsec - start.tv_nsec) / 1e9;
}

// Synchronization
pthread_mutex_t file_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t stats_mutex = PTHREAD_MUTEX_INITIALIZER;

// Shared state for workers
typedef struct {
    difficulty_config_t* diff_config;
    diff_stats_t* diff_stats;
    const char* output_file;
    const game_module_t* module; // Pointer to game module
    void* module_ctx;           // Context returned by module init
} worker_ctx_t;

void* worker_thread(void* arg) {
    worker_ctx_t* ctx = (worker_ctx_t*)arg;
    unsigned int seed = time(NULL) ^ pthread_self(); // simple thread-local seed
    
    while (keep_running) {
        // Check if target reached (loose check)
        int gen = 0;
        int target = 0;
        
        pthread_mutex_lock(&stats_mutex);
        gen = ctx->diff_stats->generated;
        target = ctx->diff_stats->target;
        int stop = ctx->diff_stats->stop_signal;
        pthread_mutex_unlock(&stats_mutex);
        
        if (gen >= target || stop) break;

        // Process Game Tick
        // We pass the diff config as context? No, main initialized the module context.
        // Wait, module_ctx is per-difficulty or global?
        // In our minesweeper impl, init returns the `difficulty_config`. 
        // So we need to call init PER difficulty? Yes.
        
        game_result_t result = ctx->module->process(ctx->module_ctx, rand_r(&seed));
        
        bool success = result.success;
        
        if (success) {
            // Write to file (Critical Section 1)
            pthread_mutex_lock(&file_mutex);
            write_csv_row(ctx->output_file, 
                          ctx->diff_config->name, 
                          seed, 
                          result.score, 
                          result.csv_data);
            pthread_mutex_unlock(&file_mutex);
        }
        
        // Update Stats (Critical Section 2)
        pthread_mutex_lock(&stats_mutex);
        ctx->diff_stats->attempts++;
        if (success) {
            ctx->diff_stats->generated++;
        } else {
            ctx->diff_stats->failures++;
        }
        pthread_mutex_unlock(&stats_mutex);
        
        // Free result data
        if (result.csv_data) free(result.csv_data);
    }
    return NULL;
}

void render_dashboard(int active_diff_idx, diff_stats_t* stats, int count, const char* game_title, int num_threads) {
    printf("%s", MOVE_TOP);
    printf("  ____                        _____                    \n");
    printf(" / ___| __ _ _ __ ___   ___  |  ___|__  _ __ __ _  ___ \n");
    printf("| |  _ / _` | '_ ` _ \\ / _ \\ | |_ / _ \\| '__/ _` |/ _ \\\n");
    printf("| |_| | (_| | | | | | |  __/ |  _| (_) | | | (_| |  __/\n");
    printf(" \\____|\\__,_|_| |_| |_|\\___| |_|  \\___/|_|  \\__, |\\___|\n");
    printf("                                            |___/      \n");
    
    // Centered header (width ~57)
    // "=== Puzzle GENERATOR (%d threads) == Ctrl+C to Stop ==="
    printf(" === Puzzle GENERATOR (%d threads) == Ctrl+C to Stop ===\n", num_threads);
    printf("                                                       \n\n"); // Spacer instead of Status line
    
    // Fixed widths: Game(12) | Difficulty(15) | Generated(10) | Target(8) | Attempts(12) | Success%(8) | Time(10)
    printf("%-1s %-12s | %-15s | %-10s | %-8s | %-12s | %-8s | %-10s\n", 
           "", "Game", "Difficulty", "Generated", "Target", "Attempts", "Success%", "Time");
    printf("-----------------------------------------------------------------------------------------------------\n");
    
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);

    for(int i=0; i<count; i++) {
        long long attempts = 0;
        int generated = 0;
        int status = 0;
        
        pthread_mutex_lock(&stats_mutex);
        attempts = stats[i].attempts;
        generated = stats[i].generated;
        status = stats[i].status;
        int stopped = stats[i].stop_signal;
        pthread_mutex_unlock(&stats_mutex);
        
        double success_rate = 0.0;
        if (attempts > 0) {
            success_rate = (double)generated / attempts * 100.0;
        }
        
        char indicator = (i == active_diff_idx && keep_running) ? '>' : ' ';
        
        // Calculate elapsed
        double elapsed = 0;
        if (status == 1) { // Running
             elapsed = get_elapsed_seconds(stats[i].start_time, now);
        } else if (status == 2) { // Done
             elapsed = get_elapsed_seconds(stats[i].start_time, stats[i].end_time);
        }
        
        int total_seconds = (int)elapsed;
        int minutes = total_seconds / 60;
        int seconds = total_seconds % 60;
        int hundredths = (int)((elapsed - total_seconds) * 100);
        
        char timeout_str[32] = "";
        if (stopped) {
            snprintf(timeout_str, sizeof(timeout_str), " %stimeout%s", COLOR_RED, COLOR_RESET);
        }

        printf("%c %-12s | %-15s | %-10d | %-8d | %-12lld | %6.2f%% | %02d:%02d:%02d%s\n", 
               indicator,
               stats[i].game_name,
               stats[i].name, 
               generated, 
               stats[i].target, 
               attempts, 
               success_rate,
               minutes, seconds, hundredths, timeout_str);
    }
    printf("\n");
}

int main(int argc, char** argv) {
    (void)argc; (void)argv; // Parsing args later?
    
    signal(SIGINT, handle_sigint);
    srand(time(NULL));

    // Hardcoded module selection for now
    const game_module_t* engine = &MINESWEEPER_MODULE;

    game_config_t* config = load_config("game_forge.yaml");
    if (!config) {
        fprintf(stderr, "Error loading config\n");
        return 1;
    }
    
    int num_threads = config->threads > 0 ? config->threads : 1;
    printf("Starting %s with %d threads...\n", engine->game_name, num_threads);

    const char* output_file = config->output_file ? config->output_file : "minesweeper.csv";
    write_csv_header(output_file, engine->csv_header, config->append);

    // Init stats
    diff_stats_t* stats = calloc(config->difficulty_count, sizeof(diff_stats_t));
    for(size_t i=0; i<config->difficulty_count; i++) {
        strncpy(stats[i].game_name, engine->game_name, 19); 
        strncpy(stats[i].name, config->difficulties[i].name, 49);
        stats[i].target = config->difficulties[i].count;
        stats[i].status = 0;
        stats[i].stop_signal = 0;
    }

    printf("%s%s", CLEAR_SCREEN, HIDE_CURSOR);

    for (size_t i = 0; i < config->difficulty_count; i++) {
        if (!keep_running) break;

        difficulty_config_t* diff = &config->difficulties[i];

        // Init Module for this difficulty
        void* mod_ctx = engine->init(diff);

        // Mark start
        pthread_mutex_lock(&stats_mutex);
        stats[i].status = 1; 
        clock_gettime(CLOCK_MONOTONIC, &stats[i].start_time);
        pthread_mutex_unlock(&stats_mutex);
        
        // Determine number of threads for this work
        pthread_t* threads = malloc(num_threads * sizeof(pthread_t));
        worker_ctx_t* ctx = malloc(num_threads * sizeof(worker_ctx_t));
        
        for(int t=0; t<num_threads; t++) {
            ctx[t].diff_config = diff;
            ctx[t].diff_stats = &stats[i];
            ctx[t].output_file = output_file;
            ctx[t].module = engine;
            ctx[t].module_ctx = mod_ctx; // Shared context logic? 
                                       // Minesweeper just uses (difficulty_config_t*) as context which is read-only.
                                       // Safe for threads if only reading.
            
            pthread_create(&threads[t], NULL, worker_thread, &ctx[t]);
        }
        
        // Main thread becomes dashboard renderer
        while (keep_running) {
             pthread_mutex_lock(&stats_mutex);
             int gen = stats[i].generated;
             int tar = stats[i].target;
             pthread_mutex_unlock(&stats_mutex);
             
             render_dashboard(i, stats, config->difficulty_count, engine->game_name, num_threads);
             
             if (gen >= tar) break;
             
             // Check Max Time
             struct timespec now;
             clock_gettime(CLOCK_MONOTONIC, &now);
             double elapsed = get_elapsed_seconds(stats[i].start_time, now);
             int max_time = get_int_property(diff, "max_time", 0);
             
             if (max_time > 0 && elapsed >= max_time) {
                 pthread_mutex_lock(&stats_mutex);
                 stats[i].stop_signal = 1;
                 pthread_mutex_unlock(&stats_mutex);
                 break;
             }

             struct timespec ts;
             ts.tv_sec = 0;
             ts.tv_nsec = 100000000; // 100ms
             nanosleep(&ts, NULL);
        }
        
        // Record end time
        pthread_mutex_lock(&stats_mutex);
        clock_gettime(CLOCK_MONOTONIC, &stats[i].end_time);
        stats[i].status = 2;
        pthread_mutex_unlock(&stats_mutex);

        // Join threads
        for(int t=0; t<num_threads; t++) {
            pthread_join(threads[t], NULL);
        }
        
        engine->cleanup(mod_ctx);
        
        free(threads);
        free(ctx);
        
        // Final render for this difficulty
        render_dashboard(i, stats, config->difficulty_count, engine->game_name, num_threads);
    }
    
    printf("%s\nDone.\n", SHOW_CURSOR);

    free(stats);
    free_config(config);
    return 0;
}
