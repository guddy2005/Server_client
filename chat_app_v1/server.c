#define _GNU_SOURCE
#include <arpa/inet.h>
#include <netinet/in.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>
#include <errno.h>
#include <ctype.h>

#define DEFAULT_PORT 5000
#define BACKLOG 10
#define MAX_MESSAGE_LEN 1024
#define MAX_USERNAME_LEN 50
#define MAX_GROUP_NAME_LEN 50
#define MAX_USERS 100
#define MAX_GROUPS 50
#define MAX_GROUP_MEMBERS 100
#define MAX_MESSAGES 2000
#define MAX_GROUP_DELIVERIES 5000
#define MAX_INVITES 500
#define DATA_DIR "data"
#define MAX_CONTACTS 100
 enum
{
    STATUS_SENT = 0,
    STATUS_DELIVERED = 1,
    STATUS_READ = 2,
} DeliveryStatus;

typedef enum
{
    INVITE_PENDING = 0,
    INVITE_ACCEPTED = 1,
    INVITE_REJECTED = 2,
} InviteStatus;

typedef struct
{
    int id;
    int socket;
    char username[MAX_USERNAME_LEN];
    char password[128];  
    int connected;
    int online;
    int contacts[MAX_CONTACTS];
    int contact_count;
} User;

typedef struct
{
    int id;
    char name[MAX_GROUP_NAME_LEN];
    int owner_id;
    int active;
    int member_count;
    int member_ids[MAX_GROUP_MEMBERS];
} Group;

typedef struct
{
    long message_id;
    int sender_id;
    int receiver_id;
    int group_id;
    char content[MAX_MESSAGE_LEN];
    time_t timestamp;
    int status;
} Message;

typedef struct
{
    long message_id;
    int recipient_id;
    int status;
} GroupDelivery;

typedef struct
{
    int group_id;
    int invitee_id;
    int inviter_id;
    int status;
} Invite;

static User users[MAX_USERS];
static int user_count = 0;
static Group groups[MAX_GROUPS];
static int group_count = 0;
static Message messages[MAX_MESSAGES];
static int message_count = 0;
static GroupDelivery deliveries[MAX_GROUP_DELIVERIES];
static int delivery_count = 0;
static Invite invites[MAX_INVITES];
static int invite_count = 0;

static long next_user_id = 1;
static int next_group_id = 1;
static long next_message_id = 1001;

static pthread_mutex_t users_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t groups_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t messages_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t invites_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t send_mutex = PTHREAD_MUTEX_INITIALIZER;

static int ensure_data_directory(void)
{
    struct stat st;
    if (stat(DATA_DIR, &st) == 0)
    {
        if (S_ISDIR(st.st_mode))
            return 0;
        return -1;
    }
    if (mkdir(DATA_DIR, 0755) < 0 && errno != EEXIST)
        return -1;
    return 0;
}

static void sanitize_text(char *dst, const char *src, size_t size)
{
    size_t i = 0;
    while (*src && i + 1 < size)
    {
        if (*src == '|' || *src == '\n' || *src == '\r')
            dst[i++] = ' ';
        else
            dst[i++] = *src;
        src++;
    }
    dst[i] = '\0';}
static User *find_user_by_id(int id);
void add_contact(int current_user_id, int contact_user_id)
{
    User *current_user = find_user_by_id(current_user_id);
    User *contact_user = find_user_by_id(contact_user_id);

    if (current_user == NULL)
    {
        printf("Current user not found\n");
        return;
    }

    if (contact_user == NULL)
    {
        printf("User to add not found\n");
        return;
    }

    if (current_user_id == contact_user_id)
    {
        printf("You cannot add yourself\n");
        return;
    }

    for (int i = 0; i < current_user->contact_count; i++)
    {
        if (current_user->contacts[i] == contact_user_id)
        {
            printf("User is already in contacts\n");
            return;
        }
    }

    if (current_user->contact_count >= MAX_CONTACTS)
    {
        printf("Contact list is full\n");
        return;
    }

    current_user->contacts[current_user->contact_count] = contact_user_id;
    current_user->contact_count++;

    printf("%s added to contacts\n", contact_user->username);
}
static int send_all(int sockfd, const void *buffer, size_t length)
{
    const char *data = buffer;
    size_t total_sent = 0;
    while (total_sent < length)
    {
        ssize_t sent = send(sockfd, data + total_sent, length - total_sent, 0);
        if (sent <= 0)
            return -1;
        total_sent += sent;
    }
    return 0;
}

static int recv_all(int sockfd, void *buffer, size_t length)
{
    char *data = buffer;
    size_t total_received = 0;
    while (total_received < length)
    {
        ssize_t received = recv(sockfd, data + total_received, length - total_received, 0);
        if (received <= 0)
            return -1;
        total_received += received;
    }
    return 0;
}

static int send_message(int sockfd, const char *message)
{
    uint32_t len = (uint32_t)strlen(message);
    if (len > MAX_MESSAGE_LEN)
        return -1;
    uint32_t net_len = htonl(len);
    if (send_all(sockfd, &net_len, sizeof(net_len)) < 0)
        return -1;
    if (send_all(sockfd, message, len) < 0)
        return -1;
    return 0;
}

static int recv_message(int sockfd, char *buffer, size_t buffer_size)
{
    uint32_t net_len;
    if (recv_all(sockfd, &net_len, sizeof(net_len)) < 0)
        return -1;
    uint32_t len = ntohl(net_len);
    if (len == 0 || len > buffer_size - 1)
        return -1;
    if (recv_all(sockfd, buffer, len) < 0)
        return -1;
    buffer[len] = '\0';
    return 0;
}

static void save_users_locked(void)
{
    char path[256];
    snprintf(path, sizeof(path), "%s/users.dat", DATA_DIR);
    FILE *file = fopen(path, "w");
    if (!file)
        return;
    for (int i = 0; i < user_count; ++i)
        fprintf(file, "%d|%s\n", users[i].id, users[i].username);
    fclose(file);
}

static void save_groups_locked(void)
{
    char path[256];
    snprintf(path, sizeof(path), "%s/groups.dat", DATA_DIR);
    FILE *file = fopen(path, "w");
    if (!file)
        return;
    for (int i = 0; i < group_count; ++i)
        fprintf(file, "%d|%s|%d|%d\n", groups[i].id, groups[i].name, groups[i].owner_id, groups[i].active);
    fclose(file);
}

static void save_group_members_locked(void)
{
    char path[256];
    snprintf(path, sizeof(path), "%s/group_members.dat", DATA_DIR);
    FILE *file = fopen(path, "w");
    if (!file)
        return;
    for (int i = 0; i < group_count; ++i)
    {
        for (int j = 0; j < groups[i].member_count; ++j)
            fprintf(file, "%d|%d\n", groups[i].id, groups[i].member_ids[j]);
    }
    fclose(file);
}

