#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <arpa/inet.h>
#include <sys/socket.h>

#define PORT 8080
#define BUFFER_SIZE 1024

int main(void)
{
    int server_fd;
    int client_fd;

    struct sockaddr_in server_addr;
    struct sockaddr_in client_addr;

    socklen_t client_addr_len = sizeof(client_addr);

    char buffer[BUFFER_SIZE];

    // 1. TCP socket 생성
    server_fd = socket(AF_INET, SOCK_STREAM, 0);

    if (server_fd == -1) {
        perror("socket");
        exit(EXIT_FAILURE);
    }

    // 2. 서버 주소 설정
    memset(&server_addr, 0, sizeof(server_addr));

    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    server_addr.sin_port = htons(PORT);

    // 3. socket에 IP/Port 할당
    if (bind(
            server_fd,
            (struct sockaddr *)&server_addr,
            sizeof(server_addr)
        ) == -1) {

        perror("bind");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    // 4. 연결 요청 대기 상태
    if (listen(server_fd, 5) == -1) {
        perror("listen");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    printf("Echo server listening on port %d...\n", PORT);

    // 5. client 연결 수락
    client_fd = accept(
        server_fd,
        (struct sockaddr *)&client_addr,
        &client_addr_len
    );

    if (client_fd == -1) {
        perror("accept");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    printf("Client connected.\n");

    // 6. 데이터 수신 → 그대로 다시 전송
    while (1) {

        ssize_t received = recv(
            client_fd,
            buffer,
            sizeof(buffer),
            0
        );

        if (received == -1) {
            perror("recv");
            break;
        }

        if (received == 0) {
            printf("Client disconnected.\n");
            break;
        }

        printf("Received %zd bytes\n", received);

        ssize_t sent_total = 0;

        while (sent_total < received) {

            ssize_t sent = send(
                client_fd,
                buffer + sent_total,
                received - sent_total,
                0
            );

            if (sent == -1) {
                perror("send");
                break;
            }

            sent_total += sent;
        }
    }

    // 7. socket 정리
    close(client_fd);
    close(server_fd);

    return 0;
}