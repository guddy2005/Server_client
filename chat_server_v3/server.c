#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/select.h>
#include "protocal.h"

typedef struct MessageNode {
    char sender[MAX_NAME];
    char message[MAX_MSG];
    struct MessageNode *next;
} MessageNode;

typedef struct {
    char receiver_key[MAX_NAME];
    MessageNode *head;
} OfflineMapSlot;

typedef struct {
    int socket;
    char name[MAX_NAME];
    int online;
} Client;

Client clients[MAX_CLIENTS];
OfflineMapSlot offline_map[MAX_CLIENTS];

void map_store_offline_message(const char *src, const char *dest, const char *text) {
    int i, empty_slot = -1;

    for (i = 0; i < MAX_CLIENTS; i++) {
        if (strcmp(offline_map[i].receiver_key, dest) == 0) {
            empty_slot = i;
            break;
        }
        if (offline_map[i].receiver_key[0] == '\0' && empty_slot == -1) {
            empty_slot = i;
        }
    }

    if (empty_slot == -1) {
        printf("[MAP ERROR] System storage map limits reached!\n");
        return;
    }

    if (offline_map[empty_slot].receiver_key[0] == '\0') {
        strncpy(offline_map[empty_slot].receiver_key, dest, MAX_NAME - 1);
        offline_map[empty_slot].head = NULL;
    }

    MessageNode *new_node = (MessageNode *)malloc(sizeof(MessageNode));
    if (!new_node) return;

    strncpy(new_node->sender, src, MAX_NAME - 1);
    strncpy(new_node->message, text, MAX_MSG - 1);
    new_node->next = NULL;

    if (offline_map[empty_slot].head == NULL) {
        offline_map[empty_slot].head = new_node;
    } else {
        MessageNode *temp = offline_map[empty_slot].head;
        while (temp->next != NULL) {
            temp = temp->next;
        }
        temp->next = new_node;
    }
    printf("[MAP INSTANCE] Cached offline entry in RAM from %s ➔ %s\n", src, dest);
}

void map_flush_offline_messages(int sock, const char *username) {
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (strcmp(offline_map[i].receiver_key, username) == 0) {
            MessageNode *current = offline_map[i].head;

            while (current != NULL) {
                Packet p;
                memset(&p, 0, sizeof(Packet));
                p.type = CMD_SEND_MSG;
                strcpy(p.sender, current->sender);
                strcpy(p.message, current->message);
                send(sock, &p, sizeof(Packet), 0);

                MessageNode *to_free = current;
                current = current->next;
                free(to_free);
            }

            offline_map[i].head = NULL;
            memset(offline_map[i].receiver_key, 0, MAX_NAME);
            printf("[MAP DISPATCH] RAM cache cleared and flushed for user: %s\n", username);
            break;
        }
    }
}

void notify_peers(const char *username, PacketType type, int skip_socket) {
    Packet p;
    memset(&p, 0, sizeof(Packet));
    p.type = type;
    strcpy(p.sender, username);

    for(int i = 0; i < MAX_CLIENTS; i++) {
        if (clients[i].online && clients[i].socket != skip_socket) {
            send(clients[i].socket, &p, sizeof(Packet), 0);
        }
    }
}

void send_server_alert(int sock, const char *err_msg) {
    Packet p;
    memset(&p, 0, sizeof(Packet));
    p.type = SERVER_ALERT;
    strcpy(p.sender, "SERVER");
    strcpy(p.message, err_msg);
    send(sock, &p, sizeof(Packet), 0);
}

int main() {
    int server_fd, max_fd, activity, i;
    struct sockaddr_in saddr;
    Packet pkt;

    for (i = 0; i < MAX_CLIENTS; i++) {
        clients[i].socket = -1;
        clients[i].online = 0;
        memset(clients[i].name, 0, MAX_NAME);

        memset(offline_map[i].receiver_key, 0, MAX_NAME);
        offline_map[i].head = NULL;
    }

    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    saddr.sin_family = AF_INET;
    saddr.sin_port = htons(PORT);
    saddr.sin_addr.s_addr = INADDR_ANY;

    if (bind(server_fd, (struct sockaddr *)&saddr, sizeof(saddr)) < 0) {
        perror("Bind failed");
        exit(EXIT_FAILURE);
    }
    listen(server_fd, 5);

    printf("[Active] Server Started on Port %d \n", PORT);

    fd_set readfds;

    while(1) {
        FD_ZERO(&readfds);
        FD_SET(server_fd, &readfds);
        max_fd = server_fd;

        for (i = 0; i < MAX_CLIENTS; i++) {
            int sock = clients[i].socket;
            if(sock > 0) {
                FD_SET(sock, &readfds);
            }
            if(sock > max_fd) {
                max_fd = sock;
            }
        }

        activity = select(max_fd + 1, &readfds, NULL, NULL, NULL);
        if (activity < 0) continue;

        if (FD_ISSET(server_fd, &readfds)) {
            int new_socket = accept(server_fd, NULL, NULL);
            int slot_found = 0;
            for (i = 0; i < MAX_CLIENTS; i++) {
                if (clients[i].socket == -1) {
                    clients[i].socket = new_socket;
                    clients[i].online = 0;
                    slot_found = 1;
                    break;
                }
            }
            if (!slot_found) {
                send_server_alert(new_socket, "Server full.");
                close(new_socket);
            }
        }

        for (i = 0; i < MAX_CLIENTS; i++) {
            int current_sock = clients[i].socket;

            if (current_sock > 0 && FD_ISSET(current_sock, &readfds)) {
                int bytes = recv(current_sock, &pkt, sizeof(Packet), 0);

                if (bytes <= 0) {
                    if (clients[i].online) {
                        printf("[Disconnected] User %s Left\n", clients[i].name);
                        notify_peers(clients[i].name, NOTIFY_OFFLINE, current_sock);
                    }
                    close(current_sock);
                    clients[i].socket = -1;
                    clients[i].online = 0;
                    memset(clients[i].name, 0, MAX_NAME);
                } 
                else {
                    if (pkt.type == CMD_LOGIN) {
                        int duplicate = 0;
                        for (int j = 0; j < MAX_CLIENTS; j++) {
                            if (clients[j].online && strcmp(clients[j].name, pkt.sender) == 0) {
                                duplicate = 1; break;
                            }
                        }

                        if (duplicate) {
                            send_server_alert(current_sock, "You are already logged online.");
                            close(current_sock);
                            clients[i].socket = -1;
                        } else {
                            strncpy(clients[i].name, pkt.sender, MAX_NAME - 1);
                            clients[i].online = 1;
                            printf("[New Client] %s is now active.\n", clients[i].name);
                            notify_peers(clients[i].name, NOTIFY_ONLINE, current_sock);
                            map_flush_offline_messages(current_sock, clients[i].name);
                        }
                    } 
                    else if (pkt.type == CMD_SEND_MSG) {
                        int dest_found = 0;
                        for (int j = 0; j < MAX_CLIENTS; j++) {
                            if (clients[j].socket != -1 && strcmp(clients[j].name, pkt.receiver) == 0) {
                                dest_found = 1;
                                if (clients[j].online) {
                                    send(clients[j].socket, &pkt, sizeof(Packet), 0);
                                } else {
                                    map_store_offline_message(pkt.sender, pkt.receiver, pkt.message);
                                }
                                break;
                            }
                        }
                        if (!dest_found) {
                            map_store_offline_message(pkt.sender, pkt.receiver, pkt.message); 
                            send_server_alert(current_sock, "Target user doesn't exist yet. Saved inside memory");
                        }
                    }
                }
            }
        }
    }
    return 0;
}