static void save_messages_locked(void)
{
    char path[256];
    snprintf(path, sizeof(path), "%s/messages.dat", DATA_DIR);
    FILE *file = fopen(path, "w");
    if (!file)
        return;
    for (int i = 0; i < message_count; ++i)
    {
        char content[MAX_MESSAGE_LEN];
        sanitize_text(content, messages[i].content, sizeof(content));
        fprintf(file, "%ld|%d|%d|%d|%ld|%d|%s\n",
                messages[i].message_id,
                messages[i].sender_id,
                messages[i].receiver_id,
                messages[i].group_id,
                (long)messages[i].timestamp,
                messages[i].status,
                content);
    }
    fclose(file);
}

static void save_deliveries_locked(void)
{
    char path[256];
    snprintf(path, sizeof(path), "%s/group_deliveries.dat", DATA_DIR);
    FILE *file = fopen(path, "w");
    if (!file)
        return;
    for (int i = 0; i < delivery_count; ++i)
        fprintf(file, "%ld|%d|%d\n", deliveries[i].message_id, deliveries[i].recipient_id, deliveries[i].status);
    fclose(file);
}

static void save_invites_locked(void)
{
    char path[256];
    snprintf(path, sizeof(path), "%s/invites.dat", DATA_DIR);
    FILE *file = fopen(path, "w");
    if (!file)
        return;
    for (int i = 0; i < invite_count; ++i)
        fprintf(file, "%d|%d|%d|%d\n", invites[i].group_id, invites[i].invitee_id, invites[i].inviter_id, invites[i].status);
    fclose(file);
}

static void save_all_data(void)
{
    pthread_mutex_lock(&users_mutex);
    save_users_locked();
    pthread_mutex_unlock(&users_mutex);
    pthread_mutex_lock(&groups_mutex);
    save_groups_locked();
    save_group_members_locked();
    pthread_mutex_unlock(&groups_mutex);
    pthread_mutex_lock(&messages_mutex);
    save_messages_locked();
    save_deliveries_locked();
    pthread_mutex_unlock(&messages_mutex);
    pthread_mutex_lock(&invites_mutex);
    save_invites_locked();
    pthread_mutex_unlock(&invites_mutex);
}

static void load_users(void)
{
    char path[256];
    snprintf(path, sizeof(path), "%s/users.dat", DATA_DIR);
    FILE *file = fopen(path, "r");
    if (!file)
        return;
    char line[256];
    while (fgets(line, sizeof(line), file) && user_count < MAX_USERS)
    {
        char *saveptr = NULL;
        char *token = strtok_r(line, "|\n", &saveptr);
        if (!token)
            continue;
        users[user_count].id = atoi(token);
        token = strtok_r(NULL, "|\n", &saveptr);
        if (!token)
            continue;
        strncpy(users[user_count].username, token, sizeof(users[user_count].username) - 1);
        users[user_count].username[sizeof(users[user_count].username) - 1] = '\0';
        users[user_count].connected = 0;
        users[user_count].online = 0;
        if (users[user_count].id >= next_user_id)
            next_user_id = users[user_count].id + 1;
        user_count++;
    }
    fclose(file);
}

static void load_groups(void)
{
    char path[256];
    snprintf(path, sizeof(path), "%s/groups.dat", DATA_DIR);
    FILE *file = fopen(path, "r");
    if (!file)
        return;
    char line[256];
    while (fgets(line, sizeof(line), file) && group_count < MAX_GROUPS)
    {
        char *saveptr = NULL;
        char *token = strtok_r(line, "|\n", &saveptr);
        if (!token)
            continue;
        groups[group_count].id = atoi(token);
        token = strtok_r(NULL, "|\n", &saveptr);
        if (!token)
            continue;
        strncpy(groups[group_count].name, token, sizeof(groups[group_count].name) - 1);
        groups[group_count].name[sizeof(groups[group_count].name) - 1] = '\0';
        token = strtok_r(NULL, "|\n", &saveptr);
        if (!token)
            continue;
        groups[group_count].owner_id = atoi(token);
        token = strtok_r(NULL, "|\n", &saveptr);
        groups[group_count].active = token ? atoi(token) : 1;
        groups[group_count].member_count = 0;
        if (groups[group_count].id >= next_group_id)
            next_group_id = groups[group_count].id + 1;
        group_count++;
    }
    fclose(file);
}

static void load_group_members(void)
{
    char path[256];
    snprintf(path, sizeof(path), "%s/group_members.dat", DATA_DIR);
    FILE *file = fopen(path, "r");
    if (!file)
        return;
    char line[256];
    while (fgets(line, sizeof(line), file))
    {
        char *saveptr = NULL;
        char *token = strtok_r(line, "|\n", &saveptr);
        if (!token)
            continue;
        int gid = atoi(token);
        token = strtok_r(NULL, "|\n", &saveptr);
        if (!token)
            continue;
        int uid = atoi(token);
        for (int i = 0; i < group_count; ++i)
        {
            if (groups[i].id == gid && groups[i].member_count < MAX_GROUP_MEMBERS)
                groups[i].member_ids[groups[i].member_count++] = uid;
        }
    }
    fclose(file);
}

static void load_messages(void)
{
    char path[256];
    snprintf(path, sizeof(path), "%s/messages.dat", DATA_DIR);
    FILE *file = fopen(path, "r");
    if (!file)
        return;
    char line[2048];
    while (fgets(line, sizeof(line), file) && message_count < MAX_MESSAGES)
    {
        char *saveptr = NULL;
        char *token = strtok_r(line, "|\n", &saveptr);
        if (!token)
            continue;
        messages[message_count].message_id = atol(token);
        token = strtok_r(NULL, "|\n", &saveptr);
        if (!token)
            continue;
        messages[message_count].sender_id = atoi(token);
        token = strtok_r(NULL, "|\n", &saveptr);
        if (!token)
            continue;
        messages[message_count].receiver_id = atoi(token);
        token = strtok_r(NULL, "|\n", &saveptr);
        if (!token)
            continue;
        messages[message_count].group_id = atoi(token);
        token = strtok_r(NULL, "|\n", &saveptr);
        if (!token)
            continue;
        messages[message_count].timestamp = (time_t)atol(token);
        token = strtok_r(NULL, "|\n", &saveptr);
        if (!token)
            continue;
        messages[message_count].status = atoi(token);
        token = strtok_r(NULL, "\n", &saveptr);
        if (!token)
            token = "";
        strncpy(messages[message_count].content, token, sizeof(messages[message_count].content) - 1);
        messages[message_count].content[sizeof(messages[message_count].content) - 1] = '\0';
        if (messages[message_count].message_id >= next_message_id)
            next_message_id = messages[message_count].message_id + 1;
        message_count++;
    }
    fclose(file);
}

