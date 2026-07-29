//#include<stdio.h>

//int main()
//{
    //int arr[5]={10,20,30,40,50};

   // printf("%d\n",arr[0]);
   // printf("%d\n",arr[1]);
  //  printf("%d\n",arr[2]);

  //  return 0;
//}
//
#include<stdio.h>

int main()
{
    int arr[5]={10,20,30,40,50};

    printf("%p\n",(void *)arr);
    printf("%p\n",(void *)&arr[0]);

    return 0;
}
