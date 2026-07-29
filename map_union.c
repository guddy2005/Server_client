#include <stdio.h>
#include <string.h>

typedef struct
{
    char imsi[20];
} UpdateLocation;

typedef struct
{
    char sms[160];
} ForwardSM;

typedef union
{
    UpdateLocation ul;
    ForwardSM sm;

} MAP_DATA;

int main()
{
    MAP_DATA data;

    strcpy(data.sm.sms, "Hello MAP");

    printf("%s\n", data.sm.sms);

    return 0;
}