static void load_deliveries(void)
{
    char path[256];
    snprintf(path, sizeof(path), "%s/group_deliveries.dat", DATA_DIR);
    FILE *file = fopen(path, "r");
    if (!file)
        return;
    char line[256];
    while (fgets(line, sizeof(line), file) && delivery_count < MAX_GROUP_DELIVERIES)
    {
        char *saveptr = NULL;
        char *token = strtok_r(line, "|\n", &saveptr);
        if (!token)
            continue;
        deliveries[delivery_count].message_id = atol(token);
        token = strtok_r(NULL, "|\n", &saveptr);
        if (!token)
            continue;
        deliveries[delivery_count].recipient_id = atoi(token);
        token = strtok_r(NULL, "|\n", &saveptr);
        deliveries[delivery_count].status = token ? atoi(token) : STATUS_SENT;
        delivery_count++;
    }
    fclose(file);
}

static void load_invites(void)
{
    char path[256];
    snprintf(path, sizeof(path), "%s/invites.dat", DATA_DIR);
    FILE *file = fopen(path, "r");
    if (!file)
        return;
    char line[256];
    while (fgets(line, sizeof(line), file) && invite_count < MAX_INVITES)
    {
        char *saveptr = NULL;
        char *token = strtok_r(line, "|\n", &saveptr);
        if (!token)
            continue;
        invites[invite_count].group_id = atoi(token);
        token = strtok_r(NULL, "|\n", &saveptr);
        if (!token)
            continue;
        invites[invite_count].invitee_id = atoi(token);
        token = strtok_r(NULL, "|\n", &saveptr);
        if (!token)
            continue;
        invites[invite_count].inviter_id = atoi(token);
        token = strtok_r(NULL, "|\n", &saveptr);
        invites[invite_count].status = token ? atoi(token) : INVITE_PENDING;
        invite_count++;
    }
    fclose(file);
}

static void load_all_data(void)
{
    if (ensure_data_directory() < 0)
        fprintf(stderr, "Warning: cannot create data directory '%s'\n", DATA_DIR);
    load_users();
    load_groups();
    load_group_members();
    load_messages();
    load_deliveries();
    load_invites();
}

static User *find_user_by_name(const char *username)
{
    for (int i = 0; i < user_count; ++i)
        if (strcmp(users[i].username, username) == 0)
            return &users[i];
    return NULL;
}

static User *find_user_by_id(int id)
{
    for (int i = 0; i < user_count; ++i)
        if (users[i].id == id)
            return &users[i];
    return NULL;
}

static Group *find_group_by_name(const char *name)
{
    for (int i = 0; i < group_count; ++i)
        if (groups[i].active && strcmp(groups[i].name, name) == 0)
            return &groups[i];
    return NULL;
}

static Group *find_group_by_id(int id)
{
    for (int i = 0; i < group_count; ++i)
        if (groups[i].id == id)
            return &groups[i];
    return NULL;
}

static int is_group_member(const Group *group, int user_id)
{
    for (int i = 0; i < group->member_count; ++i)
        if (group->member_ids[i] == user_id)
            return 1;
    return 0;
}

static int add_group_member(Group *group, int user_id)
{
    if (group->member_count >= MAX_GROUP_MEMBERS)
        return -1;
    if (is_group_member(group, user_id))
        return 0;
    group->member_ids[group->member_count++] = user_id;
    return 0;
}

static int remove_group_member(Group *group, int user_id)
{
    int found = 0;
    for (int i = 0; i < group->member_count; ++i)
    {
        if (group->member_ids[i] == user_id)
        {
            found = 1;
        }
        if (found && i + 1 < group->member_count)
        {
            group->member_ids[i] = group->member_ids[i + 1];
        }
    }
    if (found)
        group->member_count--;
    return found ? 0 : -1;
}

static void report_error(int sockfd, const char *text)
{
    char buffer[MAX_MESSAGE_LEN];
    snprintf(buffer, sizeof(buffer), "ERROR %s", text);
    send_message(sockfd, buffer);
}

static void report_info(int sockfd, const char *text)
{
    char buffer[MAX_MESSAGE_LEN];
    snprintf(buffer, sizeof(buffer), "INFO %s", text);
    send_message(sockfd, buffer);
}

static int send_to_user(int user_id, const char *text)
{
    User *recipient = find_user_by_id(user_id);
    if (!recipient || !recipient->connected)
        return -1;
    return send_message(recipient->socket, text);
}

static void save_message_locked(void)
{
    save_messages_locked();
}

static void save_delivery_locked(void)
{
    save_deliveries_locked();
}

static void deliver_pending_group_messages(int user_id)
{
    pthread_mutex_lock(&messages_mutex);
    for (int i = 0; i < delivery_count; ++i)
    {
        if (deliveries[i].recipient_id != user_id || deliveries[i].status != STATUS_SENT)
            continue;
        long msg_id = deliveries[i].message_id;
        Message *msg = NULL;
        for (int j = 0; j < message_count; ++j)
            if (messages[j].message_id == msg_id)
                msg = &messages[j];
        if (!msg || msg->group_id == 0)
            continue;
        User *recipient = find_user_by_id(user_id);
        if (!recipient || !recipient->connected || !recipient->online)
            continue;
        Group *group = find_group_by_id(msg->group_id);
        User *sender = find_user_by_id(msg->sender_id);
        if (!group || !sender)
            continue;
        char payload[MAX_MESSAGE_LEN];
        snprintf(payload, sizeof(payload), "GMSG %ld %s %s %s", msg->message_id, group->name, sender->username, msg->content);
        if (send_message(recipient->socket, payload) == 0)
        {
            // keep status until ack arrives
        }
    }
    pthread_mutex_unlock(&messages_mutex);
}

static void deliver_pending_private_messages(int user_id)
{
    pthread_mutex_lock(&messages_mutex);
    for (int i = 0; i < message_count; ++i)
    {
        if (messages[i].receiver_id != user_id || messages[i].group_id != 0 || messages[i].status != STATUS_SENT)
            continue;
        User *recipient = find_user_by_id(user_id);
        if (!recipient || !recipient->connected || !recipient->online)
            continue;
        User *sender = find_user_by_id(messages[i].sender_id);
        if (!sender)
            continue;
        char payload[MAX_MESSAGE_LEN];
        snprintf(payload, sizeof(payload), "MSG %ld %s %s", messages[i].message_id, sender->username, messages[i].content);
        if (send_message(recipient->socket, payload) == 0)
        {
            // still wait for ack
        }
    }
    pthread_mutex_unlock(&messages_mutex);
}

static void deliver_pending_messages(int user_id)
{
    deliver_pending_private_messages(user_id);
    deliver_pending_group_messages(user_id);
}



