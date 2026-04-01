#ifndef ALGORITHMS_SLL_H
#define ALGORITHMS_SLL_H

#include "sll.h"
#include "maze.h"
#include <string.h>
#include <limits.h>

// ── 1. DFS ────────────────────────────────────────────────────────────────────
int sll_dfs(const Maze *m) {
    int total=m->rows*m->cols, count=0;
    int *vis=calloc(total,sizeof(int));
    SLL stack; stack.head=NULL;
    SLLNode *s=sll_createNode(0); sll_pushFront(&stack,s); vis[0]=1;
    while (!sll_isEmpty(&stack)) {
        SLLNode *cur=sll_popFront(&stack);
        int pos=cur->value, r=pos/m->cols, c=pos%m->cols;
        count++; free(cur);
        for (int d=0;d<4;d++) {
            if (mwall(m,r,c,d)) continue;
            int nr=r+DR[d],nc=c+DC[d],np=nr*m->cols+nc;
            if (!vis[np]) { vis[np]=1; SLLNode *nd=sll_createNode(np); sll_pushFront(&stack,nd); }
        }
    }
    free(vis); return count;
}

// ── 2. BFS ────────────────────────────────────────────────────────────────────
int sll_bfs(const Maze *m) {
    int total=m->rows*m->cols, count=0;
    int *vis=calloc(total,sizeof(int));
    SLL queue; queue.head=NULL; SLLNode *tail=NULL;
    SLLNode *s=sll_createNode(0); queue.head=tail=s; vis[0]=1;
    while (!sll_isEmpty(&queue)) {
        SLLNode *cur=sll_popFront(&queue);
        if (tail==cur) tail=NULL;
        int pos=cur->value, r=pos/m->cols, c=pos%m->cols;
        count++; free(cur);
        for (int d=0;d<4;d++) {
            if (mwall(m,r,c,d)) continue;
            int nr=r+DR[d],nc=c+DC[d],np=nr*m->cols+nc;
            if (!vis[np]) {
                vis[np]=1; SLLNode *nd=sll_createNode(np);
                if (tail) { tail->next=nd; tail=nd; } else { queue.head=tail=nd; }
            }
        }
    }
    free(vis); return count;
}

// ── 3. IDDFS ──────────────────────────────────────────────────────────────────
static int sll_dls(const Maze *m, int *vis, int limit) {
    int total=m->rows*m->cols; memset(vis,0,total*sizeof(int));
    int count=0;
    SLL stack; stack.head=NULL;
    SLLNode *sp=sll_createNode(0), *sd=sll_createNode(0);
    sll_pushFront(&stack,sd); sll_pushFront(&stack,sp); vis[0]=1;
    while (!sll_isEmpty(&stack)) {
        SLLNode *pc=sll_popFront(&stack), *dc=sll_popFront(&stack);
        int pos=pc->value, depth=dc->value, r=pos/m->cols, c=pos%m->cols;
        count++; free(pc); free(dc);
        if (depth>=limit) continue;
        for (int d=0;d<4;d++) {
            if (mwall(m,r,c,d)) continue;
            int nr=r+DR[d],nc=c+DC[d],np=nr*m->cols+nc;
            if (!vis[np]) {
                vis[np]=1;
                SLLNode *nd=sll_createNode(np), *dd=sll_createNode(depth+1);
                sll_pushFront(&stack,dd); sll_pushFront(&stack,nd);
            }
        }
    }
    return count;
}

int sll_iddfs(const Maze *m) {
    int total=m->rows*m->cols, goal=total-1, tv=0;
    int *vis=malloc(total*sizeof(int));
    int max_depth = total < 500 ? total : 500;
    for (int lim=0;lim<=max_depth;lim++) {
        tv+=sll_dls(m,vis,lim);
        if (vis[goal]) break;
    }
    free(vis); return tv;
}

