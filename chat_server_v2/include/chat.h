#ifndef CHAT_H
#define CHAT_H


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>

#include <pthread.h>

#define SERVER_IP      "127.0.0.1"
#define PORT           19901

#define MAX_CLIENTS    100
#define MAX_NAME       32
#define MAX_MSG        512

typedef enum
{
    REGISTER = 1,
    ONLINE_LIST,
    CHAT_MESSAGE,
    CHAT_STATUS,  
    CHAT_EXIT

} PacketType;


typedef struct
{
    PacketType type;

    char sender[MAX_NAME];
    char receiver[MAX_NAME];
    char message[MAX_MSG];

} Packet;


typedef struct
{
    int socket;
    int connected;
    int online;
    char name[MAX_NAME];

} Client;

/* Global Client List */
extern Client clients[MAX_CLIENTS];
extern int server_socket;
extern char my_name[MAX_NAME];
/* Server Functions */
void start_server(void);
void *client_handler(void *arg);
void *receive_thread(void *arg);
/* Client Functions */
void start_client(void);
void show_help(void);
/* Client Manager */
int add_client(Client *client);
void remove_client(int socket);
Client *find_client_by_socket(int socket);

/* Global Client List */
extern Client clients[MAX_CLIENTS];
extern int server_socket;
extern char my_name[MAX_NAME];
void send_online_users(int socket);
Client *find_client(const char *name);
/* Packet Functions */
int send_packet(int socket, Packet *packet);
int recv_packet(int socket, Packet *packet);

#endif
