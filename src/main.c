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

#include "log/log.h"

#include "workerpool.h"
#include "list/list.h"
#include "tcp_server.h"

//Tests
#include "tcp_server_test.h"
#include "tun_test.h"

int main(int argc, char **argv) {
    (void)argc;
    (void)argv;

    appname_g = argv[0];
    log_create(appname_g, "main");
    LOG_ADD_MODULE("default", 1, L_INFO, NULL);

    //TODO this is a place holder for unit-tests
    test_tcp_server();
    test_tun();

    MR_LOG_INFO("running main");
    MR_LOG_END();

    tcp_server_t *server = tcp_server_create();

    tcp_server_listen(server);
    
    workerpool_t *wp = workerpool_create("tcp-server-pool", 5);
    workerpool_start(wp, tcp_server_handle_connection, server);

    tcp_server_destroy(&server);
    log_destroy();

    return 0;
}