// ── 4. Bidirectional BFS ──────────────────────────────────────────────────────
int sll_bibfs(const Maze *m) {
    int total=m->rows*m->cols, goal=total-1, count=0;
    int *visF=calloc(total,sizeof(int)), *visB=calloc(total,sizeof(int));
    SLL fwd,bwd; fwd.head=bwd.head=NULL;
    SLLNode *tailF=NULL,*tailB=NULL;
    SLLNode *sf=sll_createNode(0); fwd.head=tailF=sf; visF[0]=1;
    SLLNode *sb=sll_createNode(goal); bwd.head=tailB=sb; visB[goal]=1;
    while (!sll_isEmpty(&fwd)&&!sll_isEmpty(&bwd)) {
        SLLNode *cur=sll_popFront(&fwd); if(tailF==cur) tailF=NULL;
        int pos=cur->value,r=pos/m->cols,c=pos%m->cols; count++; free(cur);
        for (int d=0;d<4;d++) {
            if (mwall(m,r,c,d)) continue;
            int nr=r+DR[d],nc=c+DC[d],np=nr*m->cols+nc;
            if (!visF[np]) {
                visF[np]=1; SLLNode *nd=sll_createNode(np);
                if (tailF) { tailF->next=nd; tailF=nd; } else { fwd.head=tailF=nd; }
            }
        }
        if (visF[goal]) break;
        cur=sll_popFront(&bwd); if(tailB==cur) tailB=NULL;
        pos=cur->value; r=pos/m->cols; c=pos%m->cols; count++; free(cur);
        for (int d=0;d<4;d++) {
            if (mwall(m,r,c,d)) continue;
            int nr=r+DR[d],nc=c+DC[d],np=nr*m->cols+nc;
            if (!visB[np]) {
                visB[np]=1; SLLNode *nd=sll_createNode(np);
                if (tailB) { tailB->next=nd; tailB=nd; } else { bwd.head=tailB=nd; }
            }
        }
        SLLNode *probe=fwd.head;
        while (probe) { if (visB[probe->value]) goto sll_bibfs_done; probe=probe->next; }
    }
sll_bibfs_done:
    while (!sll_isEmpty(&fwd)) { SLLNode *n=sll_popFront(&fwd); free(n); }
    while (!sll_isEmpty(&bwd)) { SLLNode *n=sll_popFront(&bwd); free(n); }
    free(visF); free(visB); return count;
}

// ── 5. Greedy Best-First ──────────────────────────────────────────────────────
int sll_gbfs(const Maze *m) {
    int total=m->rows*m->cols, gr=m->rows-1, gc=m->cols-1, count=0;
    int *vis=calloc(total,sizeof(int));
    SLL open; open.head=NULL;
    SLLNode *s=sll_createNode(0); open.head=s; vis[0]=1;
    while (open.head) {
        SLLNode *best=open.head,*bestPrev=NULL,*cur=open.head->next,*prev=open.head;
        int bhr=best->value/m->cols, bhc=best->value%m->cols;
        int bestH=heuristic(bhr,bhc,gr,gc);
        while (cur) {
            int r=cur->value/m->cols,c=cur->value%m->cols,h=heuristic(r,c,gr,gc);
            if (h<bestH) { bestH=h; best=cur; bestPrev=prev; }
            prev=cur; cur=cur->next;
        }
        if (bestPrev) bestPrev->next=best->next; else open.head=best->next;
        int pos=best->value,r=pos/m->cols,c=pos%m->cols; count++; free(best);
        if (pos==total-1) break;
        for (int d=0;d<4;d++) {
            if (mwall(m,r,c,d)) continue;
            int nr=r+DR[d],nc=c+DC[d],np=nr*m->cols+nc;
            if (!vis[np]) {
                vis[np]=1; SLLNode *nd=sll_createNode(np);
                nd->next=open.head; open.head=nd;
            }
        }
    }
    while (open.head) { SLLNode *n=open.head; open.head=n->next; free(n); }
    free(vis); return count;
}

// ── 6. A* ─────────────────────────────────────────────────────────────────────
int sll_astar(const Maze *m) {
    int total=m->rows*m->cols, gr=m->rows-1, gc=m->cols-1, count=0;
    int *vis=calloc(total,sizeof(int)), *g=malloc(total*sizeof(int));
    for (int i=0;i<total;i++) { g[i]=INT_MAX; } g[0]=0;
    SLL open; open.head=NULL;
    SLLNode *s=sll_createNode(0); open.head=s; vis[0]=1;
    while (open.head) {
        SLLNode *best=open.head,*bestPrev=NULL,*cur=open.head->next,*prev=open.head;
        int br=best->value/m->cols,bc=best->value%m->cols;
        int bestF=g[best->value]+heuristic(br,bc,gr,gc);
        while (cur) {
            int r=cur->value/m->cols,c=cur->value%m->cols,f=g[cur->value]+heuristic(r,c,gr,gc);
            if (f<bestF) { bestF=f; best=cur; bestPrev=prev; }
            prev=cur; cur=cur->next;
        }
        if (bestPrev) bestPrev->next=best->next; else open.head=best->next;
        int pos=best->value,r=pos/m->cols,c=pos%m->cols; count++; free(best);
        if (pos==total-1) break;
        for (int d=0;d<4;d++) {
            if (mwall(m,r,c,d)) continue;
            int nr=r+DR[d],nc=c+DC[d],np=nr*m->cols+nc; int ng=g[pos]+1;
            if (ng<g[np]) {
                g[np]=ng;
                if (!vis[np]) {
                    vis[np]=1; SLLNode *nd=sll_createNode(np);
                    nd->next=open.head; open.head=nd;
                }
            }
        }
    }
    while (open.head) { SLLNode *n=open.head; open.head=n->next; free(n); }
    free(vis); free(g); return count;
}

#endif
