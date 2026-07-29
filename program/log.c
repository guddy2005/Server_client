#include <stdio.h>

int main()
{
    FILE *log;

    log = fopen("application.log", "a");

    if (log == NULL)
    {
        printf("Unable to open log file.\n");
        return 1;
    }

    fprintf(log, "Program Started\n");
    fprintf(log, "User Logged In\n");
    fprintf(log, "Processing Request\n");
    fprintf(log, "Program Finished\n");

    fclose(log);

    printf("Logs stored successfully.\n");

    return 0;
}
