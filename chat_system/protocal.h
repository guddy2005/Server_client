#ifndef PROTOCAL_H
#define PROTOCAL_H


#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/types.h>


#define SERVER_IP "127.0.0.1"
#define PORT 19903
#define MAX_NAME 32
#define MAX_CLIENTS 100
#define MAX_MSG 512

typedef enum {
   CMD_LOGIN,
   CMD_SEND_MSG,
   SERVER_ALERT,
   NOTIFY_ONLINE,
   NOTIFY_OFFLINE,
}PacketType;

typedef struct {
    PacketType type;
    char sender[MAX_NAME];
    char receiver[MAX_NAME];
    char message[MAX_MSG]; 
} Packet;

typedef struct {
    char sender[MAX_NAME];
    char receiver[MAX_NAME];
    char message[MAX_MSG];
} DiskMessage;

#endif