void help_function(int sockfd)
{
    report_info(sockfd,
        "Commands available:\n"
        "/register <username> <password>\n"
        "/login <username> <password>\n"
        "/online\n"
        "/offline\n"
        "/users\n"
        "/msg <user> <message>\n"
        "/create_group <name>\n"
        "/groups\n"
        "/group_members <group>\n"
        "/invite <group> <user>\n"
        "/accept <group>\n"
        "/reject <group>\n"
        "/join_group <group>\n"
        "/leave_group <group>\n"
        "/kick <group> <user>\n"
        "/rename_group <old> <new>\n"
        "/delete_group <group>\n"
        "/gmsg <group> <message>\n"
        "/history\n"
        "/help\n"
        "/quit");
}
static int create_user(const char *username, const char *password)
{
    if (user_count >= MAX_USERS)
        return -1;

    users[user_count].id = next_user_id++;

    strncpy(users[user_count].username,
            username,
            sizeof(users[user_count].username) - 1);

    users[user_count].username[
        sizeof(users[user_count].username) - 1] = '\0';

    strncpy(users[user_count].password,
            password,
            sizeof(users[user_count].password) - 1);

    users[user_count].password[
        sizeof(users[user_count].password) - 1] = '\0';

    users[user_count].connected = 0;
    users[user_count].online = 0;
    users[user_count].socket = -1;

    user_count++;

    save_users_locked();

    return 0;

}
static int validate_password(const char *password)
{
    if (!password || strlen(password) < 8)
        return 0;

    int upper = 0;
    int lower = 0;
    int digit = 0;

    for (int i = 0; password[i] != '\0'; i++)
    {
        if (isupper((unsigned char)password[i]))
            upper = 1;

        if (islower((unsigned char)password[i]))
            lower = 1;

        if (isdigit((unsigned char)password[i]))
            digit = 1;
    }

    return upper && lower && digit;
}
static int create_group(const char *name, int owner_id)
{
    if (group_count >= MAX_GROUPS)
        return -1;
    groups[group_count].id = next_group_id++;
    strncpy(groups[group_count].name, name, sizeof(groups[group_count].name) - 1);
    groups[group_count].name[sizeof(groups[group_count].name) - 1] = '\0';
    groups[group_count].owner_id = owner_id;
    groups[group_count].active = 1;
    groups[group_count].member_count = 0;
    groups[group_count].member_ids[groups[group_count].member_count++] = owner_id;
    group_count++;
    save_groups_locked();
    save_group_members_locked();
    return 0;
}

static void notify_group_members(Group *group, const char *message)
{
    for (int i = 0; i < group->member_count; ++i)
    {
        User *member = find_user_by_id(group->member_ids[i]);
        if (member && member->connected)
            report_info(member->socket, message);
    }
}

static void handle_register(int sockfd,
                            int *current_user_id,
                            const char *username,
                            const char *password)
{
    if (!username || username[0] == '\0' ||
        !password || password[0] == '\0')
    {
        report_error(sockfd,
                     "Usage: /register <username> <password>");
        return;
    }

    if (!validate_password(password))
    {
        report_error(sockfd,
                     "Password must be at least 8 characters "
                     "and contain uppercase, lowercase and digit.");
        return;
    }

    pthread_mutex_lock(&users_mutex);

    if (find_user_by_name(username)!= NULL)
    {
        pthread_mutex_unlock(&users_mutex);
        report_error(sockfd, "Username already exists.");
        return;
    }

    if (create_user(username, password) < 0)
    {
        pthread_mutex_unlock(&users_mutex);
        report_error(sockfd, "Unable to register new user.");
        return;
    }

    pthread_mutex_unlock(&users_mutex);

    report_info(sockfd, "Registration completed.");
}
static void handle_login(int sockfd,
                         int *current_user_id,
                         const char *username,
                         const char *password)
{
    if (!username || username[0] == '\0' ||
        !password || password[0] == '\0')
    {
        report_error(sockfd,
                     "Usage: /login <username> <password>");
        return;
    }

    pthread_mutex_lock(&users_mutex);

    User *user = find_user_by_name(username);

    if (!user)
    {
        pthread_mutex_unlock(&users_mutex);
        report_error(sockfd, "User not found.");
        return;
    }

    /* Password check */
    if (strcmp(user->password, password) != 0)
    {
        pthread_mutex_unlock(&users_mutex);
        report_error(sockfd, "Invalid password.");
        return;
    }

    if (user->connected)
    {
        pthread_mutex_unlock(&users_mutex);
        report_error(sockfd,
                     "User already logged in from another session.");
        return;
    }

    user->connected = 1;
    user->online = 1;
    user->socket = sockfd;

    *current_user_id = user->id;

    pthread_mutex_unlock(&users_mutex);

    report_info(sockfd,
                "Login successful. Status set to ONLINE.");

    deliver_pending_messages(*current_user_id);
}
static void handle_status(int sockfd, int current_user_id, int online)
{
    if (current_user_id <= 0)
    {
        report_error(sockfd, "Please login first.");
        return;
    }
    pthread_mutex_lock(&users_mutex);
    User *user = find_user_by_id(current_user_id);
    if (!user)
    {
        pthread_mutex_unlock(&users_mutex);
        report_error(sockfd, "Current user not found.");
        return;
    }
  if (online && user->online)
    {
        pthread_mutex_unlock(&users_mutex);
        report_info(sockfd, "You are already ONLINE.");
        return;
    }
 if (!online && !user->online)
    {
        pthread_mutex_unlock(&users_mutex);
        report_info(sockfd, "You are already OFFLINE.");
        return;
    }
    user->online = online;
    pthread_mutex_unlock(&users_mutex);
    report_info(sockfd, online ? "Status set to ONLINE." : "Status set to OFFLINE.");
    if (online)
        deliver_pending_messages(current_user_id);
}

static void handle_users(int sockfd)
{
    char buffer[MAX_MESSAGE_LEN];
    size_t offset = 0;
    offset += snprintf(buffer + offset, sizeof(buffer) - offset, "Users:\n");
    pthread_mutex_lock(&users_mutex);
    for (int i = 0; i < user_count; ++i)
    {
        const char *status = users[i].online ? "ONLINE" : "OFFLINE";
        offset += snprintf(buffer + offset, sizeof(buffer) - offset, "%s   %s\n", users[i].username, status);
        if (offset + 100 >= sizeof(buffer))
            break;
    }
    pthread_mutex_unlock(&users_mutex);
    send_message(sockfd, buffer);
}

static void handle_private_message(int sockfd, int current_user_id, const char *target_username, const char *text)
{
    if (current_user_id <= 0)
    {
        report_error(sockfd, "Please login first.");
        return;
    }
    if (!target_username || !text || text[0] == '\0')
    {
        report_error(sockfd, "Usage: /msg <user> <message>");
        return;
    }
    pthread_mutex_lock(&users_mutex);
    User *recipient = find_user_by_name(target_username);
    if (!recipient)
    {
        pthread_mutex_unlock(&users_mutex);
        report_error(sockfd, "User not found.");
        return;
    }
    if (recipient->id == current_user_id)
    {
        pthread_mutex_unlock(&users_mutex);
        report_error(sockfd, "Cannot message yourself.");
        return;
    }
    Message message = {0};
    message.message_id = next_message_id++;
    message.sender_id = current_user_id;
    message.receiver_id = recipient->id;
    message.group_id = 0;
    message.timestamp = time(NULL);
    message.status = STATUS_SENT;
    strncpy(message.content, text, sizeof(message.content) - 1);
    pthread_mutex_unlock(&users_mutex);

    pthread_mutex_lock(&messages_mutex);
    if (message_count < MAX_MESSAGES)
        messages[message_count++] = message;
    save_message_locked();
    pthread_mutex_unlock(&messages_mutex);

    char ack[MAX_MESSAGE_LEN];
    snprintf(ack, sizeof(ack), "Message ID: %ld SENT", message.message_id);
    report_info(sockfd, ack);

    pthread_mutex_lock(&users_mutex);
    int should_deliver = recipient->connected && recipient->online;
    int recipient_sock = recipient->socket;
    pthread_mutex_unlock(&users_mutex);

    if (should_deliver)
    {
        char payload[MAX_MESSAGE_LEN];
        User *sender = find_user_by_id(current_user_id);
        snprintf(payload, sizeof(payload), "MSG %ld %s %s", message.message_id, sender->username, message.content);
        if (send_message(recipient_sock, payload) < 0)
            fprintf(stderr, "Failed to deliver private message to %s\n", recipient->username);
    }
}

