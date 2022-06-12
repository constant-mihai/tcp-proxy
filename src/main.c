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
#include "tcp_server_test.h"


int main(int argc, char **argv) {
    (void)argc;
    (void)argv;

    //TODO this is a place holder for unit-tests
    test_tcp_server();

    printf("running main\n");

    tcp_server_t *server = tcp_server_create();

    tcp_server_listen(server);
    
    workerpool_t *wp = workerpool_create(5);
    workerpool_start(wp, tcp_server_handle_connection, server);

    tcp_server_destroy(&server);

    return 0;
}
