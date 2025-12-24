#include "wdtest_impl.h"
#include "coverage.h"

#ifdef __KERNEL__

#include <linux/slab.h>
#include <linux/types.h>
#include <linux/stddef.h>
#define malloc(size) kmalloc(size, GFP_KERNEL)

#endif
#include <linux/module.h>
#include "top.h"
MODULE_LICENSE("GPL");

struct ds_list_head task_list;
extern void TEST_CASES(void);

void MOCK_FUNC(const char* FuncName, const char* NewName) {}

void init_test() {
    ds_list_head_init(&task_list);
    TEST_CASES();
}

void print_failed(struct test_info *info) {
    pr_err("\t[Test Failed!], file=%s, line=%d\n", info->file, info->line);
}

int check_i32_eq(s32 a, s32 b, struct test_info info) {
    if (a == b) {
        return 0;
    }

    if (a != b) {
        print_failed(&info);
        return -1;
    }
    return -1;
}

int check_u32_eq(u32 a, u32 b, struct test_info info) {
    if (a == b) {
        return 0;
    }

    if (a != b) {
        print_failed(&info);
        return -1;
    }
    
    return -1;
}

int check_i64_eq(s64 a, s64 b, struct test_info info) {
    if (a == b) {
        return 0;
    }

    if (a != b) {
        print_failed(&info);
        return -1;
    }
    
    return -1;
}

int check_u64_eq(u64 a, u64 b, struct test_info info) {
    if (a == b) {
        return 0;
    }

    if (a != b) {
        print_failed(&info);
        return -1;
    }
    
    return -1;
}

int check_mem_eq(void *a, void *b, int size, struct test_info info) {
    if (memcmp(a, b, size) == 0) {
        return 0;
    }

    return -1;
}

void init_test_task(char *name, struct test_task *new_task, test_fp fp) {
    ds_list_head_init(&(new_task->head));
    new_task->fp = fp;
    new_task->name = name;
    ds_list_addnext(&task_list, &(new_task->head));
}

void add_test(char *name, test_fp fp) {
    
    struct test_task *new_task = malloc(sizeof(struct test_task));
    init_test_task(name, new_task, fp);
}


void run_test() {
    clean_registry();
    struct ds_list_head *cur = task_list.next;
    while (cur != &task_list) {
        struct test_task *cur_task = container_of(cur, struct test_task, head);
        pr_warn("[%s] Running Task\n", cur_task->name);
        int cur_state = cur_task->fp();

        cur = cur->next;
    }
}


void cleanup_test() {
    
}
