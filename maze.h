#ifndef MAZE_H
#define MAZE_H

#include <stdlib.h>
#include <string.h>

#define NORTH 0
#define EAST  1
#define SOUTH 2
#define WEST  3

static const int DR[] = {-1, 0, 1, 0};
static const int DC[] = { 0, 1, 0,-1};
static const int OPP[] = {SOUTH, WEST, NORTH, EAST};

typedef struct {
    int rows, cols;
    char *walls;   // [r*cols*4 + c*4 + dir]
} Maze;

static inline int  mwall(const Maze *m, int r, int c, int d) { return m->walls[(r*m->cols+c)*4+d]; }
static inline void msetWall(Maze *m, int r, int c, int d, int v) { m->walls[(r*m->cols+c)*4+d]=(char)v; }
static inline int  inBounds(const Maze *m, int r, int c) { return r>=0&&r<m->rows&&c>=0&&c<m->cols; }

Maze *maze_create(int rows, int cols) {
    Maze *m   = malloc(sizeof(Maze));
    m->rows   = rows; m->cols = cols;
    m->walls  = malloc(rows * cols * 4);
    memset(m->walls, 1, rows * cols * 4);
    return m;
}

void maze_free(Maze *m) { free(m->walls); free(m); }

// Iterative recursive backtracker
void maze_generate(Maze *m, int sr, int sc, unsigned int seed) {
    srand(seed);
    int total    = m->rows * m->cols;
    int *visited = calloc(total, sizeof(int));
    int *stack   = malloc(total * sizeof(int));
    int top      = 0;

    visited[sr * m->cols + sc] = 1;
    stack[top++] = sr * m->cols + sc;

    while (top > 0) {
        int cur = stack[top-1];
        int r   = cur / m->cols, c = cur % m->cols;
        int dirs[4] = {0,1,2,3};
        for (int i = 3; i > 0; i--) {
            int j = rand() % (i+1);
            int t = dirs[i]; dirs[i] = dirs[j]; dirs[j] = t;
        }
        int moved = 0;
        for (int i = 0; i < 4; i++) {
            int d = dirs[i], nr = r+DR[d], nc = c+DC[d];
            if (inBounds(m,nr,nc) && !visited[nr*m->cols+nc]) {
                msetWall(m, r,  c,  d,      0);
                msetWall(m, nr, nc, OPP[d], 0);
                visited[nr*m->cols+nc] = 1;
                stack[top++] = nr*m->cols+nc;
                moved = 1; break;
            }
        }
        if (!moved) top--;
    }
    free(visited); free(stack);
}

// Manhattan heuristic
static inline int heuristic(int r, int c, int gr, int gc) {
    return abs(r-gr) + abs(c-gc);
}

#endif
