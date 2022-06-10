#pragma once

#include "buffer/buffer.h"
#include <list/list.h>

#define EPOLL_TIMEOUT   100
#define MAX_EVENTS      32
#define NUMBER_OF_LISTENER_THREADS 1
#define MAX_LISTENER_SOCKETS 1024
#define MAX_CONNECTION_SOCKETS 65535
#define WAIT_TIME_NANOSECONDS 1000

#define CLOSE(fd) \
    do { \
        if (close(fd) == -1) { \
            fprintf(stderr, "error closing fd: %d, %s\n", fd, strerror(errno)); \
            exit(1); \
        } \
    } while(0)

typedef struct connection_queue {
    list_t *q;
    int *epoll_fds;
    buffer_t *listen_fds;
    buffer_t *conn_fds;
    pthread_mutex_t mutex;
    pthread_cond_t cv;
}connection_queue_t;

void *tcp_server_listen(void *arg);

connection_queue_t *connection_create();
void connection_enqueue(connection_queue_t *q, buffer_t *buff);
buffer_t *connection_pop(connection_queue_t *q);
void connection_process(connection_queue_t *queue);
void connection_destroy(connection_queue_t **connq);
