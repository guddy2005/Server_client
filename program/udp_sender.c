#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>

int main() {
    int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    struct sockaddr_in servaddr;

    servaddr.sin_family = AF_INET;
    servaddr.sin_port = htons(18080);
    inet_pton(AF_INET, "127.0.0.1", &servaddr.sin_addr);

    const char *msg = "Hey! I am packet";
    sendto(sockfd, msg, strlen(msg), 0, (const struct sockaddr *)&servaddr, sizeof(servaddr));
    printf("UDP Message Sent!\n");

    close(sockfd);
    return 0;
}
