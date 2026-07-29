#include "../include/chat.h"


void broadcast_join(const char *name) {
Packet packet;
 memset(&packet,0,sizeof(packet));
 packet.type = CHAT_MESSAGE;
 strcpy(packet.sender,"SERVER");
sprintf(packet.message,"%s joined the chat.",name);
for(int i=0;i<MAX_CLIENTS;i++) {
 if(clients[i].online &&
   strcmp(clients[i].name, name) != 0)
{
    send_packet(clients[i].socket, &packet);
}
 }
}
void broadcast_leave(const char *name) { 
Packet packet; 
memset(&packet,0,sizeof(packet));
 packet.type = CHAT_MESSAGE; 
strcpy(packet.sender,"SERVER"); 
sprintf(packet.message,"%s left the chat.",name); 
for(int i=0;i<MAX_CLIENTS;i++) {
 if(clients[i].online) { 
send_packet(clients[i].socket,&packet); 
}
 } 
}
void *client_handler(void *arg)
{
    int socket = *(int *)arg;
    free(arg);

    Packet packet;
    Client client;

    memset(&client, 0, sizeof(Client));

    client.socket = socket;

    /* Register */
    if (recv_packet(socket, &packet) < 0)
    {
        close(socket);
        return NULL;
    }

    strcpy(client.name, packet.sender);

   if(add_client(&client) < 0)
{
    Packet reply;

    memset(&reply, 0, sizeof(Packet));

    reply.type = CHAT_MESSAGE;

    strcpy(reply.sender, "SERVER");

    strcpy(reply.message,
           "Username already exists. Please connect with another username.");

    send_packet(socket, &reply);

    close(socket);

    return NULL;
}

    printf("%s Connected\n", client.name);

    send_online_users(socket);
    broadcast_join(client.name);
    while (1)
    {
        if (recv_packet(socket, &packet) < 0)
        {
            printf("%s Disconnected\n", client.name);
             broadcast_leave(client.name);
            remove_client(socket);

            close(socket);

            return NULL;
        }

        Client *self = find_client_by_socket(socket);

        switch (packet.type)
        {

        case ONLINE_LIST:

            send_online_users(socket);

            break;

        
      

      case CHAT_MESSAGE:
{
    Client  *receiver;

    receiver = find_client(packet.receiver);



if(receiver == NULL)
{
    Packet reply;

    memset(&reply, 0, sizeof(Packet));

    reply.type = CHAT_MESSAGE;

    strcpy(reply.sender, "SERVER");

    sprintf(reply.message,
            "%s is offline or does not exist.",
            packet.receiver);

    send_packet(socket, &reply);

    break;
}
    if(receiver->socket == socket)
    {
        printf("%s tried to send message to self\n", packet.sender);
        break;
    }
if(receiver->socket == socket)
    {
        printf("%s tried to send message to self\n", packet.sender);
        break;
    }

    send_packet(receiver->socket, &packet);

    break;
}
case CHAT_STATUS:
{
    Client *self = find_client_by_socket(socket);

    if(self == NULL)
        break;

    if(strcmp(packet.message, "offline") == 0)
    {
        self->online = 0;
        printf("%s is now offline\n", self->name);
    }
    else if(strcmp(packet.message, "online") == 0)
    {
        self->online = 1;
        printf("%s is now online\n", self->name);
    }

    break;
}     

case CHAT_EXIT:
{
     broadcast_leave(client.name);
    remove_client(socket);
    close(socket);
    return NULL;
}
                         


        default:
            break;
        }
    }
}

void start_server(void)
{
    int server_socket;

    struct sockaddr_in server_addr;
    struct sockaddr_in client_addr;

    socklen_t len = sizeof(client_addr);

    pthread_t tid;

    server_socket = socket(AF_INET,
                           SOCK_STREAM,
                           0);

    if(server_socket<0)
    {
        perror("socket");
        exit(EXIT_FAILURE);
    }

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);
    server_addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(server_socket,
         (struct sockaddr *)&server_addr,
         sizeof(server_addr)) < 0)
{
    perror("bind");
    exit(EXIT_FAILURE);
}

if (listen(server_socket,
           MAX_CLIENTS) < 0)
{
    perror("listen");
    exit(EXIT_FAILURE);
}

printf("Chat Server Started...\n");
    while(1)
    {
        int *client_socket =
            malloc(sizeof(int));

        *client_socket =
            accept(server_socket,
                   (struct sockaddr *)&client_addr,
                   &len);

        pthread_create(&tid,
                       NULL,
                       client_handler,
                       client_socket);

        pthread_detach(tid);
    }
}
