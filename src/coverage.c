#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "coverage.h"
// Global registry
static block_registry_entry_t registry[MAX_TRACKED_BLOCKS];
static unsigned long long total_registered_blocks = 0;

/**
 * beginning_func
 * * Called at the start of every instrumented block.
 * 1. Checks if the block name is already registered.
 * 2. If yes, increments the hit count.
 * 3. If no, assigns a new 'long long index' and registers it.
 */
void beginning_func(const char* name) {
    // 1. Try to find existing block (Linear search)
    // Note: For huge projects, a hash table would be faster, but this is sufficient for
    // typical file-level instrumentation.
    for (unsigned long long i = 0; i < total_registered_blocks; i++) {
        if (strcmp(registry[i].name, name) == 0) {
            registry[i].hit_count++;
            return;
        }
    }

    // 2. Register new block if not found
    if (total_registered_blocks < MAX_TRACKED_BLOCKS) {
        // We can safely store the pointer 'name' directly because Clang generates 
        // string literals (e.g., "if_then"), which have static storage duration.
        // If you were passing dynamic strings, you would need strdup(name).
        registry[total_registered_blocks].name = name;
        registry[total_registered_blocks].index = total_registered_blocks; // Assign the ID
        registry[total_registered_blocks].hit_count = 1;
        
        total_registered_blocks++;
    } else {
        fprintf(stderr, "[Coverage Error] Max block limit (%d) reached. Increase MAX_TRACKED_BLOCKS.\n", MAX_TRACKED_BLOCKS);
    }
}

/**
 * end_func
 * * Called at the end of blocks or before returns.
 * Currently a placeholder, but can be used for:
 * - Stack depth tracking (to ensure function enters/exits match)
 * - Timing execution of blocks
 */
void end_func(void) {
    // No-op for basic coverage
}

/**
 * print_coverage_report
 * * Automatically called when the program exits (via main return or exit()).
 * Prints the registry state.
 */
void __attribute__((destructor)) print_coverage_report() {
    printf("\n=============================================\n");
    printf("          CODE COVERAGE REPORT               \n");
    printf("=============================================\n");
    printf("%-5s | %-20s | %s\n", "ID", "Block Name", "Executions");
    printf("------+----------------------+---------------\n");

    for (unsigned long long i = 0; i < total_registered_blocks; i++) {
        printf("%-5llu | %-20s | %llu\n", 
               registry[i].index, 
               registry[i].name, 
               registry[i].hit_count);
    }
    
    printf("=============================================\n");
    printf("Total Blocks Executed: %llu\n", total_registered_blocks);
}