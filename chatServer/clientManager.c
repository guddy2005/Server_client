#define _DEFAULT_SOURCE
#define _GNU_SOURCE
#include <string.h>
#include <stddef.h>
#include "header.h"

Client clients[MAX_CLIENTS] = {0};

/* Add Client */
int add_client(Client *client)
{
	int i;

	/* Duplicate username */
	for(i = 0; i < MAX_CLIENTS; i++)
	{
		if(clients[i].connected &&
				strcasecmp(clients[i].name, client->name) == 0)
		{
			return -1;
		}
	}

	/* Empty slot */
	for(i = 0; i < MAX_CLIENTS; i++)
	{
		if(clients[i].connected == 0)
		{
			clients[i] = *client;
			clients[i].connected = 1;
			clients[i].online = 1;

			printf("%s Joined\n", clients[i].name);

			return 0;
		}
	}

	return -1;
}

/* Remove Client */
void remove_client(int socket)
{
	int i;

	for(i = 0; i < MAX_CLIENTS; i++)
	{
		if(clients[i].socket == socket)
		{
			printf("%s Left\n", clients[i].name);

			memset(&clients[i], 0, sizeof(Client));
			return;
		}
	}
}

/* Find By Name */
Client *find_client(const char *name)
{
	int i;

	for(i = 0; i < MAX_CLIENTS; i++)
	{
		if(clients[i].online &&
				strcasecmp(clients[i].name, name) == 0)
		{
			return &clients[i];
		}
	}

	return NULL;
}

/* Find By Socket */
Client *find_client_by_socket(int socket)
{
	int i;

	for(i = 0; i < MAX_CLIENTS; i++)
	{
		if(clients[i].connected &&
				clients[i].socket == socket)
		{
			return &clients[i];
		}
	}

	return NULL;
}

/* Send Online Users */
void send_online_users(int socket)
{
	Packet packet;

	int i;

	memset(&packet, 0, sizeof(Packet));

	packet.type = ONLINE_LIST;

	strcpy(packet.message, "===== Online Users =====\n");

	for(i = 0; i < MAX_CLIENTS; i++)
	{
		if(clients[i].online && clients[i].socket != socket)
		{
			strcat(packet.message, clients[i].name);
			strcat(packet.message, "\n");
		}
	}

	send_packet(socket, &packet);
}


