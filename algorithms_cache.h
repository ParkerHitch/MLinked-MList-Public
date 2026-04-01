#ifndef ALGORITHMS_CACHE_H
#define ALGORITHMS_CACHE_H

#include "linkedlist.h"
#include "maze.h"
#include <string.h>
#include <limits.h>

// ── helpers ───────────────────────────────────────────────────────────────────
static Stack cache_makeStack(void) {
    Stack s; s.list.head = NULL;
    memset(&s.list.cache, 0, sizeof(s.list.cache));
    return s;
}
static Deque cache_makeDeque(void) {
    Deque d; d.list.head = NULL; d.tail = NULL;
    memset(&d.list.cache, 0, sizeof(d.list.cache));
    return d;
}

// ── 1. DFS ────────────────────────────────────────────────────────────────────
int cache_dfs(const Maze *m) {
    int total=m->rows*m->cols, count=0;
    int *vis=calloc(total,sizeof(int));
    Stack stack=cache_makeStack();
    Node *s=createNode(0); push(&stack,s); vis[0]=1;
    while (!stackIsEmpty(&stack)) {
        Node *cur=pop(&stack);
        int pos=cur->value, r=pos/m->cols, c=pos%m->cols;
        count++; free(cur);
        for (int d=0;d<4;d++) {
            if (mwall(m,r,c,d)) continue;
            int nr=r+DR[d],nc=c+DC[d],np=nr*m->cols+nc;
            if (!vis[np]) { vis[np]=1; Node *nd=createNode(np); push(&stack,nd); }
        }
    }
    free(vis); return count;
}

// ── 2. BFS ────────────────────────────────────────────────────────────────────
int cache_bfs(const Maze *m) {
    int total=m->rows*m->cols, count=0;
    int *vis=calloc(total,sizeof(int));
    Deque dq=cache_makeDeque();
    Node *s=createNode(0); enqueueBack(&dq,s); vis[0]=1;
    while (!dequeIsEmpty(&dq)) {
        Node *cur=dequeueFront(&dq);
        int pos=cur->value, r=pos/m->cols, c=pos%m->cols;
        count++; free(cur);
        for (int d=0;d<4;d++) {
            if (mwall(m,r,c,d)) continue;
            int nr=r+DR[d],nc=c+DC[d],np=nr*m->cols+nc;
            if (!vis[np]) { vis[np]=1; Node *nd=createNode(np); enqueueBack(&dq,nd); }
        }
    }
    free(vis); return count;
}

// ── 3. IDDFS ──────────────────────────────────────────────────────────────────
static int cache_dls(const Maze *m, int *vis, int limit) {
    int total=m->rows*m->cols; memset(vis,0,total*sizeof(int));
    int count=0;
    Stack stack=cache_makeStack();
    Node *spos=createNode(0), *sdep=createNode(0);
    push(&stack,sdep); push(&stack,spos); vis[0]=1;
    while (!stackIsEmpty(&stack)) {
        Node *pcur=pop(&stack), *dcur=pop(&stack);
        int pos=pcur->value, depth=dcur->value, r=pos/m->cols, c=pos%m->cols;
        count++; free(pcur); free(dcur);
        if (depth>=limit) continue;
        for (int d=0;d<4;d++) {
            if (mwall(m,r,c,d)) continue;
            int nr=r+DR[d],nc=c+DC[d],np=nr*m->cols+nc;
            if (!vis[np]) {
                vis[np]=1;
                Node *nd=createNode(np), *dd=createNode(depth+1);
                push(&stack,dd); push(&stack,nd);
            }
        }
    }
    return count;
}

int cache_iddfs(const Maze *m) {
    int total=m->rows*m->cols, goal=total-1, tv=0;
    int *vis=malloc(total*sizeof(int));
    int max_depth=total<500?total:500;
    for (int lim=0;lim<=max_depth;lim++) {
        tv+=cache_dls(m,vis,lim);
        if (vis[goal]) break;
    }
    free(vis); return tv;
}

