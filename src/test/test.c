#include "../top.h"


int add (int a, int b) {
    return a + b;
}

int fail_add(int a, int b) {
    return a + a + b + b;
}

void check_while() {
    int x = 5;
    while(x --) {
    }
}

void check_if() {
    int x = 5;
    if (x == 1) {
        return;
    } 
    else if (x == 2) {
        return;
    }
    else {
        return;
    }
}


int check_add() {
    ASSERT_u32_EQ(add(1, 3), 4);
    return 0;
}

int check_fail_add() {
    ASSERT_u32_EQ(fail_add(1, 3), 4);
    return 0;
}

int new_abcd (int a, int b) {
    return a + b + 1;
}

int main () {

    INIT_TEST();
    
    MOCK_FUNC("abcd", new_abcd);

    TEST("Test add", check_add);
    TEST("Test fail", check_fail_add);

    RUN_TEST();
    CLEANUP_TEST();


    return 0;
}
