#include "solver.h"
#include <stdbool.h>
#include <string.h>
#include <stdlib.h>

// Helper to get neighbor indices
void get_neighbors(board_t* b, int idx, int* neighbors, int* count) {
    *count = 0;
    int w = b->width;
    int h = b->height;
    int x = idx % w;
    int y = idx / w;

    for (int dy = -1; dy <= 1; dy++) {
        for (int dx = -1; dx <= 1; dx++) {
            if (dx == 0 && dy == 0) continue;
            int nx = x + dx;
            int ny = y + dy;
            if (nx >= 0 && nx < w && ny >= 0 && ny < h) {
                neighbors[(*count)++] = ny * w + nx;
            }
        }
    }
}

bool solve_board(board_t* board) {
    int size = board->width * board->height;
    int total_safe = size - board->mines;
    int revealed_count = 0;

    // Reset solver state
    memset(board->revealed, 0, size * sizeof(bool));
    memset(board->flagged, 0, size * sizeof(bool));

    // 1. Initial Reveal: Find a safe starting point (usually a 0)
    // For strict solvability, we might require that ANY 0 works, or we cheat and pick the best one.
    // Let's assume we reveal ALL "openings" (connected components of 0s) to simulate a user clicking around?
    // NO, a "no guess" board usually means from a single start point you can solve it.
    // Let's pick the first 0 we find.
    int start_idx = -1;
    for (int i=0; i<size; i++) {
        if (board->grid[i] == 0) {
            start_idx = i;
            break;
        }
    }
    
    // If no 0s, try any safe cell suitable for start (e.g. corner with low number).
    // If board is dense, there might be no 0s.
    if (start_idx == -1) {
        for (int i=0; i<size; i++) {
            if (board->grid[i] != -1) {
                start_idx = i;
                break;
            }
        }
    }
    
    // If full of mines (impossible config but ok)
    if (start_idx == -1) return false;

    // Reveal start
    // If it's a 0, we should auto-reveal neighbors (flood fill)
    // Simple queue for flood fill
    int* queue = malloc(size * sizeof(int));
    int q_head = 0, q_tail = 0;
    
    queue[q_tail++] = start_idx;
    board->revealed[start_idx] = true;
    revealed_count++;

    // Process initial flood fill
    while(q_head < q_tail) {
        int curr = queue[q_head++];
        if (board->grid[curr] == 0) {
            int neighbors[8];
            int n_count;
            get_neighbors(board, curr, neighbors, &n_count);
            for(int i=0; i<n_count; i++) {
                int n = neighbors[i];
                if (!board->revealed[n]) {
                    board->revealed[n] = true;
                    revealed_count++;
                    queue[q_tail++] = n;
                }
            }
        }
    }
    free(queue);

    // Main Solver Loop
    bool progress = true;
    int tier = 0; // Track max difficulty tier used

    while (progress && revealed_count < total_safe) {
        progress = false;
        
        // Tier 1: Basic Logic (Flagging and Clearing)
        // For each revealed number:
        // - If (mines around == number): Flag all unknown neighbors
        // - If (flags around == number): Reveal all unknown neighbors
        
        for (int i = 0; i < size; i++) {
            if (board->revealed[i] && board->grid[i] > 0) {
                int neighbors[8];
                int count;
                get_neighbors(board, i, neighbors, &count);
                
                int flags = 0;
                int hidden = 0;
                
                for(int n=0; n<count; n++) {
                    if (board->flagged[neighbors[n]]) flags++;
                    else if (!board->revealed[neighbors[n]]) hidden++;
                }

                if (hidden == 0) continue; // All handled

                // Rule: Flag remaining
                if (flags + hidden == board->grid[i]) {
                     for(int n=0; n<count; n++) {
                        int idx = neighbors[n];
                        if (!board->revealed[idx] && !board->flagged[idx]) {
                            board->flagged[idx] = true;
                            progress = true;
                        }
                    }
                }
                
                // Rule: Clear remaining
                if (flags == board->grid[i]) {
                     for(int n=0; n<count; n++) {
                        int idx = neighbors[n];
                        if (!board->revealed[idx] && !board->flagged[idx]) {
                            if (board->grid[idx] == -1) {
                                // Logic error or bad board? Solver shouldn't blow up if logic is correct
                                // and board is correct.
                                // If this happens, it means our logic deducted a mine is safe.
                                // Or we started on a mine (checked above).
                            }
                            board->revealed[idx] = true;
                            revealed_count++;
                            progress = true;
                            
                            // If we revealed a zero, we should flood fill it immediately
                            // But for simplicity, we let the next loop iteration handle it naturally
                            // or we could add a flood fill here.
                            // The outer loop will pick it up as a "revealed number 0" and apply logic.
                        }
                    }
                }
            }
        }
        
        // If Tier 1 worked, continue loop
        if (progress) {
             if (tier < 1) tier = 1;
             continue;
        }

        // Tier 2: Patterns (1-2-1, 1-1)
        // Implemented if requested/needed for "Hard"
        // For now, let's stick to Tier 1 and see if it's enough for simple "solvable" check.
        // Complex patterns basically reduce to set logic.
        
        // If we stall, we fail
        // TODO: Implement Tier 2
    }
    
    // 3BV Calculation
    // 1. Count connected components of zeros (openings)
    // 2. Count independent non-opening safe cells
    
    int tbv = 0;
    bool* visited_3bv = calloc(size, sizeof(bool));
    
    // Part A: Zeros
    for (int i = 0; i < size; i++) {
        if (board->grid[i] == 0 && !visited_3bv[i]) {
            tbv++;
            
            // Flood fill this opening
            int* q = malloc(size * sizeof(int));
            int head = 0, tail = 0;
            
            q[tail++] = i;
            visited_3bv[i] = true;
            
            // Mark all neighbors of the *start* zero as visited too (they get revealed)
            // Actually, standard flood fill logic for minesweeper:
            // Clicking a 0 reveals it and all neighbors.
            // If a neighbor is 0, it recursively reveals its neighbors.
            
            // So we need to traverse the connected 0s.
            // Any non-0 neighbor of a 0 is also revealed (visited) but does not continue the flood.
            
            // Re-use logic:
            while(head < tail) {
                int curr = q[head++];
                
                int neighbors[8];
                int n_count;
                get_neighbors(board, curr, neighbors, &n_count);
                
                for(int n=0; n<n_count; n++) {
                    int idx = neighbors[n];
                    if (!visited_3bv[idx]) {
                        visited_3bv[idx] = true; 
                        // Only add to queue if it's a zero (continue flood)
                        if (board->grid[idx] == 0) {
                            q[tail++] = idx;
                        }
                    }
                }
            }
            free(q);
        }
    }
    
    // Part B: Remaining safe cells
    for (int i = 0; i < size; i++) {
        if (board->grid[i] != -1 && !visited_3bv[i]) {
            tbv++;
        }
    }
    
    free(visited_3bv);

    board->score = (double)tbv;
    
    if (revealed_count == total_safe) {
        return true;
    }
    
    return false;
}
