#include <arpa/inet.h>
#include <netinet/in.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#define DEFAULT_HOST "127.0.0.1"
#define DEFAULT_PORT 5000
#define MAX_MESSAGE_LEN 1024

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

static void print_incoming(const char *text)
{
    printf("\n%s\n> ", text);
    fflush(stdout);
}

static void print_help(void)
{
    printf("Commands available:\n");
    printf("/register <username> <password>\n");
    printf("/login <username> <password>\n");
    printf("/logout\n");
    printf("/help\n");
    printf("/online\n");
    printf("/offline\n");
    printf("/users\n");
    printf("/contacts\n");
    printf("/add_contact <username>\n");
    printf("/remove_contact <username>\n");
    printf("/block <username>\n");
    printf("/unblock <username>\n");
    printf("/mute <username>\n");
    printf("/unmute <username>\n");
    printf("/history <username|group_name> [page]\n");
    printf("/clear_history <username|group_name>\n");
    printf("/msg <user> <message>\n");
    printf("/reply <user> <message>\n");
    printf("/change_password <old> <new>\n");
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
    printf("/promote <group> <user>\n");
    printf("/demote <group> <user>\n");
    printf("/gmsg <group> <message>\n");
    printf("/quit\n");
}

static void process_server_message(int sockfd, const char *message, const struct sockaddr_in *server_addr, socklen_t server_len)
{
    if (strncmp(message, "ERROR ", 6) == 0 || strncmp(message, "INFO ", 5) == 0)
    {
        print_incoming(message);
        return;
    }
    if (strncmp(message, "MSG ", 4) == 0)
    {
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
            char header[MAX_MESSAGE_LEN];
            snprintf(header, sizeof(header), "Private message from %s: %s", sender, content);
            print_incoming(header);
            char ack[MAX_MESSAGE_LEN];
            snprintf(ack, sizeof(ack), "/ack %s", msg_id);
            send_message_udp(sockfd, ack, server_addr, server_len);
            return;
        }
    }
    if (strncmp(message, "GMSG ", 5) == 0)
    {
        char msg_id[32];
        char group[MAX_MESSAGE_LEN];
        char sender[MAX_MESSAGE_LEN];
        const char *payload = message + 5;
        if (sscanf(payload, "%31s %1023s %1023s", msg_id, group, sender) >= 3)
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
            char ack[MAX_MESSAGE_LEN];
            snprintf(ack, sizeof(ack), "/ackg %s", msg_id);
            send_message_udp(sockfd, ack, server_addr, server_len);
            return;
        }
    }
    print_incoming(message);
}

static void *receive_thread(void *arg)
{
    struct sockaddr_in server_addr;
    socklen_t server_len = sizeof(server_addr);
    int sockfd = *(int *)arg;
    char buffer[MAX_MESSAGE_LEN + 1];
    while (1)
    {
        if (recv_message_udp(sockfd, buffer, sizeof(buffer), &server_addr, &server_len) < 0)
            break;
        process_server_message(sockfd, buffer, &server_addr, server_len);
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

    int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
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

    pthread_t receiver;
    if (pthread_create(&receiver, NULL, receive_thread, &sockfd) != 0)
    {
        perror("pthread_create");
        close(sockfd);
        return EXIT_FAILURE;
    }

    printf("UDP client ready. Server %s:%d\n", host, port);
    print_help();

    char input[MAX_MESSAGE_LEN + 2];
    while (1)
    {
        printf("> ");
        fflush(stdout);
        if (!fgets(input, sizeof(input), stdin))
            break;
        size_t len = strcspn(input, "\n");
        input[len] = '\0';
        if (len == 0)
            continue;
        if (strcmp(input, "/help") == 0)
        {
            print_help();
            continue;
        }
        send_message_udp(sockfd, input, &server_addr, sizeof(server_addr));
        if (strcmp(input, "/quit") == 0)
            break;
    }

    close(sockfd);
    pthread_cancel(receiver);
    pthread_join(receiver, NULL);
    return EXIT_SUCCESS;
}
