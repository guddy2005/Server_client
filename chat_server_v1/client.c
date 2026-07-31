#include "header.h"
#include <stdio.h>
#include<string.h>
int server_socket;
char my_name[MAX_NAME];

void *receive_thread(void *arg)
{
	Packet packet;

	while (1)
	{
		if (recv_packet(server_socket, &packet) < 0)
		{
			printf("\nDisconnected From Server.\n");
			exit(0);
		}

		switch (packet.type)
		{
			case ONLINE_LIST:

                              	packet.message[strcspn(packet.message, "\r\n")] = '\0';			
                                printf("\r%s\n", packet.message);
				break;



			case CHAT_MESSAGE:
                               packet.message[strcspn(packet.message, "\r\n")] = '\0';
				printf("\r%s : %s\n",
						packet.sender,
						packet.message);
				break;

			case CHAT_EXIT:

				printf("\rChat Ended.\n");
				break;

			default:
				break;
		}

		printf("Command : ");
		fflush(stdout);
	}

	return NULL;
}

void show_help(void)
{
	printf("\n========================================\n");
	printf("           CHAT COMMANDS\n");
	printf("========================================\n");
	printf("list              - Show online users\n");
	printf("chat <username>   - Request private chat\n");
	printf("msg <message>     - Send message\n");
	printf("end               - End current chat\n");
	printf("help              - Show commands\n");
	printf("exit              - Disconnect\n");
	printf("========================================\n\n");
}
int main(int argc, char *argv[]) 
{
	struct sockaddr_in server_addr;

	Packet packet;

	pthread_t tid;

	char input[MAX_MSG];

	server_socket = socket(AF_INET,
			SOCK_STREAM,
			0);

	if (server_socket < 0)
	{
		perror("socket");
		exit(EXIT_FAILURE);
	}

	server_addr.sin_family = AF_INET;
	server_addr.sin_port = htons(PORT);
	server_addr.sin_addr.s_addr = inet_addr(SERVER_IP);

	if (connect(server_socket,
				(struct sockaddr *)&server_addr,
				sizeof(server_addr)) < 0)
	{
		perror("connect");
		exit(EXIT_FAILURE);
	}

	printf("Connected To Server\n");

	memset(&packet,0,sizeof(Packet));

	packet.type = REGISTER;

	while(1)
	{
		printf("Enter Username : ");
		fgets(my_name, MAX_NAME, stdin);

		my_name[strcspn(my_name,"\n")] = '\0';

		memset(&packet,0,sizeof(packet));

		packet.type = REGISTER;
		strcpy(packet.sender,my_name);

		send_packet(server_socket,&packet);

		recv_packet(server_socket,&packet);

		if(strcmp(packet.message,"OK") == 0)
		{
			printf("Username Accepted\n");
			break;
		}

		printf("Username already exists.\n");
		printf("Try another username.\n");
	}
	pthread_create(&tid,
			NULL,
			receive_thread,
			NULL);
         
        printf("Command : ");
        fflush(stdout);
	while(1)
	{

		fgets(input,
				MAX_MSG,
				stdin);

		input[strcspn(input,"\n")] = '\0';
                 if (strlen(input) == 0) {
                    printf("Command : ");
                    fflush(stdout);

                    continue;
    }
		memset(&packet,0,sizeof(Packet));

		strcpy(packet.sender,my_name);

		if(strcmp(input,"list")==0)
		{
			packet.type = ONLINE_LIST;

			send_packet(server_socket,&packet);
		}
		else if(strcmp(input,"offline")==0)
		{
			memset(&packet, 0, sizeof(Packet));

			packet.type = CHAT_STATUS;

			strcpy(packet.message, "offline");

			send_packet(server_socket, &packet);
		}

		else if(strcmp(input,"online")==0)
		{
			memset(&packet, 0, sizeof(Packet));

			packet.type = CHAT_STATUS;

			strcpy(packet.message, "online");

			send_packet(server_socket, &packet);
		}
		else if(strncmp(input,"send ",5)==0)
		{
			int n;

			memset(&packet,0,sizeof(packet));

			packet.type = CHAT_MESSAGE;

			n = sscanf(input + 5,
					"%31s %511[^\n]",
					packet.receiver,
					packet.message);

			if(n != 2)
			{
				printf("\nUsage: send <username> <message>\n");
				continue;
			}

			strcpy(packet.sender, my_name);

			send_packet(server_socket, &packet);
		}

		else if(strcmp(input,"exit")==0)
		{
			packet.type = CHAT_EXIT;

			send_packet(server_socket,
					&packet);

			break;
		}



		else
		{
			printf("\nInvalid Command!\n");
			printf("Available Commands:\n");
			printf("list\n");
			printf("online\n");
			printf("offline\n");
			printf("send <username> <message>\n");
			printf("help\n");
			printf("exit\n\n");
		}
	}
	close(server_socket);
	return 0;
}

