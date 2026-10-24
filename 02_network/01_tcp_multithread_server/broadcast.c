#include "broadcast.h"
#include <errno.h>
#include <stdlib.h>
#include <sys/socket.h>

static int send_message(int fd, const message_t *message) {
    size_t sent_total = 0;
    while (sent_total < message->length) {
        ssize_t sent = send(fd, message->data + sent_total,
                            message->length - sent_total, MSG_NOSIGNAL);
        if (sent < 0 && errno == EINTR) continue;
        if (sent <= 0) return -1;
        sent_total += (size_t)sent;
    }
    return 0;
}

void broadcast_message(client_manager_t *clients, const message_t *message) {
    int fds[MAX_CLIENTS];
    size_t count = get_client_list(clients, fds);
    for (size_t i = 0; i < count; ++i)
        /* Handler owns close/free; shutdown wakes its recv on send failure. */
        if (send_message(fds[i], message) < 0) shutdown(fds[i], SHUT_RDWR);
    release_client_list(clients);
}

void *broadcast_manager(void *argument) {
    server_t *server = argument;
    message_t *message;
    while ((message = dequeue_message(&server->queue)) != NULL) {
        broadcast_message(&server->clients, message);
        free(message);
    }
    return NULL;
}
