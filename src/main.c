#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <errno.h>

#include "workerpool.h"
#include "list/list.h"
#include "tcp_server.h"
#include "test_connection.h"

void *handle_connection(void *arg) {
    connection_queue_t *queue = (connection_queue_t*) arg;
    connection_process(queue);

    return NULL;
}

int main(int argc, char **argv) {
    (void)argc;
    (void)argv;

    //TODO this is a place holder for unit-tests
    test_connections();

    printf("running main\n");

    pthread_t thread_id;
    void *thread_result;

    connection_queue_t *queue = connection_create();

    int status = pthread_create(&thread_id,
                                NULL,
                                tcp_server_listen,
                                queue);
    if (status != 0) {
        printf("Error starting listener thread %s\n",
               strerror (status));
        exit(1);
    }
    
    workerpool_t *wp = workerpool_create(5);
    workerpool_start(wp, handle_connection, queue);

    status = pthread_join(thread_id,
                          &thread_result);
    if (status != 0) {
        printf("Error joining listener thread :%s\n",
               strerror (status));
        exit(1);
    }

    connection_destroy(&queue);

    return 0;
}
