#include "message_queue.h"
#include <stdlib.h>
#include <string.h>

int initialize_message_queue(message_queue_t *q) {
    *q = (message_queue_t){0};
    int error = pthread_mutex_init(&q->mutex, NULL);
    if (error) return error;
    error = pthread_cond_init(&q->available, NULL);
    if (error) pthread_mutex_destroy(&q->mutex);
    return error;
}

int enqueue_message(message_queue_t *q, const char *data, size_t length) {
    if (length > BUFFER_SIZE) return -1;
    message_t *message = malloc(sizeof(*message));
    if (!message) return -1;
    memcpy(message->data, data, length);
    message->length = length;
    message->next = NULL;
    pthread_mutex_lock(&q->mutex);
    if (q->closed) {
        pthread_mutex_unlock(&q->mutex);
        free(message);
        return -1;
    }
    if (q->tail) q->tail->next = message;
    else q->head = message;
    q->tail = message;
    pthread_cond_signal(&q->available);
    pthread_mutex_unlock(&q->mutex);
    return 0;
}

message_t *dequeue_message(message_queue_t *q) {
    pthread_mutex_lock(&q->mutex);
    while (!q->head && !q->closed)
        pthread_cond_wait(&q->available, &q->mutex);
    message_t *message = q->head;
    if (message) {
        q->head = message->next;
        if (!q->head) q->tail = NULL;
    }
    pthread_mutex_unlock(&q->mutex);
    return message;
}

void close_message_queue(message_queue_t *q) {
    pthread_mutex_lock(&q->mutex);
    q->closed = true;
    pthread_cond_broadcast(&q->available);
    pthread_mutex_unlock(&q->mutex);
}

void destroy_message_queue(message_queue_t *q) {
    message_t *message = q->head;
    while (message) {
        message_t *next = message->next;
        free(message);
        message = next;
    }
    pthread_cond_destroy(&q->available);
    pthread_mutex_destroy(&q->mutex);
}
