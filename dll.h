#ifndef DLL_H
#define DLL_H

#include <stdlib.h>
#include <stdio.h>

// Standard doubly linked list — prev + next pointers, no cache
typedef struct DLLNode {
    int value;
    struct DLLNode *prev;
    struct DLLNode *next;
} DLLNode;

typedef struct {
    DLLNode *head;
    DLLNode *tail;
} DLL;

static inline DLLNode *dll_createNode(int val) {
    DLLNode *n = malloc(sizeof(DLLNode));
    if (!n) return NULL;
    n->value = val;
    n->prev  = NULL;
    n->next  = NULL;
    return n;
}

static inline void dll_pushFront(DLL *list, DLLNode *nd) {
    nd->prev = NULL;
    nd->next = list->head;
    if (list->head) list->head->prev = nd;
    list->head = nd;
    if (!list->tail) list->tail = nd;
}

static inline DLLNode *dll_popFront(DLL *list) {
    if (!list->head) return NULL;
    DLLNode *top = list->head;
    list->head   = top->next;
    if (list->head) list->head->prev = NULL;
    else            list->tail = NULL;
    top->next = top->prev = NULL;
    return top;
}

static inline void dll_pushBack(DLL *list, DLLNode *nd) {
    nd->next = NULL;
    nd->prev = list->tail;
    if (list->tail) list->tail->next = nd;
    list->tail = nd;
    if (!list->head) list->head = nd;
}

// O(1) — key advantage of doubly linked list
static inline DLLNode *dll_popBack(DLL *list) {
    if (!list->tail) return NULL;
    DLLNode *back = list->tail;
    list->tail    = back->prev;
    if (list->tail) list->tail->next = NULL;
    else            list->head = NULL;
    back->next = back->prev = NULL;
    return back;
}

// Traverse to position — O(n), no cache
static inline DLLNode *dll_get(DLL *list, int pos) {
    DLLNode *t = list->head;
    for (int i = 0; i < pos && t; i++) t = t->next;
    return t;
}

static inline int dll_isEmpty(DLL *list) { return list->head == NULL; }

#endif
