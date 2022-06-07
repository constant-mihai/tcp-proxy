#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>

#include "list.h"

list_t *list_create() {
    list_t *l = (list_t*)malloc(sizeof(list_t));
    l->head = l->tail = NULL;

    return l;
}

// insert after prev
void list_insert(list_t *list, node_t *prev, int value) {
    node_t *n = (node_t*)calloc(1, sizeof(node_t));
    n->value = value;

    if (list->head == NULL && list->tail == NULL) {
        n->next = list->head;
        list->head = list->tail = n;
        n->prev = NULL;
        return;
    }

    if (prev == NULL) {
        n->next = list->head;
        list->head = n;
        n->prev = NULL;
        return;
    }

    n->next = prev->next;
    n->prev = prev;

    if (prev->next != NULL) {
        prev->next->prev = n;
    } else {
        list->tail = n;
    }

    prev->next = n;
}

void list_append(list_t *list, int value) {
    node_t *it = list->head;
    while(it != NULL && it->next != NULL) {
        it = it->next;
    }

    return list_insert(list, it, value);
}

int list_remove(list_t *list, int value) {
    node_t *it = list->head;
    while (it != NULL && it->value != value) {
        it = it->next;
    }

    if (it == NULL) return -1;

    if (it->prev != NULL) {
        it->prev->next = it->next;
    } else {
        list->head = it->next;
    }

    if (it->next != NULL) {
        it->next->prev = it->prev;
    } else {
        list->tail = it->prev;
    }

    free(it);
    return 0;
}

node_t *list_pop(list_t *list) {
    if (!list->tail) return NULL;

    node_t *t = list->tail;
    if (list->tail->prev != NULL) {
        list->tail = list->tail->prev;
        list->tail->next = NULL;
    } else {
        list->tail = list->head = NULL;
    }

    t->prev = NULL;
    return t;
}
