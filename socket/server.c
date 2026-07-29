#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <errno.h>
#include <string.h>

#define PORT 45001

int main(){

	int serverSocket;
	serverSocket = socket(AF_INET,SOCK_STREAM,0);
	struct sockaddr_in serverAddr;

	memset(&serverAddr, 0, sizeof(serverAddr));
	serverAddr.sin_family = AF_INET;
	serverAddr.sin_port =htons(PORT);
	serverAddr.sin_addr.s_addr = INADDR_ANY;




	printf("Socket FD = %d\n", serverSocket);


	if (serverSocket<0)
		{		
		printf("socket connection failed\n");
		return -1;
		}

	if(bind(serverSocket,(struct sockaddr *)&serverAddr, sizeof(serverAddr)) < 0)
		{	
    		printf("Bind Failed: %s\n", strerror(errno));
    		return -1;
		}		



	printf("Socket CeationSucessfully\n");


	if(listen(serverSocket,5) < 0)
		{		
    		printf("Listen Failed\n");
   		return -1;
		}

	int clientSocket;
	struct sockaddr_in clientAddr;

	socklen_t clientlen = sizeof(clientAddr);

	printf("waiting forrr clienttttt..........\n");

	clientSocket = accept(serverSocket,(struct sockaddr *)&clientAddr,&clientlen);

	char buffer[1024];
	if(clientSocket<0){
		printf("Accept failed\n");
		return -1;

		}	
        printf("Client Connected\n");

	recv(clientSocket,
     	buffer,
     	sizeof(buffer),
     	0);

	printf("Client Says : %s\n", buffer);
	char reply[] = "Hello Client";

	send(clientSocket,
     	reply,
     	strlen(reply)+1,
     	0);

	printf("Reply Sent\n");





	printf("Server Listening on Port %d\n",PORT);
	close(serverSocket);

	return 0;
	
	}
