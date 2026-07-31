#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/select.h>

#define MAX_CLIENTS 100
#define MAX_GROUPS 100
#define MAX_GROUP_MEMBERS 100
#define PORT 18880
#define BUFFER_SIZE 1024

typedef struct
{
    int socket;
    char username[30];
    int online;
} Client;

typedef struct
{
    char username[30];
} GroupMember;

typedef struct
{
    int id;
    char group_name[50];
    char admin[30];
    GroupMember members[100];
    int member_count;
    int active;
} Group;

Client clients[MAX_CLIENTS];
Group groups[MAX_GROUPS];
int next_group_id = 1;

int find_client_socket(int socket)
{
    int i;
    for (i = 0; i < MAX_CLIENTS; i++)
    {
        if (clients[i].online && clients[i].socket == socket)
        {
            return i;
        }
    }
    return -1;
}

int find_client_by_name(const char *name)
{
    int i;
    for (i = 0; i < MAX_CLIENTS; i++)
    {
        if (clients[i].online && strcmp(clients[i].username, name) == 0)
        {
            return i;
        }
    }
    return -1;
}

int send_to_client(int socket, const char *msg)
{
    if (socket < 0)
    {
        return 0;
    }
    send(socket, msg, strlen(msg), 0);
    return 1;
}

void broadcast(const char *msg)
{
    int i;
    for (i = 0; i < MAX_CLIENTS; i++)
    {
        if (clients[i].online)
        {
            send_to_client(clients[i].socket, msg);
        }
    }
}

int find_group(int group_id)
{
    int i;
    for (i = 0; i < MAX_GROUPS; i++)
    {
        if (groups[i].active && groups[i].id == group_id)
        {
            return i;
        }
    }
    return -1;
}

int find_group_by_name(const char *name)
{

int i;
    for (i = 0; i < MAX_GROUPS; i++)
    {
        if (groups[i].active && strcmp(groups[i].group_name, name) == 0)
        {
            return i;
        }
    }
    return -1;
}

int is_member(int group_id, const char *username)
{
    int i, index;
    index = find_group(group_id);
    if (index < 0)
    {
        return 0;
    }
    for (i = 0; i < groups[index].member_count; i++)
    {
        if (strcmp(groups[index].members[i].username, username) == 0)
        {
            return 1;
        }
    }
    return 0;
}

int is_admin(int group_id, const char *username)
{
    int index;
    index = find_group(group_id);
    if (index < 0)
    {
        return 0;
    }
    return strcmp(groups[index].admin, username) == 0;
}

int add_member(int group_id, const char *username)
{
    int index;
    index = find_group(group_id);
    if (index < 0 || is_member(group_id, username))
    {
        return 0;
    }
    if (groups[index].member_count >= MAX_GROUP_MEMBERS)
    {
        return 0;
    }
    strcpy(groups[index].members[groups[index].member_count].username, username);
    groups[index].member_count++;
    return 1;
}

int remove_member(int group_id, const char *username)
{
    int index, i, j;
    index = find_group(group_id);
    if (index < 0)
    {
        return 0;
    }
    for (i = 0; i < groups[index].member_count; i++)
    {
        if (strcmp(groups[index].members[i].username, username) == 0)
        {
            for (j = i; j < groups[index].member_count - 1; j++)
            {
                groups[index].members[j] = groups[index].members[j + 1];
            }
            groups[index].member_count--;
            if (strcmp(groups[index].admin, username) == 0 && groups[index].member_count > 0)
            {
                strcpy(groups[index].admin, groups[index].members[0].username);
            }
            if (groups[index].member_count == 0)
            {
                groups[index].active = 0;
            }
            return 1;
        }
    }
    return 0;
}
 
void notify_group(int group_id, const char *msg)
{
    int index, i;
    index = find_group(group_id);
    if (index < 0)
    {
        return;
    }
    for (i = 0; i < MAX_CLIENTS; i++)
    {
        if (clients[i].online && is_member(group_id, clients[i].username))
        {
            send_to_client(clients[i].socket, msg);
        }
    }
}

char *next_token(char **cursor)
{
    while (**cursor == ' ' || **cursor == '\t')
    {
        (*cursor)++;
    }
    if (**cursor == '\0')
    {
        return "";
    }
    char *start = *cursor;
    while (**cursor != '\0' && **cursor != ' ' && **cursor != '\t' && **cursor != '\n' && **cursor != '\r')
    {
        (*cursor)++;
    }
    if (**cursor != '\0')
    {
        **cursor = '\0';
        (*cursor)++;
    }
    return start;
}

