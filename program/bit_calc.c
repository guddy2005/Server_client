#include <stdio.h>

int add(int a, int b)
{
    while (b != 0)
    {
        int carry = a & b;
        a = a ^ b;
        b = carry << 1;
    }
    return a;
}

int subtract(int a, int b)
{
    while (b != 0)
    {
        int borrow = (~a) & b;
        a = a ^ b;
        b = borrow << 1;
    }
    return a;
}

int multiply(int a, int b)
{
    int result = 0;

    while (b > 0)
    {
        if (b & 1)
            result = add(result, a);

        a <<= 1;
        b >>= 1;
    }

    return result;
}

int divide(int dividend, int divisor)
{
    if (divisor == 0)
        return 0;

    int quotient = 0;

    while (dividend >= divisor)
    {
        dividend = subtract(dividend, divisor);
        quotient = add(quotient, 1);
    }

    return quotient;
}

int main()
{
    int a, b, choice;

    printf("Enter  number A: ");

    scanf("%d", &a);
    printf("Enter Number B:");
    scanf("%d",&b);

    printf("1.Add\n");
    printf("2.Subtract\n");
    printf("3.Multiply\n");
    printf("4.Divide\n");

    printf("Enter choice: ");
    scanf("%d", &choice);

    switch (choice)
    {
        case 1:
            printf("Result = %d\n", add(a, b));
            break;

        case 2:
            printf("Result = %d\n", subtract(a, b));
            break;

        case 3:
            printf("Result = %d\n", multiply(a, b));
            break;

        case 4:
            printf("Result = %d\n", divide(a, b));
            break;

        default:
            printf("Invalid Choice\n");
    }

    return 0;
}
