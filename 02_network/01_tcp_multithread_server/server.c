#include "data_structure.h"
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

typedef struct {
    int idx;
    client_t** clients;
    queue_t* mq;
    pthread_mutex_t* mutex;
    pthread_cond_t* slot_available;
} receiver_arg_t;

typedef struct {
    queue_t* mq;
    client_t** clients;
    pthread_mutex_t* mutex;
} sender_arg_t;

void *receiver(void *arg) {
    receiver_arg_t* ra = arg;
    int fd = ra->clients[ra->idx];
    char buffer[BUFFER_SIZE];

    while (1) {
        ssize_t received = recv(
            fd,
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
        char *message = malloc((size_t)received + 1);
        if(message == NULL) {
            perror("malloc");
            break;
        }

        memcpy(message, buffer, (size_t)received);
        message[received] = '\0';

        Queue_Enqueue(ra->mq, message);

         printf("Received %zd bytes\n", received);
    }

    pthread_mutex_lock(ra->mutex);
    free(ra->clients[ra->idx]);
    pthread_cond_signal(ra->slot_available);
    pthread_mutex_unlock(ra->mutex);
    close(fd);
    free(ra);
}

void *sender(void *arg) {
    sender_arg_t* sa = arg;
    ssize_t sent_total = 0;
    char buffer[BUFFER_SIZE];

    Queue_Dequeue(sa->mq, &buffer);
    
    while (sent_total < received) {

        ssize_t sent = send(
            sa->clients,
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

int main() {
    pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
    pthread_cond_t slot_available = PTHREAD_COND_INITIALIZER;
    pthread_t sender_tid;
    int server_fd;
    client_t *clients[MAX] = {NULL};
    queue_t mq;

    struct sockaddr_in server_addr;
    struct sockaddr_in client_addr[MAX];

    socklen_t client_addr_len;

    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    Queue_Init(&mq);

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

    sender_arg_t arg = {
        .clients = clients,
        .mq = &mq,
        .mutex = &mutex
    };

    pthread_create(&sender_tid, NULL, sender, &arg);

    // 클라이언트 수는 cond variable로 관리.
    while(1) {
        int slot;

        pthread_mutex_lock(&mutex);
        while (1) {
            for (slot = 0; slot < MAX && clients[slot] != NULL; slot++) {}
            if (slot < MAX) {
                break;
            }
            pthread_cond_wait(&slot_available, &mutex);
        }

        clients[slot] = malloc(sizeof(client_t));

        if (clients[slot] == NULL) {
            perror("malloc");
            exit(EXIT_FAILURE);
        }

        pthread_mutex_unlock(&mutex);

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

        printf("Client %d connected.\n", slot+1);

        receiver_arg_t* arg = malloc(sizeof(receiver_arg_t));
        arg->clients = clients;
        arg->idx = slot;
        arg->mq = &mq;
        arg->mutex = &mutex;


        if (pthread_create(&clients[slot]->tid, NULL, receiver, arg) != 0) {
            perror("pthread_create");
            exit(EXIT_FAILURE);
        }

        pthread_detach(clients[slot]->tid);
    }
}
