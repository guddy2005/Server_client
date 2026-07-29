#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <string.h>


#define PORT 45001

int main(){
int clientSocket;

struct sockaddr_in serverAddr;

clientSocket = socket(AF_INET,SOCK_STREAM,0);

if (clientSocket <0){
printf("Socket creation Failed\n");
return -1;
}
 printf("client socket FD =%d\n",clientSocket);

memset(&serverAddr, 0, sizeof(serverAddr));

    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(PORT);

    inet_pton(AF_INET, "127.0.0.1", &serverAddr.sin_addr);

    if(connect(clientSocket,
               (struct sockaddr *)&serverAddr,
               sizeof(serverAddr)) < 0)
    {
        printf("Connection Failed\n");
        return -1;
    }

   printf("Connected To Server Successfully\n");

char message[] = "Hello Server";

send(clientSocket,
     message,
     strlen(message)+1,
     0);

printf("Message Sent\n");

char buffer[1024];

recv(clientSocket,
     buffer,
     sizeof(buffer),
     0);

printf("Server Says : %s\n", buffer);

close(clientSocket);

return 0;
}






