#pragma once

long long count;

typedef struct connection_queue {
    list_t *q;
    pthread_mutex_t mutex;
}connection_queue_t;

void *tcp_server_listen(void *arg);

void connection_enqueue(connection_queue_t *q, int connfd);
int connection_pop(connection_queue_t *q);
