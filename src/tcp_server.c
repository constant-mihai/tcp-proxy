#include <arpa/inet.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <netinet/in.h>
#include <unistd.h>
#include <pthread.h>
#include <stdlib.h>

#include "list.h"
#include "tcp_server.h"

void *tcp_server_listen(void *arg) {
    connection_queue_t *queue = (connection_queue_t*) arg;
    printf("starting tcp_server_listen\n");

    int                 listenfd, connfd;
    socklen_t           clilen;
    struct sockaddr_in  cliaddr, servaddr;

    if ( (listenfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        printf("socket error %s\n", strerror(errno));
    }

    bzero(&servaddr, sizeof(servaddr));
    servaddr.sin_family      = AF_INET;
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    servaddr.sin_port        = htons(12345);

    if (bind(listenfd,
             (struct sockaddr *) &servaddr,
             sizeof(servaddr)) < 0) {
        printf("bind error %s\n", strerror(errno));
    }

    if (listen(listenfd, 1024 /* listen queue size */ ) < 0) {
        printf("listen error %s\n", strerror(errno));
    }

    for ( ; ; ) {
        clilen = sizeof(cliaddr);
        if ( (connfd = accept(listenfd,
                              (struct sockaddr*) &cliaddr,
                              &clilen)) < 0) {
            if (errno == EINTR)
                continue;       /* back to for() */
            else
                printf("accept error\n");
        }

        connection_enqueue(queue, connfd);
    }
    if (close(listenfd) == -1)
        printf("close error: %s\n", strerror(errno));
    // TODO neeeds to be closed after the connfd is popped and
    // processed.
    //Close(connfd);
}

void connection_enqueue(connection_queue_t *connq, int connfd) {
    int ret = pthread_mutex_lock(&(connq->mutex));
    if (ret != 0) {
        printf("error locking the mutex %d\n", ret);
        exit(1);
    }

    list_append(connq->q, connfd);

    ret = pthread_mutex_unlock(&(connq->mutex));
    if (ret != 0) {
        printf("error unlocking the mutex %d\n", ret);
        exit(1);
    }
}

int connection_pop(connection_queue_t *connq) {
    int ret = pthread_mutex_lock(&(connq->mutex));
    if (ret != 0) {
        printf("error locking the mutex %d\n", ret);
        exit(1);
    }

    node_t *n = list_pop(connq->q);

    ret = pthread_mutex_unlock(&(connq->mutex));
    if (ret != 0) {
        printf("error unlocking the mutex %d\n", ret);
        exit(1);
    }

    if (n == NULL) return -1;
    else {
        int value = n->value;
        free(n);
        return value;
    }
}
