#include <stdio.h>

int main(){
int x =50;

int *ptr;
ptr =&x;

printf ("%d\n",x);
printf("%p\n",(void *)ptr);
printf("%d\n",*ptr);
 return 0;

}
