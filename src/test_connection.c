#include "test_connection.h" 
#include "tcp_server.h"
#include <pthread.h>

void *enqueue_thread(void *arg) {
    connection_queue_t *queue = (connection_queue_t*) arg;
    buffer_t *buf1 = buffer_create(1500);
    buffer_t *buf2 = buffer_create(1500);
    connection_enqueue(queue, buf1);
    connection_enqueue(queue, buf2);
    sleep(1);
    connection_enqueue(queue, buf1);
    connection_enqueue(queue, buf2);
    return NULL;
}

void *dequeue_thread(void *arg) {
    connection_queue_t *queue = (connection_queue_t*) arg;
    buffer_t *pop1 = connection_pop(queue);
    buffer_t *pop2 = connection_pop(queue);
    return NULL;
}

void test_connections() {
    printf("test_connections\n");
    connection_queue_t *queue = connection_create();

    pthread_t thread_id[2];
    int status = pthread_create(&thread_id[0],
                                NULL,
                                enqueue_thread,
                                queue);
    if (status != 0) {
        printf("Error starting enque thread %s\n",
               strerror (status));
        exit(1);
    }

    status = pthread_create(&thread_id[1],
                            NULL,
                            dequeue_thread,
                            queue);
    if (status != 0) {
        printf("Error starting dequeue thread %s\n",
               strerror (status));
        exit(1);
    }

    status = pthread_join(thread_id[0], NULL);
    if (status != 0) {
        printf("Error joining enqueue thread :%s\n",
               strerror (status));
        exit(1);
    }

    status = pthread_join(thread_id[1], NULL);
    if (status != 0) {
        printf("Error joining dequeue thread :%s\n",
               strerror (status));
        exit(1);
    }
    connection_destroy(&queue);
}