static void handle_ack(int sockfd, int current_user_id, const char *message_id_text)
{
    if (current_user_id <= 0)
        return;
    if (!message_id_text)
        return;
    long message_id = atol(message_id_text);
    if (message_id <= 0)
        return;
    pthread_mutex_lock(&messages_mutex);
    for (int i = 0; i < message_count; ++i)
    {
        if (messages[i].message_id == message_id && messages[i].group_id == 0 && messages[i].receiver_id == current_user_id)
        {
            if (messages[i].status == STATUS_SENT)
                messages[i].status = STATUS_DELIVERED;
            save_message_locked();
            pthread_mutex_unlock(&messages_mutex);
            pthread_mutex_lock(&users_mutex);
            User *sender = find_user_by_id(messages[i].sender_id);
            pthread_mutex_unlock(&users_mutex);
            if (sender && sender->connected)
            {
                char payload[MAX_MESSAGE_LEN];
                snprintf(payload, sizeof(payload), "INFO Message %ld delivered to %s.", message_id, users[current_user_id - 1].username);
                send_message(sender->socket, payload);
            }
            return;
        }
    }
    pthread_mutex_unlock(&messages_mutex);
}

static int is_invite_pending(int group_id, int invitee_id)
{
    for (int i = 0; i < invite_count; ++i)
    {
        if (invites[i].group_id == group_id && invites[i].invitee_id == invitee_id && invites[i].status == INVITE_PENDING)
            return 1;
    }
    return 0;
}

static void handle_create_group(int sockfd, int current_user_id, const char *group_name)
{
    if (current_user_id <= 0)
    {
        report_error(sockfd, "Please login first.");
        return;
    }
    if (!group_name || group_name[0] == '\0')
    {
        report_error(sockfd, "Usage: /create_group <group_name>");
        return;
    }
    pthread_mutex_lock(&groups_mutex);
    if (find_group_by_name(group_name))
    {
        pthread_mutex_unlock(&groups_mutex);
        report_error(sockfd, "Group name already exists.");
        return;
    }
    if (create_group(group_name, current_user_id) < 0)
    {
        pthread_mutex_unlock(&groups_mutex);
        report_error(sockfd, "Unable to create group.");
        return;
    }
    pthread_mutex_unlock(&groups_mutex);
    report_info(sockfd, "Group created successfully.");
}

static void handle_groups(int sockfd)
{
    char buffer[MAX_MESSAGE_LEN];
    size_t offset = snprintf(buffer, sizeof(buffer), "Groups:\n");
    pthread_mutex_lock(&groups_mutex);
    for (int i = 0; i < group_count; ++i)
    {
        if (!groups[i].active)
            continue;
        User *owner = find_user_by_id(groups[i].owner_id);
        const char *owner_name = owner ? owner->username : "unknown";
        offset += snprintf(buffer + offset, sizeof(buffer) - offset, "%s (owner: %s)\n", groups[i].name, owner_name);
        if (offset + 100 >= sizeof(buffer))
            break;
    }
    pthread_mutex_unlock(&groups_mutex);
    send_message(sockfd, buffer);
}

static void handle_group_members(int sockfd, const char *group_name)
{
    if (!group_name || group_name[0] == '\0')
    {
        report_error(sockfd, "Usage: /group_members <group_name>");
        return;
    }
    pthread_mutex_lock(&groups_mutex);
    Group *group = find_group_by_name(group_name);
    if (!group)
    {
        pthread_mutex_unlock(&groups_mutex);
        report_error(sockfd, "Group not found.");
        return;
    }
    char buffer[MAX_MESSAGE_LEN];
    size_t offset = snprintf(buffer, sizeof(buffer), "%s\n", group->name);
    for (int i = 0; i < group->member_count; ++i)
    {
        User *member = find_user_by_id(group->member_ids[i]);
        if (!member)
            continue;
        const char *status = member->online ? "ONLINE" : "OFFLINE";
        const char *role = (member->id == group->owner_id) ? "OWNER" : "MEMBER";
        offset += snprintf(buffer + offset, sizeof(buffer) - offset, "%s   %s   %s\n", member->username, status, role);
        if (offset + 100 >= sizeof(buffer))
            break;
    }
    pthread_mutex_unlock(&groups_mutex);
    send_message(sockfd, buffer);
}

static void handle_invite(int sockfd, int current_user_id, const char *group_name, const char *target_username)
{
    if (current_user_id <= 0)
    {
        report_error(sockfd, "Please login first.");
        return;
    }
    if (!group_name || !target_username)
    {
        report_error(sockfd, "Usage: /invite <group_name> <username>");
        return;
    }
    pthread_mutex_lock(&groups_mutex);
    Group *group = find_group_by_name(group_name);
    if (!group)
    {
        pthread_mutex_unlock(&groups_mutex);
        report_error(sockfd, "Group not found.");
        return;
    }
    if (group->owner_id != current_user_id)
    {
        pthread_mutex_unlock(&groups_mutex);
        report_error(sockfd, "Only the owner can invite users.");
        return;
    }
    if (is_group_member(group, current_user_id) == 0)
    {
        pthread_mutex_unlock(&groups_mutex);
        report_error(sockfd, "Only group members can invite.");
        return;
    }
    pthread_mutex_unlock(&groups_mutex);
    pthread_mutex_lock(&users_mutex);
    User *target = find_user_by_name(target_username);
    if (!target)
    {
        pthread_mutex_unlock(&users_mutex);
        report_error(sockfd, "User not found.");
        return;
    }
    pthread_mutex_unlock(&users_mutex);
    pthread_mutex_lock(&invites_mutex);
    if (is_invite_pending(group->id, target->id))
    {
        pthread_mutex_unlock(&invites_mutex);
        report_error(sockfd, "Invitation already pending.");
        return;
    }
    if (invite_count >= MAX_INVITES)
    {
        pthread_mutex_unlock(&invites_mutex);
        report_error(sockfd, "Unable to add invitation.");
        return;
    }
    invites[invite_count].group_id = group->id;
    invites[invite_count].invitee_id = target->id;
    invites[invite_count].inviter_id = current_user_id;
    invites[invite_count].status = INVITE_PENDING;
    invite_count++;
    save_invites_locked();
    pthread_mutex_unlock(&invites_mutex);
    report_info(sockfd, "Invitation created.");
    if (target->connected)
    {
        char notice[MAX_MESSAGE_LEN];
        snprintf(notice, sizeof(notice), "INFO %s invited you to %s.", users[current_user_id - 1].username, group_name);
        send_message(target->socket, notice);
    }
}

