#include <arpa/inet.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <netinet/in.h>
#include <unistd.h>
#include <pthread.h>
#include <stdlib.h>
#include <sys/epoll.h>
#include <fcntl.h>
#include <sys/time.h>

#include <buffer/buffer.h>
#include <utils/utils.h>
#include "tcp_server.h"

void receive(connection_queue_t *queue, int connfd) {
    buffer_t *buf = buffer_create(1500); // TODO, what's a good buffer size?
    ssize_t nrecv = 0; //, nsend = 0;

    // TODO, needs a stop condition
    for ( ; ; ) {
        if ((nrecv = recv(connfd, buf->val, sizeof(buf), 0)) == -1) {
            printf("error on recv() %s\n", strerror(errno));
            exit(1);
        }
        buf->len = nrecv;
        printf("enqueuing buf : %.*s\n", (int)buf->len, (char*)buf->val);
        connection_enqueue(queue, buf);
    }
}

void epoll_ctl_add(int epfd, int fd, uint32_t events) {
	struct epoll_event ev;
	ev.events = events;
	ev.data.fd = fd;
	if (epoll_ctl(epfd, EPOLL_CTL_ADD, fd, &ev) == -1) {
        fprintf(stderr, "error on activating epoll_fd: %s\n", strerror(errno));
		exit(1);
	}
}

