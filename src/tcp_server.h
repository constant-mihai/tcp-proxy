#pragma once

#include <stdint.h>
#include "buffer/buffer.h"
#include <list/list.h>

#define EPOLL_TIMEOUT   100
#define MAX_EVENTS      32
#define NUMBER_OF_LISTENER_THREADS 1
#define MAX_LISTENER_SOCKETS 1024
#define MAX_CONNECTION_SOCKETS 65535
#define WAIT_TIME_NANOSECONDS 1000

const char* appname_g;

#define CLOSE(fd) \
    do { \
        if (close(fd) == -1) { \
            fprintf(stderr, "error closing fd: %d, %s\n", fd, strerror(errno)); \
            exit(1); \
        } \
    } while(0)

typedef struct tcp_server {
    int *epoll_fds;
    buffer_t *listen_fds;
    buffer_t *conn_fds;
} tcp_server_t;

void epoll_ctl_add(int epfd, int fd, uint32_t events);

void tcp_server_receive(int connfd);

void *tcp_server_handle_connection(void *arg);

tcp_server_t *tcp_server_create();

void tcp_server_listen(tcp_server_t *server);

void tcp_server_destroy(tcp_server_t **connq);
