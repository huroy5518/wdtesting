#include <linux/module.h>
#include "example.h"
static int example_add(int a, int b) {
    return a + b;
}
static int example_wrong_add(int a, int b) {
    return a + b + b;
}

static int example_if(void) {
    int a = 10;
    
    a = example_wrong_add(10, 10);

    if (a * 2 < 18) {
        a = 5;
    } 
    else {
        a = 3;
        
        if (a > 5) {
            a = 4;
        } else {
            a = 100;
        }
    }
    
    return a;
}

MODULE_LICENSE("GPL");