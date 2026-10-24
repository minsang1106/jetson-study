#include "server.h"
#include "broadcast.h"
#include "connection.h"
#include <arpa/inet.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

int initialize_server(server_t *server) {
    *server = (server_t){.listen_fd = -1};
    int error = initialize_client_manager(&server->clients);
    if (error) { fprintf(stderr, "client manager: %s\n", strerror(error)); return -1; }
    error = initialize_message_queue(&server->queue);
    if (error) {
        fprintf(stderr, "message queue: %s\n", strerror(error));
        destroy_client_manager(&server->clients);
        return -1;
    }
    /* All worker threads inherit this mask; main consumes termination signals. */
    sigemptyset(&server->signals);
    sigaddset(&server->signals, SIGINT);
    sigaddset(&server->signals, SIGTERM);
    error = pthread_sigmask(SIG_BLOCK, &server->signals, &server->previous_signals);
    if (error) {
        fprintf(stderr, "pthread_sigmask: %s\n", strerror(error));
        destroy_message_queue(&server->queue);
        destroy_client_manager(&server->clients);
        return -1;
    }
    server->listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server->listen_fd < 0) { perror("socket"); goto fail; }
    int reuse = 1;
    if (setsockopt(server->listen_fd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) < 0) {
        perror("SO_REUSEADDR"); goto fail;
    }
    struct sockaddr_in address = {
        .sin_family = AF_INET,
        .sin_addr.s_addr = htonl(INADDR_ANY),
        .sin_port = htons(PORT)
    };
    if (bind(server->listen_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        perror("bind"); goto fail;
    }
    if (listen(server->listen_fd, MAX_CLIENTS) < 0) { perror("listen"); goto fail; }
    printf("chat server listening on port %d...\n", PORT);
    return 0;
fail:
    cleanup_server(server);
    return -1;
}

int run_server(server_t *server) {
    pthread_t broadcaster, connections;
    int error = pthread_create(&broadcaster, NULL, broadcast_manager, server);
    if (error) { fprintf(stderr, "broadcaster: %s\n", strerror(error)); return -1; }
    error = pthread_create(&connections, NULL, connection_manager, server);
    if (error) {
        fprintf(stderr, "connection manager: %s\n", strerror(error));
        close_message_queue(&server->queue);
        pthread_join(broadcaster, NULL);
        return -1;
    }
    int signal_number;
    error = sigwait(&server->signals, &signal_number);
    if (error) fprintf(stderr, "sigwait: %s\n", strerror(error));
    stop_clients(&server->clients);
    /* On Linux shutdown wakes the connection manager's blocking accept. */
    shutdown(server->listen_fd, SHUT_RDWR);
    pthread_join(connections, NULL);
    wait_for_clients(&server->clients);
    close_message_queue(&server->queue);
    pthread_join(broadcaster, NULL);
    return error || server->connection_error ? -1 : 0;
}

void cleanup_server(server_t *server) {
    if (server->listen_fd >= 0) close(server->listen_fd);
    destroy_message_queue(&server->queue);
    destroy_client_manager(&server->clients);
    pthread_sigmask(SIG_SETMASK, &server->previous_signals, NULL);
}
