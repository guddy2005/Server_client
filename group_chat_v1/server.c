#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/socket.h>
#include <netinet/in.h>

#define PORT 19900
#define MAX_USERS 100
#define MAX_OFFLINE_MSGS 50
#define BUFFER_SIZE 2048

int next_available_group_id = 1; 

typedef struct {
    char message[BUFFER_SIZE];
} OfflineMessage;

typedef struct {
    char username[50];
    int socket_fd;            
    int is_admin;             
    int is_online;            
    int current_group_id;     
    OfflineMessage inbox[MAX_OFFLINE_MSGS];
    int offline_msg_count;
} UserAccount;

UserAccount user_db[MAX_USERS];
int total_users = 0;
pthread_mutex_t db_mutex = PTHREAD_MUTEX_INITIALIZER;

void broadcast_to_group_members(char *message, char *sender_name, int group_id) {
    pthread_mutex_lock(&db_mutex);
    for (int i = 0; i < total_users; i++) {
        if (user_db[i].current_group_id == group_id) {
            if (strcmp(user_db[i].username, sender_name) == 0) {
                continue;
            }
            if (user_db[i].is_online == 1 && user_db[i].socket_fd != -1) {
                send(user_db[i].socket_fd, message, strlen(message), 0);
            } else {
                if (user_db[i].offline_msg_count < MAX_OFFLINE_MSGS) {
                    int idx = user_db[i].offline_msg_count;
                    strncpy(user_db[i].inbox[idx].message, message, BUFFER_SIZE - 1);
                    user_db[i].offline_msg_count++;
                }
            }
        }
    }
    pthread_mutex_unlock(&db_mutex);
}

void execute_group_kick(int admin_idx, char *target_name, int group_id) {
    pthread_mutex_lock(&db_mutex);
    if (user_db[admin_idx].is_admin != 1) {
        send(user_db[admin_idx].socket_fd, "Error: You are not an admin.\n", 29, 0);
        pthread_mutex_unlock(&db_mutex);
        return;
    }
    for (int i = 0; i < total_users; i++) {
        if (strcmp(user_db[i].username, target_name) == 0) {
            if (user_db[i].current_group_id == group_id) {
                if (user_db[i].socket_fd != -1) {
                    send(user_db[i].socket_fd, "System: You have been kicked from the group.\n", 45, 0);
                    close(user_db[i].socket_fd);
                    user_db[i].socket_fd = -1;
                }
                user_db[i].is_online = 0;
                user_db[i].current_group_id = 0;
                send(user_db[admin_idx].socket_fd, "System: User kicked successfully.\n", 34, 0);
            } else {
                send(user_db[admin_idx].socket_fd, "Error: User is not in your group.\n", 34, 0);
            }
            break;
        }
    }
    pthread_mutex_unlock(&db_mutex);
}

void execute_group_promote(int admin_idx, char *target_name, int group_id) {
    pthread_mutex_lock(&db_mutex);
    if (user_db[admin_idx].is_admin != 1) {
        send(user_db[admin_idx].socket_fd, "Error: You are not an admin.\n", 29, 0);
        pthread_mutex_unlock(&db_mutex);
        return;
    }
    for (int i = 0; i < total_users; i++) {
        if (strcmp(user_db[i].username, target_name) == 0) {
            if (user_db[i].current_group_id == group_id) {
                user_db[i].is_admin = 1;
                if (user_db[i].socket_fd != -1) {
                    send(user_db[i].socket_fd, "System: You have been promoted to co-admin!\n", 44, 0);
                }
                send(user_db[admin_idx].socket_fd, "System: User promoted successfully.\n", 36, 0);
            } else {
                send(user_db[admin_idx].socket_fd, "Error: User is not in your group.\n", 34, 0);
            }
            break;
        }
    }
    pthread_mutex_unlock(&db_mutex);
}

void deliver_offline_messages(int user_idx) {
    pthread_mutex_lock(&db_mutex);
    if (user_db[user_idx].offline_msg_count > 0) {
        char announcement[100];
        snprintf(announcement, sizeof(announcement), "📩 You missed %d group messages while offline:\n", user_db[user_idx].offline_msg_count);
        send(user_db[user_idx].socket_fd, announcement, strlen(announcement), 0);
        for (int i = 0; i < user_db[user_idx].offline_msg_count; i++) {
            send(user_db[user_idx].socket_fd, user_db[user_idx].inbox[i].message, strlen(user_db[user_idx].inbox[i].message), 0);
        }
        user_db[user_idx].offline_msg_count = 0;
    }
    pthread_mutex_unlock(&db_mutex);
}

