#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>

#define PORT 45501
#define BUFFER_SIZE 1024

int main()
{
    int client_fd;
    int ret;
    int total_messages;
    char buffer[BUFFER_SIZE];
    char custom_message[BUFFER_SIZE];
    struct sockaddr_in server_addr;

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);
    server_addr.sin_addr.s_addr = inet_addr("127.0.0.1");

   
        client_fd = socket(AF_INET, SOCK_STREAM, 0);

        if(client_fd < 0)
        {
            perror("Socket Failed");
            sleep(2);
           
        }

        printf("\nTrying to connect...\n");

        ret = connect(client_fd,
                     (struct sockaddr *)&server_addr,
                     sizeof(server_addr));

        if(ret < 0)
        {
            printf("Server Not Available...\n");
            close(client_fd);
            sleep(2);
           
        }

        printf("Connected to Server\n");
 
       while(1){
        printf("Enter Message: ");
        memset(custom_message, 0, BUFFER_SIZE);
        if (fgets(custom_message, BUFFER_SIZE, stdin) == NULL) {
            close(client_fd);
            break;
        }
        custom_message[strcspn(custom_message, "\n")] = 0;

        printf("Enter Count: ");
        if (scanf("%d", &total_messages) <= 0) {
            total_messages = 0;
        }
        
        while (getchar() != '\n'); 

       
        for(int m = 1; m <= total_messages; m++) {
          
            fflush(stdout);

            memset(buffer, 0, BUFFER_SIZE);
            sprintf(buffer, "%s [Count: #%d]\n", custom_message, m);

            if(send(client_fd, buffer, strlen(buffer), 0) <= 0) {
                printf("\nServer Disconnected during test.\n");
                break;
            }

            memset(buffer, 0, BUFFER_SIZE);
            if(recv(client_fd, buffer, BUFFER_SIZE, 0) <= 0) {
                printf("\nServer Disconnected while receiving reply.\n");
                break;
            }
            printf("\nServer Reply: %s\n", buffer);
        }
        
       
    }

    return 0;
}	
