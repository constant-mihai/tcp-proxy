#pragma once
#include <pthread.h>

typedef struct workerpool {
    size_t cap;
    size_t len;
    pthread_t *thread_ids;
    void *thread_results;
    char *name;
}workerpool_t;

typedef struct worker_arg {
    void *caller_arg;
    char *name;
    int id;
} worker_arg_t;

typedef void* (*thread_function)(void*);

workerpool_t *workerpool_create(const char* name, size_t size);

void workerpool_start(workerpool_t* wp, thread_function fn, void *arg);
