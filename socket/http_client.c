#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <string.h>

#define PORT 45001

int main()
{
    int sockfd;

    struct sockaddr_in serverAddr;

    sockfd = socket(AF_INET, SOCK_STREAM, 0);

    if(sockfd < 0)
    {
        printf("Socket Creation Failed\n");
        return -1;
    }

    memset(&serverAddr, 0, sizeof(serverAddr));

    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(PORT);

    inet_pton(AF_INET,
              "127.0.0.1",
              &serverAddr.sin_addr);

    if(connect(sockfd,
               (struct sockaddr *)&serverAddr,
               sizeof(serverAddr)) < 0)
    {
        printf("Connection Failed\n");
        return -1;
    }

    printf("Connected To HTTP Server\n");

    return 0;
}
