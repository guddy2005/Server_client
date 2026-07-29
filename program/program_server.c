#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <unistd.h>

#include <arpa/inet.h>

#include <sys/socket.h>

#define PORT 45503

#define BUFFER_SIZE 1024

int main(){
	int server_fd;
	int client_fd;
	char buffer[BUFFER_SIZE];
	struct sockaddr_in server_addr;
	struct sockaddr_in client_addr;
	
	socklen_t client_len;

	server_fd=socket(AF_INET,SOCK_STREAM,0);


	if (server_fd <0){
		printf("socket creaton failed\n");
		exit(1);
	}
	printf("socket created sucessfully\n");
	server_addr.sin_family=AF_INET;
	server_addr.sin_port=htons(PORT);
	server_addr.sin_addr.s_addr=INADDR_ANY;
	if (bind(server_fd,
         (struct sockaddr *)&server_addr,
         sizeof(server_addr)) < 0)
	{
   		 printf("Bind failed\n");
    		 exit(1);
	}

	printf("Bind successful\n");
	if (listen(server_fd, 5) < 0)
	{	
    	printf("Listen failed\n");
    	exit(1);
	}

	printf("Waiting for client...\n");
	
	client_len = sizeof(client_addr);

	client_fd = accept(server_fd,
                   (struct sockaddr *)&client_addr,
                   &client_len);

	if (client_fd < 0)
	{
    		printf("Accept failed\n");
    		exit(1);
	}	

	printf("Client Connected\n");






	int bytes;

	bytes = recv(client_fd, buffer, BUFFER_SIZE, 0);

	if (bytes < 0)
	{
	    printf("Receive Failed\n");
	}
	else
	{
	    buffer[bytes] = '\0';

	    printf("Client : %s\n", buffer);
	
	}
char message[] = "Hello Client";

send(client_fd,
     message,
     strlen(message),
     0);
close(client_fd);

close(server_fd);	


return 0;
}



