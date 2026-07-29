#include<stdio.h>

void second()
{
    printf("Inside second()\n");
}

void first()
{
    printf("Inside first() - Before second()\n");

    second();

    printf("Inside first() - After second()\n");
}

int main()
{
    printf("Inside main() - Before first()\n");

    first();

    printf("Inside main() - After first()\n");

    return 0;
}

