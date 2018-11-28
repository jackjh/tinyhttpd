#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>

int main(int argc, char* argv[]) {
    int sockfd;
    struct sockaddr_in client_addr;
    int res;
    char ch = 'A';

    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    client_addr.sin_family = AF_INET;
    client_addr.sin_addr.s_addr = inet_addr("127.0.0.1");
    client_addr.sin_port = htons(9734);
    int len = sizeof(client_addr);
    res = connect(sockfd, (struct sockaddr*)&client_addr, len);
    if(res == -1) {
        perror("client error!");
        exit(1);
    }

    write(sockfd, &ch, 1);
    read(sockfd, &ch, 1);
    printf("char from server is: %c\n", ch);
    close(sockfd);

    return 0;
}