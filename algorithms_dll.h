#ifndef ALGORITHMS_DLL_H
#define ALGORITHMS_DLL_H

#include "dll.h"
#include "maze.h"
#include <string.h>
#include <limits.h>

// ── 1. DFS ────────────────────────────────────────────────────────────────────
int dll_dfs(const Maze *m) {
    int total=m->rows*m->cols, count=0;
    int *vis=calloc(total,sizeof(int));
    DLL stack; stack.head=stack.tail=NULL;
    DLLNode *s=dll_createNode(0); dll_pushFront(&stack,s); vis[0]=1;
    while (!dll_isEmpty(&stack)) {
        DLLNode *cur=dll_popFront(&stack);
        int pos=cur->value,r=pos/m->cols,c=pos%m->cols; count++; free(cur);
        for (int d=0;d<4;d++) {
            if (mwall(m,r,c,d)) continue;
            int nr=r+DR[d],nc=c+DC[d],np=nr*m->cols+nc;
            if (!vis[np]) { vis[np]=1; DLLNode *nd=dll_createNode(np); dll_pushFront(&stack,nd); }
        }
    }
    free(vis); return count;
}

// ── 2. BFS ────────────────────────────────────────────────────────────────────
int dll_bfs(const Maze *m) {
    int total=m->rows*m->cols, count=0;
    int *vis=calloc(total,sizeof(int));
    DLL queue; queue.head=queue.tail=NULL;
    DLLNode *s=dll_createNode(0); dll_pushBack(&queue,s); vis[0]=1;
    while (!dll_isEmpty(&queue)) {
        DLLNode *cur=dll_popFront(&queue);
        int pos=cur->value,r=pos/m->cols,c=pos%m->cols; count++; free(cur);
        for (int d=0;d<4;d++) {
            if (mwall(m,r,c,d)) continue;
            int nr=r+DR[d],nc=c+DC[d],np=nr*m->cols+nc;
            if (!vis[np]) { vis[np]=1; DLLNode *nd=dll_createNode(np); dll_pushBack(&queue,nd); }
        }
    }
    free(vis); return count;
}

// ── 3. IDDFS ──────────────────────────────────────────────────────────────────
static int dll_dls(const Maze *m, int *vis, int limit) {
    int total=m->rows*m->cols; memset(vis,0,total*sizeof(int));
    int count=0;
    DLL stack; stack.head=stack.tail=NULL;
    DLLNode *sp=dll_createNode(0),*sd=dll_createNode(0);
    dll_pushFront(&stack,sd); dll_pushFront(&stack,sp); vis[0]=1;
    while (!dll_isEmpty(&stack)) {
        DLLNode *pc=dll_popFront(&stack),*dc=dll_popFront(&stack);
        int pos=pc->value,depth=dc->value,r=pos/m->cols,c=pos%m->cols;
        count++; free(pc); free(dc);
        if (depth>=limit) continue;
        for (int d=0;d<4;d++) {
            if (mwall(m,r,c,d)) continue;
            int nr=r+DR[d],nc=c+DC[d],np=nr*m->cols+nc;
            if (!vis[np]) {
                vis[np]=1;
                DLLNode *nd=dll_createNode(np),*dd=dll_createNode(depth+1);
                dll_pushFront(&stack,dd); dll_pushFront(&stack,nd);
            }
        }
    }
    return count;
}

int dll_iddfs(const Maze *m) {
    int total=m->rows*m->cols,goal=total-1,tv=0;
    int *vis=malloc(total*sizeof(int));
    int max_depth = total < 500 ? total : 500;
    for (int lim=0;lim<=max_depth;lim++) {
        tv+=dll_dls(m,vis,lim);
        if (vis[goal]) break;
    }
    free(vis); return tv;
}

// ── 4. Bidirectional BFS ──────────────────────────────────────────────────────
int dll_bibfs(const Maze *m) {
    int total=m->rows*m->cols,goal=total-1,count=0;
    int *visF=calloc(total,sizeof(int)),*visB=calloc(total,sizeof(int));
    DLL fwd,bwd; fwd.head=fwd.tail=bwd.head=bwd.tail=NULL;
    DLLNode *sf=dll_createNode(0); dll_pushBack(&fwd,sf); visF[0]=1;
    DLLNode *sb=dll_createNode(goal); dll_pushBack(&bwd,sb); visB[goal]=1;
    while (!dll_isEmpty(&fwd)&&!dll_isEmpty(&bwd)) {
        DLLNode *cur=dll_popFront(&fwd);
        int pos=cur->value,r=pos/m->cols,c=pos%m->cols; count++; free(cur);
        for (int d=0;d<4;d++) {
            if (mwall(m,r,c,d)) continue;
            int nr=r+DR[d],nc=c+DC[d],np=nr*m->cols+nc;
            if (!visF[np]) { visF[np]=1; DLLNode *nd=dll_createNode(np); dll_pushBack(&fwd,nd); }
        }
        if (visF[goal]) break;
        cur=dll_popFront(&bwd);
        pos=cur->value; r=pos/m->cols; c=pos%m->cols; count++; free(cur);
        for (int d=0;d<4;d++) {
            if (mwall(m,r,c,d)) continue;
            int nr=r+DR[d],nc=c+DC[d],np=nr*m->cols+nc;
            if (!visB[np]) { visB[np]=1; DLLNode *nd=dll_createNode(np); dll_pushBack(&bwd,nd); }
        }
        DLLNode *probe=fwd.head;
        while (probe) { if (visB[probe->value]) goto dll_bibfs_done; probe=probe->next; }
    }
dll_bibfs_done:
    while (!dll_isEmpty(&fwd)) { DLLNode *n=dll_popFront(&fwd); free(n); }
    while (!dll_isEmpty(&bwd)) { DLLNode *n=dll_popFront(&bwd); free(n); }
    free(visF); free(visB); return count;
}

