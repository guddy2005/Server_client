#include<stdio.h>
#include<stdlib.h>

int main()
{
    int *arr;
    int i;

    arr = (int *)calloc(5,sizeof(int));

    for(i=0;i<5;i++)
    {

        printf("%d\n ",arr[i]);
    }

    free(arr);

    return 0;
}
