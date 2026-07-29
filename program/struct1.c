#include<stdio.h>

struct Test
{
    char a;
    int b;
};

int main()
{
    printf("%zu\n", sizeof(struct Test));

    return 0;
}
