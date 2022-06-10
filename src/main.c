#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <errno.h>
#include <netinet/in.h>
#include <sys/epoll.h>
#include <fcntl.h>

#include "workerpool.h"
#include "list/list.h"
#include "tcp_server.h"
#include "test_connection.h"

void receive(int connfd) {
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
    }
}

// TODO in the current pattern the listenfd is created before the worker pool. There is one epoll_fd
// created also before the worker pool.
// There are multiple accept threads which are polling the same epoll_fd.
// Another pattern I could try is to have mulitple listener threads, each with it's own epoll_fd.
void *handle_connection(void *arg) {
    int event_count, connfd, listenfd;
    struct sockaddr_in  cliaddr;
    socklen_t           clilen;
    struct epoll_event events[MAX_EVENTS];

    tcp_server_t *server = (tcp_server_t*) arg;

    for ( ; ; ) {
		event_count = epoll_wait(*server->epoll_fds, events, MAX_EVENTS, EPOLL_TIMEOUT);

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
                buffer_append_mem(server->conn_fds, (void*)&connfd, sizeof(int));
                // TODO, every thread could have it's own epoll fd.
                epoll_ctl_add(*server->epoll_fds,
                              connfd,
                              EPOLLIN | EPOLLET | EPOLLRDHUP | EPOLLHUP);
            } else if (events[i].events & EPOLLIN) {
                receive(events[i].data.fd);
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

int main(int argc, char **argv) {
    (void)argc;
    (void)argv;

    //TODO this is a place holder for unit-tests
    test_tcp_server();

    printf("running main\n");

    tcp_server_t *server = tcp_server_create();

    tcp_server_listen(server);
    
    workerpool_t *wp = workerpool_create(5);
    workerpool_start(wp, handle_connection, server);

    tcp_server_destroy(&server);

    return 0;
}