// ── 4. Bidirectional BFS ──────────────────────────────────────────────────────
int cache_bibfs(const Maze *m) {
    int total=m->rows*m->cols, goal=total-1, count=0;
    int *visF=calloc(total,sizeof(int)), *visB=calloc(total,sizeof(int));
    Deque fwd=cache_makeDeque(), bwd=cache_makeDeque();
    Node *sf=createNode(0);    enqueueBack(&fwd,sf); visF[0]=1;
    Node *sb=createNode(goal); enqueueBack(&bwd,sb); visB[goal]=1;
    while (!dequeIsEmpty(&fwd)&&!dequeIsEmpty(&bwd)) {
        Node *cur=dequeueFront(&fwd);
        int pos=cur->value,r=pos/m->cols,c=pos%m->cols; count++; free(cur);
        for (int d=0;d<4;d++) {
            if (mwall(m,r,c,d)) continue;
            int nr=r+DR[d],nc=c+DC[d],np=nr*m->cols+nc;
            if (!visF[np]) { visF[np]=1; Node *nd=createNode(np); enqueueBack(&fwd,nd); }
        }
        if (visF[goal]) break;
        cur=dequeueFront(&bwd);
        pos=cur->value; r=pos/m->cols; c=pos%m->cols; count++; free(cur);
        for (int d=0;d<4;d++) {
            if (mwall(m,r,c,d)) continue;
            int nr=r+DR[d],nc=c+DC[d],np=nr*m->cols+nc;
            if (!visB[np]) { visB[np]=1; Node *nd=createNode(np); enqueueBack(&bwd,nd); }
        }
        Node *probe=fwd.list.head;
        while (probe) { if (visB[probe->value]) goto cache_bibfs_done; probe=probe->next; }
    }
cache_bibfs_done:
    while (!dequeIsEmpty(&fwd)) { Node *n=dequeueFront(&fwd); free(n); }
    while (!dequeIsEmpty(&bwd)) { Node *n=dequeueFront(&bwd); free(n); }
    free(visF); free(visB); return count;
}

// ── 5. Greedy Best-First ──────────────────────────────────────────────────────
int cache_gbfs(const Maze *m) {
    int total=m->rows*m->cols, gr=m->rows-1, gc=m->cols-1, count=0;
    int *vis=calloc(total,sizeof(int));
    List open; open.head=NULL;
    memset(&open.cache,0,sizeof(open.cache));
    Node *s=createNode(0); open.head=s; vis[0]=1;

    while (open.head) {
        Node *best=open.head, *bestPrev=NULL;
        Node *cur=open.head->next, *prev=open.head;
        int bhr=best->value/m->cols, bhc=best->value%m->cols;
        int bestH=heuristic(bhr,bhc,gr,gc), bestPos=0, scanPos=1;

        while (cur) {
            int r=cur->value/m->cols, c=cur->value%m->cols;
            int h=heuristic(r,c,gr,gc);
            if (h<bestH) { bestH=h; best=cur; bestPrev=prev; bestPos=scanPos; }
            prev=cur; cur=cur->next; scanPos++;
        }

        if (bestPrev) bestPrev->next=best->next;
        else          open.head    =best->next;
        invalidateCache_pub(&open.cache, bestPos);

        int pos=best->value, r=pos/m->cols, c=pos%m->cols;
        count++; free(best);
        if (pos==total-1) break;

        for (int d=0;d<4;d++) {
            if (mwall(m,r,c,d)) continue;
            int nr=r+DR[d],nc=c+DC[d],np=nr*m->cols+nc;
            if (!vis[np]) {
                vis[np]=1;
                Node *nd=createNode(np);
                nd->next=open.head; open.head=nd;
            }
        }
    }
    while (open.head) { Node *n=open.head; open.head=n->next; free(n); }
    free(vis); return count;
}

// ── 6. A* ─────────────────────────────────────────────────────────────────────
int cache_astar(const Maze *m) {
    int total=m->rows*m->cols, gr=m->rows-1, gc=m->cols-1, count=0;
    int *vis=calloc(total,sizeof(int));
    int *g=malloc(total*sizeof(int));
    for (int i=0;i<total;i++) { g[i]=INT_MAX; } g[0]=0;
    List open; open.head=NULL;
    memset(&open.cache,0,sizeof(open.cache));
    Node *s=createNode(0); open.head=s; vis[0]=1;

    while (open.head) {
        Node *best=open.head, *bestPrev=NULL;
        Node *cur=open.head->next, *prev=open.head;
        int br=best->value/m->cols, bc=best->value%m->cols;
        int bestF=g[best->value]+heuristic(br,bc,gr,gc), bestPos=0, scanPos=1;

        while (cur) {
            int r=cur->value/m->cols, c=cur->value%m->cols;
            int f=g[cur->value]+heuristic(r,c,gr,gc);
            if (f<bestF) { bestF=f; best=cur; bestPrev=prev; bestPos=scanPos; }
            prev=cur; cur=cur->next; scanPos++;
        }

        if (bestPrev) bestPrev->next=best->next;
        else          open.head    =best->next;
        invalidateCache_pub(&open.cache, bestPos);

        int pos=best->value, r=pos/m->cols, c=pos%m->cols;
        count++; free(best);
        if (pos==total-1) break;

        for (int d=0;d<4;d++) {
            if (mwall(m,r,c,d)) continue;
            int nr=r+DR[d],nc=c+DC[d],np=nr*m->cols+nc; int ng=g[pos]+1;
            if (ng<g[np]) {
                g[np]=ng;
                if (!vis[np]) {
                    vis[np]=1;
                    Node *nd=createNode(np);
                    nd->next=open.head; open.head=nd;
                }
            }
        }
    }
    while (open.head) { Node *n=open.head; open.head=n->next; free(n); }
    free(vis); free(g); return count;
}

#endif

