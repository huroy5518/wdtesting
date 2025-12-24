#include <linux/module.h>
#include "top.h"
MODULE_LICENSE("GPL");
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
void end_func(int idx) {
    registry[idx].end_hit_count ++;
}

int _wd_get_begin_blk_count(int idx) {
    return registry[idx].begin_hit_count;
}

int _wd_get_end_blk_count(int idx) {
    return registry[idx].end_hit_count;
}

void clean_registry(void) {
    for (int i = 0; i < MAX_TRACKED_BLOCKS; i ++) {
        registry[i].end_hit_count = 0;
        registry[i].begin_hit_count = 0;
    }
}
