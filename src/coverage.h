#ifndef __COVERAGE_H
#define __COVERAGE_H


#define MAX_TRACKED_BLOCKS 65536

// Structure to store block metadata
typedef struct {
    unsigned long long begin_hit_count;   // How many times this block executed
    unsigned long long end_hit_count;   // How many times this block executed
} block_registry_entry_t;

void beginning_func(int idx);
void end_func(int idx);


#endif