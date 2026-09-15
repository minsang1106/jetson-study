#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <arpa/inet.h>
#include <sys/socket.h>

#define SERVER_IP "127.0.0.1"
#define PORT 8080
#define BUFFER_SIZE 1024

int main(void)
{
    int sockfd;

    struct sockaddr_in server_addr;

    char buffer[BUFFER_SIZE];

    // 1. TCP socket 생성
    sockfd = socket(AF_INET, SOCK_STREAM, 0);

    if (sockfd == -1) {
        perror("socket");
        exit(EXIT_FAILURE);
    }

    // 2. 서버 주소 설정
    memset(&server_addr, 0, sizeof(server_addr));

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);

    if (inet_pton(
            AF_INET,
            SERVER_IP,
            &server_addr.sin_addr
        ) != 1) {

        perror("inet_pton");
        close(sockfd);
        exit(EXIT_FAILURE);
    }

    // 3. 서버 접속
    if (connect(
            sockfd,
            (struct sockaddr *)&server_addr,
            sizeof(server_addr)
        ) == -1) {

        perror("connect");
        close(sockfd);
        exit(EXIT_FAILURE);
    }

    printf("Connected to server.\n");

    while (1) {

        printf("> ");

        if (fgets(buffer, sizeof(buffer), stdin) == NULL) {
            break;
        }

        size_t length = strlen(buffer);

        // 서버에 전송
        ssize_t sent_total = 0;

        while (sent_total < (ssize_t)length) {

            ssize_t sent = send(
                sockfd,
                buffer + sent_total,
                length - sent_total,
                0
            );

            if (sent == -1) {
                perror("send");
                close(sockfd);
                exit(EXIT_FAILURE);
            }

            sent_total += sent;
        }

        // echo를 모두 받을 때까지 수신
        ssize_t received_total = 0;

        while (received_total < (ssize_t)length) {

            ssize_t received = recv(
                sockfd,
                buffer + received_total,
                length - received_total,
                0
            );

            if (received == -1) {
                perror("recv");
                close(sockfd);
                exit(EXIT_FAILURE);
            }

            if (received == 0) {
                printf("Server disconnected.\n");
                close(sockfd);
                return 0;
            }

            received_total += received;
        }

        printf("Echo: %.*s",
               (int)received_total,
               buffer);
    }

    close(sockfd);

    return 0;
}