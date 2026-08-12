#define _GNU_SOURCE
#include <arpa/inet.h>
#include <netinet/in.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>
#include <errno.h>
#include <mysql/mysql.h>

#define DEFAULT_PORT 5000
#define MAX_MESSAGE_LEN 1024
#define MAX_USERNAME_LEN 50
#define MAX_PASSWORD_LEN 50
#define MAX_CONTACTS 100
#define MAX_GROUP_NAME_LEN 50
#define MAX_USERS 100
#define MAX_GROUPS 50
#define MAX_GROUP_MEMBERS 100
#define MAX_MESSAGES 2000
#define MAX_GROUP_DELIVERIES 5000
#define MAX_INVITES 500
#define USER_INACTIVITY_TIMEOUT 300
#define HISTORY_PAGE_SIZE 10
#define DATA_DIR "data"

typedef enum
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
    char username[MAX_USERNAME_LEN];
    char password[MAX_PASSWORD_LEN];
    int online;
    int addr_set;
    struct sockaddr_in addr;
    time_t last_active;
    int contact_count;
    int contact_ids[MAX_CONTACTS];
    int blocked_count;
    int blocked_ids[MAX_CONTACTS];
    int muted_count;
    int muted_ids[MAX_CONTACTS];
} User;

typedef struct
{
    int id;
    char name[MAX_GROUP_NAME_LEN];
    int owner_id;
    int active;
    int member_count;
    int member_ids[MAX_GROUP_MEMBERS];
    int admin_count;
    int admin_ids[MAX_GROUP_MEMBERS];
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
MYSQL *conn = NULL;

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
static User *find_user_by_id(int id);
static Group *find_group_by_id(int id);
static long next_user_id = 1;
static int next_group_id = 1;
static long next_message_id = 1001;

static pthread_mutex_t users_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t groups_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t messages_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t invites_mutex = PTHREAD_MUTEX_INITIALIZER;

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

static MYSQL *mysql_conn = NULL;
static const char *db_host = NULL;
static const char *db_user = NULL;
static const char *db_pass = NULL;
static const char *db_name = NULL;
static unsigned int db_port = 3306;

static const char *get_env_or_default(const char *name, const char *fallback)
{
    const char *value = getenv(name);
    return (value && value[0] != '\0') ? value : fallback;
}

static int mysql_execute(const char *sql)
{
    if (!mysql_conn)
        return -1;
    if (mysql_query(mysql_conn, sql))
    {
        fprintf(stderr, "MySQL error: %s\n", mysql_error(mysql_conn));
        return -1;
    }
    return 0;
}

static void mysql_escape_string_safe(char *dst, size_t dst_size, const char *src)
{
    if (!dst_size)
        return;
    if (!src)
    {
        dst[0] = '\0';
        return;
    }
    if (!mysql_conn)
    {
        strncpy(dst, src, dst_size - 1);
        dst[dst_size - 1] = '\0';
        return;
    }
    unsigned long escaped_len = mysql_real_escape_string(mysql_conn, dst, src, (unsigned long)strlen(src));
    if (escaped_len >= dst_size)
        dst[dst_size - 1] = '\0';
    else
        dst[escaped_len] = '\0';
}

static int mysql_connect_db(void)
{
    db_host = get_env_or_default("CHAT_DB_HOST", "localhost");
    db_user = get_env_or_default("CHAT_DB_USER", "guddy_196");
    db_pass = get_env_or_default("CHAT_DB_PASS", "GT10fe$@hb");
    db_name = get_env_or_default("CHAT_DB_NAME", "chatdb");
    const char *db_port_str = getenv("CHAT_DB_PORT");
    if (db_port_str && db_port_str[0] != '\0')
        db_port = (unsigned int)atoi(db_port_str);

    mysql_conn = mysql_init(NULL);
    if (!mysql_conn)
    {
        fprintf(stderr, "MySQL init failed\n");
        return -1;
    }
    if (!mysql_real_connect(mysql_conn, db_host, db_user, db_pass, NULL, db_port, NULL, 0))
    {
        fprintf(stderr, "MySQL connect failed: %s\n", mysql_error(mysql_conn));
        mysql_close(mysql_conn);
        mysql_conn = NULL;
        return -1;
    }

    char query[512];
    snprintf(query, sizeof(query), "CREATE DATABASE IF NOT EXISTS `%s` DEFAULT CHARACTER SET utf8mb4", db_name);
    if (mysql_execute(query) < 0)
        return -1;
    if (mysql_select_db(mysql_conn, db_name))
    {
        fprintf(stderr, "MySQL select db failed: %s\n", mysql_error(mysql_conn));
        return -1;
    }
    return 0;
}

static int mysql_init_schema(void)
{
    const char *queries[] = {
        "CREATE TABLE IF NOT EXISTS users (id INT AUTO_INCREMENT PRIMARY KEY, username VARCHAR(50) UNIQUE NOT NULL, password VARCHAR(50) NOT NULL, online TINYINT(1) NOT NULL DEFAULT 0, last_active BIGINT DEFAULT 0)",
        "CREATE TABLE IF NOT EXISTS contacts (user_id INT NOT NULL, contact_id INT NOT NULL, PRIMARY KEY (user_id, contact_id))",
        "CREATE TABLE IF NOT EXISTS groups (id INT AUTO_INCREMENT PRIMARY KEY, name VARCHAR(50) UNIQUE NOT NULL, owner_id INT NOT NULL, active TINYINT(1) NOT NULL DEFAULT 1)",
        "CREATE TABLE IF NOT EXISTS group_members (group_id INT NOT NULL, user_id INT NOT NULL, PRIMARY KEY (group_id, user_id))",
        "CREATE TABLE IF NOT EXISTS messages (id BIGINT AUTO_INCREMENT PRIMARY KEY, sender_id INT NOT NULL, receiver_id INT NOT NULL, group_id INT NOT NULL, content VARCHAR(1024) NOT NULL, timestamp BIGINT NOT NULL, status TINYINT(1) NOT NULL)",
        "CREATE TABLE IF NOT EXISTS group_deliveries (message_id BIGINT NOT NULL, recipient_id INT NOT NULL, status TINYINT(1) NOT NULL, PRIMARY KEY (message_id, recipient_id))",
        "CREATE TABLE IF NOT EXISTS invites (group_id INT NOT NULL, invitee_id INT NOT NULL, inviter_id INT NOT NULL, status TINYINT(1) NOT NULL, PRIMARY KEY (group_id, invitee_id))"};
    for (size_t i = 0; i < sizeof(queries) / sizeof(queries[0]); ++i)
    {
        if (mysql_execute(queries[i]) < 0)
            return -1;
    }
    return 0;
}

static int db_insert_user(const User *user)
{
    if (!mysql_conn || !user)
        return -1;
    char escaped_user[MAX_USERNAME_LEN * 2 + 1];
    char escaped_pass[MAX_PASSWORD_LEN * 2 + 1];
    mysql_escape_string_safe(escaped_user, sizeof(escaped_user), user->username);
    mysql_escape_string_safe(escaped_pass, sizeof(escaped_pass), user->password);
    char query[512];
    snprintf(query, sizeof(query), "INSERT INTO users (id, username, password, online, last_active) VALUES (%d, '%s', '%s', %d, %ld)",
             user->id, escaped_user, escaped_pass, user->online, (long)user->last_active);
    return mysql_execute(query);
}

static int db_insert_contact(int user_id, int contact_id)
{
    if (!mysql_conn)
        return -1;
    char query[256];
    snprintf(query, sizeof(query), "INSERT IGNORE INTO contacts (user_id, contact_id) VALUES (%d, %d)", user_id, contact_id);
    return mysql_execute(query);
}

static int db_insert_group(const Group *group)
{
    if (!mysql_conn || !group)
        return -1;
    char escaped_name[MAX_GROUP_NAME_LEN * 2 + 1];
    mysql_escape_string_safe(escaped_name, sizeof(escaped_name), group->name);
    char query[512];
    snprintf(query, sizeof(query), "INSERT INTO groups (id, name, owner_id, active) VALUES (%d, '%s', %d, %d)",
             group->id, escaped_name, group->owner_id, group->active);
    return mysql_execute(query);
}

static int db_insert_group_member(int group_id, int user_id)
{
    if (!mysql_conn)
        return -1;
    char query[256];
    snprintf(query, sizeof(query), "INSERT IGNORE INTO group_members (group_id, user_id) VALUES (%d, %d)", group_id, user_id);
    return mysql_execute(query);
}

static int db_remove_group_member(int group_id, int user_id)
{
    if (!mysql_conn)
        return -1;
    char query[256];
    snprintf(query, sizeof(query), "DELETE FROM group_members WHERE group_id = %d AND user_id = %d", group_id, user_id);
    return mysql_execute(query);
}

static int db_update_group_name(int group_id, const char *name)
{
    if (!mysql_conn || !name)
        return -1;
    char escaped_name[MAX_GROUP_NAME_LEN * 2 + 1];
    mysql_escape_string_safe(escaped_name, sizeof(escaped_name), name);
    char query[512];
    snprintf(query, sizeof(query), "UPDATE groups SET name = '%s' WHERE id = %d", escaped_name, group_id);
    return mysql_execute(query);
}

static int db_mark_group_inactive(int group_id)
{
    if (!mysql_conn)
        return -1;
    char query[256];
    snprintf(query, sizeof(query), "UPDATE groups SET active = 0 WHERE id = %d", group_id);
    return mysql_execute(query);
}

static int db_insert_message(const Message *message)
{
    if (!mysql_conn || !message)
        return -1;
    char escaped_content[MAX_MESSAGE_LEN * 2 + 1];
    mysql_escape_string_safe(escaped_content, sizeof(escaped_content), message->content);
    char query[2048];
    snprintf(query, sizeof(query),
             "INSERT INTO messages (id, sender_id, receiver_id, group_id, content, timestamp, status) VALUES (%ld, %d, %d, %d, '%s', %ld, %d)",
             message->message_id, message->sender_id, message->receiver_id, message->group_id,
             escaped_content, (long)message->timestamp, message->status);
    return mysql_execute(query);
}

static int db_update_message_status(long message_id, int status)
{
    if (!mysql_conn)
        return -1;
    char query[256];
    snprintf(query, sizeof(query), "UPDATE messages SET status = %d WHERE id = %ld", status, message_id);
    return mysql_execute(query);
}

static int db_insert_delivery(long message_id, int recipient_id, int status)
{
    if (!mysql_conn)
        return -1;
    char query[256];
    snprintf(query, sizeof(query), "INSERT IGNORE INTO group_deliveries (message_id, recipient_id, status) VALUES (%ld, %d, %d)",
             message_id, recipient_id, status);
    return mysql_execute(query);
}

static int db_update_delivery_status(long message_id, int recipient_id, int status)
{
    if (!mysql_conn)
        return -1;
    char query[256];
    snprintf(query, sizeof(query), "UPDATE group_deliveries SET status = %d WHERE message_id = %ld AND recipient_id = %d", status, message_id, recipient_id);
    return mysql_execute(query);
}

static int db_insert_invite(int group_id, int invitee_id, int inviter_id, int status)
{
    if (!mysql_conn)
        return -1;
    char query[256];
    snprintf(query, sizeof(query), "INSERT IGNORE INTO invites (group_id, invitee_id, inviter_id, status) VALUES (%d, %d, %d, %d)",
             group_id, invitee_id, inviter_id, status);
    return mysql_execute(query);
}

static int db_update_invite_status(int group_id, int invitee_id, int status)
{
    if (!mysql_conn)
        return -1;
    char query[256];
    snprintf(query, sizeof(query), "UPDATE invites SET status = %d WHERE group_id = %d AND invitee_id = %d", status, group_id, invitee_id);
    return mysql_execute(query);
}

static void mysql_close_db(void)
{
    if (mysql_conn)
    {
        mysql_close(mysql_conn);
        mysql_conn = NULL;
    }
}

static void load_users_from_db(void)
{
    if (!mysql_conn)
        return;
    if (mysql_query(mysql_conn, "SELECT id, username, password, online, last_active FROM users ORDER BY id"))
        return;
    MYSQL_RES *result = mysql_store_result(mysql_conn);
    if (!result)
        return;
    MYSQL_ROW row;
    while ((row = mysql_fetch_row(result)) && user_count < MAX_USERS)
    {
        users[user_count].id = atoi(row[0]);
        strncpy(users[user_count].username, row[1] ? row[1] : "", sizeof(users[user_count].username) - 1);
        users[user_count].username[sizeof(users[user_count].username) - 1] = '\0';
        strncpy(users[user_count].password, row[2] ? row[2] : "", sizeof(users[user_count].password) - 1);
        users[user_count].password[sizeof(users[user_count].password) - 1] = '\0';
        users[user_count].online = 0;
        users[user_count].addr_set = 0;
        users[user_count].contact_count = 0;
        users[user_count].last_active = row[4] ? (time_t)atol(row[4]) : 0;
        if (users[user_count].id >= next_user_id)
            next_user_id = users[user_count].id + 1;
        user_count++;
    }
    mysql_free_result(result);
}

static void load_groups_from_db(void)
{
    if (!mysql_conn)
        return;
    if (mysql_query(mysql_conn, "SELECT id, name, owner_id, active FROM groups ORDER BY id"))
        return;
    MYSQL_RES *result = mysql_store_result(mysql_conn);
    if (!result)
        return;
    MYSQL_ROW row;
    while ((row = mysql_fetch_row(result)) && group_count < MAX_GROUPS)
    {
        groups[group_count].id = atoi(row[0]);
        strncpy(groups[group_count].name, row[1] ? row[1] : "", sizeof(groups[group_count].name) - 1);
        groups[group_count].name[sizeof(groups[group_count].name) - 1] = '\0';
        groups[group_count].owner_id = atoi(row[2]);
        groups[group_count].active = row[3] ? atoi(row[3]) : 1;
        groups[group_count].member_count = 0;
        groups[group_count].admin_count = 0;
        if (groups[group_count].id >= next_group_id)
            next_group_id = groups[group_count].id + 1;
        group_count++;
    }
    mysql_free_result(result);
}

static void load_contacts_from_db(void)
{
    if (!mysql_conn)
        return;
    if (mysql_query(mysql_conn, "SELECT user_id, contact_id FROM contacts"))
        return;
    MYSQL_RES *result = mysql_store_result(mysql_conn);
    if (!result)
        return;
    MYSQL_ROW row;
    while ((row = mysql_fetch_row(result)))
    {
        int user_id = atoi(row[0]);
        int contact_id = atoi(row[1]);
        User *user = find_user_by_id(user_id);
        if (!user || user->contact_count >= MAX_CONTACTS)
            continue;
        user->contact_ids[user->contact_count++] = contact_id;
    }
    mysql_free_result(result);
}

static void load_group_members_from_db(void)
{
    if (!mysql_conn)
        return;
    if (mysql_query(mysql_conn, "SELECT group_id, user_id FROM group_members"))
        return;
    MYSQL_RES *result = mysql_store_result(mysql_conn);
    if (!result)
        return;
    MYSQL_ROW row;
    while ((row = mysql_fetch_row(result)))
    {
        int group_id = atoi(row[0]);
        int user_id = atoi(row[1]);
        Group *group = find_group_by_id(group_id);
        if (!group || group->member_count >= MAX_GROUP_MEMBERS)
            continue;
        group->member_ids[group->member_count++] = user_id;
    }
    mysql_free_result(result);
}

static void load_messages_from_db(void)
{
    if (!mysql_conn)
        return;
    if (mysql_query(mysql_conn, "SELECT id, sender_id, receiver_id, group_id, content, timestamp, status FROM messages ORDER BY id"))
        return;
    MYSQL_RES *result = mysql_store_result(mysql_conn);
    if (!result)
        return;
    MYSQL_ROW row;
    while ((row = mysql_fetch_row(result)) && message_count < MAX_MESSAGES)
    {
        messages[message_count].message_id = atol(row[0]);
        messages[message_count].sender_id = atoi(row[1]);
        messages[message_count].receiver_id = atoi(row[2]);
        messages[message_count].group_id = atoi(row[3]);
        strncpy(messages[message_count].content, row[4] ? row[4] : "", sizeof(messages[message_count].content) - 1);
        messages[message_count].content[sizeof(messages[message_count].content) - 1] = '\0';
        messages[message_count].timestamp = row[5] ? (time_t)atol(row[5]) : 0;
        messages[message_count].status = row[6] ? atoi(row[6]) : STATUS_SENT;
        if (messages[message_count].message_id >= next_message_id)
            next_message_id = messages[message_count].message_id + 1;
        message_count++;
    }
    mysql_free_result(result);
}

static void load_deliveries_from_db(void)
{
    if (!mysql_conn)
        return;
    if (mysql_query(mysql_conn, "SELECT message_id, recipient_id, status FROM group_deliveries"))
        return;
    MYSQL_RES *result = mysql_store_result(mysql_conn);
    if (!result)
        return;
    MYSQL_ROW row;
    while ((row = mysql_fetch_row(result)) && delivery_count < MAX_GROUP_DELIVERIES)
    {
        deliveries[delivery_count].message_id = atol(row[0]);
        deliveries[delivery_count].recipient_id = atoi(row[1]);
        deliveries[delivery_count].status = row[2] ? atoi(row[2]) : STATUS_SENT;
        delivery_count++;
    }
    mysql_free_result(result);
}

static void load_invites_from_db(void)
{
    if (!mysql_conn)
        return;
    if (mysql_query(mysql_conn, "SELECT group_id, invitee_id, inviter_id, status FROM invites"))
        return;
    MYSQL_RES *result = mysql_store_result(mysql_conn);
    if (!result)
        return;
    MYSQL_ROW row;
    while ((row = mysql_fetch_row(result)) && invite_count < MAX_INVITES)
    {
        invites[invite_count].group_id = atoi(row[0]);
        invites[invite_count].invitee_id = atoi(row[1]);
        invites[invite_count].inviter_id = atoi(row[2]);
        invites[invite_count].status = row[3] ? atoi(row[3]) : INVITE_PENDING;
        invite_count++;
    }
    mysql_free_result(result);
}

static void load_all_data_from_db(void)
{
    if (!mysql_conn)
        return;
    load_users_from_db();
    load_groups_from_db();
    load_group_members_from_db();
    load_contacts_from_db();
    load_messages_from_db();
    load_deliveries_from_db();
    load_invites_from_db();
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
    dst[i] = '\0';
}

static int send_message_udp(int sockfd, const char *message, const struct sockaddr_in *addr, socklen_t addrlen)
{
    uint32_t len = (uint32_t)strlen(message);
    if (len > MAX_MESSAGE_LEN)
        return -1;
    char packet[MAX_MESSAGE_LEN + sizeof(uint32_t)];
    uint32_t net_len = htonl(len);
    memcpy(packet, &net_len, sizeof(net_len));
    memcpy(packet + sizeof(net_len), message, len);
    ssize_t sent = sendto(sockfd, packet, sizeof(net_len) + len, 0,
                          (const struct sockaddr *)addr, addrlen);
    return sent == (ssize_t)(sizeof(net_len) + len) ? 0 : -1;
}

static int recv_message_udp(int sockfd, char *buffer, size_t buffer_size,
                            struct sockaddr_in *addr, socklen_t *addrlen)
{
    char packet[MAX_MESSAGE_LEN + sizeof(uint32_t)];
    ssize_t received = recvfrom(sockfd, packet, sizeof(packet), 0,
                                (struct sockaddr *)addr, addrlen);
    if (received <= 0)
        return -1;
    if (received < (ssize_t)sizeof(uint32_t))
        return -1;
    uint32_t net_len;
    memcpy(&net_len, packet, sizeof(net_len));
    uint32_t len = ntohl(net_len);
    if (len == 0 || len > buffer_size - 1 || received != (ssize_t)(sizeof(net_len) + len))
        return -1;
    memcpy(buffer, packet + sizeof(net_len), len);
    buffer[len] = '\0';
    return 0;
}

static int addr_equal(const struct sockaddr_in *a, const struct sockaddr_in *b)
{
    return a->sin_family == b->sin_family &&
           a->sin_addr.s_addr == b->sin_addr.s_addr &&
           a->sin_port == b->sin_port;
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

static User *find_user_by_addr(const struct sockaddr_in *addr)
{
    for (int i = 0; i < user_count; ++i)
        if (users[i].addr_set && addr_equal(&users[i].addr, addr))
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

static int is_group_admin(const Group *group, int user_id)
{
    if (!group)
        return 0;
    if (group->owner_id == user_id)
        return 1;
    for (int i = 0; i < group->admin_count; ++i)
        if (group->admin_ids[i] == user_id)
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

static int add_group_admin(Group *group, int user_id)
{
    if (!group || !is_group_member(group, user_id) || group->admin_count >= MAX_GROUP_MEMBERS)
        return -1;
    if (group->owner_id == user_id)
        return 0;
    for (int i = 0; i < group->admin_count; ++i)
        if (group->admin_ids[i] == user_id)
            return 0;
    group->admin_ids[group->admin_count++] = user_id;
    return 0;
}

static int remove_group_admin(Group *group, int user_id)
{
    if (!group)
        return -1;
    for (int i = 0; i < group->admin_count; ++i)
    {
        if (group->admin_ids[i] == user_id)
        {
            for (int j = i; j + 1 < group->admin_count; ++j)
                group->admin_ids[j] = group->admin_ids[j + 1];
            group->admin_count--;
            return 0;
        }
    }
    return -1;
}

static void update_user_activity(User *user, const struct sockaddr_in *addr)
{
    if (!user)
        return;
    user->last_active = time(NULL);
    if (!user->addr_set || !addr_equal(&user->addr, addr))
    {
        user->addr = *addr;
        user->addr_set = 1;
    }
    user->online = 1;
}

static void report_error(int sockfd, const char *text, const struct sockaddr_in *addr, socklen_t addrlen)
{
    char buffer[MAX_MESSAGE_LEN];
    snprintf(buffer, sizeof(buffer), "ERROR %s", text);
    send_message_udp(sockfd, buffer, addr, addrlen);
}

static void report_info(int sockfd, const char *text, const struct sockaddr_in *addr, socklen_t addrlen)
{
    char buffer[MAX_MESSAGE_LEN];
    snprintf(buffer, sizeof(buffer), "INFO %s", text);
    send_message_udp(sockfd, buffer, addr, addrlen);
}

static int create_user(const char *username, const char *password)
{
    if (user_count >= MAX_USERS)
        return -1;
    users[user_count].id = next_user_id++;
    strncpy(users[user_count].username, username, sizeof(users[user_count].username) - 1);
    users[user_count].username[sizeof(users[user_count].username) - 1] = '\0';
    strncpy(users[user_count].password, password, sizeof(users[user_count].password) - 1);
    users[user_count].password[sizeof(users[user_count].password) - 1] = '\0';
    users[user_count].online = 0;
    users[user_count].addr_set = 0;
    users[user_count].last_active = 0;
    users[user_count].contact_count = 0;
    user_count++;
    return 0;
}

static int add_contact(int user_id, int contact_id)
{
    User *user = find_user_by_id(user_id);
    if (!user)
        return -1;
    for (int i = 0; i < user->contact_count; ++i)
        if (user->contact_ids[i] == contact_id)
            return 0;
    if (user->contact_count >= MAX_CONTACTS)
        return -1;
    user->contact_ids[user->contact_count++] = contact_id;
    return 0;
}

static int remove_contact(User *user, int contact_id)
{
    if (!user)
        return -1;
    for (int i = 0; i < user->contact_count; ++i)
    {
        if (user->contact_ids[i] == contact_id)
        {
            for (int j = i; j + 1 < user->contact_count; ++j)
                user->contact_ids[j] = user->contact_ids[j + 1];
            user->contact_count--;
            return 0;
        }
    }
    return -1;
}

static int is_user_blocked(const User *recipient, int sender_id)
{
    if (!recipient)
        return 0;
    for (int i = 0; i < recipient->blocked_count; ++i)
        if (recipient->blocked_ids[i] == sender_id)
            return 1;
    return 0;
}

static int add_blocked(User *user, int target_id)
{
    if (!user)
        return -1;
    for (int i = 0; i < user->blocked_count; ++i)
        if (user->blocked_ids[i] == target_id)
            return 0;
    if (user->blocked_count >= MAX_CONTACTS)
        return -1;
    user->blocked_ids[user->blocked_count++] = target_id;
    return 0;
}

static int remove_blocked(User *user, int target_id)
{
    if (!user)
        return -1;
    for (int i = 0; i < user->blocked_count; ++i)
    {
        if (user->blocked_ids[i] == target_id)
        {
            for (int j = i; j + 1 < user->blocked_count; ++j)
                user->blocked_ids[j] = user->blocked_ids[j + 1];
            user->blocked_count--;
            return 0;
        }
    }
    return -1;
}

static int is_user_muted(const User *user, int target_id)
{
    if (!user)
        return 0;
    for (int i = 0; i < user->muted_count; ++i)
        if (user->muted_ids[i] == target_id)
            return 1;
    return 0;
}

static int add_muted(User *user, int target_id)
{
    if (!user)
        return -1;
    for (int i = 0; i < user->muted_count; ++i)
        if (user->muted_ids[i] == target_id)
            return 0;
    if (user->muted_count >= MAX_CONTACTS)
        return -1;
    user->muted_ids[user->muted_count++] = target_id;
    return 0;
}

static int remove_muted(User *user, int target_id)
{
    if (!user)
        return -1;
    for (int i = 0; i < user->muted_count; ++i)
    {
        if (user->muted_ids[i] == target_id)
        {
            for (int j = i; j + 1 < user->muted_count; ++j)
                user->muted_ids[j] = user->muted_ids[j + 1];
            user->muted_count--;
            return 0;
        }
    }
    return -1;
}

static void delete_group_deliveries(long message_id)
{
    pthread_mutex_lock(&messages_mutex);
    int write_index = 0;
    for (int i = 0; i < delivery_count; ++i)
    {
        if (deliveries[i].message_id == message_id)
            continue;
        deliveries[write_index++] = deliveries[i];
    }
    delivery_count = write_index;
    pthread_mutex_unlock(&messages_mutex);
}

static void delete_message_by_id(long message_id)
{
    pthread_mutex_lock(&messages_mutex);
    int write_index = 0;
    for (int i = 0; i < message_count; ++i)
    {
        if (messages[i].message_id == message_id)
            continue;
        messages[write_index++] = messages[i];
    }
    message_count = write_index;
    pthread_mutex_unlock(&messages_mutex);
    delete_group_deliveries(message_id);
}

static int db_delete_messages_by_clause(const char *clause)
{
    if (!mysql_conn)
        return -1;
    char query[1024];
    snprintf(query, sizeof(query), "DELETE FROM messages WHERE %s", clause);
    return mysql_execute(query);
}

static int db_delete_group_deliveries_by_message(long message_id)
{
    if (!mysql_conn)
        return -1;
    char query[256];
    snprintf(query, sizeof(query), "DELETE FROM group_deliveries WHERE message_id = %ld", message_id);
    return mysql_execute(query);
}

static void clear_private_history(int user_id, int target_id)
{
    pthread_mutex_lock(&messages_mutex);
    int write_index = 0;
    for (int i = 0; i < message_count; ++i)
    {
        int is_pair = (messages[i].group_id == 0) &&
                      ((messages[i].sender_id == user_id && messages[i].receiver_id == target_id) ||
                       (messages[i].sender_id == target_id && messages[i].receiver_id == user_id));
        if (is_pair)
        {
            if (mysql_conn)
                db_delete_group_deliveries_by_message(messages[i].message_id);
            continue;
        }
        messages[write_index++] = messages[i];
    }
    message_count = write_index;
    pthread_mutex_unlock(&messages_mutex);
    if (mysql_conn)
    {
        char clause[256];
        snprintf(clause, sizeof(clause), "group_id = 0 AND ((sender_id = %d AND receiver_id = %d) OR (sender_id = %d AND receiver_id = %d))",
                 user_id, target_id, target_id, user_id);
        db_delete_messages_by_clause(clause);
    }
}

static void clear_group_history_by_id(int group_id)
{
    pthread_mutex_lock(&messages_mutex);
    int write_index = 0;
    for (int i = 0; i < message_count; ++i)
    {
        if (messages[i].group_id == group_id)
        {
            if (mysql_conn)
                db_delete_group_deliveries_by_message(messages[i].message_id);
            continue;
        }
        messages[write_index++] = messages[i];
    }
    message_count = write_index;
    pthread_mutex_unlock(&messages_mutex);
    if (mysql_conn)
    {
        char clause[256];
        snprintf(clause, sizeof(clause), "group_id = %d", group_id);
        db_delete_messages_by_clause(clause);
        snprintf(clause, sizeof(clause), "message_id IN (SELECT id FROM messages WHERE group_id = %d)", group_id);
        mysql_execute(clause);
    }
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

static void deliver_pending_private_messages(int sockfd, int user_id)
{
    pthread_mutex_lock(&messages_mutex);
    for (int i = 0; i < message_count; ++i)
    {
        if (messages[i].receiver_id != user_id || messages[i].group_id != 0 || messages[i].status != STATUS_SENT)
            continue;
        User *recipient = find_user_by_id(user_id);
        if (!recipient || !recipient->online || !recipient->addr_set)
            continue;
        if (is_user_blocked(recipient, messages[i].sender_id) || is_user_muted(recipient, messages[i].sender_id))
            continue;
        User *sender = find_user_by_id(messages[i].sender_id);
        if (!sender)
            continue;
        char payload[MAX_MESSAGE_LEN];
        snprintf(payload, sizeof(payload), "MSG %ld %s %s", messages[i].message_id, sender->username, messages[i].content);
        send_message_udp(sockfd, payload, &recipient->addr, sizeof(recipient->addr));
    }
    pthread_mutex_unlock(&messages_mutex);
}

static void deliver_pending_group_messages(int sockfd, int user_id)
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
        if (!recipient || !recipient->online || !recipient->addr_set)
            continue;
        if (is_user_blocked(recipient, msg->sender_id) || is_user_muted(recipient, msg->sender_id))
            continue;
        Group *group = find_group_by_id(msg->group_id);
        User *sender = find_user_by_id(msg->sender_id);
        if (!group || !sender)
            continue;
        char payload[MAX_MESSAGE_LEN];
        snprintf(payload, sizeof(payload), "GMSG %ld %s %s %s", msg->message_id, group->name, sender->username, msg->content);
        send_message_udp(sockfd, payload, &recipient->addr, sizeof(recipient->addr));
    }
    pthread_mutex_unlock(&messages_mutex);
}

static void deliver_pending_messages(int sockfd, int user_id)
{
    deliver_pending_private_messages(sockfd, user_id);
    deliver_pending_group_messages(sockfd, user_id);
}

static void handle_register(int sockfd, const char *username, const char *password,
                            const struct sockaddr_in *addr, socklen_t addrlen)
{
    if (!username || !password || username[0] == '\0' || password[0] == '\0')
    {
        report_error(sockfd, "Usage: /register <username> <password>", addr, addrlen);
        return;
    }
    pthread_mutex_lock(&users_mutex);
    if (find_user_by_name(username))
    {
        pthread_mutex_unlock(&users_mutex);
        report_error(sockfd, "Username already exists.", addr, addrlen);
        return;
    }
    if (create_user(username, password) < 0)
    {
        pthread_mutex_unlock(&users_mutex);
        report_error(sockfd, "Unable to register new user.", addr, addrlen);
        return;
    }
    User *new_user = &users[user_count - 1];

    if (db_insert_user(new_user) < 0)
    {
        user_count--;
        pthread_mutex_unlock(&users_mutex);
        report_error(sockfd, "Unable to save user to database.", addr, addrlen);
        return;
    }
    pthread_mutex_unlock(&users_mutex);
    report_info(sockfd, "Registration completed.", addr, addrlen);
}

static void handle_login(int sockfd, const char *username, const char *password,
                         const struct sockaddr_in *addr, socklen_t addrlen)
{
    if (!username || !password || username[0] == '\0' || password[0] == '\0')
    {
        report_error(sockfd, "Usage: /login <username> <password>", addr, addrlen);
        return;
    }
    pthread_mutex_lock(&users_mutex);
    User *user = find_user_by_name(username);
    if (!user)
    {
        pthread_mutex_unlock(&users_mutex);
        report_error(sockfd, "User not found.", addr, addrlen);
        return;
    }
    if (strcmp(user->password, password) != 0)
    {
        pthread_mutex_unlock(&users_mutex);
        report_error(sockfd, "Incorrect password.", addr, addrlen);
        return;
    }
    user->online = 1;
    update_user_activity(user, addr);
    pthread_mutex_unlock(&users_mutex);
    printf("SERVER LOG: User '%s' logged in from %s:%d\n", username, inet_ntoa(addr->sin_addr), ntohs(addr->sin_port));
    report_info(sockfd, "Login successful. Status set to ONLINE.", addr, addrlen);
    deliver_pending_messages(sockfd, user->id);
}

static void handle_status(int sockfd, User *user, int online,
                          const struct sockaddr_in *addr, socklen_t addrlen)
{
    if (!user)
    {
        report_error(sockfd, "Please login first.", addr, addrlen);
        return;
    }
    pthread_mutex_lock(&users_mutex);
    user->online = online;
    pthread_mutex_unlock(&users_mutex);
    report_info(sockfd, online ? "Status set to ONLINE." : "Status set to OFFLINE.", addr, addrlen);
    if (online)
        deliver_pending_messages(sockfd, user->id);
}

static void handle_users(int sockfd, const struct sockaddr_in *addr, socklen_t addrlen)
{
    char buffer[MAX_MESSAGE_LEN];
    size_t offset = snprintf(buffer, sizeof(buffer), "Users:\n");
    pthread_mutex_lock(&users_mutex);
    for (int i = 0; i < user_count; ++i)
    {
        const char *status = users[i].online ? "ONLINE" : "OFFLINE";
        offset += snprintf(buffer + offset, sizeof(buffer) - offset, "%s   %s\n", users[i].username, status);
        if (offset + 100 >= sizeof(buffer))
            break;
    }
    pthread_mutex_unlock(&users_mutex);
    send_message_udp(sockfd, buffer, addr, addrlen);
}

static void handle_contacts(int sockfd, User *user, const struct sockaddr_in *addr, socklen_t addrlen)
{
    if (!user)
    {
        report_error(sockfd, "Please login first.", addr, addrlen);
        return;
    }
    char buffer[MAX_MESSAGE_LEN];
    size_t offset = snprintf(buffer, sizeof(buffer), "Contacts:\n");
    pthread_mutex_lock(&users_mutex);
    for (int i = 0; i < user->contact_count; ++i)
    {
        User *contact = find_user_by_id(user->contact_ids[i]);
        if (!contact)
            continue;
        const char *status = contact->online ? "ONLINE" : "OFFLINE";
        offset += snprintf(buffer + offset, sizeof(buffer) - offset, "%s   %s\n", contact->username, status);
        if (offset + 100 >= sizeof(buffer))
            break;
    }
    pthread_mutex_unlock(&users_mutex);
    send_message_udp(sockfd, buffer, addr, addrlen);
}

static void handle_add_contact(int sockfd, User *user, const char *target_username,
                               const struct sockaddr_in *addr, socklen_t addrlen)
{
    if (!user)
    {
        report_error(sockfd, "Please login first.", addr, addrlen);
        return;
    }
    if (!target_username || target_username[0] == '\0')
    {
        report_error(sockfd, "Usage: /add_contact <username>", addr, addrlen);
        return;
    }
    pthread_mutex_lock(&users_mutex);
    User *target = find_user_by_name(target_username);
    if (!target)
    {
        pthread_mutex_unlock(&users_mutex);
        report_error(sockfd, "User not found.", addr, addrlen);
        return;
    }
    if (target->id == user->id)
    {
        pthread_mutex_unlock(&users_mutex);
        report_error(sockfd, "Cannot add yourself as a contact.", addr, addrlen);
        return;
    }
    pthread_mutex_unlock(&users_mutex);
    if (add_contact(user->id, target->id) < 0)
    {
        report_error(sockfd, "Unable to add contact.", addr, addrlen);
        return;
    }
    report_info(sockfd, "Contact added.", addr, addrlen);
}

static void handle_remove_contact(int sockfd, User *user, const char *target_username,
                                  const struct sockaddr_in *addr, socklen_t addrlen)
{
    if (!user)
    {
        report_error(sockfd, "Please login first.", addr, addrlen);
        return;
    }
    if (!target_username || target_username[0] == '\0')
    {
        report_error(sockfd, "Usage: /remove_contact <username>", addr, addrlen);
        return;
    }
    pthread_mutex_lock(&users_mutex);
    User *target = find_user_by_name(target_username);
    pthread_mutex_unlock(&users_mutex);
    if (!target)
    {
        report_error(sockfd, "User not found.", addr, addrlen);
        return;
    }
    if (remove_contact(user, target->id) < 0)
    {
        report_error(sockfd, "Contact not found.", addr, addrlen);
        return;
    }
    report_info(sockfd, "Contact removed.", addr, addrlen);
}

static void handle_block(int sockfd, User *user, const char *target_username,
                         const struct sockaddr_in *addr, socklen_t addrlen)
{
    if (!user)
    {
        report_error(sockfd, "Please login first.", addr, addrlen);
        return;
    }
    if (!target_username || target_username[0] == '\0')
    {
        report_error(sockfd, "Usage: /block <username>", addr, addrlen);
        return;
    }
    pthread_mutex_lock(&users_mutex);
    User *target = find_user_by_name(target_username);
    pthread_mutex_unlock(&users_mutex);
    if (!target)
    {
        report_error(sockfd, "User not found.", addr, addrlen);
        return;
    }
    if (target->id == user->id)
    {
        report_error(sockfd, "Cannot block yourself.", addr, addrlen);
        return;
    }
    if (add_blocked(user, target->id) < 0)
    {
        report_error(sockfd, "Unable to block user.", addr, addrlen);
        return;
    }
    report_info(sockfd, "User blocked.", addr, addrlen);
}

static void handle_unblock(int sockfd, User *user, const char *target_username,
                           const struct sockaddr_in *addr, socklen_t addrlen)
{
    if (!user)
    {
        report_error(sockfd, "Please login first.", addr, addrlen);
        return;
    }
    if (!target_username || target_username[0] == '\0')
    {
        report_error(sockfd, "Usage: /unblock <username>", addr, addrlen);
        return;
    }
    pthread_mutex_lock(&users_mutex);
    User *target = find_user_by_name(target_username);
    pthread_mutex_unlock(&users_mutex);
    if (!target)
    {
        report_error(sockfd, "User not found.", addr, addrlen);
        return;
    }
    if (remove_blocked(user, target->id) < 0)
    {
        report_error(sockfd, "User not blocked.", addr, addrlen);
        return;
    }
    report_info(sockfd, "User unblocked.", addr, addrlen);
}

static void handle_mute(int sockfd, User *user, const char *target_username,
                        const struct sockaddr_in *addr, socklen_t addrlen)
{
    if (!user)
    {
        report_error(sockfd, "Please login first.", addr, addrlen);
        return;
    }
    if (!target_username || target_username[0] == '\0')
    {
        report_error(sockfd, "Usage: /mute <username>", addr, addrlen);
        return;
    }
    pthread_mutex_lock(&users_mutex);
    User *target = find_user_by_name(target_username);
    pthread_mutex_unlock(&users_mutex);
    if (!target)
    {
        report_error(sockfd, "User not found.", addr, addrlen);
        return;
    }
    if (target->id == user->id)
    {
        report_error(sockfd, "Cannot mute yourself.", addr, addrlen);
        return;
    }
    if (add_muted(user, target->id) < 0)
    {
        report_error(sockfd, "Unable to mute user.", addr, addrlen);
        return;
    }
    report_info(sockfd, "User muted.", addr, addrlen);
}

static void handle_unmute(int sockfd, User *user, const char *target_username,
                          const struct sockaddr_in *addr, socklen_t addrlen)
{
    if (!user)
    {
        report_error(sockfd, "Please login first.", addr, addrlen);
        return;
    }
    if (!target_username || target_username[0] == '\0')
    {
        report_error(sockfd, "Usage: /unmute <username>", addr, addrlen);
        return;
    }
    pthread_mutex_lock(&users_mutex);
    User *target = find_user_by_name(target_username);
    pthread_mutex_unlock(&users_mutex);
    if (!target)
    {
        report_error(sockfd, "User not found.", addr, addrlen);
        return;
    }
    if (remove_muted(user, target->id) < 0)
    {
        report_error(sockfd, "User not muted.", addr, addrlen);
        return;
    }
    report_info(sockfd, "User unmuted.", addr, addrlen);
}

static void handle_change_password(int sockfd, User *user, const char *old_password, const char *new_password,
                                   const struct sockaddr_in *addr, socklen_t addrlen)
{
    if (!user)
    {
        report_error(sockfd, "Please login first.", addr, addrlen);
        return;
    }
    if (!old_password || !new_password || old_password[0] == '\0' || new_password[0] == '\0')
    {
        report_error(sockfd, "Usage: /change_password <old_password> <new_password>", addr, addrlen);
        return;
    }
    if (strcmp(user->password, old_password) != 0)
    {
        report_error(sockfd, "Incorrect current password.", addr, addrlen);
        return;
    }
    strncpy(user->password, new_password, sizeof(user->password) - 1);
    user->password[sizeof(user->password) - 1] = '\0';
    if (mysql_conn)
    {
        char escaped_password[MAX_PASSWORD_LEN * 2 + 1];
        mysql_escape_string_safe(escaped_password, sizeof(escaped_password), user->password);
        char query[256];
        snprintf(query, sizeof(query), "UPDATE users SET password = '%s' WHERE id = %d", escaped_password, user->id);
        mysql_execute(query);
    }
    report_info(sockfd, "Password changed successfully.", addr, addrlen);
}

static void handle_promote(int sockfd, User *user, const char *group_name, const char *target_username,
                           const struct sockaddr_in *addr, socklen_t addrlen)
{
    if (!user)
    {
        report_error(sockfd, "Please login first.", addr, addrlen);
        return;
    }
    if (!group_name || !target_username)
    {
        report_error(sockfd, "Usage: /promote <group_name> <username>", addr, addrlen);
        return;
    }
    pthread_mutex_lock(&groups_mutex);
    Group *group = find_group_by_name(group_name);
    if (!group)
    {
        pthread_mutex_unlock(&groups_mutex);
        report_error(sockfd, "Group not found.", addr, addrlen);
        return;
    }
    if (group->owner_id != user->id)
    {
        pthread_mutex_unlock(&groups_mutex);
        report_error(sockfd, "Only the group owner can promote members.", addr, addrlen);
        return;
    }
    if (!is_group_member(group, user->id))
    {
        pthread_mutex_unlock(&groups_mutex);
        report_error(sockfd, "You are not a group member.", addr, addrlen);
        return;
    }
    pthread_mutex_unlock(&groups_mutex);
    pthread_mutex_lock(&users_mutex);
    User *target = find_user_by_name(target_username);
    pthread_mutex_unlock(&users_mutex);
    if (!target)
    {
        report_error(sockfd, "User not found.", addr, addrlen);
        return;
    }
    pthread_mutex_lock(&groups_mutex);
    if (!is_group_member(group, target->id))
    {
        pthread_mutex_unlock(&groups_mutex);
        report_error(sockfd, "Target is not a member of the group.", addr, addrlen);
        return;
    }
    if (add_group_admin(group, target->id) < 0)
    {
        pthread_mutex_unlock(&groups_mutex);
        report_error(sockfd, "Unable to promote member.", addr, addrlen);
        return;
    }
    pthread_mutex_unlock(&groups_mutex);
    report_info(sockfd, "Member promoted to admin.", addr, addrlen);
}

static void handle_demote(int sockfd, User *user, const char *group_name, const char *target_username,
                          const struct sockaddr_in *addr, socklen_t addrlen)
{
    if (!user)
    {
        report_error(sockfd, "Please login first.", addr, addrlen);
        return;
    }
    if (!group_name || !target_username)
    {
        report_error(sockfd, "Usage: /demote <group_name> <username>", addr, addrlen);
        return;
    }
    pthread_mutex_lock(&groups_mutex);
    Group *group = find_group_by_name(group_name);
    if (!group)
    {
        pthread_mutex_unlock(&groups_mutex);
        report_error(sockfd, "Group not found.", addr, addrlen);
        return;
    }
    if (group->owner_id != user->id)
    {
        pthread_mutex_unlock(&groups_mutex);
        report_error(sockfd, "Only the group owner can demote admins.", addr, addrlen);
        return;
    }
    pthread_mutex_unlock(&groups_mutex);
    pthread_mutex_lock(&users_mutex);
    User *target = find_user_by_name(target_username);
    pthread_mutex_unlock(&users_mutex);
    if (!target)
    {
        report_error(sockfd, "User not found.", addr, addrlen);
        return;
    }
    pthread_mutex_lock(&groups_mutex);
    if (!is_group_member(group, target->id))
    {
        pthread_mutex_unlock(&groups_mutex);
        report_error(sockfd, "Target is not a member of the group.", addr, addrlen);
        return;
    }
    if (remove_group_admin(group, target->id) < 0)
    {
        pthread_mutex_unlock(&groups_mutex);
        report_error(sockfd, "Target is not an admin.", addr, addrlen);
        return;
    }
    pthread_mutex_unlock(&groups_mutex);
    report_info(sockfd, "Admin privileges revoked.", addr, addrlen);
}

static void handle_help(int sockfd, const struct sockaddr_in *addr, socklen_t addrlen)
{
    char buffer[MAX_MESSAGE_LEN];
    snprintf(buffer, sizeof(buffer),
             "INFO Commands available:\n"
             "/register <username> <password>\n"
             "/login <username> <password>\n"
             "/logout\n"
             "/help\n"
             "/online /offline\n"
             "/users /contacts\n"
             "/add_contact <username> /remove_contact <username>\n"
             "/block <username> /unblock <username>\n"
             "/mute <username> /unmute <username>\n"
             "/msg <user> <message> /reply <user> <message>\n"
             "/history <username|group_name> [page]\n"
             "/clear_history <username|group_name>\n"
             "/change_password <old> <new>\n"
             "/create_group <name> /groups /group_members <name>\n"
             "/invite <group> <user> /accept <group> /reject <group>\n"
             "/join_group <group> /leave_group <group>\n"
             "/kick <group> <user> /rename_group <old> <new>\n"
             "/delete_group <group> /promote <group> <user> /demote <group> <user>\n"
             "/gmsg <group> <message>\n"
             "/quit\n");
    send_message_udp(sockfd, buffer, addr, addrlen);
}

static void handle_clear_history(int sockfd, User *user, const char *target_name,
                                 const struct sockaddr_in *addr, socklen_t addrlen)
{
    if (!user)
    {
        report_error(sockfd, "Please login first.", addr, addrlen);
        return;
    }
    if (!target_name || target_name[0] == '\0')
    {
        report_error(sockfd, "Usage: /clear_history <username|group_name>", addr, addrlen);
        return;
    }
    pthread_mutex_lock(&groups_mutex);
    Group *group = find_group_by_name(target_name);
    pthread_mutex_unlock(&groups_mutex);
    if (group)
    {
        if (group->owner_id != user->id)
        {
            report_error(sockfd, "Only the group owner can clear group history.", addr, addrlen);
            return;
        }
        clear_group_history_by_id(group->id);
        report_info(sockfd, "Group history cleared.", addr, addrlen);
        return;
    }
    pthread_mutex_lock(&users_mutex);
    User *target = find_user_by_name(target_name);
    pthread_mutex_unlock(&users_mutex);
    if (!target)
    {
        report_error(sockfd, "No such user or group.", addr, addrlen);
        return;
    }
    clear_private_history(user->id, target->id);
    report_info(sockfd, "Private history cleared.", addr, addrlen);
}

static void handle_history(int sockfd, User *user, const char *name, int page,
                           const struct sockaddr_in *addr, socklen_t addrlen)
{
    if (!user)
    {
        report_error(sockfd, "Please login first.", addr, addrlen);
        return;
    }
    if (!name || name[0] == '\0')
    {
        report_error(sockfd, "Usage: /history <username|group_name> [page]", addr, addrlen);
        return;
    }
    if (page < 1)
        page = 1;
    int matches[MAX_MESSAGES];
    int match_count = 0;
    int is_group = 0;
    Group *group = NULL;
    pthread_mutex_lock(&groups_mutex);
    group = find_group_by_name(name);
    if (group && is_group_member(group, user->id))
        is_group = 1;
    pthread_mutex_unlock(&groups_mutex);
    User *target = NULL;
    if (!is_group)
    {
        pthread_mutex_lock(&users_mutex);
        target = find_user_by_name(name);
        pthread_mutex_unlock(&users_mutex);
    }
    if (!is_group && !target)
    {
        report_error(sockfd, "No such user or group.", addr, addrlen);
        return;
    }
    pthread_mutex_lock(&messages_mutex);
    for (int i = 0; i < message_count; ++i)
    {
        if (is_group)
        {
            if (messages[i].group_id == group->id)
                matches[match_count++] = i;
        }
        else
        {
            if (messages[i].group_id == 0 &&
                ((messages[i].sender_id == user->id && messages[i].receiver_id == target->id) ||
                 (messages[i].sender_id == target->id && messages[i].receiver_id == user->id)))
            {
                matches[match_count++] = i;
            }
        }
    }
    pthread_mutex_unlock(&messages_mutex);
    if (match_count == 0)
    {
        report_info(sockfd, "No history found.", addr, addrlen);
        return;
    }
    int total_pages = (match_count + HISTORY_PAGE_SIZE - 1) / HISTORY_PAGE_SIZE;
    if (page > total_pages)
    {
        report_error(sockfd, "No more history pages.", addr, addrlen);
        return;
    }
    int start = match_count - page * HISTORY_PAGE_SIZE;
    if (start < 0)
        start = 0;
    int end = match_count - (page - 1) * HISTORY_PAGE_SIZE;
    char buffer[MAX_MESSAGE_LEN];
    size_t offset = 0;
    if (is_group)
        offset += snprintf(buffer + offset, sizeof(buffer) - offset, "History for group %s (page %d/%d):\n", group->name, page, total_pages);
    else
        offset += snprintf(buffer + offset, sizeof(buffer) - offset, "Private history with %s (page %d/%d):\n", target->username, page, total_pages);
    for (int idx = start; idx < end; ++idx)
    {
        int msg_index = matches[idx];
        pthread_mutex_lock(&messages_mutex);
        User *sender = find_user_by_id(messages[msg_index].sender_id);
        char content[MAX_MESSAGE_LEN];
        strncpy(content, messages[msg_index].content, sizeof(content) - 1);
        content[sizeof(content) - 1] = '\0';
        pthread_mutex_unlock(&messages_mutex);
        const char *sender_name = sender ? sender->username : "unknown";
        offset += snprintf(buffer + offset, sizeof(buffer) - offset, "%s: %s\n", sender_name, content);
        if (offset + 200 >= sizeof(buffer))
            break;
    }
    send_message_udp(sockfd, buffer, addr, addrlen);
}

static void handle_private_message(int sockfd, User *user, const char *target_username, const char *text,
                                   const struct sockaddr_in *addr, socklen_t addrlen)
{
    if (!user)
    {
        report_error(sockfd, "Please login first.", addr, addrlen);
        return;
    }
    if (!target_username || !text || text[0] == '\0')
    {
        report_error(sockfd, "Usage: /msg <user> <message>", addr, addrlen);
        return;
    }
    pthread_mutex_lock(&users_mutex);
    User *recipient = find_user_by_name(target_username);
    if (!recipient)
    {
        pthread_mutex_unlock(&users_mutex);
        report_error(sockfd, "User not found.", addr, addrlen);
        return;
    }
    if (recipient->id == user->id)
    {
        pthread_mutex_unlock(&users_mutex);
        report_error(sockfd, "Cannot message yourself.", addr, addrlen);
        return;
    }
    if (is_user_blocked(recipient, user->id))
    {
        pthread_mutex_unlock(&users_mutex);
        report_error(sockfd, "Your messages are blocked by that user.", addr, addrlen);
        return;
    }
    int recipient_online = recipient->online && recipient->addr_set;
    int recipient_muted = is_user_muted(recipient, user->id);
    pthread_mutex_unlock(&users_mutex);
    Message message = {0};
    message.message_id = next_message_id++;
    message.sender_id = user->id;
    message.receiver_id = recipient->id;
    message.group_id = 0;
    message.timestamp = time(NULL);
    message.status = STATUS_SENT;
    strncpy(message.content, text, sizeof(message.content) - 1);
    pthread_mutex_lock(&messages_mutex);
    if (message_count < MAX_MESSAGES)
        messages[message_count++] = message;
    if (db_insert_message(&message) < 0)
    {
        pthread_mutex_unlock(&messages_mutex);
        report_error(sockfd, "Unable to save message to database.", addr, addrlen);
        return;
    }
    pthread_mutex_unlock(&messages_mutex);
    report_info(sockfd, "Private message queued.", addr, addrlen);
    if (recipient_online && !recipient_muted)
    {
        char payload[MAX_MESSAGE_LEN];
        snprintf(payload, sizeof(payload), "MSG %ld %s %s", message.message_id, user->username, message.content);
        send_message_udp(sockfd, payload, &recipient->addr, sizeof(recipient->addr));
    }
}

static void handle_ack(int sockfd, User *user, const char *message_id_text,
                       const struct sockaddr_in *addr, socklen_t addrlen)
{
    if (!user || !message_id_text)
        return;
    long message_id = atol(message_id_text);
    if (message_id <= 0)
        return;
    pthread_mutex_lock(&messages_mutex);
    for (int i = 0; i < message_count; ++i)
    {
        if (messages[i].message_id == message_id && messages[i].group_id == 0 && messages[i].receiver_id == user->id)
        {
            if (messages[i].status == STATUS_SENT)
                messages[i].status = STATUS_DELIVERED;
            pthread_mutex_unlock(&messages_mutex);
            pthread_mutex_lock(&users_mutex);
            User *sender = find_user_by_id(messages[i].sender_id);
            pthread_mutex_unlock(&users_mutex);
            if (sender && sender->online && sender->addr_set)
            {
                char payload[MAX_MESSAGE_LEN];
                snprintf(payload, sizeof(payload), "INFO Message %ld delivered to %s.", message_id, user->username);
                send_message_udp(sockfd, payload, &sender->addr, sizeof(sender->addr));
            }
            return;
        }
    }
    pthread_mutex_unlock(&messages_mutex);
}

static void handle_group_message(int sockfd, User *user, const char *group_name, const char *text,
                                 const struct sockaddr_in *addr, socklen_t addrlen)
{
    if (!user)
    {
        report_error(sockfd, "Please login first.", addr, addrlen);
        return;
    }
    if (!group_name || !text || text[0] == '\0')
    {
        report_error(sockfd, "Usage: /gmsg <group_name> <message>", addr, addrlen);
        return;
    }
    pthread_mutex_lock(&groups_mutex);
    Group *group = find_group_by_name(group_name);
    if (!group)
    {
        pthread_mutex_unlock(&groups_mutex);
        report_error(sockfd, "Group not found.", addr, addrlen);
        return;
    }
    if (!is_group_member(group, user->id))
    {
        pthread_mutex_unlock(&groups_mutex);
        report_error(sockfd, "You are not a member of this group.", addr, addrlen);
        return;
    }
    pthread_mutex_unlock(&groups_mutex);
    Message message = {0};
    message.message_id = next_message_id++;
    message.sender_id = user->id;
    message.receiver_id = 0;
    message.group_id = group->id;
    message.timestamp = time(NULL);
    message.status = STATUS_SENT;
    strncpy(message.content, text, sizeof(message.content) - 1);
    pthread_mutex_lock(&messages_mutex);
    if (message_count < MAX_MESSAGES)
        messages[message_count++] = message;
    pthread_mutex_unlock(&messages_mutex);
    pthread_mutex_lock(&groups_mutex);
    for (int i = 0; i < group->member_count; ++i)
    {
        int recipient_id = group->member_ids[i];
        if (recipient_id == user->id)
            continue;
        User *recipient = find_user_by_id(recipient_id);
        if (recipient && is_user_blocked(recipient, user->id))
            continue;
        if (delivery_count < MAX_GROUP_DELIVERIES)
        {
            deliveries[delivery_count].message_id = message.message_id;
            deliveries[delivery_count].recipient_id = recipient_id;
            deliveries[delivery_count].status = STATUS_SENT;
            delivery_count++;
        }
    }
    pthread_mutex_unlock(&groups_mutex);
    report_info(sockfd, "Group message queued.", addr, addrlen);
    pthread_mutex_lock(&groups_mutex);
    for (int i = 0; i < group->member_count; ++i)
    {
        int recipient_id = group->member_ids[i];
        if (recipient_id == user->id)
            continue;
        User *recipient = find_user_by_id(recipient_id);
        if (!recipient || !recipient->online || !recipient->addr_set)
            continue;
        if (is_user_blocked(recipient, user->id) || is_user_muted(recipient, user->id))
            continue;
        char payload[MAX_MESSAGE_LEN];
        snprintf(payload, sizeof(payload), "GMSG %ld %s %s %s", message.message_id, group->name, user->username, message.content);
        send_message_udp(sockfd, payload, &recipient->addr, sizeof(recipient->addr));
    }
    pthread_mutex_unlock(&groups_mutex);
}

static void handle_group_ack(int sockfd, User *user, const char *message_id_text,
                             const struct sockaddr_in *addr, socklen_t addrlen)
{
    if (!user || !message_id_text)
        return;
    long message_id = atol(message_id_text);
    if (message_id <= 0)
        return;
    pthread_mutex_lock(&messages_mutex);
    for (int i = 0; i < delivery_count; ++i)
    {
        if (deliveries[i].message_id == message_id && deliveries[i].recipient_id == user->id)
        {
            if (deliveries[i].status == STATUS_SENT)
                deliveries[i].status = STATUS_DELIVERED;
            break;
        }
    }
    pthread_mutex_unlock(&messages_mutex);
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
    groups[group_count].admin_count = 0;
    group_count++;
    return 0;
}

static void handle_create_group(int sockfd, User *user, const char *group_name,
                                const struct sockaddr_in *addr, socklen_t addrlen)
{
    if (!user)
    {
        report_error(sockfd, "Please login first.", addr, addrlen);
        return;
    }
    if (!group_name || group_name[0] == '\0')
    {
        report_error(sockfd, "Usage: /create_group <group_name>", addr, addrlen);
        return;
    }
    pthread_mutex_lock(&groups_mutex);
    if (find_group_by_name(group_name))
    {
        pthread_mutex_unlock(&groups_mutex);
        report_error(sockfd, "Group name already exists.", addr, addrlen);
        return;
    }
    if (create_group(group_name, user->id) < 0)
    {
        pthread_mutex_unlock(&groups_mutex);
        report_error(sockfd, "Unable to create group.", addr, addrlen);
        return;
    }
    pthread_mutex_unlock(&groups_mutex);
    report_info(sockfd, "Group created successfully.", addr, addrlen);
}

static void handle_groups(int sockfd, const struct sockaddr_in *addr, socklen_t addrlen)
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
    send_message_udp(sockfd, buffer, addr, addrlen);
}

static void handle_group_members(int sockfd, const char *group_name,
                                 const struct sockaddr_in *addr, socklen_t addrlen)
{
    if (!group_name || group_name[0] == '\0')
    {
        report_error(sockfd, "Usage: /group_members <group_name>", addr, addrlen);
        return;
    }
    pthread_mutex_lock(&groups_mutex);
    Group *group = find_group_by_name(group_name);
    if (!group)
    {
        pthread_mutex_unlock(&groups_mutex);
        report_error(sockfd, "Group not found.", addr, addrlen);
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
    send_message_udp(sockfd, buffer, addr, addrlen);
}

static void handle_invite(int sockfd, User *user, const char *group_name, const char *target_username,
                          const struct sockaddr_in *addr, socklen_t addrlen)
{
    if (!user)
    {
        report_error(sockfd, "Please login first.", addr, addrlen);
        return;
    }
    if (!group_name || !target_username)
    {
        report_error(sockfd, "Usage: /invite <group_name> <username>", addr, addrlen);
        return;
    }
    pthread_mutex_lock(&groups_mutex);
    Group *group = find_group_by_name(group_name);
    if (!group)
    {
        pthread_mutex_unlock(&groups_mutex);
        report_error(sockfd, "Group not found.", addr, addrlen);
        return;
    }
    if (group->owner_id != user->id)
    {
        pthread_mutex_unlock(&groups_mutex);
        report_error(sockfd, "Only the owner can invite users.", addr, addrlen);
        return;
    }
    pthread_mutex_unlock(&groups_mutex);
    pthread_mutex_lock(&users_mutex);
    User *target = find_user_by_name(target_username);
    pthread_mutex_unlock(&users_mutex);
    if (!target)
    {
        report_error(sockfd, "User not found.", addr, addrlen);
        return;
    }
    pthread_mutex_lock(&invites_mutex);
    if (is_invite_pending(group->id, target->id))
    {
        pthread_mutex_unlock(&invites_mutex);
        report_error(sockfd, "Invitation already pending.", addr, addrlen);
        return;
    }
    if (invite_count >= MAX_INVITES)
    {
        pthread_mutex_unlock(&invites_mutex);
        report_error(sockfd, "Unable to add invitation.", addr, addrlen);
        return;
    }
    invites[invite_count].group_id = group->id;
    invites[invite_count].invitee_id = target->id;
    invites[invite_count].inviter_id = user->id;
    invites[invite_count].status = INVITE_PENDING;
    invite_count++;
    pthread_mutex_unlock(&invites_mutex);
    report_info(sockfd, "Invitation created.", addr, addrlen);
    if (target->online && target->addr_set)
    {
        char notice[MAX_MESSAGE_LEN];
        snprintf(notice, sizeof(notice), "INFO %s invited you to %s.", user->username, group_name);
        send_message_udp(sockfd, notice, &target->addr, sizeof(target->addr));
    }
}

static void handle_accept_reject(int sockfd, User *user, const char *group_name, int accept,
                                 const struct sockaddr_in *addr, socklen_t addrlen)
{
    if (!user)
    {
        report_error(sockfd, "Please login first.", addr, addrlen);
        return;
    }
    if (!group_name)
    {
        report_error(sockfd, accept ? "Usage: /accept <group_name>" : "Usage: /reject <group_name>", addr, addrlen);
        return;
    }
    pthread_mutex_lock(&groups_mutex);
    Group *group = find_group_by_name(group_name);
    if (!group)
    {
        pthread_mutex_unlock(&groups_mutex);
        report_error(sockfd, "Group not found.", addr, addrlen);
        return;
    }
    pthread_mutex_unlock(&groups_mutex);
    pthread_mutex_lock(&invites_mutex);
    for (int i = 0; i < invite_count; ++i)
    {
        if (invites[i].group_id == group->id && invites[i].invitee_id == user->id && invites[i].status == INVITE_PENDING)
        {
            invites[i].status = accept ? INVITE_ACCEPTED : INVITE_REJECTED;
            pthread_mutex_unlock(&invites_mutex);
            if (accept)
            {
                pthread_mutex_lock(&groups_mutex);
                add_group_member(group, user->id);
                pthread_mutex_unlock(&groups_mutex);
                report_info(sockfd, "Invitation accepted. You joined the group.", addr, addrlen);
            }
            else
            {
                report_info(sockfd, "Invitation rejected.", addr, addrlen);
            }
            return;
        }
    }
    pthread_mutex_unlock(&invites_mutex);
    report_error(sockfd, "No pending invitation found.", addr, addrlen);
}

static void handle_join_group(int sockfd, User *user, const char *group_name,
                              const struct sockaddr_in *addr, socklen_t addrlen)
{
    if (!user)
    {
        report_error(sockfd, "Please login first.", addr, addrlen);
        return;
    }
    if (!group_name || group_name[0] == '\0')
    {
        report_error(sockfd, "Usage: /join_group <group_name>", addr, addrlen);
        return;
    }
    pthread_mutex_lock(&groups_mutex);
    Group *group = find_group_by_name(group_name);
    if (!group)
    {
        pthread_mutex_unlock(&groups_mutex);
        report_error(sockfd, "Group not found.", addr, addrlen);
        return;
    }
    if (is_group_member(group, user->id))
    {
        pthread_mutex_unlock(&groups_mutex);
        report_error(sockfd, "Already a member of the group.", addr, addrlen);
        return;
    }
    add_group_member(group, user->id);
    pthread_mutex_unlock(&groups_mutex);
    report_info(sockfd, "Joined group successfully.", addr, addrlen);
}

static void handle_leave_group(int sockfd, User *user, const char *group_name,
                               const struct sockaddr_in *addr, socklen_t addrlen)
{
    if (!user)
    {
        report_error(sockfd, "Please login first.", addr, addrlen);
        return;
    }
    if (!group_name || group_name[0] == '\0')
    {
        report_error(sockfd, "Usage: /leave_group <group_name>", addr, addrlen);
        return;
    }
    pthread_mutex_lock(&groups_mutex);
    Group *group = find_group_by_name(group_name);
    if (!group)
    {
        pthread_mutex_unlock(&groups_mutex);
        report_error(sockfd, "Group not found.", addr, addrlen);
        return;
    }
    if (!is_group_member(group, user->id))
    {
        pthread_mutex_unlock(&groups_mutex);
        report_error(sockfd, "You are not a member of this group.", addr, addrlen);
        return;
    }
    int was_owner = group->owner_id == user->id;
    int found = 0;
    for (int i = 0; i < group->member_count; ++i)
    {
        if (group->member_ids[i] == user->id)
        {
            found = 1;
            for (int j = i; j + 1 < group->member_count; ++j)
                group->member_ids[j] = group->member_ids[j + 1];
            group->member_count--;
            break;
        }
    }
    if (!found)
    {
        pthread_mutex_unlock(&groups_mutex);
        report_error(sockfd, "You are not a member of this group.", addr, addrlen);
        return;
    }
    if (was_owner)
    {
        if (group->member_count > 0)
        {
            group->owner_id = group->member_ids[0];
            pthread_mutex_unlock(&groups_mutex);
            report_info(sockfd, "You left the group and ownership transferred to another member.", addr, addrlen);
            return;
        }
        group->active = 0;
        pthread_mutex_unlock(&groups_mutex);
        report_info(sockfd, "You left and the group was deleted because no members remained.", addr, addrlen);
        return;
    }
    pthread_mutex_unlock(&groups_mutex);
    report_info(sockfd, "You left the group.", addr, addrlen);
}

static void handle_kick(int sockfd, User *user, const char *group_name, const char *target_username,
                        const struct sockaddr_in *addr, socklen_t addrlen)
{
    if (!user)
    {
        report_error(sockfd, "Please login first.", addr, addrlen);
        return;
    }
    if (!group_name || !target_username)
    {
        report_error(sockfd, "Usage: /kick <group_name> <username>", addr, addrlen);
        return;
    }
    pthread_mutex_lock(&groups_mutex);
    Group *group = find_group_by_name(group_name);
    if (!group)
    {
        pthread_mutex_unlock(&groups_mutex);
        report_error(sockfd, "Group not found.", addr, addrlen);
        return;
    }
    if (group->owner_id != user->id)
    {
        pthread_mutex_unlock(&groups_mutex);
        report_error(sockfd, "Only the owner can kick members.", addr, addrlen);
        return;
    }
    pthread_mutex_unlock(&groups_mutex);
    pthread_mutex_lock(&users_mutex);
    User *target = find_user_by_name(target_username);
    pthread_mutex_unlock(&users_mutex);
    if (!target)
    {
        report_error(sockfd, "User not found.", addr, addrlen);
        return;
    }
    if (target->id == user->id)
    {
        report_error(sockfd, "Owner cannot kick themselves.", addr, addrlen);
        return;
    }
    pthread_mutex_lock(&groups_mutex);
    if (!is_group_member(group, target->id))
    {
        pthread_mutex_unlock(&groups_mutex);
        report_error(sockfd, "Target is not a member of the group.", addr, addrlen);
        return;
    }
    for (int i = 0; i < group->member_count; ++i)
    {
        if (group->member_ids[i] == target->id)
        {
            for (int j = i; j + 1 < group->member_count; ++j)
                group->member_ids[j] = group->member_ids[j + 1];
            group->member_count--;
            break;
        }
    }
    pthread_mutex_unlock(&groups_mutex);
    report_info(sockfd, "Member removed from group.", addr, addrlen);
    if (target->online && target->addr_set)
    {
        char notice[MAX_MESSAGE_LEN];
        snprintf(notice, sizeof(notice), "INFO You were removed from %s.", group_name);
        send_message_udp(sockfd, notice, &target->addr, sizeof(target->addr));
    }
}

static void handle_rename_group(int sockfd, User *user, const char *old_name, const char *new_name,
                                const struct sockaddr_in *addr, socklen_t addrlen)
{
    if (!user)
    {
        report_error(sockfd, "Please login first.", addr, addrlen);
        return;
    }
    if (!old_name || !new_name)
    {
        report_error(sockfd, "Usage: /rename_group <old_name> <new_name>", addr, addrlen);
        return;
    }
    pthread_mutex_lock(&groups_mutex);
    Group *group = find_group_by_name(old_name);
    if (!group)
    {
        pthread_mutex_unlock(&groups_mutex);
        report_error(sockfd, "Group not found.", addr, addrlen);
        return;
    }
    if (group->owner_id != user->id)
    {
        pthread_mutex_unlock(&groups_mutex);
        report_error(sockfd, "Only the owner can rename the group.", addr, addrlen);
        return;
    }
    if (find_group_by_name(new_name))
    {
        pthread_mutex_unlock(&groups_mutex);
        report_error(sockfd, "A group with the new name already exists.", addr, addrlen);
        return;
    }
    strncpy(group->name, new_name, sizeof(group->name) - 1);
    group->name[sizeof(group->name) - 1] = '\0';
    pthread_mutex_unlock(&groups_mutex);
    report_info(sockfd, "Group renamed successfully.", addr, addrlen);
}

static void handle_logout(int sockfd, User *user, const struct sockaddr_in *addr, socklen_t addrlen)
{
    if (!user)
    {
        report_error(sockfd, "You are not logged in.", addr, addrlen);
        return;
    }
    pthread_mutex_lock(&users_mutex);
    user->online = 0;
    pthread_mutex_unlock(&users_mutex);
    printf("SERVER LOG: User '%s' logged out from %s:%d\n", user->username, inet_ntoa(addr->sin_addr), ntohs(addr->sin_port));
    report_info(sockfd, "Logout successful. You are now offline.", addr, addrlen);
}

static void handle_delete_group(int sockfd, User *user, const char *group_name,
                                const struct sockaddr_in *addr, socklen_t addrlen)
{
    if (!user)
    {
        report_error(sockfd, "Please login first.", addr, addrlen);
        return;
    }
    if (!group_name || group_name[0] == '\0')
    {
        report_error(sockfd, "Usage: /delete_group <group_name>", addr, addrlen);
        return;
    }
    pthread_mutex_lock(&groups_mutex);
    Group *group = find_group_by_name(group_name);
    if (!group)
    {
        pthread_mutex_unlock(&groups_mutex);
        report_error(sockfd, "Group not found.", addr, addrlen);
        return;
    }
    if (group->owner_id != user->id)
    {
        pthread_mutex_unlock(&groups_mutex);
        report_error(sockfd, "Only the owner can delete the group.", addr, addrlen);
        return;
    }
    group->active = 0;
    pthread_mutex_unlock(&groups_mutex);
    report_info(sockfd, "Group deleted successfully.", addr, addrlen);
}

static void process_command(int sockfd, const char *message, const struct sockaddr_in *addr, socklen_t addrlen)
{
    printf("SERVER LOG: Received from %s:%d -> %s\n", inet_ntoa(addr->sin_addr), ntohs(addr->sin_port), message);

    char buffer[MAX_MESSAGE_LEN];
    strncpy(buffer, message, sizeof(buffer));
    buffer[sizeof(buffer) - 1] = '\0';
    char *saveptr = NULL;
    char *command = strtok_r(buffer, " ", &saveptr);
    if (!command)
        return;

    pthread_mutex_lock(&users_mutex);
    User *user = find_user_by_addr(addr);
    pthread_mutex_unlock(&users_mutex);

    if (strcmp(command, "/register") == 0)
    {
        char *reg_username = strtok_r(NULL, " ", &saveptr);
        char *reg_password = strtok_r(NULL, " ", &saveptr);
        handle_register(sockfd, reg_username, reg_password, addr, addrlen);
        return;
    }

    if (strcmp(command, "/login") == 0)
    {
        char *login_username = strtok_r(NULL, " ", &saveptr);
        char *login_password = strtok_r(NULL, " ", &saveptr);
        if (user != NULL)
        {
            report_error(sockfd, "You are already logged in. Please /logout first.", addr, addrlen);
            return;
        }
        handle_login(sockfd, login_username, login_password, addr, addrlen);
        return;
    }
    if (strcmp(command, "/logout") == 0)
    {
        if (user == NULL)
        {
            sendto(sockfd, "ERROR You are not logged in.\n", 29, 0, (struct sockaddr *)addr, addrlen);
            return;
        }

        handle_logout(sockfd, user, addr, addrlen);
        return;
    }

    if (strcmp(command, "/online") == 0)
    {
        handle_status(sockfd, user, 1, addr, addrlen);
        return;
    }
    if (strcmp(command, "/offline") == 0)
    {
        handle_status(sockfd, user, 0, addr, addrlen);
        return;
    }
    if (strcmp(command, "/users") == 0)
    {
        handle_users(sockfd, addr, addrlen);
        return;
    }
    if (strcmp(command, "/contacts") == 0)
    {
        handle_contacts(sockfd, user, addr, addrlen);
        return;
    }
    if (strcmp(command, "/add_contact") == 0)
    {
        handle_add_contact(sockfd, user, strtok_r(NULL, " ", &saveptr), addr, addrlen);
        return;
    }
    if (strcmp(command, "/remove_contact") == 0)
    {
        handle_remove_contact(sockfd, user, strtok_r(NULL, " ", &saveptr), addr, addrlen);
        return;
    }
    if (strcmp(command, "/block") == 0)
    {
        handle_block(sockfd, user, strtok_r(NULL, " ", &saveptr), addr, addrlen);
        return;
    }
    if (strcmp(command, "/unblock") == 0)
    {
        handle_unblock(sockfd, user, strtok_r(NULL, " ", &saveptr), addr, addrlen);
        return;
    }
    if (strcmp(command, "/mute") == 0)
    {
        handle_mute(sockfd, user, strtok_r(NULL, " ", &saveptr), addr, addrlen);
        return;
    }
    if (strcmp(command, "/unmute") == 0)
    {
        handle_unmute(sockfd, user, strtok_r(NULL, " ", &saveptr), addr, addrlen);
        return;
    }
    if (strcmp(command, "/change_password") == 0)
    {
        char *old_pw = strtok_r(NULL, " ", &saveptr);
        char *new_pw = strtok_r(NULL, " ", &saveptr);
        handle_change_password(sockfd, user, old_pw, new_pw, addr, addrlen);
        return;
    }
    if (strcmp(command, "/help") == 0)
    {
        handle_help(sockfd, addr, addrlen);
        return;
    }
    if (strcmp(command, "/history") == 0)
    {
        char *name_arg = strtok_r(NULL, " ", &saveptr);
        char *page_arg = strtok_r(NULL, " ", &saveptr);
        int page = 1;
        if (page_arg)
            page = atoi(page_arg) > 0 ? atoi(page_arg) : 1;
        handle_history(sockfd, user, name_arg, page, addr, addrlen);
        return;
    }
    if (strcmp(command, "/msg") == 0 || strcmp(command, "/reply") == 0)
    {
        char *target = strtok_r(NULL, " ", &saveptr);
        char *text = saveptr;
        handle_private_message(sockfd, user, target, text, addr, addrlen);
        return;
    }
    if (strcmp(command, "/ack") == 0)
    {
        handle_ack(sockfd, user, strtok_r(NULL, " ", &saveptr), addr, addrlen);
        return;
    }
    if (strcmp(command, "/gmsg") == 0)
    {
        char *group_name = strtok_r(NULL, " ", &saveptr);
        char *text = saveptr;
        handle_group_message(sockfd, user, group_name, text, addr, addrlen);
        return;
    }
    if (strcmp(command, "/ackg") == 0)
    {
        handle_group_ack(sockfd, user, strtok_r(NULL, " ", &saveptr), addr, addrlen);
        return;
    }
    if (strcmp(command, "/create_group") == 0)
    {
        handle_create_group(sockfd, user, strtok_r(NULL, " ", &saveptr), addr, addrlen);
        return;
    }
    if (strcmp(command, "/groups") == 0)
    {
        handle_groups(sockfd, addr, addrlen);
        return;
    }
    if (strcmp(command, "/group_members") == 0)
    {
        handle_group_members(sockfd, strtok_r(NULL, " ", &saveptr), addr, addrlen);
        return;
    }
    if (strcmp(command, "/invite") == 0)
    {
        char *group_name = strtok_r(NULL, " ", &saveptr);
        char *target_username = strtok_r(NULL, " ", &saveptr);
        handle_invite(sockfd, user, group_name, target_username, addr, addrlen);
        return;
    }
    if (strcmp(command, "/accept") == 0)
    {
        handle_accept_reject(sockfd, user, strtok_r(NULL, " ", &saveptr), 1, addr, addrlen);
        return;
    }
    if (strcmp(command, "/reject") == 0)
    {
        handle_accept_reject(sockfd, user, strtok_r(NULL, " ", &saveptr), 0, addr, addrlen);
        return;
    }
    if (strcmp(command, "/join_group") == 0)
    {
        handle_join_group(sockfd, user, strtok_r(NULL, " ", &saveptr), addr, addrlen);
        return;
    }
    if (strcmp(command, "/leave_group") == 0)
    {
        handle_leave_group(sockfd, user, strtok_r(NULL, " ", &saveptr), addr, addrlen);
        return;
    }
    if (strcmp(command, "/kick") == 0)
    {
        char *group_name = strtok_r(NULL, " ", &saveptr);
        char *target_username = strtok_r(NULL, " ", &saveptr);
        handle_kick(sockfd, user, group_name, target_username, addr, addrlen);
        return;
    }
    if (strcmp(command, "/rename_group") == 0)
    {
        char *old_name = strtok_r(NULL, " ", &saveptr);
        char *new_name = strtok_r(NULL, " ", &saveptr);
        handle_rename_group(sockfd, user, old_name, new_name, addr, addrlen);
        return;
    }
    if (strcmp(command, "/delete_group") == 0)
    {
        handle_delete_group(sockfd, user, strtok_r(NULL, " ", &saveptr), addr, addrlen);
        return;
    }
    if (strcmp(command, "/promote") == 0)
    {
        char *group_name = strtok_r(NULL, " ", &saveptr);
        char *target_username = strtok_r(NULL, " ", &saveptr);
        handle_promote(sockfd, user, group_name, target_username, addr, addrlen);
        return;
    }
    if (strcmp(command, "/demote") == 0)
    {
        char *group_name = strtok_r(NULL, " ", &saveptr);
        char *target_username = strtok_r(NULL, " ", &saveptr);
        handle_demote(sockfd, user, group_name, target_username, addr, addrlen);
        return;
    }
    if (strcmp(command, "/clear_history") == 0)
    {
        handle_clear_history(sockfd, user, strtok_r(NULL, " ", &saveptr), addr, addrlen);
        return;
    }
    if (strcmp(command, "/quit") == 0)
    {
        if (user)
        {
            pthread_mutex_lock(&users_mutex);
            user->online = 0;
            pthread_mutex_unlock(&users_mutex);
        }
        report_info(sockfd, "Goodbye.", addr, addrlen);
        return;
    }
    report_error(sockfd, "Unknown command.", addr, addrlen);
}

static void *inactivity_monitor(void *arg)
{
    int sock = *(int *)arg;
    (void)sock;
    while (1)
    {
        sleep(1);
        time_t now = time(NULL);
        pthread_mutex_lock(&users_mutex);
        for (int i = 0; i < user_count; ++i)
        {
            if (users[i].online && users[i].last_active > 0 && difftime(now, users[i].last_active) >= USER_INACTIVITY_TIMEOUT)
                users[i].online = 0;
        }
        pthread_mutex_unlock(&users_mutex);
    }
    return NULL;
}

int main(int argc, char *argv[])
{
    int port = DEFAULT_PORT;
    if (argc >= 2)
        port = atoi(argv[1]);
    if (port <= 0 || port > 65535)
        port = DEFAULT_PORT;

    if (ensure_data_directory() < 0)
        fprintf(stderr, "Warning: could not create data directory.\n");

    if (mysql_connect_db() < 0)
    {
        fprintf(stderr, "Database connection failed.\n");
        return EXIT_FAILURE;
    }

    if (mysql_init_schema() < 0)
    {
        fprintf(stderr, "Database schema initialization failed.\n");
        mysql_close_db();
        return EXIT_FAILURE;
    }

    load_all_data_from_db();
    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0)
    {
        perror("socket");
        return EXIT_FAILURE;
    }

    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(port);

    if (bind(sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0)
    {
        perror("bind");
        close(sock);
        return EXIT_FAILURE;
    }

    pthread_t thread_id;
    if (pthread_create(&thread_id, NULL, inactivity_monitor, &sock) != 0)
    {
        perror("pthread_create");
        close(sock);
        return EXIT_FAILURE;
    }
    pthread_detach(thread_id);

    printf("UDP server listening on port %d\n", port);

    char buffer[MAX_MESSAGE_LEN + 1];
    while (1)
    {
        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);
        if (recv_message_udp(sock, buffer, sizeof(buffer), &client_addr, &client_len) < 0)
            continue;
        process_command(sock, buffer, &client_addr, client_len);
        pthread_mutex_lock(&users_mutex);
        User *user = find_user_by_addr(&client_addr);
        if (user)
            update_user_activity(user, &client_addr);
        pthread_mutex_unlock(&users_mutex);
    }

    close(sock);
    return EXIT_SUCCESS;
}