static void handle_accept_reject(int sockfd, int current_user_id, const char *group_name, int accept)
{
    if (current_user_id <= 0)
    {
        report_error(sockfd, "Please login first.");
        return;
    }
    if (!group_name)
    {
        report_error(sockfd, accept ? "Usage: /accept <group_name>" : "Usage: /reject <group_name>");
        return;
    }
    pthread_mutex_lock(&groups_mutex);
    Group *group = find_group_by_name(group_name);
    if (!group)
    {
        pthread_mutex_unlock(&groups_mutex);
        report_error(sockfd, "Group not found.");
        return;
    }
    pthread_mutex_unlock(&groups_mutex);
    pthread_mutex_lock(&invites_mutex);
    for (int i = 0; i < invite_count; ++i)
    {
        if (invites[i].group_id == group->id && invites[i].invitee_id == current_user_id && invites[i].status == INVITE_PENDING)
        {
            invites[i].status = accept ? INVITE_ACCEPTED : INVITE_REJECTED;
            save_invites_locked();
            pthread_mutex_unlock(&invites_mutex);
            if (accept)
            {
                pthread_mutex_lock(&groups_mutex);
                add_group_member(group, current_user_id);
                save_group_members_locked();
                save_groups_locked();
                pthread_mutex_unlock(&groups_mutex);
                report_info(sockfd, "Invitation accepted. You joined the group.");
            }
            else
            {
                report_info(sockfd, "Invitation rejected.");
            }
            return;
        }
    }
    pthread_mutex_unlock(&invites_mutex);
    report_error(sockfd, "No pending invitation found.");
}

static void handle_join_group(int sockfd, int current_user_id, const char *group_name)
{
    if (current_user_id <= 0)
    {
        report_error(sockfd, "Please login first.");
        return;
    }
    if (!group_name || group_name[0] == '\0')
    {
        report_error(sockfd, "Usage: /join_group <group_name>");
        return;
    }
    pthread_mutex_lock(&groups_mutex);
    Group *group = find_group_by_name(group_name);
    if (!group)
    {
        pthread_mutex_unlock(&groups_mutex);
        report_error(sockfd, "Group not found.");
        return;
    }
    if (is_group_member(group, current_user_id))
    {
        pthread_mutex_unlock(&groups_mutex);
        report_error(sockfd, "Already a member of the group.");
        return;
    }
    add_group_member(group, current_user_id);
    save_group_members_locked();
    pthread_mutex_unlock(&groups_mutex);
    report_info(sockfd, "Joined group successfully.");
}

static void handle_leave_group(int sockfd, int current_user_id, const char *group_name)
{
    if (current_user_id <= 0)
    {
        report_error(sockfd, "Please login first.");
        return;
    }
    if (!group_name || group_name[0] == '\0')
    {
        report_error(sockfd, "Usage: /leave_group <group_name>");
        return;
    }
    pthread_mutex_lock(&groups_mutex);
    Group *group = find_group_by_name(group_name);
    if (!group)
    {
        pthread_mutex_unlock(&groups_mutex);
        report_error(sockfd, "Group not found.");
        return;
    }
    if (!is_group_member(group, current_user_id))
    {
        pthread_mutex_unlock(&groups_mutex);
        report_error(sockfd, "You are not a member of this group.");
        return;
    }
    int was_owner = group->owner_id == current_user_id;
    remove_group_member(group, current_user_id);
    if (was_owner)
    {
        if (group->member_count > 0)
        {
            group->owner_id = group->member_ids[0];
            save_groups_locked();
            save_group_members_locked();
            pthread_mutex_unlock(&groups_mutex);
            report_info(sockfd, "You left the group and ownership transferred to another member.");
            return;
        }
        group->active = 0;
        save_groups_locked();
        save_group_members_locked();
        pthread_mutex_unlock(&groups_mutex);
        report_info(sockfd, "You left and the group was deleted because no members remained.");
        return;
    }
    save_group_members_locked();
    pthread_mutex_unlock(&groups_mutex);
    report_info(sockfd, "You left the group.");
}

static void handle_kick(int sockfd, int current_user_id, const char *group_name, const char *target_username)
{
    if (current_user_id <= 0)
    {
        report_error(sockfd, "Please login first.");
        return;
    }
    if (!group_name || !target_username)
    {
        report_error(sockfd, "Usage: /kick <group_name> <username>");
        return;
    }
    pthread_mutex_lock(&groups_mutex);
    Group *group = find_group_by_name(group_name);
    if (!group)
    {
        pthread_mutex_unlock(&groups_mutex);
        report_error(sockfd, "Group not found.");
        return;
    }
    if (group->owner_id != current_user_id)
    {
        pthread_mutex_unlock(&groups_mutex);
        report_error(sockfd, "Only the owner can kick members.");
        return;
    }
    pthread_mutex_unlock(&groups_mutex);
    pthread_mutex_lock(&users_mutex);
    User *target = find_user_by_name(target_username);
    pthread_mutex_unlock(&users_mutex);
    if (!target)
    {
        report_error(sockfd, "User not found.");
        return;
    }
    if (target->id == current_user_id)
    {
        report_error(sockfd, "Owner cannot kick themselves.");
        return;
    }
    pthread_mutex_lock(&groups_mutex);
    if (!is_group_member(group, target->id))
    {
        pthread_mutex_unlock(&groups_mutex);
        report_error(sockfd, "Target is not a member of the group.");
        return;
    }
    remove_group_member(group, target->id);
    save_group_members_locked();
    pthread_mutex_unlock(&groups_mutex);
    report_info(sockfd, "Member removed from group.");
    if (target->connected)
    {
        char notice[MAX_MESSAGE_LEN];
        snprintf(notice, sizeof(notice), "INFO You were removed from %s.", group_name);
        send_message(target->socket, notice);
    }
}

