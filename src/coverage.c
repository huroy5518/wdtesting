#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "coverage.h"
// Global registry
// static block_registry_entry_t registry[MAX_TRACKED_BLOCKS];
// static unsigned long long total_registered_blocks = 0;

static block_registry_entry_t registry[MAX_TRACKED_BLOCKS];

/**
 * beginning_func
 * * Called at the start of every instrumented block.
 * 1. Checks if the block name is already registered.
 * 2. If yes, increments the hit count.
 * 3. If no, assigns a new 'long long index' and registers it.
 */
void beginning_func(int idx) {
    // 1. Try to find existing block (Linear search)
    registry[idx].begin_hit_count ++;
}

/**
 * end_func
 * * Called at the end of blocks or before returns.
 * Currently a placeholder, but can be used for:
 * - Stack depth tracking (to ensure function enters/exits match)
 * - Timing execution of blocks
 */
void end_func(int idx) {
    // No-op for basic coverage
    registry[idx].end_hit_count ++;
}

/**
 * print_coverage_report
 * * Automatically called when the program exits (via main return or exit()).
 * Prints the registry state.
 */
void __attribute__((destructor)) print_coverage_report() {
    for (int i = 0; i < MAX_TRACKED_BLOCKS; i ++) {
        printf("Block %d: Begin Hit Count = %llu, End Hit Count = %llu\n", 
            i, 
            registry[i].begin_hit_count,
            registry[i].end_hit_count
        );
    }
}