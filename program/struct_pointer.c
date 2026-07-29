#include <stdio.h>
#include <string.h>

typedef struct Student
{
    char name[20];
    int age;
} Student;

int main()
{
    Student s1;

    strcpy(s1.name, "Rahul");
    s1.age = 20;

    printf("%s\n", s1.name);
    printf("%d\n", s1.age);

    return 0;
}
