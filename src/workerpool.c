#include "workerpool.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <utils/utils.h>
#include <log/log.h>

workerpool_t *workerpool_create(const char* name, size_t size) {
    workerpool_t *wp = (workerpool_t*)calloc(1, sizeof(workerpool_t));
    wp->thread_ids = (pthread_t*)calloc(size, sizeof(pthread_t));
    wp->cap = size;
    wp->len = 0;
    MALLOC(wp->name, strlen(name)+1, char);
    strcpy(wp->name, name);

    return wp;
}

void workerpool_start(workerpool_t *wp, thread_function fn, void* caller_arg) {
    worker_arg_t *arg = NULL;

    MALLOC(arg, wp->cap, worker_arg_t);

    for (size_t i=0; i<wp->cap; i++)
    {
        MR_LOG_INFO("initializing thread");
        mr_log_uint64("thread-idx", i);
        MR_LOG_END();

        (*(arg+i)).id = i;
        MALLOC((*(arg+i)).name, strlen(wp->name)+1, char);
        strcpy((*(arg+i)).name, wp->name);
        (*(arg+i)).caller_arg = caller_arg;
        int status = pthread_create(wp->thread_ids+i,
                                    NULL,
                                    fn,
                                    (void*)arg);
        if (status != 0) {
            const char* strerr = strerror (status);
            MR_LOG_ERR("Error starting thread");
            mr_log_error(strerr);
            MR_LOG_END();
            exit(1);
        }

        wp->len++;
    }

    for (size_t i=0; i<wp->cap; i++) {
        int status = pthread_join (wp->thread_ids[i], &wp->thread_results);
        if (status != 0) {
            const char* strerr = strerror (status);
            MR_LOG_ERR("Error joining thread");
            mr_log_uint64("thread-idx", i);
            mr_log_error(strerr);
            MR_LOG_END();
            exit(1);
        }
    }
    MR_LOG_INFO("freeing thread");
    MR_LOG_END();
    free(wp->thread_ids);
    free(wp->thread_results);
}
