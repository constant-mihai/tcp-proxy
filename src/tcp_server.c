#include <arpa/inet.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/epoll.h>
#include <fcntl.h>
#include <sys/time.h>

#include <buffer/buffer.h>
#include <utils/utils.h>
#include "tcp_server.h"

void epoll_ctl_add(int epfd, int fd, uint32_t events) {
	struct epoll_event ev;
	ev.events = events;
	ev.data.fd = fd;
	if (epoll_ctl(epfd, EPOLL_CTL_ADD, fd, &ev) == -1) {
        fprintf(stderr, "error on activating epoll_fd: %s\n", strerror(errno));
		exit(1);
	}
}

// tcp_server_listen will start listening on a socket and will add the listenfd to epoll.
// TODO: make the number of sockets configurable
// TODO: we could pass a pointer to a socket data structure in event.data.ptr. see `man epoll_ctl`.
// this pointer is returned to us when epoll detects an i/o event on listenfd.
void tcp_server_listen(tcp_server_t *server) {
    int                 listenfd;
    struct sockaddr_in  servaddr;

    bzero(&servaddr, sizeof(servaddr));
    servaddr.sin_family      = AF_INET;
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    servaddr.sin_port        = htons(12345);

    printf("starting tcp_server_listen\n");

    if ((listenfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        fprintf(stderr, "socket error %s\n", strerror(errno));
        exit(1);
    }
    buffer_append_mem(server->listen_fds, (void*)&listenfd, sizeof(int));

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

    if (listen(listenfd, 1024 /* listen server size */ ) < 0) {
        fprintf(stderr, "listen error %s\n", strerror(errno));
        exit(1);
    }

    epoll_ctl_add(*server->epoll_fds, listenfd, EPOLLIN | EPOLLOUT | EPOLLET);
}

tcp_server_t *tcp_server_create() {
    tcp_server_t *server = NULL;
    MALLOC(server, 1, tcp_server_t);

    MALLOC(server->epoll_fds, NUMBER_OF_LISTENER_THREADS, int);
    for (int i=0; i<NUMBER_OF_LISTENER_THREADS; i++) {
        int *efd = server->epoll_fds + i;
        *efd = epoll_create(1);
        if (*efd == -1) {
            fprintf(stderr, "error creating epoll fd : %s\n", strerror(errno));
            exit(1);
        }
    }

    server->listen_fds = buffer_create(MAX_LISTENER_SOCKETS);
    server->conn_fds = buffer_create(MAX_CONNECTION_SOCKETS);

    return server;
}

void tcp_server_destroy(tcp_server_t **server) {
    int ret = 0;
    tcp_server_t *vserver = *server;

    //TODO iterate through epoll_fds and tear them down.
    int *it = NULL;
    for (int i=0; i<NUMBER_OF_LISTENER_THREADS; i++) {
        while ((it = buffer_advance(vserver->listen_fds, sizeof(int))) != NULL) {
            if (epoll_ctl(*(int*)(vserver->epoll_fds+i),
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
        //TODO do I close these here or in handle_connection?
        //ret = close(events[i].data.fd);
        //if (ret == -1) {
        //    fprintf(stderr, "error closing epoll fd: %s\n", strerror(errno));
        //    exit(1);
        //}

        ret = close(*(int*)vserver->epoll_fds);
        if (ret == -1) {
            fprintf(stderr, "error closing epoll fd: %s\n", strerror(errno));
            exit(1);
        }
    }

    buffer_destroy(&vserver->listen_fds);

    it = NULL;
    while ((it = buffer_advance(vserver->conn_fds, sizeof(int))) != NULL) {
        ret = close(*(int*)it);
    }
    buffer_destroy(&vserver->conn_fds);
}
