#include<stdio.h>
#include<stdlib.h>
#include<unistd.h>
#include<errno.h>
#include<arpa/inet.h>
#include<sys/types.h>
#include<netinet/in.h>
#include<sys/socket.h>
#include<string.h>
#include<netdb.h>

#define PORT "18801"

int main(int argc,char *argv[])
{
	int sockfd;
	struct addrinfo hints,*servinfo,*p;
	int rv;
	int numbytes;
	
	if(argc !=3)
	{
	   fprintf(stderr,"",argv[0]);
	}

	memset(&hints,0, sizeof(hints));
	hints.ai_family=AF_INET6;
	hints.ai_socktype = SOCK_DGRAM;
	
	rv = getaddrinfo(argv[1],PORT,&hints,&servinfo);
	if(rv !=0)
	{
	   fprintf(stderr,"getaddrinfo:%s\n", gai_strerror(rv));
	   return 1;
	}
	
	for(p=servinfo;p!=NULL; p=p->ai_next)
	{
		sockfd =socket(p->ai_family,p->ai_socktype, p->ai_protocol);
		if(sockfd ==-1)
		    {
		     perror("socket");
		     continue;
		     }
		break;
	 }
	 if(p == NULL)
	 {
	     fprintf(stderr,"failed to create socket\n");
	     freeaddrinfo(servinfo);
	     return 2;
	  }
	
    numbytes = sendto(sockfd,
                      argv[2],
                      strlen(argv[2]),
                      0,
                      p->ai_addr,
                      p->ai_addrlen);

    if (numbytes == -1)
    {
        perror("sendto");
        close(sockfd);
        freeaddrinfo(servinfo);
        return 1;
    }

    printf("Sent %d bytes to %s\n", numbytes, argv[1]);

    freeaddrinfo(servinfo);
    close(sockfd);

    return 0;
}











	



