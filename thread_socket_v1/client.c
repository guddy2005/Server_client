#include <stdio.h>
#include <string.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>

#define PORT 45501
#define BUFFER_SIZE 1024
#define RECONNECT_INTERVAL_SEC 2

int send_all(int sd, char *buffer, int length)
{
    int total = 0;
    int bytes;

    while(total < length)
    {
        bytes = send(sd, buffer + total, length - total, 0);

        if(bytes <= 0)
            return -1;

        total += bytes;
    }

    return total;
}

int connect_to_server(struct sockaddr_in *server_addr)
{
    int client_fd;

    while(1)
    {
        client_fd = socket(AF_INET, SOCK_STREAM, 0);
        if(client_fd < 0)
        {
            sleep(RECONNECT_INTERVAL_SEC);
            continue;
        }

        if(connect(client_fd, (struct sockaddr *)server_addr, sizeof(*server_addr)) == 0)
        {
            printf("Connected to Server\n");
            return client_fd;
        }

        close(client_fd);
        printf("Server not reachable. Retrying in %d seconds...\n", RECONNECT_INTERVAL_SEC);
        sleep(RECONNECT_INTERVAL_SEC);
    }
}

int main()
{
    int client_fd;
    int total_messages;
    int bytes;

    char buffer[BUFFER_SIZE];
    char custom_message[BUFFER_SIZE];

    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);
    server_addr.sin_addr.s_addr = inet_addr("127.0.0.1");

    client_fd = connect_to_server(&server_addr);

    while(1)
    {
        printf("\nEnter Message : ");

        if(fgets(custom_message, BUFFER_SIZE, stdin) == NULL)
            break;

        custom_message[strcspn(custom_message, "\n")] = '\0';

        printf("Enter Count : ");

        if(scanf("%d", &total_messages) != 1)
        {
            while(getchar() != '\n');
            continue;
        }
        while(getchar() != '\n');

        for(int i = 1; i <= total_messages; i++)
        {
            snprintf(buffer, sizeof(buffer), "%s [Count: #%d]", custom_message, i);

            if(send_all(client_fd, buffer, strlen(buffer)) < 0)
            {
                printf("\nServer Disconnected during send. Reconnecting...\n");
                close(client_fd);
                client_fd = connect_to_server(&server_addr);
                break;
            }

            bytes = recv(client_fd, buffer, BUFFER_SIZE - 1, 0);

            if(bytes <= 0)
            {
                printf("\nServer Disconnected during receive. Reconnecting...\n");
                close(client_fd);
                client_fd = connect_to_server(&server_addr);
                break;
            }

            buffer[bytes] = '\0';
            printf("Server Reply : %s\n", buffer);
        }
    }

    close(client_fd);
    return 0;
}
