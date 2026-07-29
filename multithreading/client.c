#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>

#define PORT 5000
#define BUFFER_SIZE 1024

int main()
{
    int sockfd;
    struct sockaddr_in server;

    char client_name[50];
    char message[256];
    char send_buffer[BUFFER_SIZE];
    char recv_buffer[BUFFER_SIZE];

    int count;
    int delay;
    int i;

    sockfd = socket(AF_INET, SOCK_STREAM, 0);

    if(sockfd < 0)
    {
        perror("socket");
        return 1;
    }

    memset(&server, 0, sizeof(server));

    server.sin_family = AF_INET;
    server.sin_port = htons(PORT);
    server.sin_addr.s_addr = inet_addr("127.0.0.1");

    if(connect(sockfd,
               (struct sockaddr *)&server,
               sizeof(server)) < 0)
    {
        perror("connect");
        close(sockfd);
        return 1;
    }

    printf("\nConnected Successfully\n\n");


    printf("Enter Client Name : ");
    scanf("%49s", client_name);
while(1){
    getchar();

    printf("Enter Message : ");
    fgets(message, sizeof(message), stdin);

    message[strcspn(message, "\n")] = '\0';

    printf("How many messages : ");
    scanf("%d", &count);

   
    for(i = 1; i <= count; i++)
    {
        memset(send_buffer, 0, sizeof(send_buffer));

        sprintf(send_buffer,
                "%s [%d/%d] : %s",
                client_name,
                i,
                count,
                message);

        if(send(sockfd,
                send_buffer,
                strlen(send_buffer),
                0) < 0)
        {
            perror("send");
            break;
        }

        printf("\n---------------------------------\n");
        printf("Message Sent : %s\n", send_buffer);

        memset(recv_buffer, 0, sizeof(recv_buffer));

        if(recv(sockfd,
                recv_buffer,
                sizeof(recv_buffer),
                0) <= 0)
        {
            printf("Server Disconnected\n");
            break;
        }

        printf("Server Reply : %s\n", recv_buffer);
        printf("---------------------------------\n");

       
    }
}
   
    return 0;
}