int parse_int(const char *value)
{
    char *end;
    long result;
    if (value == NULL || *value == '\0')
    {
        return -1;
    }
    result = strtol(value, &end, 10);
    if (*end != '\0')
    {
        return -1;
    }
    return (int)result;
}

int main(void)
{
    int server_socket, new_socket, activity, max_sd, i, client_index, group_index;
    struct sockaddr_in address;
    socklen_t addrlen;
    fd_set read_fds;
    char buffer[BUFFER_SIZE];
    char response[4096];

    memset(clients, 0, sizeof(clients));
    memset(groups, 0, sizeof(groups));
    for (i = 0; i < MAX_CLIENTS; i++)
    {
        clients[i].socket = -1;
    }

    server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket == -1)
    {
        perror("socket");
        return 1;
    }

    int opt = 1;
    setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);

    if (bind(server_socket, (struct sockaddr *)&address, sizeof(address)) < 0)
    {
        perror("bind");
        close(server_socket);
        return 1;
    }

    if (listen(server_socket, 10) < 0)
    {
        perror("listen");
        close(server_socket);
        return 1;
    }

    while (1)
    {
        FD_ZERO(&read_fds);
        FD_SET(server_socket, &read_fds);
        max_sd = server_socket;

        for (i = 0; i < MAX_CLIENTS; i++)
        {
            if (clients[i].online)
            {
                FD_SET(clients[i].socket, &read_fds);
                if (clients[i].socket > max_sd)
                {
                    max_sd = clients[i].socket;
                }
            }
        }

        activity = select(max_sd + 1, &read_fds, NULL, NULL, NULL);
        if (activity < 0)
        {
            continue;
        }

        if (FD_ISSET(server_socket, &read_fds))
        {
            addrlen = sizeof(address);
            new_socket = accept(server_socket, (struct sockaddr *)&address, &addrlen);
            if (new_socket < 0)
            {
                continue;
            }

            client_index = -1;
            for (i = 0; i < MAX_CLIENTS; i++)
            {
                if (!clients[i].online)
                {
                    client_index = i;
                    break;
                }
            }

            if (client_index < 0)
            {
                send(new_socket, "Server full.\n", 13, 0);
                close(new_socket);
                continue;
            }

            clients[client_index].socket = new_socket;
            snprintf(clients[client_index].username, sizeof(clients[client_index].username), "user%d", client_index + 1);
            clients[client_index].online = 1;
            snprintf(response, sizeof(response), "Welcome %s!\n", clients[client_index].username);

            send_to_client(new_socket, response);
           send_to_client(new_socket,
"\n"
"==================== CHAT MENU ====================\n"
"1. /groups                -> Show online users\n"
"2. /gmsg <groupID> <text> -> Send meassge to a group \n"
"3. /rename<id> <newname>  -> rename a group\n"
"4. /create                -> Create a group\n"
"5. /join <group_id>       -> Join a group\n"
"5. /members <group_id>    -> show memeber  of  group\n"
"6. /makeadmin<id><user>   -> make admin to your group\n"
"7. /leave                 -> Leave current group\n"
"8. /deletegroup <id>      ->delete the group \n"
"===================================================\n");
        }

        for (i = 0; i < MAX_CLIENTS; i++)
        {
            if (!clients[i].online)
            {
                continue;
            }
            if (FD_ISSET(clients[i].socket, &read_fds))
            {
                int read_size = recv(clients[i].socket, buffer, sizeof(buffer) - 1, 0);
                if (read_size <= 0)
                {
                    close(clients[i].socket);
                    clients[i].socket = -1;
                    clients[i].username[0] = '\0';
                    clients[i].online = 0;
                    continue;
                }

                buffer[read_size] = '\0';
                if (buffer[0] != '/')
                {
                    continue;
                }

                char *cursor = buffer;
                char *command = next_token(&cursor);

                if (strcmp(command, "/create") == 0)
                {
                    char *name = next_token(&cursor);
                    if (name[0] == '\0')
                    {
                        send_to_client(clients[i].socket, "Usage: /create <groupname>\n");
                    }
                    else if (find_group_by_name(name) >= 0)
                    {
                        send_to_client(clients[i].socket, "Group already exists.\n");
                    }
                    else
                    {
                        for (group_index = 0; group_index < MAX_GROUPS; group_index++)
                        {
                            if (!groups[group_index].active)
                            {
                                groups[group_index].id = next_group_id++;
                                strcpy(groups[group_index].group_name, name);
                                strcpy(groups[group_index].admin, clients[i].username);
                                strcpy(groups[group_index].members[0].username, clients[i].username);
                                groups[group_index].member_count = 1;
                                groups[group_index].active = 1;
                                snprintf(response, sizeof(response), "Group created with id %d\n", groups[group_index].id);
                                send_to_client(clients[i].socket, response);
                                break;
                            }
                        }
                    }
                }
                else if (strcmp(command, "/join") == 0)
                {
                    char *group_token = next_token(&cursor);
                    int group_id = parse_int(group_token);
                    if (group_id < 0)
                    {
                        send_to_client(clients[i].socket, "Usage: /join <groupid>\n");
                    }
                    else
                    {
                        group_index = find_group(group_id);
                        if (group_index < 0)
                        {
                            send_to_client(clients[i].socket, "Group not found.\n");
                        }
                        else if (is_member(group_id, clients[i].username))
                        {
                            send_to_client(clients[i].socket, "You are already a member.\n");
                        }
                        else if (add_member(group_id, clients[i].username))
                        {
                            snprintf(response, sizeof(response), "%s joined the group.\n", clients[i].username);
                            notify_group(group_id, response);
                            snprintf(response, sizeof(response), "You joined group %d.\n", group_id);
                            send_to_client(clients[i].socket, response);
                        }
                        else
                        {
                            send_to_client(clients[i].socket, "Unable to join group.\n");
                        }
                    }
                }
                else if (strcmp(command, "/leave") == 0)
                {
                    char *group_token = next_token(&cursor);
                    int group_id = parse_int(group_token);
                    if (group_id < 0)
                    {
                        send_to_client(clients[i].socket, "Usage: /leave <groupid>\n");
                    }
                    else
                    {
                        group_index = find_group(group_id);
                        if (group_index < 0)
                        {
                            send_to_client(clients[i].socket, "Group not found.\n");
                        }
                        else if (!is_member(group_id, clients[i].username))
                        {
                            send_to_client(clients[i].socket, "You are not a member.\n");
                        }
                        else if (remove_member(group_id, clients[i].username))
                        {
                            snprintf(response, sizeof(response), "%s left the group.\n", clients[i].username);
                            notify_group(group_id, response);
                            snprintf(response, sizeof(response), "You left group %d.\n", group_id);
                            send_to_client(clients[i].socket, response);
                        }
                        else
                        {
                            send_to_client(clients[i].socket, "Unable to leave group.\n");
                        }
                    }
                }
                else if (strcmp(command, "/groups") == 0)
                {
                    int count = 0;
                    response[0] = '\0';
                    strcat(response, "Groups:\n");
                    for (group_index = 0; group_index < MAX_GROUPS; group_index++)
                    {
                        if (groups[group_index].active)
                        {
                            char entry[128];
                            snprintf(entry, sizeof(entry), "%d %s admin=%s members=%d\n", groups[group_index].id, groups[group_index].group_name, groups[group_index].admin, groups[group_index].member_count);
                            strcat(response, entry);
                            count++;
                        }
                    }
                    if (count == 0)
                    {
                        strcat(response, "No groups available.\n");
                    }
                    send_to_client(clients[i].socket, response);
                }
                else if (strcmp(command, "/members") == 0)
                {
                    char *group_token = next_token(&cursor);
                    int group_id = parse_int(group_token);
                    if (group_id < 0)
                    {
                        send_to_client(clients[i].socket, "Usage: /members <groupid>\n");
                    }
                    else
                    {
                        group_index = find_group(group_id);
                        if (group_index < 0)
                        {
                            send_to_client(clients[i].socket, "Group not found.\n");
                        }
                        else
                        {
                            response[0] = '\0';
                            strcat(response, "Members:\n");
                            for (int j = 0; j < groups[group_index].member_count; j++)
                            {
                                char entry[64];
                                snprintf(entry, sizeof(entry), "%s\n", groups[group_index].members[j].username);
                                strcat(response, entry);
                            }
                            send_to_client(clients[i].socket, response);
                        }
                    }
                }
                else if (strcmp(command, "/gmsg") == 0)
                {
                    char *group_token = next_token(&cursor);
                    char *message = cursor;
                    while (*message == ' ' || *message == '\t')
                    {
                        message++;
                    }
                    int group_id = parse_int(group_token);
                    if (group_id < 0 || message[0] == '\0')
                    {
                        send_to_client(clients[i].socket, "Usage: /gmsg <groupid> <message>\n");
                    }
                    else
                    {
                        group_index = find_group(group_id);
                        if (group_index < 0)
                        {
                            send_to_client(clients[i].socket, "Group not found.\n");
                        }
                        else if (!is_member(group_id, clients[i].username))
                        {
                            send_to_client(clients[i].socket, "You are not a member.\n");
                        }
                        else
                        {
                            snprintf(response, sizeof(response), "[%d] %s: %s\n", group_id, clients[i].username, message);
                            notify_group(group_id, response);
                        }
                    }
                }
                else if (strcmp(command, "/kick") == 0)
                {
                    char *group_token = next_token(&cursor);
                    char *target = next_token(&cursor);
                    int group_id = parse_int(group_token);
                    if (group_id < 0 || target[0] == '\0')
                    {
                        send_to_client(clients[i].socket, "Usage: /kick <groupid> <username>\n");
                    }
                    else
                    {
                        group_index = find_group(group_id);
                        if (group_index < 0)
                        {
                            send_to_client(clients[i].socket, "Group not found.\n");
                        }
                        else if (!is_admin(group_id, clients[i].username))
                        {
                            send_to_client(clients[i].socket, "Only the admin can kick members.\n");
                        }
                        else if (!is_member(group_id, target))
                        {
                            send_to_client(clients[i].socket, "Target is not a member.\n");
                        }
                        else
                        {
                            remove_member(group_id, target);
                            snprintf(response, sizeof(response), "%s was kicked from the group.\n", target);
                            notify_group(group_id, response);
                            int target_index = find_client_by_name(target);
                            if (target_index >= 0)
                            {
                                snprintf(response, sizeof(response), "You were kicked from group %d.\n", group_id);
                                send_to_client(clients[target_index].socket, response);
                            }
                        }
                    }
                }
                else if (strcmp(command, "/renamegroup") == 0)
                {
                    char *group_token = next_token(&cursor);
                    char *new_name = next_token(&cursor);
                    int group_id = parse_int(group_token);
                    if (group_id < 0 || new_name[0] == '\0')
                    {
                        send_to_client(clients[i].socket, "Usage: /renamegroup <groupid> <newname>\n");
                    }
                    else
                    {
                        group_index = find_group(group_id);
                        if (group_index < 0)
                        {
                            send_to_client(clients[i].socket, "Group not found.\n");
                        }
                        else if (!is_admin(group_id, clients[i].username))
                        {
                            send_to_client(clients[i].socket, "Only the admin can rename the group.\n");
                        }
                        else if (find_group_by_name(new_name) >= 0)
                        {
                            send_to_client(clients[i].socket, "Group name already exists.\n");
                        }
                        else
                        {
                            strcpy(groups[group_index].group_name, new_name);
                            snprintf(response, sizeof(response), "Group renamed to %s.\n", new_name);
                            send_to_client(clients[i].socket, response);
                        }
                    }
                }
                else if (strcmp(command, "/deletegroup") == 0)
                {
                    char *group_token = next_token(&cursor);
                    int group_id = parse_int(group_token);
                    if (group_id < 0)
                    {
                        send_to_client(clients[i].socket, "Usage: /deletegroup <groupid>\n");
                    }
                    else
                    {
                        group_index = find_group(group_id);
                        if (group_index < 0)
                        {
                            send_to_client(clients[i].socket, "Group not found.\n");
                        }
                        else if (!is_admin(group_id, clients[i].username))
                        {
                            send_to_client(clients[i].socket, "Only the admin can delete the group.\n");
                        }
                        else
                        {
                            snprintf(response, sizeof(response), "Group %d deleted.\n", group_id);
                            notify_group(group_id, response);
                            groups[group_index].active = 0;
                            send_to_client(clients[i].socket, response);
                        }
                    }
                }
                else if (strcmp(command, "/makeadmin") == 0)
                {
                    char *group_token = next_token(&cursor);
                    char *target = next_token(&cursor);
                    int group_id = parse_int(group_token);
                    if (group_id < 0 || target[0] == '\0')
                    {
                        send_to_client(clients[i].socket, "Usage: /makeadmin <groupid> <username>\n");
                    }
                    else
                    {
                        group_index = find_group(group_id);
                        if (group_index < 0)
                        {
                            send_to_client(clients[i].socket, "Group not found.\n");
                        }
                        else if (!is_admin(group_id, clients[i].username))
                        {
                            send_to_client(clients[i].socket, "Only the admin can make another admin.\n");
                        }
                        else if (!is_member(group_id, target))
                        {
                            send_to_client(clients[i].socket, "Target is not a member.\n");
                        }
                        else
                        {
                            strcpy(groups[group_index].admin, target);
                            snprintf(response, sizeof(response), "%s is now the admin.\n", target);
                            notify_group(group_id, response);
                        }
                    }
                }
            }
        }
    }

    return 0;
}
