#include "../include/chat.h"

/* Send Packet */
int send_packet(int socket, Packet *packet)
{
    int total = 0;
    int bytes;

    while (total < sizeof(Packet))
    {
        bytes = send(socket,
                     (char *)packet + total,
                     sizeof(Packet) - total,
                     0);

        if (bytes <= 0)
        {
            perror("send");
            return -1;
        }

        total += bytes;
    }

    return 0;
}

/* Receive Packet */
int recv_packet(int socket, Packet *packet)
{
    int total = 0;
    int bytes;

    while (total < sizeof(Packet))
    {
        bytes = recv(socket,
                     (char *)packet + total,
                     sizeof(Packet) - total,
                     0);

        if (bytes <= 0)
        {
            return -1;
        }

        total += bytes;
    }

    return 0;
}
