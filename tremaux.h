#ifndef TREMAUX_H
#define TREMAUX_H

// ═══════════════════════════════════════════════════════════════════════════
// Trémaux's Algorithm — maze solver that explicitly tracks the walked path.
//
// Core idea:
//   Maintain a path list where position 0 = current cell.
//   At each step, push an unvisited neighbor onto the path.
//   At a dead end, scan path[1], path[2], ... to find the nearest junction
//   (a cell that still has at least one unvisited neighbor), then pop back
//   to it.
//
// Fair comparison:
//   - Node+cache uses traverseList_pub (cached positional API)
//   - PSLL uses psll_get_behind_current (no-cache positional API, same Node size)
//   - SLL uses sll_get (no-cache positional API)
//   - DLL uses dll_get (no-cache positional API)
// ═══════════════════════════════════════════════════════════════════════════

#include "maze.h"
#include "linkedlist.h"
#include "psll.h"
#include "sll.h"
#include "dll.h"
#include <string.h>

// ── Helpers: does cell at position k in the path have an unvisited neighbor?
static int has_unvisited_neighbor(const Maze *m, int cell, const int *visited) {
    int r = cell / m->cols, c = cell % m->cols;
    for (int d = 0; d < 4; d++) {
        if (mwall(m, r, c, d)) continue;
        int nr = r + DR[d], nc = c + DC[d];
        if (!visited[nr * m->cols + nc]) return 1;
    }
    return 0;
}

// ─────────────────────────────────────────────────────────────────────────────
// Node+cache structure (cached positional traversal)
// ─────────────────────────────────────────────────────────────────────────────
int cache_tremaux(const Maze *m) {
    int total = m->rows * m->cols;
    int goal  = total - 1;
    int *vis  = calloc(total, sizeof(int));
    int count = 0;

    // path: position 0 = current cell. Only push/pop at head — shift preserves cache.
    List path; path.head = NULL;
    memset(&path.cache, 0, sizeof(path.cache));

    Node *start = createNode(0);
    path.head = start; vis[0] = 1;

    while (path.head) {
        int cur = path.head->value;
        count++;
        if (cur == goal) break;

        // Find an unvisited neighbor of current cell
        int r = cur / m->cols, c = cur % m->cols;
        int next = -1;
        for (int d = 0; d < 4; d++) {
            if (mwall(m, r, c, d)) continue;
            int nr = r + DR[d], nc = c + DC[d], np = nr * m->cols + nc;
            if (!vis[np]) { next = np; break; }
        }

        if (next != -1) {
            vis[next] = 1;
            Node *nd = createNode(next);
            nd->next  = path.head;
            path.head = nd;
            cacheOnPush_pub(&path.cache);
        } else {
            int junction_pos = -1;
            int scan = 1;
            Node *behind, *candidate;
            while (1) {
                traverseList_inline(&path, scan, &behind, &candidate);
                if (!candidate) break;
                if (has_unvisited_neighbor(m, candidate->value, vis)) {
                    junction_pos = scan;
                    break;
                }
                scan++;
            }
            if (junction_pos == -1) break;

            for (int k = 0; k < junction_pos; k++) {
                Node *old = path.head;
                path.head = old->next;
                cacheOnPop_pub(&path.cache);
                free(old);
            }
        }
    }

    while (path.head) { Node *n = path.head; path.head = n->next; free(n); }
    free(vis);
    return count;
}

