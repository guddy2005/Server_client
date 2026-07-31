#include <stdio.h>

#include <stdlib.h>

#include <string.h>

#include <unistd.h>

#include <pthread.h>

#include <sys/socket.h>

#include <arpa/inet.h>



#define SERVER_IP "127.0.0.1" 

#define PORT 19900

#define BUFFER_SIZE 2048



void *receive_handler(void *arg) {

    int sock_fd = *((int *)arg);

    char buffer[BUFFER_SIZE];

    while (1) {

        memset(buffer, 0, BUFFER_SIZE);

        int receive = recv(sock_fd, buffer, BUFFER_SIZE - 1, 0);

        if (receive > 0) {

            printf("%s", buffer);

            fflush(stdout);

        } else if (receive == 0) {

            printf("\n\x1b[31mServer closed the connection.\x1b[0m\n");

            break;

        } else {

            break;

        }

    }

    pthread_exit(NULL);

}



int main() {

    int sock_fd;

    struct sockaddr_in server_addr;

    char name[50];

    char choice[10];

    char group_choice[10] = "0";

    char send_buffer[BUFFER_SIZE];

    char handshake_payload[100];



    printf("Enter username: ");

    fgets(name, 50, stdin);

    name[strcspn(name, "\n")] = 0;



    

    printf("\nWhat would you like to do?\n");

    printf("1. Create a new group room\n");

    printf("2. Join an existing group room\n");

    printf("Enter choice (1 or 2): ");

    fgets(choice, 10, stdin);

    choice[strcspn(choice, "\n")] = 0;



    if (strcmp(choice, "1") == 0) {

        

        strcpy(group_choice, "0"); 

    } else {

        printf("Enter the Group ID you want to join: ");

        fgets(group_choice, 10, stdin);

        group_choice[strcspn(group_choice, "\n")] = 0;

    }



    sock_fd = socket(AF_INET, SOCK_STREAM, 0);

    server_addr.sin_family = AF_INET;

    server_addr.sin_port = htons(PORT);

    inet_pton(AF_INET, SERVER_IP, &server_addr.sin_addr);



    if (connect(sock_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {

        perror("Connection failed");

        return 1;

    }



    

    snprintf(handshake_payload, sizeof(handshake_payload), "%s,%s", name, group_choice);

    send(sock_fd, handshake_payload, strlen(handshake_payload), 0);



    pthread_t recv_thread;

    pthread_create(&recv_thread, NULL, receive_handler, &sock_fd);

    pthread_detach(recv_thread);



    while (fgets(send_buffer, BUFFER_SIZE, stdin) != NULL) {

        if (strcmp(send_buffer, "exit\n") == 0) {

            break;

        }

        send_buffer[strcspn(send_buffer, "\n")] = 0;

        if (strlen(send_buffer) > 0) {

            send(sock_fd, send_buffer, strlen(send_buffer), 0);

        }

    }



    close(sock_fd);

    return 0;

} 
