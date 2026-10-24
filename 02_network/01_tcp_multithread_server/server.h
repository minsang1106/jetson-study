#ifndef SERVER_H
#define SERVER_H
#include "client_manager.h"
#include "message_queue.h"
#include <signal.h>
#define PORT 8080
typedef struct {
    int listen_fd;
    client_manager_t clients;
    message_queue_t queue;
    sigset_t signals;
    sigset_t previous_signals;
    int connection_error; /* Read only after joining connection manager. */
} server_t;
int initialize_server(server_t *server);
int run_server(server_t *server);
void cleanup_server(server_t *server);
#endif
