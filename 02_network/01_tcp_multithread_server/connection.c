#include "connection.h"
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>

typedef struct {
    server_t *server;
    client_t *client;
} handler_arg_t;

int accept_client(int listen_fd) {
    int fd;
    do { fd = accept(listen_fd, NULL, NULL); } while (fd < 0 && errno == EINTR);
    return fd;
}

ssize_t receive_message(int fd, char buffer[BUFFER_SIZE]) {
    ssize_t received;
    do { received = recv(fd, buffer, BUFFER_SIZE, 0); }
    while (received < 0 && errno == EINTR);
    return received;
}

void disconnect_client(client_manager_t *manager, client_t *client) {
    remove_client(manager, client);
    close(client->fd);
    free(client);
}

void *client_handler(void *argument) {
    handler_arg_t *arg = argument;
    server_t *server = arg->server;
    client_t *client = arg->client;
    char buffer[BUFFER_SIZE];
    for (;;) {
        ssize_t received = receive_message(client->fd, buffer);
        if (received <= 0) {
            if (received < 0) perror("recv");
            break;
        }
        if (enqueue_message(&server->queue, buffer, (size_t)received) < 0) {
            fprintf(stderr, "Could not enqueue message\n");
            break;
        }
        printf("Received %zd bytes\n", received);
    }
    disconnect_client(&server->clients, client);
    free(arg);
    /* Last access to server: cleanup may proceed after this notification. */
    client_handler_finished(&server->clients);
    return NULL;
}

int create_client_thread(server_t *server, client_t *client) {
    handler_arg_t *arg = malloc(sizeof(*arg));
    if (!arg) return ENOMEM;
    *arg = (handler_arg_t){.server = server, .client = client};
    pthread_attr_t attr;
    int error = pthread_attr_init(&attr);
    if (error) { free(arg); return error; }
    error = pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
    pthread_t tid;
    if (!error) error = pthread_create(&tid, &attr, client_handler, arg);
    pthread_attr_destroy(&attr);
    if (error) free(arg);
    return error;
}

void *connection_manager(void *argument) {
    server_t *server = argument;
    while (wait_for_client_slot(&server->clients)) {
        int fd = accept_client(server->listen_fd);
        if (fd < 0) {
            pthread_mutex_lock(&server->clients.mutex);
            bool stopping = server->clients.stopping;
            pthread_mutex_unlock(&server->clients.mutex);
            if (!stopping) {
                perror("accept");
                server->connection_error = 1;
                kill(getpid(), SIGTERM);
            }
            break;
        }
        struct timeval timeout = {.tv_sec = 2};
        if (setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout)) < 0) {
            perror("SO_SNDTIMEO");
            close(fd);
            continue;
        }
        client_t *client = add_client(&server->clients, fd);
        if (!client) { close(fd); continue; }
        printf("Client %d connected.\n", client->slot + 1);
        int error = create_client_thread(server, client);
        if (error) {
            fprintf(stderr, "pthread_create: %s\n", strerror(error));
            disconnect_client(&server->clients, client);
            client_handler_finished(&server->clients);
        }
    }
    return NULL;
}
