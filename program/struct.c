#include<stdio.h>
#include<string.h>

struct Student
{
    char name[20];
    int age;
};

int main()
{
    struct Student s[2];

    strcpy(s[0].name, "Rahul");
    s[0].age = 20;

    strcpy(s[1].name, "Aman");
    s[1].age = 22;

    printf("%s %d\n", s[0].name, s[0].age);
    printf("%s %d\n", s[1].name, s[1].age);

    return 0;
}
