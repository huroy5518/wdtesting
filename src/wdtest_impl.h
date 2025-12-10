#ifndef __WDTEST_IMPL_H
#define __WDTEST_IMPL_H

#include "list.h"
#include "type.h"

#define container_of(ptr, type, member) \
    ((type *) ((char *) (ptr) - offsetof(type, member)))

struct test_task {
    struct ds_list_head head;
    const char* name;
    test_fp fp;
};

struct test_info {
    int line;
    char *file;
};


extern struct ds_list_head task_list;
void add_test(char *name, test_fp fp);
// void check_i8_eq(i8 a, i8 b, struct test_info);
// void check_u8_eq(u8 a, u8 b, struct test_info);
// void check_i16_eq(i16 a, i16 b, struct test_info);
// void check_u16_eq(u16 a, u16 b, struct test_info);
int check_i32_eq(i32 a, i32 b, struct test_info);
int check_u32_eq(u32 a, u32 b, struct test_info);
int check_i64_eq(i64 a, i64 b, struct test_info);
int check_u64_eq(u64 a, u64 b, struct test_info);
void init_test();
void run_test();
void cleanup_test();
void MOCK_FUNC(const char* FuncName, const char* NewName);

// TEST("This is a test", test_passing)

#endif
