#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>
#include "protocal.h"
#define _XOPEN_SOURCE 500



int server_socket = -1;
char my_name[MAX_NAME];
struct sockaddr_in server_addr;

void *receive_handler(void *arg) {
    Packet packet;
    while(1) {
        if (server_socket == -1) {
            usleep(100000);
            continue;
        }

        int bytes = recv(server_socket, &packet, sizeof(Packet), 0);
        if (bytes <= 0) {
            printf("\n[⚠️ ALERT] Lost link to server! Entering auto-reconnect mode...\n");
            close(server_socket);
            server_socket = -1;
            continue;
        }

        switch(packet.type) {
            case CMD_SEND_MSG:
                printf("\n📩[%s]: %s\n", packet.sender, packet.message);
                break;
            case SERVER_ALERT:
                printf("\n❌[SERVER ALERT]: %s\n", packet.message);
                break;
            case NOTIFY_ONLINE:
                printf("\n🔔 [NOTIFICATION]: %s is now ONLINE!\n", packet.sender);
                break;
            case NOTIFY_OFFLINE:
                printf("\n🔕 [NOTIFICATION]: %s has gone OFFLINE.\n", packet.sender);
                break;
            default:
                break;
        }
        printf("Message > ");
        fflush(stdout);
    }
    return NULL;
}

void establish_connection() {
    while (1) {
        server_socket = socket(AF_INET, SOCK_STREAM, 0);
        printf("[🔄 Network] Attempting connection...\n");
        
        if (connect(server_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) == 0) {
            printf("[✔ Connected] Connection established!\n");
            
            Packet login_pkt;
            memset(&login_pkt, 0, sizeof(Packet));
            login_pkt.type = CMD_LOGIN;
            strcpy(login_pkt.sender, my_name);
            send(server_socket, &login_pkt, sizeof(Packet), 0);
            break;
        }
        
        printf("[✖ Link Failure] Server unreachable. Retrying in 3 seconds...\n");
        close(server_socket);
        sleep(3);
    }
}

int main() {
    char raw_input[MAX_MSG + MAX_NAME + 2];

    printf("Enter Username: ");
    if (fgets(my_name, MAX_NAME, stdin) == NULL) return 1;
    my_name[strcspn(my_name, "\n")] = '\0';

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);
    server_addr.sin_addr.s_addr = inet_addr(SERVER_IP);

    establish_connection();

    pthread_t tid;
    pthread_create(&tid, NULL, receive_handler, NULL);
    pthread_detach(tid);

    while(1) {
        printf("Message > ");
        fflush(stdout);
        
        if (fgets(raw_input, sizeof(raw_input), stdin) == NULL) break;
        raw_input[strcspn(raw_input, "\n")] = '\0';

        if (strlen(raw_input) == 0) continue;

        if (server_socket == -1) {
            printf("[❌ Error] Cannot transmit. Connection link is down.\n");
            establish_connection();
            continue;
        }

        char temp_input[sizeof(raw_input)];
        strcpy(temp_input, raw_input);

        char *target = strtok(temp_input, ":");
        char *msg_content = strtok(NULL, "");

        if (!target || !msg_content) {
            printf("[⚠️ Syntax Error] Use format: <username>:<message>\n");
            continue;
        }

        if (msg_content[0] == ' ') {
            msg_content++;
        }

        Packet pkt;
        memset(&pkt, 0, sizeof(Packet));
        pkt.type = CMD_SEND_MSG;
        strcpy(pkt.sender, my_name);
        strcpy(pkt.receiver, target);
        strcpy(pkt.message, msg_content);

        send(server_socket, &pkt, sizeof(Packet), 0);
    }
    return 0;
}

