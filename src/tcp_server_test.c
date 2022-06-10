#include "tcp_server.h"
#include <pthread.h>

void test_tcp_server() {
    //TODO
    printf("test_tcp_server\n");
    tcp_server_t *server = tcp_server_create();

    tcp_server_destroy(&server);
}

