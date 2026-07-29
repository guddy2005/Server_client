#include <stdio.h>

int main()
{
    unsigned int num;

    printf("Enter a decimal number: ");
    scanf("%u", &num);

    printf("Binary = ");

    for(int i = 31; i >= 0; i--)
    {
        printf("%d", (num >> i) & 1);
    }

    printf("\n");

    return 0;
}
