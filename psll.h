#ifndef PSLL_H
#define PSLL_H

// PSLL = positional SLL baseline (NO cache), using the SAME Node type as linkedlist.h.
// Provides behind/current traversal without allocations.

#include "linkedlist.h"

static inline void psll_get_behind_current(Node *head, int position,
                                          Node **outBehind, Node **outCurrent)
{
    if (position < 0) { *outBehind = NULL; *outCurrent = NULL; return; }

    Node *behind = NULL;
    Node *cur    = head;

    for (int i = 0; i < position && cur; i++) {
        behind = cur;
        cur    = cur->next;
    }

    *outBehind  = behind;
    *outCurrent = cur;
}

#endif // PSLL_H
