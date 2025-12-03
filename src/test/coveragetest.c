#include "../coverage.h"
#include <stdio.h>

int test1() {
    beginning_func("test1");
    printf("In test1\n");
    end_func();
}

int test2() {
    beginning_func("test2");
    int x = 10;
    if (x < 5) {
        beginning_func("if_1");
        printf("In test2\n");
        end_func();
    }
    end_func();
}

int main() {
    
    test1();
    test2();

    return 0;
}