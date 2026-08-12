#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>

#include "smpp.h"

static int smpp_socket = -1;

void smpp_init(void)
{
    smpp_socket = -1;
}

int smpp_connect(const char *ip, int port)
{
    struct sockaddr_in server_addr;

    smpp_socket = socket(AF_INET, SOCK_STREAM, 0);

    if (smpp_socket < 0)
    {
        perror("socket");
        return -1;
    }

    memset(&server_addr, 0, sizeof(server_addr));

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);

    if (inet_pton(AF_INET, ip, &server_addr.sin_addr) <= 0)
    {
        perror("inet_pton");
        close(smpp_socket);
        smpp_socket = -1;
        return -1;
    }

    if (connect(smpp_socket,
                (struct sockaddr *)&server_addr,
                sizeof(server_addr)) < 0)
    {
        perror("connect");
        close(smpp_socket);
        smpp_socket = -1;
        return -1;
    }

    printf("Connected to SMSC %s:%d\n", ip, port);

    return 0;
}
int smpp_bind_transceiver(const char *system_id, const char *password)
{
    return 0;
}

int smpp_submit_sm(const char *destination, const char *message)
{
    return 0;
}

void smpp_disconnect(void)
{
    if (smpp_socket >= 0)
    {
        close(smpp_socket);
        smpp_socket = -1;
    }
}
