#include "workerpool.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

workerpool_t *workerpool_create(size_t size) {
    workerpool_t *wp = (workerpool_t*)calloc(1, sizeof(workerpool_t));
    wp->thread_ids = (pthread_t*)calloc(size, sizeof(pthread_t));
    wp->size = size;

    return wp;
}

void workerpool_start(workerpool_t *wp, thread_function fn, void* arg) {
    for (size_t i=0; i<wp->size; i++) {
        printf("initializing thread %ld\n", i);
        int status = pthread_create(wp->thread_ids+i,
                                    NULL,
                                    fn,
                                    arg);
        if (status != 0) {
            printf("Error starting thread %s\n", strerror (status));
            exit(1);
        }
    }

    for (size_t i=0; i<wp->size; i++) {
        int status = pthread_join (wp->thread_ids[i], &wp->thread_results);
        if (status != 0) {
            printf("Error joining thread %ld: %s\n", i, strerror (status));
            exit(1);
        }
    }
    printf("freeing thread\n");
    free(wp->thread_ids);
    free(wp->thread_results);
}
