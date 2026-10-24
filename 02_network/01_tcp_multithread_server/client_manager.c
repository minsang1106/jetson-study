#include "client_manager.h"
#include <stdlib.h>
#include <sys/socket.h>

int initialize_client_manager(client_manager_t *m) {
    *m = (client_manager_t){0};
    int error = pthread_mutex_init(&m->mutex, NULL);
    if (error) return error;
    error = pthread_cond_init(&m->changed, NULL);
    if (error) pthread_mutex_destroy(&m->mutex);
    return error;
}

bool wait_for_client_slot(client_manager_t *m) {
    pthread_mutex_lock(&m->mutex);
    while (!m->stopping && m->active_handlers == MAX_CLIENTS)
        pthread_cond_wait(&m->changed, &m->mutex);
    bool ready = !m->stopping;
    pthread_mutex_unlock(&m->mutex);
    return ready;
}

client_t *add_client(client_manager_t *m, int fd) {
    client_t *client = malloc(sizeof(*client));
    if (!client) return NULL;
    pthread_mutex_lock(&m->mutex);
    int slot = 0;
    while (slot < MAX_CLIENTS && m->clients[slot]) ++slot;
    if (m->stopping || slot == MAX_CLIENTS) {
        pthread_mutex_unlock(&m->mutex);
        free(client);
        return NULL;
    }
    *client = (client_t){.fd = fd, .slot = slot};
    m->clients[slot] = client;
    ++m->active_handlers;
    pthread_mutex_unlock(&m->mutex);
    return client;
}

void remove_client(client_manager_t *m, client_t *client) {
    pthread_mutex_lock(&m->mutex);
    m->clients[client->slot] = NULL;
    pthread_mutex_unlock(&m->mutex);
}

void client_handler_finished(client_manager_t *m) {
    pthread_mutex_lock(&m->mutex);
    --m->active_handlers;
    pthread_cond_broadcast(&m->changed);
    pthread_mutex_unlock(&m->mutex);
}

size_t get_client_list(client_manager_t *m, int fds[MAX_CLIENTS]) {
    pthread_mutex_lock(&m->mutex);
    size_t count = 0;
    for (int i = 0; i < MAX_CLIENTS; ++i)
        if (m->clients[i]) fds[count++] = m->clients[i]->fd;
    return count;
}

void release_client_list(client_manager_t *m) {
    pthread_mutex_unlock(&m->mutex);
}

void stop_clients(client_manager_t *m) {
    pthread_mutex_lock(&m->mutex);
    m->stopping = true;
    for (int i = 0; i < MAX_CLIENTS; ++i)
        if (m->clients[i]) shutdown(m->clients[i]->fd, SHUT_RDWR);
    pthread_cond_broadcast(&m->changed);
    pthread_mutex_unlock(&m->mutex);
}

void wait_for_clients(client_manager_t *m) {
    pthread_mutex_lock(&m->mutex);
    while (m->active_handlers)
        pthread_cond_wait(&m->changed, &m->mutex);
    pthread_mutex_unlock(&m->mutex);
}

void destroy_client_manager(client_manager_t *m) {
    pthread_cond_destroy(&m->changed);
    pthread_mutex_destroy(&m->mutex);
}
