#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <time.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/select.h>
#include <poll.h>
#include <sys/epoll.h>

#define PORT 5000
#define MAX_CLIENTS 10
#define BUFFER_SIZE 1024

void run_select_server(int);
void run_poll_server(int);
void run_epoll_server(int);

void timestamp()
{
    struct timespec ts;
    struct tm *tm_info;
    char buf[64];

    clock_gettime(CLOCK_REALTIME, &ts);

    tm_info = localtime(&ts.tv_sec);

    strftime(buf,sizeof(buf),"%H:%M:%S",tm_info);

    printf("[%s.%03ld] ",buf,ts.tv_nsec/1000000);
}

int create_server()
{
    int server_fd;
    struct sockaddr_in server;

    server_fd = socket(AF_INET,SOCK_STREAM,0);

    if(server_fd<0)
    {
        perror("socket");
        exit(1);
    }

    int opt=1;

    setsockopt(server_fd,
               SOL_SOCKET,
               SO_REUSEADDR,
               &opt,
               sizeof(opt));

    memset(&server,0,sizeof(server));

    server.sin_family=AF_INET;
    server.sin_addr.s_addr=INADDR_ANY;
    server.sin_port=htons(PORT);

    if(bind(server_fd,
            (struct sockaddr*)&server,
            sizeof(server))<0)
    {
        perror("bind");
        exit(1);
    }

    if(listen(server_fd,10)<0)
    {
        perror("listen");
        exit(1);
    }

    timestamp();
    printf("Server Started on Port %d\n",PORT);

    return server_fd;
}

