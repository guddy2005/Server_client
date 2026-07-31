#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/select.h>

#define PORT 18880
#define BUFFER_SIZE 1024

int main(void)
{
    int sockfd;
    struct sockaddr_in server_addr;
    fd_set read_fds;
    char send_buffer[BUFFER_SIZE];
    char recv_buffer[BUFFER_SIZE];
    int activity;
    int running = 1;

    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if(sockfd < 0)
    {
        perror("socket");
        return 1;
    }

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);
    if(inet_pton(AF_INET, "127.0.0.1", &server_addr.sin_addr) <= 0)
    {
        perror("inet_pton");
        close(sockfd);
        return 1;
    }

    if(connect(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0)
    {
        perror("connect");
        close(sockfd);
        return 1;
    }

    while(running)
    {
        FD_ZERO(&read_fds);
        FD_SET(sockfd, &read_fds);
        FD_SET(STDIN_FILENO, &read_fds);

        activity = select(sockfd + 1, &read_fds, NULL, NULL, NULL);
        if(activity < 0)
        {
            perror("select");
            break;
        }

        if(FD_ISSET(STDIN_FILENO, &read_fds))
        {
            if(fgets(send_buffer, sizeof(send_buffer), stdin) == NULL)
            {
                break;
            }

            if(strcmp(send_buffer, "quit\n") == 0 || strcmp(send_buffer, "exit\n") == 0)
            {
                running = 0;
                break;
            }

            if(send(sockfd, send_buffer, strlen(send_buffer), 0) < 0)
            {
                perror("send");
                break;
            }
        }

        if(FD_ISSET(sockfd, &read_fds))
        {
            int received = recv(sockfd, recv_buffer, sizeof(recv_buffer) - 1, 0);
            if(received <= 0)
            {
                break;
            }
            recv_buffer[received] = '\0';
            fputs(recv_buffer, stdout);
            fflush(stdout);
        }
    }

    close(sockfd);
    return 0;
}
