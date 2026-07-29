#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/select.h>

#define PORT 45501
#define MAX_CLIENTS 4
#define BUFFER_SIZE 1024

int main()
{
    int server_fd;
    int client_socket[MAX_CLIENTS];
    int max_sd;
    int activity;
    int new_socket;
    int sd;
    int valread;
    int i;
    struct sockaddr_in server_addr;

    fd_set readfds;

    char buffer[BUFFER_SIZE];

    int addrlen = sizeof(server_addr);

    for(int i=0;i<MAX_CLIENTS;i++)
        client_socket[i]=0;
    server_fd = socket(AF_INET, SOCK_STREAM, 0);

    if(server_fd < 0)
    {
        perror("Socket Failed");
        exit(1);
    }

    int opt = 1;

    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(PORT);

    if(bind(server_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0)
    {
        perror("Bind Failed");
        exit(1);
    }

    if(listen(server_fd, 5) < 0)
    {
        perror("Listen Failed");
        exit(1);
    }

    
    printf("Server Started on Port %d\n", PORT);
    printf("Waiting for Clients...\n");
   

    while(1)
    {
        FD_ZERO(&readfds);

        FD_SET(server_fd, &readfds);

        max_sd = server_fd;

        for(int i=0;i<MAX_CLIENTS;i++)
        {
            sd = client_socket[i];

            if(sd > 0)
                FD_SET(sd, &readfds);

            if(sd > max_sd)
                max_sd = sd;
        }

        activity = select(max_sd + 1, &readfds, NULL, NULL, NULL);

        if(activity < 0)
        {
            perror("Select Error");
        }
        if(FD_ISSET(server_fd, &readfds))
        {
            new_socket = accept(server_fd,
                                (struct sockaddr *)&server_addr,
                                (socklen_t *)&addrlen);

            if(new_socket < 0)
            {
                perror("Accept Failed");
                exit(1);
            }

            printf("Client Connected\n");
            printf("Client FD : %d\n", new_socket);
            for(int i = 0; i < MAX_CLIENTS; i++)
            {
                if(client_socket[i] == 0)
                {
                    client_socket[i] = new_socket;
                    break;
                }
            }
        }

        for(int i = 0; i < MAX_CLIENTS; i++)
        {
            sd = client_socket[i];

            if(FD_ISSET(sd, &readfds))
            {
                memset(buffer, 0, BUFFER_SIZE);

                valread = recv(sd, buffer, BUFFER_SIZE, 0);

                if(valread <= 0)
                {
                    printf("Client Disconnected\n");
                    printf("Client FD : %d\n", sd);
                    close(sd);

                    client_socket[i] = 0;
                }
               else
		{
    		    printf("FD %d : %s\n", sd, buffer);

                    char reply_buffer[BUFFER_SIZE + 50];

                    sprintf(reply_buffer,"FD %d : %s",sd,buffer);
                    send(sd, reply_buffer, strlen(reply_buffer), 0);
 		}
            }
        }
    }

    close(server_fd);

    return 0;
}
