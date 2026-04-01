#ifndef SLL_H
#define SLL_H

#include <stdlib.h>
#include <stdio.h>

// Standard singly linked list — no cache, no extras
typedef struct SLLNode {
    int value;
    struct SLLNode *next;
} SLLNode;

typedef struct {
    SLLNode *head;
} SLL;

static inline SLLNode *sll_createNode(int val) {
    SLLNode *n = malloc(sizeof(SLLNode));
    if (!n) return NULL;
    n->value = val;
    n->next  = NULL;
    return n;
}

// Push to front (stack push / deque front)
static inline void sll_pushFront(SLL *list, SLLNode *nd) {
    nd->next   = list->head;
    list->head = nd;
}

// Pop from front
static inline SLLNode *sll_popFront(SLL *list) {
    if (!list->head) return NULL;
    SLLNode *top = list->head;
    list->head   = top->next;
    top->next    = NULL;
    return top;
}

// Push to back (enqueue)
static inline void sll_pushBack(SLL *list, SLLNode *nd) {
    nd->next = NULL;
    if (!list->head) { list->head = nd; return; }
    SLLNode *t = list->head;
    while (t->next) t = t->next;
    t->next = nd;
}

// Pop from back (O(n) — same as your dequeueBack)
static inline SLLNode *sll_popBack(SLL *list) {
    if (!list->head) return NULL;
    if (!list->head->next) {
        SLLNode *n = list->head;
        list->head = NULL;
        return n;
    }
    SLLNode *t = list->head;
    while (t->next->next) t = t->next;
    SLLNode *back = t->next;
    t->next = NULL;
    return back;
}

// Traverse to position — pure O(n), no cache
static inline SLLNode *sll_get(SLL *list, int pos) {
    SLLNode *t = list->head;
    for (int i = 0; i < pos && t; i++) t = t->next;
    return t;
}

static inline int sll_isEmpty(SLL *list) { return list->head == NULL; }

#endif
