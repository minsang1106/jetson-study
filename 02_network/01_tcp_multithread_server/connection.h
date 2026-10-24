#ifndef CONNECTION_H
#define CONNECTION_H
#include "server.h"
#include <sys/types.h>
int accept_client(int listen_fd);
int create_client_thread(server_t *server, client_t *client);
void *client_handler(void *argument);
ssize_t receive_message(int fd, char buffer[BUFFER_SIZE]);
void disconnect_client(client_manager_t *manager, client_t *client);
void *connection_manager(void *argument);
#endif