int main()
{
    int choice;
    int server_fd;

    printf("\n");
    printf("=====================================\n");
    printf(" Event Multiplexing Demo\n");
    printf("=====================================\n");
    printf("1. select()\n");
    printf("2. poll()\n");
    printf("3. epoll()\n");
    printf("Choice : ");

    scanf("%d",&choice);

    server_fd=create_server();

    switch(choice)
    {
        case 1:
            run_select_server(server_fd);
            break;

        case 2:
            run_poll_server(server_fd);
            break;

        case 3:
            run_epoll_server(server_fd);
            break;

        default:
            printf("Invalid Choice\n");
    }

    close(server_fd);

    return 0;
}
void run_select_server(int server_fd)
{
    fd_set master;
    fd_set readfds;

    int fdmax;
    int newfd;
    int i;
    int nbytes;

    struct sockaddr_in client_addr;
    socklen_t addrlen;

    char buffer[BUFFER_SIZE];

    FD_ZERO(&master);
    FD_ZERO(&readfds);

    FD_SET(server_fd,&master);

    fdmax=server_fd;

    timestamp();
    printf("SELECT Server Running...\n");

    while(1)
    {
        readfds=master;

        if(select(fdmax+1,&readfds,NULL,NULL,NULL)==-1)
        {
            perror("select");
            exit(1);
        }

        for(i=0;i<=fdmax;i++)
        {
            if(FD_ISSET(i,&readfds))
            {
                if(i==server_fd)
                {
                    addrlen=sizeof(client_addr);

                    newfd=accept(server_fd,
                                 (struct sockaddr *)&client_addr,
                                 &addrlen);

                    if(newfd==-1)
                    {
                        perror("accept");
                        continue;
                    }

                    FD_SET(newfd,&master);

                    if(newfd>fdmax)
                        fdmax=newfd;

                    timestamp();
                    printf("Client Connected\n");
                    printf("FD : %d\n",newfd);
                    printf("IP : %s\n",
                           inet_ntoa(client_addr.sin_addr));
                    printf("\n");
                }
                else
                {
                    memset(buffer,0,sizeof(buffer));

                    nbytes=recv(i,
                                buffer,
                                sizeof(buffer),
                                0);

                    if(nbytes<=0)
                    {
                        timestamp();
                        printf("Client Disconnected FD : %d\n\n",i);

                        close(i);

                        FD_CLR(i,&master);
                    }
                    else
                    {
                        timestamp();
                        printf("Ready Socket : %d\n",i);

                        printf("Received : %s\n\n",
                               buffer);

                        send(i,
                             buffer,
                             strlen(buffer),
                             0);
                    }
                }
            }
        }
    }
}
void run_poll_server(int server_fd)
{
    struct pollfd fds[MAX_CLIENTS + 1];
    struct sockaddr_in client_addr;

    socklen_t addrlen;

    int nfds = 1;
    int newfd;
    int i;
    int nbytes;

    char buffer[BUFFER_SIZE];

    fds[0].fd = server_fd;
    fds[0].events = POLLIN;

    timestamp();
    printf("POLL Server Running...\n");

    while(1)
    {
        if(poll(fds, nfds, -1) < 0)
        {
            perror("poll");
            exit(1);
        }

        if(fds[0].revents & POLLIN)
        {
            addrlen = sizeof(client_addr);

            newfd = accept(server_fd,
                           (struct sockaddr *)&client_addr,
                           &addrlen);

            if(newfd >= 0)
            {
                fds[nfds].fd = newfd;
                fds[nfds].events = POLLIN;
                nfds++;

                timestamp();
                printf("Client Connected\n");
                printf("FD : %d\n", newfd);
                printf("IP : %s\n\n",
                       inet_ntoa(client_addr.sin_addr));
            }
        }

        for(i = 1; i < nfds; i++)
        {
            if(fds[i].revents & POLLIN)
            {
                memset(buffer, 0, sizeof(buffer));

                nbytes = recv(fds[i].fd,
                              buffer,
                              sizeof(buffer),
                              0);

                if(nbytes <= 0)
                {
                    timestamp();
                    printf("Client Disconnected FD : %d\n\n",
                           fds[i].fd);

                    close(fds[i].fd);

                    fds[i] = fds[nfds - 1];
                    nfds--;
                    i--;
                }
                else
                {
                    timestamp();
                    printf("Ready Socket : %d\n",
                           fds[i].fd);

                    printf("Received : %s\n\n",
                           buffer);

                    send(fds[i].fd,
                         buffer,
                         strlen(buffer),
                         0);
                }
            }
        }
    }
}
void run_epoll_server(int server_fd)
{
    int epfd;
    struct epoll_event ev;
    struct epoll_event events[MAX_CLIENTS + 1];

    struct sockaddr_in client_addr;
    socklen_t addrlen;

    char buffer[BUFFER_SIZE];

    int newfd;
    int nfds;
    int i;
    int nbytes;

    epfd = epoll_create1(0);

    if(epfd == -1)
    {
        perror("epoll_create1");
        exit(1);
    }

    ev.events = EPOLLIN;
    ev.data.fd = server_fd;

    if(epoll_ctl(epfd,
                 EPOLL_CTL_ADD,
                 server_fd,
                 &ev) == -1)
    {
        perror("epoll_ctl");
        exit(1);
    }

    timestamp();
    printf("EPOLL Server Running...\n");

    while(1)
    {
        nfds = epoll_wait(epfd,
                          events,
                          MAX_CLIENTS + 1,
                          -1);

        if(nfds == -1)
        {
            perror("epoll_wait");
            exit(1);
        }

        for(i = 0; i < nfds; i++)
        {
            if(events[i].data.fd == server_fd)
            {
                addrlen = sizeof(client_addr);

                newfd = accept(server_fd,
                               (struct sockaddr *)&client_addr,
                               &addrlen);

                if(newfd == -1)
                    continue;

                ev.events = EPOLLIN;
                ev.data.fd = newfd;

                epoll_ctl(epfd,
                          EPOLL_CTL_ADD,
                          newfd,
                          &ev);

                timestamp();
                printf("Client Connected\n");
                printf("FD : %d\n", newfd);
                printf("IP : %s\n\n",
                       inet_ntoa(client_addr.sin_addr));
            }
            else
            {
                memset(buffer, 0, sizeof(buffer));

                nbytes = recv(events[i].data.fd,
                              buffer,
                              sizeof(buffer),
                              0);

                if(nbytes <= 0)
                {
                    timestamp();

                    printf("Client Disconnected FD : %d\n\n",
                           events[i].data.fd);

                    epoll_ctl(epfd,
                              EPOLL_CTL_DEL,
                              events[i].data.fd,
                              NULL);

                    close(events[i].data.fd);
                }
                else
                {
                    timestamp();

                    printf("Ready Socket : %d\n",
                           events[i].data.fd);

                    printf("Received : %s\n\n",
                           buffer);

                    send(events[i].data.fd,
                         buffer,
                         strlen(buffer),
                         0);
                }
            }
        }
    }

    close(epfd);
}
