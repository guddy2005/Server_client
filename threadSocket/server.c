#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <signal.h>
#include <sys/socket.h>

#define PORT 45501
#define BUFFER_SIZE 1024

void send_all(int sd, char *buffer, int length)
{
    int total = 0;
    int bytes;

    while (total < length)
    {
        bytes = send(sd, buffer + total, length - total, 0);

        if (bytes <= 0)
            return;

        total += bytes;
    }
}

void *client_handler(void *arg)
{
    int sd = *(int *)arg;
    free(arg);

    char buffer[BUFFER_SIZE + 1];
    char reply[BUFFER_SIZE + 100];
    int valread;

    while (1)
    {
        valread = recv(sd, buffer, BUFFER_SIZE, 0);

        if (valread <= 0)
        {
            printf("Client Disconnected\n");
            printf("Client FD : %d\n", sd);
            close(sd);
            break;
        }

        buffer[valread] = '\0';

        printf("FD %d : %s\n", sd, buffer);

        int len = snprintf(reply,
                           sizeof(reply),
                           "FD %d : %s",
                           sd,
                           buffer);

        send_all(sd, reply, len);
    }

    return NULL;
}

int main()
{
    int server_fd;
    int new_socket;
    int addrlen;
    int opt = 1;

    struct sockaddr_in server_addr;

    pthread_t tid;

    signal(SIGPIPE, SIG_IGN);

    server_fd = socket(AF_INET, SOCK_STREAM, 0);

    if (server_fd < 0)
    {
        perror("socket");
        exit(EXIT_FAILURE);
    }

    setsockopt(server_fd,
               SOL_SOCKET,
               SO_REUSEADDR,
               &opt,
               sizeof(opt));

#ifdef SO_REUSEPORT
    setsockopt(server_fd,
               SOL_SOCKET,
               SO_REUSEPORT,
               &opt,
               sizeof(opt));
#endif

    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(PORT);

    if (bind(server_fd,
             (struct sockaddr *)&server_addr,
             sizeof(server_addr)) < 0)
    {
        perror("bind");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    if (listen(server_fd, 1024) < 0)
    {
        perror("listen");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    printf("Server Started...\n");

    addrlen = sizeof(server_addr);

    while (1)
    {
        new_socket = accept(server_fd,
                            (struct sockaddr *)&server_addr,
                            (socklen_t *)&addrlen);

        if (new_socket < 0)
        {
            perror("accept");
            continue;
        }

        printf("Client Connected\n");
        printf("Client FD : %d\n", new_socket);

        int *client = malloc(sizeof(int));

        if (client == NULL)
        {
            close(new_socket);
            continue;
        }

        *client = new_socket;

        if (pthread_create(&tid,
                           NULL,
                           client_handler,
                           client) != 0)
        {
            perror("pthread_create");
            close(new_socket);
            free(client);
            continue;
        }

        pthread_detach(tid);
    }

    close(server_fd);

    return 0;
}
