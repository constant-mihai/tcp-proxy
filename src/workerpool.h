#pragma once
#include <pthread.h>

typedef struct workerpool {
    size_t size;
    pthread_t *thread_ids;
    void *thread_results;
}workerpool_t;

typedef void* (*thread_function)(void*);

workerpool_t *workerpool_create(size_t size);

void workerpool_start(workerpool_t* wp, thread_function fn, void *arg);