void *tcp_server_listen(void *arg) {
    connection_queue_t *connq = (connection_queue_t*) arg;
    printf("starting tcp_server_listen\n");
	
    struct epoll_event events[MAX_EVENTS];
    int                 listenfd, connfd, event_count;
    socklen_t           clilen;
    struct sockaddr_in  cliaddr, servaddr;

    if ((listenfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        fprintf(stderr, "socket error %s\n", strerror(errno));
        exit(1);
    }
    buffer_append_mem(connq->listen_fds, (void*)&listenfd, sizeof(int));

    bzero(&servaddr, sizeof(servaddr));
    servaddr.sin_family      = AF_INET;
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    servaddr.sin_port        = htons(12345);

    if (bind(listenfd,
             (struct sockaddr *) &servaddr,
             sizeof(servaddr)) < 0) {
        fprintf(stderr, "bind error %s\n", strerror(errno));
        exit(1);
    }

    //TODO handle errors
	if (fcntl(listenfd,
              F_SETFD,
              fcntl(listenfd, F_GETFD, 0) | O_NONBLOCK) == -1) {
        fprintf(stderr, "error setting fd options %s", strerror(errno));
		exit(1);
	}

    // TODO I should refactor this and store the fd in an array.
    // I should also use epoll for all the accepted connections.
    if (listen(listenfd, 1024 /* listen queue size */ ) < 0) {
        fprintf(stderr, "listen error %s\n", strerror(errno));
        exit(1);
    }

    epoll_ctl_add(*connq->epoll_fds, listenfd, EPOLLIN | EPOLLOUT | EPOLLET);

    for ( ; ; ) {
		event_count = epoll_wait(*connq->epoll_fds, events, MAX_EVENTS, EPOLL_TIMEOUT);

        if (event_count == -1) {
            if (errno == EINTR) continue;
            else {
                fprintf(stderr, "error on epoll wait %s\n", strerror(errno));
                exit(1);
            }
        }

        if (event_count == 0) {
            // epoll wait timed out
            continue;
        }
        
        for (int i = 0; i < event_count; i++) {
			if (events[i].data.fd == listenfd) {
                clilen = sizeof(cliaddr);
                //TODO handle errors
                if ((connfd = accept(listenfd,
                                     (struct sockaddr*) &cliaddr,
                                     &clilen)) < 0) {
                    if (errno == EINTR)
                        continue;
                    else
                        printf("accept error\n");

                    //TODO handle errors
                    if (fcntl(connfd,
                              F_SETFD,
                              fcntl(connfd, F_GETFD, 0) | O_NONBLOCK) == -1) {
                        fprintf(stderr, "error setting fd options %s", strerror(errno));
                        exit(1);
                    }
                }
                buffer_append_mem(connq->conn_fds, (void*)&connfd, sizeof(int));
                // TODO, every thread should have it's own epoll fd.
                epoll_ctl_add(*connq->epoll_fds,
                              connfd,
                              EPOLLIN | EPOLLET | EPOLLRDHUP | EPOLLHUP);
            } else if (events[i].events & EPOLLIN) {
                receive(connq, events[i].data.fd);
            } else if (events[i].events & (EPOLLRDHUP | EPOLLHUP)) {
                epoll_ctl(*connq->epoll_fds, EPOLL_CTL_DEL,
                          events[i].data.fd, NULL);
                CLOSE(events[i].data.fd);
                continue;

            } else {
                fprintf(stderr, "unexpected event\n");
            }
        }
    }
    //TODO this moves into the connection_destroy
    //if (close(listenfd) == -1) printf("error closing listenfd: %s\n", strerror(errno));
    //if (close(connfd) == -1) printf("error closing connfd: %s\n", strerror(errno));
}

connection_queue_t *connection_create() {
    int ret = 0;
    connection_queue_t *queue = NULL;
    MALLOC(queue, 1, connection_queue_t);
    queue->q = list_create();

    ret = pthread_mutex_init(&queue->mutex, NULL);
    if (ret != 0) {
        fprintf(stderr, "error initializing mutex %d\n", ret);
        exit(1);
    }

    ret = pthread_cond_init(&queue->cv, NULL);
    if (ret != 0) {
        fprintf(stderr, "error initializing condition variable%d\n", ret);
        exit(1);
    }

    MALLOC(queue->epoll_fds, NUMBER_OF_LISTENER_THREADS, int);
    for (int i=0; i<NUMBER_OF_LISTENER_THREADS; i++) {
        int *efd = queue->epoll_fds + i;
        *efd = epoll_create(1);
        if (*efd == -1) {
            fprintf(stderr, "error creating epoll fd : %s\n", strerror(errno));
            exit(1);
        }
    }

    queue->listen_fds = buffer_create(MAX_LISTENER_SOCKETS);
    queue->conn_fds = buffer_create(MAX_CONNECTION_SOCKETS);

    return queue;
}

void connection_enqueue(connection_queue_t *connq, buffer_t *buf) {
    int ret = pthread_mutex_lock(&(connq->mutex));
    if (ret != 0) {
        printf("error locking the mutex %d\n", ret);
        exit(1);
    }

    list_append(connq->q, buf);
    ret = pthread_cond_signal(&connq->cv);
    if (ret != 0) {
        printf("error signaling the condition variable %d\n", ret);
        exit(1);
    }

    ret = pthread_mutex_unlock(&(connq->mutex));
    if (ret != 0) {
        printf("error unlocking the mutex %d\n", ret);
        exit(1);
    }
}

buffer_t *connection_pop(connection_queue_t *connq) {
    struct timespec   ts;

    int ret = pthread_mutex_lock(&(connq->mutex));
    if (ret != 0) {
        printf("error locking the mutex %d\n", ret);
        exit(1);
    }

    //TODO: how expensive is getting the time?
    /* Convert from timeval to timespec */
    if (clock_gettime(CLOCK_REALTIME, &ts)) {
        fprintf(stderr, "error getting time %s\n", strerror(errno));
        exit(1);
    }
    ts.tv_sec += 1;
    // TODO: if I want to use nanoseconds, I have to make sure they don't overflow
    // otherwise I will get an EINVAL.
    //ts.tv_nsec += WAIT_TIME_NANOSECONDS;
    //TODO a cond wait here introduces a thundering herd problem 
    ret = pthread_cond_timedwait(&connq->cv, &connq->mutex, &ts);
    if (ret != 0) {
        if (ret == ETIMEDOUT ) {
            ;
        } else {
            printf("error on condition wait %d: %s\n", ret, strerror(ret));
            exit(1);
        }
    }

    node_t *n = list_pop(connq->q);

    ret = pthread_mutex_unlock(&(connq->mutex));
    if (ret != 0) {
        printf("error unlocking the mutex %d\n", ret);
        exit(1);
    }

    if (n == NULL) return NULL;
    else {
        buffer_t *buf = n->value;
        free(n);
        return buf;
    }
}

void connection_process(connection_queue_t *queue) {
    while (1) {
        buffer_t* buf = connection_pop(queue);
        if (buf == NULL) continue;

        printf("de-queuing buf : %.*s\n", (int)buf->len, (char*)buf->val);
        buffer_destroy(&buf);
    }
}

void connection_destroy(connection_queue_t **connq) {
    int ret = 0;
    connection_queue_t *vconnq = *connq;
    ret = pthread_mutex_destroy(&vconnq->mutex);
    if (ret != 0) {
        fprintf(stderr, "error destroying mutex %d\n", ret);
        exit(1);
    }

    ret = pthread_cond_destroy(&vconnq->cv);
    if (ret != 0) {
        fprintf(stderr, "error destroying condition variable %d\n", ret);
        exit(1);
    }

    list_destroy(&vconnq->q);

    //TODO iterate through epoll_fds and tear them down.
    int *it = NULL;
    for (int i=0; i<NUMBER_OF_LISTENER_THREADS; i++) {
        while ((it = buffer_advance(vconnq->listen_fds, sizeof(int))) != NULL) {
            if (epoll_ctl(*(int*)(vconnq->epoll_fds+i),
                          EPOLL_CTL_DEL,
                          *(int*)it,
                          NULL) == -1) {
                fprintf(stderr,
                        "error removing epoll_fd from monitoring list: %s\n",
                        strerror(errno));
                exit(1);
            }

            ret = close(*(int*)it);
        }
        //ret = close(events[i].data.fd);
        //if (ret == -1) {
        //    fprintf(stderr, "error closing epoll fd: %s\n", strerror(errno));
        //    exit(1);
        //}

        ret = close(*(int*)vconnq->epoll_fds);
        if (ret == -1) {
            fprintf(stderr, "error closing epoll fd: %s\n", strerror(errno));
            exit(1);
        }
    }

    buffer_destroy(&vconnq->listen_fds);

    it = NULL;
    while ((it = buffer_advance(vconnq->conn_fds, sizeof(int))) != NULL) {
        ret = close(*(int*)it);
    }
    buffer_destroy(&vconnq->conn_fds);
}
