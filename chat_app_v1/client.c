#include <arpa/inet.h>
#include <netinet/in.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#define DEFAULT_PORT 5000
#define DEFAULT_HOST "127.0.0.1"
#define MAX_MESSAGE_LEN 1024
#define MAX_GROUP_NAME_LEN 50

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

static void send_ack(int sockfd, const char *prefix, const char *message_id)
{
    char payload[MAX_MESSAGE_LEN];
    if (prefix && message_id)
    {
        snprintf(payload, sizeof(payload), "%s %s", prefix, message_id);
        send_message(sockfd, payload);
    }
}

static void print_incoming(const char *text)
{
    printf("\n%s\n> ", text);
    fflush(stdout);
}

static void process_server_message(int sockfd, const char *message)
{
    if (strncmp(message, "ERROR ", 6) == 0)
    {
        print_incoming(message);
        return;
    }
    if (strncmp(message, "INFO ", 5) == 0)
    {
        print_incoming(message);
        return;
    }
    if (strncmp(message, "MSG ", 4) == 0)
    {
        char header[MAX_MESSAGE_LEN];
        char msg_id[32];
        char sender[MAX_MESSAGE_LEN];
        const char *payload = message + 4;
        if (sscanf(payload, "%31s %1023s", msg_id, sender) >= 2)
        {
            const char *content = strchr(payload, ' ');
            if (content)
            {
                content = strchr(content + 1, ' ');
                if (content)
                    content++;
            }
            if (!content)
                content = "";
            snprintf(header, sizeof(header), "message from %s: %s", sender, content);
            print_incoming(header);
            send_ack(sockfd, "/ack", msg_id);
            return;
        }
    }
    if (strncmp(message, "GMSG ", 5) == 0)
    {
        char msg_id[32];
        char group[MAX_GROUP_NAME_LEN];
        char sender[MAX_MESSAGE_LEN];
        const char *payload = message + 5;
        if (sscanf(payload, "%31s %49s %1023s", msg_id, group, sender) >= 3)
        {
            const char *content = strchr(payload, ' ');
            if (content)
            {
                content = strchr(content + 1, ' ');
                if (content)
                {
                    content = strchr(content + 1, ' ');
                    if (content)
                        content++;
                }
            }
            if (!content)
                content = "";
            char header[MAX_MESSAGE_LEN];
            snprintf(header, sizeof(header), "Group %s message from %s: %s", group, sender, content);
            print_incoming(header);
            send_ack(sockfd, "/ackg", msg_id);
            return;
        }
    }
    print_incoming(message);
}

static void *receive_thread(void *arg)
{
    int sockfd = *(int *)arg;
    char message[MAX_MESSAGE_LEN + 1];
    while (1)
    {
        if (recv_message(sockfd, message, sizeof(message)) < 0)
        {
            printf("\nDisconnected from server.\n");
            break;
        }
        process_server_message(sockfd, message);
    }
    return NULL;
}

int main(int argc, char *argv[])
{
    const char *host = DEFAULT_HOST;
    int port = DEFAULT_PORT;
    if (argc >= 2)
        host = argv[1];
    if (argc >= 3)
    {
        port = atoi(argv[2]);
        if (port <= 0 || port > 65535)
        {
            fprintf(stderr, "Invalid port number. Using default %d.\n", DEFAULT_PORT);
            port = DEFAULT_PORT;
        }
    }

    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0)
    {
        perror("socket");
        return EXIT_FAILURE;
    }

    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    if (inet_pton(AF_INET, host, &server_addr.sin_addr) <= 0)
    {
        fprintf(stderr, "Invalid host address: %s\n", host);
        close(sockfd);
        return EXIT_FAILURE;
    }

    if (connect(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0)
    {
        perror("connect");
        close(sockfd);
        return EXIT_FAILURE;
    }

    printf("Connected to %s:%d\n", host, port);
    printf("Commands available:\n");
    printf("/register <username>\n");
    printf("/login <username>\n");
    printf("/online\n");
    printf("/offline\n");
    printf("/users\n");
    printf("/msg <user> <message>\n");
    printf("/create_group <name>\n");
    printf("/groups\n");
    printf("/group_members <group>\n");
    printf("/invite <group> <user>\n");
    printf("/accept <group>\n");
    printf("/reject <group>\n");
    printf("/join_group <group>\n");
    printf("/leave_group <group>\n");
    printf("/kick <group> <user>\n");
    printf("/rename_group <old> <new>\n");
    printf("/delete_group <group>\n");
    printf("/gmsg <group> <message>\n");
    printf("/quit\n");

    pthread_t receiver;
    if (pthread_create(&receiver, NULL, receive_thread, &sockfd) != 0)
    {
        perror("pthread_create");
        close(sockfd);
        return EXIT_FAILURE;
    }

    char input[MAX_MESSAGE_LEN + 2];
    while (1)
    {
        printf("> ");
        fflush(stdout);

        if (fgets(input, sizeof(input), stdin) == NULL)
            break;

        size_t len = strcspn(input, "\n");
        input[len] = '\0';

        if (len == 0)
            continue;

        if (send_message(sockfd, input) < 0)
        {
            printf("Failed to send message.\n");
            break;
        }

        if (strcmp(input, "/quit") == 0)
            break;
    }

    close(sockfd);
    pthread_join(receiver, NULL);
    printf("Client exiting.\n");
    return EXIT_SUCCESS;
}