// ── 5. Greedy Best-First ──────────────────────────────────────────────────────
int dll_gbfs(const Maze *m) {
    int total=m->rows*m->cols,gr=m->rows-1,gc=m->cols-1,count=0;
    int *vis=calloc(total,sizeof(int));
    DLL open; open.head=open.tail=NULL;
    DLLNode *s=dll_createNode(0); dll_pushFront(&open,s); vis[0]=1;
    while (!dll_isEmpty(&open)) {
        DLLNode *best=open.head,*bestPrev=NULL,*cur=open.head->next,*prev=open.head;
        int bhr=best->value/m->cols, bhc=best->value%m->cols;
        int bestH=heuristic(bhr,bhc,gr,gc);
        while (cur) {
            int r=cur->value/m->cols,c=cur->value%m->cols,h=heuristic(r,c,gr,gc);
            if (h<bestH) { bestH=h; best=cur; bestPrev=prev; }
            prev=cur; cur=cur->next;
        }
        // O(1) removal using prev pointer
        if (bestPrev) {
            bestPrev->next=best->next;
            if (best->next) best->next->prev=bestPrev;
            else open.tail=bestPrev;
        } else {
            open.head=best->next;
            if (open.head) open.head->prev=NULL; else open.tail=NULL;
        }
        int pos=best->value,r=pos/m->cols,c=pos%m->cols; count++; free(best);
        if (pos==total-1) break;
        for (int d=0;d<4;d++) {
            if (mwall(m,r,c,d)) continue;
            int nr=r+DR[d],nc=c+DC[d],np=nr*m->cols+nc;
            if (!vis[np]) { vis[np]=1; DLLNode *nd=dll_createNode(np); dll_pushFront(&open,nd); }
        }
    }
    while (!dll_isEmpty(&open)) { DLLNode *n=dll_popFront(&open); free(n); }
    free(vis); return count;
}

// ── 6. A* ─────────────────────────────────────────────────────────────────────
int dll_astar(const Maze *m) {
    int total=m->rows*m->cols,gr=m->rows-1,gc=m->cols-1,count=0;
    int *vis=calloc(total,sizeof(int)),*g=malloc(total*sizeof(int));
    for (int i=0;i<total;i++) { g[i]=INT_MAX; } g[0]=0;
    DLL open; open.head=open.tail=NULL;
    DLLNode *s=dll_createNode(0); dll_pushFront(&open,s); vis[0]=1;
    while (!dll_isEmpty(&open)) {
        DLLNode *best=open.head,*bestPrev=NULL,*cur=open.head->next,*prev=open.head;
        int br=best->value/m->cols,bc=best->value%m->cols;
        int bestF=g[best->value]+heuristic(br,bc,gr,gc);
        while (cur) {
            int r=cur->value/m->cols,c=cur->value%m->cols,f=g[cur->value]+heuristic(r,c,gr,gc);
            if (f<bestF) { bestF=f; best=cur; bestPrev=prev; }
            prev=cur; cur=cur->next;
        }
        if (bestPrev) {
            bestPrev->next=best->next;
            if (best->next) best->next->prev=bestPrev; else open.tail=bestPrev;
        } else {
            open.head=best->next;
            if (open.head) open.head->prev=NULL; else open.tail=NULL;
        }
        int pos=best->value,r=pos/m->cols,c=pos%m->cols; count++; free(best);
        if (pos==total-1) break;
        for (int d=0;d<4;d++) {
            if (mwall(m,r,c,d)) continue;
            int nr=r+DR[d],nc=c+DC[d],np=nr*m->cols+nc; int ng=g[pos]+1;
            if (ng<g[np]) {
                g[np]=ng;
                if (!vis[np]) { vis[np]=1; DLLNode *nd=dll_createNode(np); dll_pushFront(&open,nd); }
            }
        }
    }
    while (!dll_isEmpty(&open)) { DLLNode *n=dll_popFront(&open); free(n); }
    free(vis); free(g); return count;
}

#endif
