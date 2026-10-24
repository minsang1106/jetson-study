#ifndef CLIENT_MANAGER_H
#define CLIENT_MANAGER_H
#include <pthread.h>
#include <stdbool.h>
#include <stddef.h>
#define MAX_CLIENTS 5
typedef struct { int fd; int slot; } client_t;
typedef struct {
    client_t *clients[MAX_CLIENTS];
    size_t active_handlers;
    bool stopping;
    pthread_mutex_t mutex;
    pthread_cond_t changed;
} client_manager_t;
int initialize_client_manager(client_manager_t *manager);
client_t *add_client(client_manager_t *manager, int fd);
void remove_client(client_manager_t *manager, client_t *client);
void client_handler_finished(client_manager_t *manager);
bool wait_for_client_slot(client_manager_t *manager);
/* Locks the list until release_client_list(): sockets stay valid during sends. */
size_t get_client_list(client_manager_t *manager, int fds[MAX_CLIENTS]);
void release_client_list(client_manager_t *manager);
void stop_clients(client_manager_t *manager);
void wait_for_clients(client_manager_t *manager);
void destroy_client_manager(client_manager_t *manager);
#endif
