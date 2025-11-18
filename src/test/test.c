#include "../top.h"

int add (int a, int b) {
    return a + b;
}

int fail_add(int a, int b) {
    return a + a + b + b;
}


int check_add() {
    ASSERT_u32_EQ(add(1, 3), 4);
}

int check_fail_add() {
    ASSERT_u32_EQ(fail_add(1, 3), 4);
}


int main () {

    INIT_TEST();

    TEST("Test add", check_add);
    TEST("Test fail", check_fail_add);

    RUN_TEST();
    CLEANUP_TEST();


    return 0;
}