static void handle_rename_group(int sockfd, int current_user_id, const char *old_name, const char *new_name)
{
    if (current_user_id <= 0)
    {
        report_error(sockfd, "Please login first.");
        return;
    }
    if (!old_name || !new_name)
    {
        report_error(sockfd, "Usage: /rename_group <old_name> <new_name>");
        return;
    }
    pthread_mutex_lock(&groups_mutex);
    Group *group = find_group_by_name(old_name);
    if (!group)
    {
        pthread_mutex_unlock(&groups_mutex);
        report_error(sockfd, "Group not found.");
        return;
    }
    if (group->owner_id != current_user_id)
    {
        pthread_mutex_unlock(&groups_mutex);
        report_error(sockfd, "Only the owner can rename the group.");
        return;
    }
    if (find_group_by_name(new_name))
    {
        pthread_mutex_unlock(&groups_mutex);
        report_error(sockfd, "A group with the new name already exists.");
        return;
    }
    strncpy(group->name, new_name, sizeof(group->name) - 1);
    group->name[sizeof(group->name) - 1] = '\0';
    save_groups_locked();
    save_group_members_locked();
    pthread_mutex_unlock(&groups_mutex);
    char notice[MAX_MESSAGE_LEN];
    snprintf(notice, sizeof(notice), "Group %s was renamed to %s.", old_name, new_name);
    notify_group_members(group, notice);
    report_info(sockfd, "Group renamed successfully.");
}

static void handle_delete_group(int sockfd, int current_user_id, const char *group_name)
{
    if (current_user_id <= 0)
    {
        report_error(sockfd, "Please login first.");
        return;
    }
    if (!group_name || group_name[0] == '\0')
    {
        report_error(sockfd, "Usage: /delete_group <group_name>");
        return;
    }
    pthread_mutex_lock(&groups_mutex);
    Group *group = find_group_by_name(group_name);
    if (!group)
    {
        pthread_mutex_unlock(&groups_mutex);
        report_error(sockfd, "Group not found.");
        return;
    }
    if (group->owner_id != current_user_id)
    {
        pthread_mutex_unlock(&groups_mutex);
        report_error(sockfd, "Only the owner can delete the group.");
        return;
    }
    group->active = 0;
    save_groups_locked();
    save_group_members_locked();
    pthread_mutex_unlock(&groups_mutex);
    char notice[MAX_MESSAGE_LEN];
    snprintf(notice, sizeof(notice), "Group %s was deleted by %s.", group_name, users[current_user_id - 1].username);
    notify_group_members(group, notice);
    report_info(sockfd, "Group deleted successfully.");
}

static void handle_group_message(int sockfd, int current_user_id, const char *group_name, const char *text)
{
    if (current_user_id <= 0)
    {
        report_error(sockfd, "Please login first.");
        return;
    }
    if (!group_name || !text || text[0] == '\0')
    {
        report_error(sockfd, "Usage: /gmsg <group_name> <message>");
        return;
    }
    pthread_mutex_lock(&groups_mutex);
    Group *group = find_group_by_name(group_name);
    if (!group)
    {
        pthread_mutex_unlock(&groups_mutex);
        report_error(sockfd, "Group not found.");
        return;
    }
    if (!is_group_member(group, current_user_id))
    {
        pthread_mutex_unlock(&groups_mutex);
        report_error(sockfd, "You are not a member of this group.");
        return;
    }
    pthread_mutex_unlock(&groups_mutex);
    Message message = {0};
    message.message_id = next_message_id++;
    message.sender_id = current_user_id;
    message.receiver_id = 0;
    message.group_id = group->id;
    message.timestamp = time(NULL);
    message.status = STATUS_SENT;
    strncpy(message.content, text, sizeof(message.content) - 1);
    pthread_mutex_lock(&messages_mutex);
    if (message_count < MAX_MESSAGES)
        messages[message_count++] = message;
    save_message_locked();
    pthread_mutex_unlock(&messages_mutex);

    pthread_mutex_lock(&groups_mutex);
    for (int i = 0; i < group->member_count; ++i)
    {
        int recipient_id = group->member_ids[i];
        if (recipient_id == current_user_id)
            continue;
        if (delivery_count < MAX_GROUP_DELIVERIES)
        {
            deliveries[delivery_count].message_id = message.message_id;
            deliveries[delivery_count].recipient_id = recipient_id;
            deliveries[delivery_count].status = STATUS_SENT;
            delivery_count++;
        }
    }
    save_deliveries_locked();
    pthread_mutex_unlock(&groups_mutex);

    char ack[MAX_MESSAGE_LEN];
    snprintf(ack, sizeof(ack), "Group message ID: %ld SENT", message.message_id);
    report_info(sockfd, ack);
    pthread_mutex_lock(&groups_mutex);
    for (int i = 0; i < group->member_count; ++i)
    {
        int recipient_id = group->member_ids[i];
        if (recipient_id == current_user_id)
            continue;
        User *recipient = find_user_by_id(recipient_id);
        if (recipient && recipient->connected && recipient->online)
        {
            char payload[MAX_MESSAGE_LEN];
            snprintf(payload, sizeof(payload), "GMSG %ld %s %s %s", message.message_id, group->name, users[current_user_id - 1].username, message.content);
            send_message(recipient->socket, payload);
        }
    }
    pthread_mutex_unlock(&groups_mutex);
}

static void handle_group_ack(int sockfd, int current_user_id, const char *message_id_text)
{
    if (current_user_id <= 0 || !message_id_text)
        return;
    long message_id = atol(message_id_text);
    if (message_id <= 0)
        return;
    pthread_mutex_lock(&messages_mutex);
    for (int i = 0; i < delivery_count; ++i)
    {
        if (deliveries[i].message_id == message_id && deliveries[i].recipient_id == current_user_id)
        {
            if (deliveries[i].status == STATUS_SENT)
                deliveries[i].status = STATUS_DELIVERED;
            save_deliveries_locked();
            break;
        }
    }
    pthread_mutex_unlock(&messages_mutex);
}

static void logout_current_user(int current_user_id)
{
    if (current_user_id <= 0)
        return;
    pthread_mutex_lock(&users_mutex);
    User *user = find_user_by_id(current_user_id);
    if (user)
    {
        user->connected = 0;
        user->online = 0;
    }
    pthread_mutex_unlock(&users_mutex);
}
void show_contacts(int sockfd, int current_user_id)
{
    User *user = find_user_by_id(current_user_id);

    if (user == NULL)
    {
        report_error(sockfd, "User not found.");
        return;
    }

    if (user->contact_count == 0)
    {
        report_info(sockfd, "Your contact list is empty.");
        return;
    }

    char response[4096];
    response[0] = '\0';

    for (int i = 0; i < user->contact_count; i++)
    {
        User *contact = find_user_by_id(user->contacts[i]);

        if (contact != NULL)
        {
            char line[256];

            snprintf(line, sizeof(line),
                     "ID: %d | Username: %s | Status: %s\n",
                     contact->id,
                     contact->username,
                     contact->online ? "ONLINE" : "OFFLINE");

            strncat(response, line,
                    sizeof(response) - strlen(response) - 1);
        }
    }

    send(sockfd, response, strlen(response), 0);
}

