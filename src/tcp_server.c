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

void tcp_server_receive(int connfd) {
    buffer_t *buf = buffer_create(1500); // TODO, what's a good buffer size?
    ssize_t nrecv = 0;

    // TODO, needs a stop condition
    for ( ; ; ) {
        nrecv = recv(connfd, buf->val, sizeof(buf), 0);
        if (nrecv == -1) {
            if (errno == EINTR) {
                break;
            } else if (errno == EAGAIN || errno == EWOULDBLOCK) {
                break;
            } else {
                printf("error on recv() %s\n", strerror(errno));
                exit(1);
            }
        } else if (nrecv == 0) {
            printf("received EOF\n");
            break;
        }
        buf->len = nrecv;
        printf("received buf : %.*s\n", (int)buf->len, (char*)buf->val);
    }
}

// TODO in the current pattern the listenfd is created before the worker pool. There is one epoll_fd
// created also before the worker pool.
// There are multiple accept threads which are polling the same epoll_fd.
// Another pattern I could try is to have mulitple listener threads, each with it's own epoll_fd.
void *tcp_server_handle_connection(void *arg) {
    int event_count, connfd, listenfd;
    struct sockaddr_in  cliaddr;
    socklen_t           clilen;
    struct epoll_event events[MAX_EVENTS];

    tcp_server_t *server = (tcp_server_t*) arg;

    for ( ; ; ) {
		event_count = epoll_wait(*server->epoll_fds,
                                 events,
                                 MAX_EVENTS,
                                 EPOLL_TIMEOUT);

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
        
        listenfd = *(int*)server->listen_fds->val;
        for (int i = 0; i < event_count; i++) {
			if (events[i].data.fd == listenfd) {
                //The  addrlen  argument is a value-result argument: the caller
                //must initialize it to contain the  size	(in  bytes)  of  the
                //structure  pointed  to by addr; on return it will contain the
                //actual size of the peer address.
                //
                //The returned address is truncated if the buffer	provided  is
                //too  small; in this case, addrlen will return a value greater
                //than was supplied to the call.
                clilen = sizeof(cliaddr);
                if ((connfd = accept(listenfd,
                                     (struct sockaddr*) &cliaddr,
                                     &clilen)) < 0) {
                    if (errno == EINTR) {
                        continue;
                    }
                    //The socket is marked nonblocking	and  no  connections
                    //are   present   to   be  accepted.   POSIX.1-2001  and
                    //POSIX.1-2008 allow either error  to  be  returned  for
                    //this  case, and do not require these constants to have
                    //the same value, so a portable application should check
                    //for both possibilities.
                    else if (errno == EAGAIN || errno == EWOULDBLOCK) {
                        continue;
                    }
                    else {
                        fprintf(stderr, "accept error\n");
                        exit(1);
                    }
                }
                //TODO handle errors
                if (fcntl(connfd,
                          F_SETFD,
                          fcntl(connfd, F_GETFD, 0) | O_NONBLOCK) == -1) {
                    fprintf(stderr, "error setting fd options %s", strerror(errno));
                    exit(1);
                }
                buffer_append_mem(server->conn_fds, (void*)&connfd, sizeof(int));
                // TODO, every thread could have it's own epoll fd.
                epoll_ctl_add(*server->epoll_fds,
                              connfd,
                              EPOLLIN | EPOLLET | EPOLLRDHUP | EPOLLHUP);
            } else if (events[i].events & EPOLLIN) {
                tcp_server_receive(events[i].data.fd);
            } else if (events[i].events & (EPOLLRDHUP | EPOLLHUP)) {
                epoll_ctl(*server->epoll_fds, EPOLL_CTL_DEL,
                          events[i].data.fd, NULL);
                CLOSE(events[i].data.fd);
                continue;
            } else {
                fprintf(stderr, "unexpected event\n");
            }
        }
    }

    return NULL;
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

    // TODO handle errors
    // Not sure if I should set this as non-blocking. Excerpt from accept() man page:
    // The argument sockfd is a socket that has been created with socket(2), bound to a lo‐
    // cal address with bind(2), and is listening for connections after a listen(2).
    // ...
    // If  no pending connections are present on the queue, and the socket is not marked as
    // nonblocking, accept() blocks the caller until  a  connection  is  present.   If	the
    // socket  is  marked  nonblocking and no pending connections are present on the queue,
    // accept() fails with the error EAGAIN or EWOULDBLOCK.
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

    epoll_ctl_add(*server->epoll_fds,
                  listenfd,
                  EPOLLIN);
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
