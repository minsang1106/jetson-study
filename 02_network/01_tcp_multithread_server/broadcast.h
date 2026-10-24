#ifndef BROADCAST_H
#define BROADCAST_H
#include "server.h"
void broadcast_message(client_manager_t *clients, const message_t *message);
void *broadcast_manager(void *argument);
#endif