static void handle_command(int sockfd, int *current_user_id, const char *message)
{
    char buffer[MAX_MESSAGE_LEN];
    strncpy(buffer, message, sizeof(buffer));
    buffer[sizeof(buffer) - 1] = '\0';
    char *saveptr = NULL;
    char *command = strtok_r(buffer, " ", &saveptr);
    if (!command)
        return;

    if (strncmp(command, "/register", 9) == 0)
{
    char *username = strtok_r(NULL, " ", &saveptr);
    char *password = strtok_r(NULL, " ", &saveptr);

    handle_register(sockfd, current_user_id, username, password);
    return;
}
    if (strncmp(command, "/login", 6) == 0)
{
    char *username = strtok_r(NULL, " ", &saveptr);
    char *password = strtok_r(NULL, " ", &saveptr);

    handle_login(sockfd, current_user_id, username, password);
    return;
}
    if (strcmp(command, "/online") == 0)
    {
        handle_status(sockfd, *current_user_id, 1);
        return;
    }
    if (strcmp(command, "/offline") == 0)
    {
        handle_status(sockfd, *current_user_id, 0);
        return;
    }
    if (strcmp(command, "/users") == 0)
    {
        handle_users(sockfd);
        return;
    }
    if (strcmp(command, "/quit") == 0)
    {
        report_info(sockfd, "Goodbye from server.");
        return;
    }
    if (strcmp(command, "/msg") == 0)
    {
        char *target = strtok_r(NULL, " ", &saveptr);
        char *text = saveptr;
        handle_private_message(sockfd, *current_user_id, target, text);
        return;
    }
    if (strcmp(command, "/ack") == 0)
    {
        handle_ack(sockfd, *current_user_id, strtok_r(NULL, " ", &saveptr));
        return;
    }
    if (strcmp(command, "/create_group") == 0)
    {
        handle_create_group(sockfd, *current_user_id, strtok_r(NULL, " ", &saveptr));
        return;
    }
    if (strncmp(message, "ADD_CONTACT ", 12) == 0)
{
    int contact_id;

    if (sscanf(message + 12, "%d", &contact_id) == 1)
    {
        add_contact(*current_user_id, contact_id);
    }
    else
    {
        printf("Usage: ADD_CONTACT <user_id>\n");
    }

    return;
}if (strcmp(message, "SHOW_CONTACTS") == 0)
{
    show_contacts(sockfd, *current_user_id);
    return;
}
    if (strcmp(command, "/groups") == 0)
    {
        handle_groups(sockfd);
        return;
    }
    if (strcmp(command, "/group_members") == 0)
    {
        handle_group_members(sockfd, strtok_r(NULL, " ", &saveptr));
        return;
    }
    if (strcmp(command, "/invite") == 0)
    {
        char *group_name = strtok_r(NULL, " ", &saveptr);
        char *username = strtok_r(NULL, " ", &saveptr);
        handle_invite(sockfd, *current_user_id, group_name, username);
        return;
    }
    if (strcmp(command, "/accept") == 0)
    {
        handle_accept_reject(sockfd, *current_user_id, strtok_r(NULL, " ", &saveptr), 1);
        return;
    }
    if (strcmp(command, "/reject") == 0)
    {
        handle_accept_reject(sockfd, *current_user_id, strtok_r(NULL, " ", &saveptr), 0);
        return;
    }
    if (strcmp(command, "/join_group") == 0)
    {
        handle_join_group(sockfd, *current_user_id, strtok_r(NULL, " ", &saveptr));
        return;
    }
    if (strcmp(command, "/leave_group") == 0)
    {
        handle_leave_group(sockfd, *current_user_id, strtok_r(NULL, " ", &saveptr));
        return;
    }
    if (strcmp(command, "/kick") == 0)
    {
        char *group_name = strtok_r(NULL, " ", &saveptr);
        char *username = strtok_r(NULL, " ", &saveptr);
        handle_kick(sockfd, *current_user_id, group_name, username);
        return;
    }
    if (strcmp(command, "/rename_group") == 0)
    {
        char *old_name = strtok_r(NULL, " ", &saveptr);
        char *new_name = strtok_r(NULL, " ", &saveptr);
        handle_rename_group(sockfd, *current_user_id, old_name, new_name);
        return;
    }
    if (strcmp(command, "/delete_group") == 0)
    {
        handle_delete_group(sockfd, *current_user_id, strtok_r(NULL, " ", &saveptr));
        return;
    }
    if (strcmp(command, "/gmsg") == 0)
    {
        char *group_name = strtok_r(NULL, " ", &saveptr);
        char *text = saveptr;
        handle_group_message(sockfd, *current_user_id, group_name, text);
        return;
    }
    if (strcmp (command,"/help")==0)
   {
    help_function(sockfd);
    return;     
}
    if (strcmp(command, "/ackg") == 0)
    {
        handle_group_ack(sockfd, *current_user_id, strtok_r(NULL, " ", &saveptr));
        return;
    }
    report_error(sockfd, "Unknown command.");
}

static void *handle_client(void *arg)
{
    int client_sock = *(int *)arg;
    free(arg);
    int current_user_id = 0;
    char buffer[MAX_MESSAGE_LEN + 1];
    while (1)
    {
        int received = recv_message(client_sock, buffer, sizeof(buffer));
        if (received < 0)
            break;
        if (strcmp(buffer, "/quit") == 0)
        {
            report_info(client_sock, "Goodbye from server.");
            break;
        }
        handle_command(client_sock, &current_user_id, buffer);
    }
    logout_current_user(current_user_id);
    close(client_sock);
    return NULL;
}

int main(int argc, char *argv[])
{
    int port = DEFAULT_PORT;
    if (argc >= 2)
    {
        port = atoi(argv[1]);
        if (port <= 0 || port > 65535)
        {
            fprintf(stderr, "Invalid port number. Using default %d.\n", DEFAULT_PORT);
            port = DEFAULT_PORT;
        }
    }
    if (ensure_data_directory() < 0)
        fprintf(stderr, "Warning: could not create data directory.\n");
    load_all_data();
    int listen_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (listen_sock < 0)
    {
        perror("socket");
        return EXIT_FAILURE;
    }
    int optval = 1;
    if (setsockopt(listen_sock, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval)) < 0)
    {
        perror("setsockopt");
        close(listen_sock);
        return EXIT_FAILURE;
    }
    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(port);
    if (bind(listen_sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0)
    {
        perror("bind");
        close(listen_sock);
        return EXIT_FAILURE;
    }
    if (listen(listen_sock, BACKLOG) < 0)
    {
        perror("listen");
        close(listen_sock);
        return EXIT_FAILURE;
    }
    printf("Server listening on port %d\n", port);
    while (1)
    {
        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);
        int *client_sock = malloc(sizeof(int));
        if (!client_sock)
        {
            fprintf(stderr, "Unable to allocate memory for client socket.\n");
            continue;
        }
        *client_sock = accept(listen_sock, (struct sockaddr *)&client_addr, &client_len);
        if (*client_sock < 0)
        {
            perror("accept");
            free(client_sock);
            continue;
        }
        pthread_t thread_id;
        if (pthread_create(&thread_id, NULL, handle_client, client_sock) != 0)
        {
            perror("pthread_create");
            close(*client_sock);
            free(client_sock);
            continue;
        }
        pthread_detach(thread_id);
    }
    close(listen_sock);
    return EXIT_SUCCESS;
}