void *handle_client(void *arg) {
    int client_fd = *((int *)arg);
    free(arg);
    char buffer[BUFFER_SIZE];
    char formatted_msg[BUFFER_SIZE + 120];
    char my_name[50] = {0};
    int my_group = 0;
    int my_index = -1;

        if (recv(client_fd, buffer, sizeof(buffer) - 1, 0) <= 0) {

        close(client_fd);

        pthread_exit(NULL);

    }

    sscanf(buffer, "%[^,],%d", my_name, &my_group);



    pthread_mutex_lock(&db_mutex);
    if (my_group == 0) {

        my_group = next_available_group_id++;

        char creation_notification[100];

        snprintf(creation_notification, sizeof(creation_notification), "System: Successfully created Group Room ID: %d\n", my_group);

        send(client_fd, creation_notification, strlen(creation_notification), 0);

    }



    int found = 0;

    for (int i = 0; i < total_users; i++) {

        if (strcmp(user_db[i].username, my_name) == 0) {

            user_db[i].socket_fd = client_fd;

            user_db[i].is_online = 1;

            user_db[i].current_group_id = my_group;

            my_index = i;

            found = 1;

            break;

        }

    }

    if (!found && total_users < MAX_USERS) {

        my_index = total_users;

        strncpy(user_db[my_index].username, my_name, 49);

        user_db[my_index].socket_fd = client_fd;

        user_db[my_index].is_online = 1;

        user_db[my_index].current_group_id = my_group;

        user_db[my_index].offline_msg_count = 0;

        user_db[my_index].is_admin = 1; // Creator becomes the admin of this group

        total_users++;

    }

    pthread_mutex_unlock(&db_mutex);
    deliver_offline_messages(my_index);
    snprintf(formatted_msg, sizeof(formatted_msg), "\x1b[32m🔔 [%s] is now online in Group %d.\x1b[0m\n", my_name, my_group);
    broadcast_to_group_members(formatted_msg, my_name, my_group);

    while (1) {
        memset(buffer, 0, BUFFER_SIZE);
        int bytes = recv(client_fd, buffer, BUFFER_SIZE - 1, 0);
        if (bytes <= 0) {
            break;
        }
        buffer[strcspn(buffer, "\n")] = 0;

        if (strncmp(buffer, "/kick ", 6) == 0) {
            char target[50];
            sscanf(buffer + 6, "%s", target);
            execute_group_kick(my_index, target, my_group);
        } 
        else if (strncmp(buffer, "/promote ", 9) == 0) {
            char target[50];
            sscanf(buffer + 9, "%s", target);
            execute_group_promote(my_index, target, my_group);
        } 
        else if (strcmp(buffer, "/status offline") == 0) {
            pthread_mutex_lock(&db_mutex);
            user_db[my_index].is_online = 0;
            pthread_mutex_unlock(&db_mutex);
            send(client_fd, "System: You are now incognito (offline mode).\n", 46, 0);
        } 
        else if (strcmp(buffer, "/status online") == 0) {
            pthread_mutex_lock(&db_mutex);
            user_db[my_index].is_online = 1;
            pthread_mutex_unlock(&db_mutex);
            send(client_fd, "System: You are now visible (online mode).\n", 43, 0);
        } 
        else {
            snprintf(formatted_msg, sizeof(formatted_msg), "[Group %d] [%s]: %s\n", my_group, my_name, buffer);
            broadcast_to_group_members(formatted_msg, my_name, my_group);
        }
    }

    pthread_mutex_lock(&db_mutex);
    user_db[my_index].socket_fd = -1;
    user_db[my_index].is_online = 0;
    pthread_mutex_unlock(&db_mutex);

    snprintf(formatted_msg, sizeof(formatted_msg), "\x1b[31m❌ [%s] has disconnected from Group %d.\x1b[0m\n", my_name, my_group);
    broadcast_to_group_members(formatted_msg, my_name, my_group);
    
    close(client_fd);
    pthread_exit(NULL);
}

int main() {
    int server_fd, *new_sock;
    struct sockaddr_in server_addr, client_addr;
    socklen_t addr_size = sizeof(struct sockaddr_in);

    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(PORT);

    bind(server_fd, (struct sockaddr *)&server_addr, sizeof(server_addr));
    listen(server_fd, 10);
    printf("Server active on port %d...\n", PORT);

    while (1) {
        int client_fd = accept(server_fd, (struct sockaddr *)&client_addr, &addr_size);
        pthread_t t_id;
        new_sock = malloc(sizeof(int));
        *new_sock = client_fd;
        pthread_create(&t_id, NULL, handle_client, (void *)new_sock);
        pthread_detach(t_id);
    }
    return 0;
}

