#include <stdio.h>
#include <string.h>
#include <arpa/inet.h>

int main() {
    int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    struct sockaddr_in servaddr, cliaddr;
    char buffer[1024];
    socklen_t len = sizeof(cliaddr);

    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = INADDR_ANY;
    servaddr.sin_port = htons(18080);

    bind(sockfd, (const struct sockaddr *)&servaddr, sizeof(servaddr));
    printf("UDP Receiver active on port 8080...\n");

    int n = recvfrom(sockfd, buffer, sizeof(buffer) - 1, 0, (struct sockaddr *)&cliaddr, &len);
    buffer[n] = '\0';
    printf("Received UDP Data: %s\n", buffer);

    return 0;
}
