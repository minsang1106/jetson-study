#ifndef DATA_STRUCTURE_H
#define DATA_STRUCTURE_H

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>

typedef struct __node_t {
    void* data;
    struct __node_t *next;
} node_t;

typedef struct __queue_t {
    node_t *head;
    node_t *tail;
    pthread_mutex_t headLock;
    pthread_mutex_t tailLock;
} queue_t;

void Queue_Init(queue_t *q);
void Queue_Enqueue(queue_t *q, void* data);
int Queue_Dequeue(queue_t *q, void** data);

#endif