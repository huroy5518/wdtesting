#ifndef __WDTEST_MARCO_H
#define __WDTEST_MARCO_H

#include "wdtest_impl.h"


#define TEST(name, fp) add_test(name, fp)
#define INIT_TEST() init_test()
#define CLEANUP_TEST() cleanup_test()
#define RUN_TEST() run_test()

#define ASSERT_i32_EQ(a, b) check_i32_eq(a, b, (struct test_info){.line=__LINE__, .file=__FILE__})
#define ASSERT_u32_EQ(a, b) check_u32_eq(a, b, (struct test_info){.line=__LINE__, .file=__FILE__})
#define ASSERT_i64_EQ(a, b) check_i64_eq(a, b, (struct test_info){.line=__LINE__, .file=__FILE__})
#define ASSERT_u64_EQ(a, b) check_u64_eq(a, b, (struct test_info){.line=__LINE__, .file=__FILE__})


#endif
