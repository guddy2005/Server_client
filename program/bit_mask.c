#include <stdio.h>

int main() {
    unsigned char num = 13; 
    int n = 3; 

    unsigned char mask = (1U << n);

    unsigned char set_num = num | mask;

    unsigned char clear_num = num & ~mask;

    unsigned char toggle_num = num ^ mask;

    if ((num & mask) != 0) {
        printf("Bit %d is SET (1)\n", n);
    } else {
        printf("Bit %d is CLEAR (0)\n", n);
    }

    printf("Cleared result: %u\n", clear_num);
    printf("Toggled result: %u\n", toggle_num);
    return 0;
}

