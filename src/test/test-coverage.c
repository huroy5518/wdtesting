#include "../coverage.h"
#include <stdio.h>

int test1() {
    printf("In test1\n");
}

int test2() {
    int x = 10;
    if (x < 5) {
        printf("In test2\n");
    }
}

int main() {
    
    test1();
    test2();

    return 0;
}