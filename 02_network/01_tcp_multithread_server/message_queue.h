#ifndef MESSAGE_QUEUE_H
#define MESSAGE_QUEUE_H
#include <pthread.h>
#include <stdbool.h>
#include <stddef.h>
#define BUFFER_SIZE 1024
typedef struct message {
    size_t length;
    char data[BUFFER_SIZE];
    struct message *next;
} message_t;
typedef struct {
    message_t *head, *tail;
    bool closed;
    pthread_mutex_t mutex;
    pthread_cond_t available;
} message_queue_t;
int initialize_message_queue(message_queue_t *queue);
int enqueue_message(message_queue_t *queue, const char *data, size_t length);
message_t *dequeue_message(message_queue_t *queue);
void close_message_queue(message_queue_t *queue);
void destroy_message_queue(message_queue_t *queue);
#endif
