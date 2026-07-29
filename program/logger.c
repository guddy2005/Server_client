#include <stdio.h>
#include <time.h>

void log_message(const char *level, const char *message)
{
    FILE *fp;
    time_t current_time;
    struct tm *time_info;
    char timestamp[30];

    /* Get Current Time */
    current_time = time(NULL);

    /* Convert into local time */
    time_info = localtime(&current_time);

    /* Format Time */
    strftime(timestamp,
             sizeof(timestamp),
             "%Y-%m-%d %H:%M:%S",
             time_info);

    /* Open Log File */
    fp = fopen("application.log", "a");

    if(fp == NULL)
    {
        printf("Unable to open log file\n");
        return;
    }

    /* Terminal Output */
    printf("[%s] [%s] %s\n",
           timestamp,
           level,
           message);

    /* File Output */
    fprintf(fp,
            "[%s] [%s] %s\n",
            timestamp,
            level,
            message);

    fclose(fp);
}

int main()
{
    log_message("INFO", "Server Started");

    log_message("INFO", "Client Connected");

    log_message("DEBUG", "Packet Received");

    log_message("WARNING", "Memory Usage High");

    log_message("ERROR", "Socket Connection Failed");

    return 0;
}
