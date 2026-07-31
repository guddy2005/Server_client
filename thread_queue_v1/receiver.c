

#include<stdio.h>
#include<stdlib.h>
#include<unistd.h>
#include<errno.h>
#include<string.h>
#include<sys/types.h>
#include<sys/socket.h>
#include<netinet/in.h>
#include<arpa/inet.h>
#include<netdb.h>



#define PORT "18801"
#define BUFFER_SIZE 1024


void *get_in_addr(struct sockaddr *sa)
{
	if(sa->sa_family == AF_INET)
	    return & (((struct sockaddr_in *)sa) ->sin_addr);
	
	return & (((struct sockaddr_in6 *)sa) ->sin6_addr);
}

int main (void)
{

	int sockfd;
	struct addrinfo hints,*servinfo, *p;
	int rv;
	int numbytes;
	struct sockaddr_storage their_addr;
	char buf[BUFFER_SIZE];
	socklen_t addr_len;
	char s[INET6_ADDRSTRLEN];

	memset(&hints,0, sizeof(hints));
	hints.ai_family=AF_INET6;
	hints.ai_socktype =SOCK_DGRAM;
	hints.ai_flags=AI_PASSIVE;

        rv=getaddrinfo(NULL,PORT,&hints,&servinfo);
        if(rv !=0)
	{
	   fprintf(stderr, "getaddrinfo:%s\n",gai_strerror(rv));
	   return 1;
	}
    for (p = servinfo; p != NULL; p = p->ai_next)
    {
        sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
        if (sockfd == -1)
        {
            perror("socket");
            continue;
        }

        if (bind(sockfd, p->ai_addr, p->ai_addrlen) == -1)
        {
            close(sockfd);
            perror("bind");
            continue;
        }

        break;
    }

    if (p == NULL)
    {
        fprintf(stderr, "Failed to bind\n");
        freeaddrinfo(servinfo);
        return 2;
    }

    freeaddrinfo(servinfo);

    printf("Waiting for data...\n");

    addr_len = sizeof(their_addr);

    numbytes = recvfrom(sockfd,
                        buf,
                        BUFFER_SIZE - 1,
                        0,
                        (struct sockaddr *)&their_addr,
                        &addr_len);


    if (numbytes == -1)
    {
        perror("recvfrom");
        close(sockfd);
        return 1;
    }

    buf[numbytes] = '\0';

    printf("Received from %s\n",
           inet_ntop(their_addr.ss_family,
                     get_in_addr((struct sockaddr *)&their_addr),
                     s,
                     sizeof(s)));

    printf("Bytes: %d\n", numbytes);
    printf("Message: %s\n", buf);

    close(sockfd);

    return 0;
}







