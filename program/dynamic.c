#include<stdio.h>
#include<stdlib.h>

int main()
{
    int *arr;
    int i;

    arr = (int *)malloc(5 * sizeof(int));

    for(i = 0; i < 5; i++)
    {
        arr[i] = (i + 1) * 10;
    }

    for(i = 0; i < 5; i++)
    {
        printf("%d ", arr[i]);
    }

    free(arr);

    return 0;
}