// ─────────────────────────────────────────────────────────────────────────────
// PSLL (same Node size as Node+cache, NO cache) — behind/current traversal baseline
// ─────────────────────────────────────────────────────────────────────────────
int psll_tremaux(const Maze *m) {
    int total = m->rows * m->cols, goal = total - 1;
    int *vis  = calloc(total, sizeof(int));
    int count = 0;

    Node *head = createNode(0);
    vis[0] = 1;

    while (head) {
        int cur = head->value;
        count++;
        if (cur == goal) break;

        int r = cur / m->cols, c = cur % m->cols;
        int next = -1;
        for (int d = 0; d < 4; d++) {
            if (mwall(m, r, c, d)) continue;
            int nr = r + DR[d], nc = c + DC[d], np = nr * m->cols + nc;
            if (!vis[np]) { next = np; break; }
        }

        if (next != -1) {
            vis[next] = 1;
            Node *nd = createNode(next);
            nd->next = head;
            head = nd;
        } else {
            int junction_pos = -1;
            for (int scan = 1; ; scan++) {
                Node *behind = NULL, *candidate = NULL;
                psll_get_behind_current(head, scan, &behind, &candidate);
                if (!candidate) break;
                if (has_unvisited_neighbor(m, candidate->value, vis)) {
                    junction_pos = scan;
                    break;
                }
            }
            if (junction_pos == -1) break;

            for (int k = 0; k < junction_pos; k++) {
                Node *old = head;
                head = old->next;
                free(old);
            }
        }
    }

    while (head) { Node *n = head; head = n->next; free(n); }
    free(vis);
    return count;
}

// ─────────────────────────────────────────────────────────────────────────────
// Standard SLL
// ─────────────────────────────────────────────────────────────────────────────
int sll_tremaux(const Maze *m) {
    int total = m->rows * m->cols, goal = total - 1;
    int *vis  = calloc(total, sizeof(int));
    int count = 0;

    SLL path; path.head = NULL;
    SLLNode *start = sll_createNode(0);
    path.head = start; vis[0] = 1;

    while (path.head) {
        int cur = path.head->value;
        count++;
        if (cur == goal) break;

        int r = cur / m->cols, c = cur % m->cols;
        int next = -1;
        for (int d = 0; d < 4; d++) {
            if (mwall(m, r, c, d)) continue;
            int nr = r + DR[d], nc = c + DC[d], np = nr * m->cols + nc;
            if (!vis[np]) { next = np; break; }
        }

        if (next != -1) {
            vis[next] = 1;
            SLLNode *nd = sll_createNode(next);
            nd->next = path.head; path.head = nd;
        } else {
            int junction_pos = -1;
            for (int scan = 1; ; scan++) {
                SLLNode *candidate = sll_get(&path, scan);
                if (!candidate) break;
                if (has_unvisited_neighbor(m, candidate->value, vis)) {
                    junction_pos = scan;
                    break;
                }
            }
            if (junction_pos == -1) break;

            for (int k = 0; k < junction_pos; k++) {
                SLLNode *old = path.head;
                path.head = old->next;
                free(old);
            }
        }
    }

    while (path.head) { SLLNode *n = path.head; path.head = n->next; free(n); }
    free(vis);
    return count;
}

// ─────────────────────────────��───────────────────────────────────────────────
// Standard DLL
// ─────────────────────────────────────────────────────────────────────────────
int dll_tremaux(const Maze *m) {
    int total = m->rows * m->cols, goal = total - 1;
    int *vis  = calloc(total, sizeof(int));
    int count = 0;

    DLL path; path.head = path.tail = NULL;
    DLLNode *start = dll_createNode(0);
    dll_pushFront(&path, start); vis[0] = 1;

    while (path.head) {
        int cur = path.head->value;
        count++;
        if (cur == goal) break;

        int r = cur / m->cols, c = cur % m->cols;
        int next = -1;
        for (int d = 0; d < 4; d++) {
            if (mwall(m, r, c, d)) continue;
            int nr = r + DR[d], nc = c + DC[d], np = nr * m->cols + nc;
            if (!vis[np]) { next = np; break; }
        }

        if (next != -1) {
            vis[next] = 1;
            DLLNode *nd = dll_createNode(next);
            dll_pushFront(&path, nd);
        } else {
            int junction_pos = -1;
            for (int scan = 1; ; scan++) {
                DLLNode *candidate = dll_get(&path, scan);
                if (!candidate) break;
                if (has_unvisited_neighbor(m, candidate->value, vis)) {
                    junction_pos = scan;
                    break;
                }
            }
            if (junction_pos == -1) break;

            for (int k = 0; k < junction_pos; k++) {
                DLLNode *old = dll_popFront(&path);
                free(old);
            }
        }
    }

    while (path.head) { DLLNode *n = dll_popFront(&path); free(n); }
    free(vis);
    return count;
}

#endif
