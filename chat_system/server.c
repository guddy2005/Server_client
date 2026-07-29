#include "protocal.h"

typedef struct{
  int socket;
  char name[MAX_NAME];
  int online;
} Client;

Client client[MAX_CLIENTS];

void dump_offline_message(const char *src, const char *dest, const char *text) {
    if (!src || !dest || !text) return;
    FILE *f = fopen("offline_msg.dat", "ab");
    if (!f) return;
    
    DiskMessage dm;
    memset(&dm, 0, sizeof(DiskMessage));
    strncpy(dm.sender, src, MAX_NAME - 1);
    strncpy(dm.receiver, dest, MAX_NAME - 1);
    strncpy(dm.message, text, MAX_MSG - 1);
    if(fwrite(&dm, sizeof(DiskMessage), 1, f)!=1{
       perror("[DISK STORAGE] Error writing to file");
   }
    fclose(f);
    printf("[DISK STORAGE] Saved persistent offline log from %s to %s\n", src, dest);
}

void flush_offline_messages(int sock, const char *username) {
    FILE *f = fopen("offline_msg.dat", "rb");
    if (!f) return;

    DiskMessage vault[1000];
    int keep_count = 0;
    DiskMessage current;

    while(fread(&current, sizeof(DiskMessage), 1, f) == 1) {
        if (strcmp(current.receiver, username) == 0) {
            Packet p;
            memset(&p, 0, sizeof(Packet));
            p.type = CMD_SEND_MSG;
            strcpy(p.sender, current.sender);
            strcpy(p.message, current.message);
            send(sock, &p, sizeof(Packet), 0);
        } else {
            if (keep_count < 1000) {
                vault[keep_count++] = current;
            }
        }
    }
    fclose(f);

    f = fopen("offline_msg.dat", "wb");
    if (f) {
        if (keep_count > 0) {
            fwrite(vault, sizeof(DiskMessage), keep_count, f);
        }
        fclose(f);
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

    printf("[Select Engine Active] Monitoring handles on port %d without threads...\n", PORT);

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
                        printf("[Disconnected] User %s left the link pipeline.\n", clients[i].name);
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
                            printf("[Identity Verified] %s is now active.\n", clients[i].name);
                            notify_peers(clients[i].name, NOTIFY_ONLINE, current_sock);
                            flush_offline_messages(current_sock, clients[i].name);
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
                                    dump_offline_message(pkt.sender, pkt.receiver, pkt.message);
                                }
                                break;
                            }
                        }
                        if (!dest_found) {
                            dump_offline_message(pkt.sender, pkt.receiver, pkt.message); 
                            send_server_alert(current_sock, "Target offline. Message cached safely.");
                        }
                    }
                }
            }
        }
    }
    return 0;
}










