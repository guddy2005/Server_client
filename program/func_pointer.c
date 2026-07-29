#include <stdio.h>

int add(int a,int b)
{
    return a+b;
}

int sub(int a,int b)
{
    return a-b;
}

int main()
{
    int (*fp)(int,int);

    fp = add;

    printf("%d\n", fp(20,10));

    fp = sub;

    printf("%d\n", fp(20,10));

    return 0;
}
