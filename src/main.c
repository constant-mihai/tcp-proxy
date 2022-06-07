#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <errno.h>

#include "workerpool.h"
#include "list.h"
#include "tcp_server.h"

void *handle_connection(void *arg) {
    char buf[1024];
    ssize_t nrecv = 0, nsend = 0;

    connection_queue_t *queue = (connection_queue_t*) arg;

    // TODO, needs a stop condition
    for ( ; ; ) {
        int connfd = connection_pop(queue);
        if (connfd < 0) {
            // queue is empty
            continue;
        }

        if ((nrecv = recv(connfd, buf, sizeof(buf), 0)) == -1) {
            printf("error on recv() %s\n", strerror(errno));
            exit(1);
        }

        printf("buff received: %.*s\n", (int)nrecv, buf);

        if ((nsend = send(connfd, buf, sizeof(buf), 0)) < 0)
        {
            printf("error on send() %s\n", strerror(errno));
            exit(1);
        }
    }
    return NULL;
}

int main(int argc, char **argv) {
    (void)argc;
    (void)argv;

    pthread_t thread_id;
    void *thread_result;
    //pthread_mutexattr_t mattr;

    connection_queue_t queue = {
        .q = list_create(),
        .mutex = PTHREAD_MUTEX_INITIALIZER,
    };

    int status = pthread_create(&thread_id,
                                NULL,
                                tcp_server_listen,
                                &queue);
    if (status != 0) {
        printf("Error starting listener thread %s\n",
               strerror (status));
        exit(1);
    }
    
    /* initialize a mutex to its default value */
    int ret = pthread_mutex_init(&(queue.mutex), NULL);
    if (ret != 0) printf("error initializing mutex %d\n", ret);

    /* initialize a mutex */
    //ret = pthread_mutex_init(&mp, &mattr); 


    workerpool_t *wp = workerpool_create(5);
    workerpool_start(wp, handle_connection, &queue);

    status = pthread_join(thread_id,
                              &thread_result);
    if (status != 0) {
        printf("Error joining listener thread :%s\n",
               strerror (status));
        exit(1);
    }
    
    ret = pthread_mutex_destroy(&(queue.mutex));

    return 0;
}
