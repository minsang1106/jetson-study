#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>

#include <arpa/inet.h>
#include <sys/socket.h>

#define PORT 8080
#define MAX 5
#define BUFFER_SIZE 1024

typedef struct {
    int fd;
    pthread_t tid;
} client_t;

void *receiver(void *arg) {
    int fd = *(int*) arg;
}

void *sender(void *arg) {

}

int main() {
    pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
    int server_fd;
    client_t *clients[MAX];

    struct sockaddr_in server_addr;
    struct sockaddr_in client_addr[MAX];

    socklen_t client_addr_len;

    server_fd = socket(AF_INET, SOCK_STREAM, 0);

    memset(&server_addr, 0, sizeof(server_addr));

    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    server_addr.sin_port = htons(PORT);

    if (bind(
            server_fd,
            (struct sockaddr *)&server_addr,
            sizeof(server_addr)
        ) == -1) {
        perror("bind");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    if (listen(server_fd, 5) == -1) {
        perror("listen");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    printf("chat server listening on port %d...\n", PORT);

    // 클라이언트 수는 cond variable로 관리.
    while(1) {
        int slot = -1;

        for(int i = 0; i < MAX; i++) {
            if(!clients[i]) {
                slot = i;
                clients[slot] = malloc(sizeof(client_t));

                if (clients[slot] == NULL) {
                    perror("malloc");
                    exit(EXIT_FAILURE);
                }

                clients[slot]->fd = accept(
                    server_fd,
                    (struct sockaddr *)&client_addr[slot],
                    &client_addr_len
                );
                
                if (clients[slot]->fd == -1) {
                    perror("accept");
                    close(server_fd);
                    exit(EXIT_FAILURE);
                }
            }
        }

        printf("Client %d connected.\n", slot+1);
    }
}

