## Detailed Commentary on `linkedlist.h` and `linkedlist.c` (Node+cache List)

This document walks through `linkedlist.h` and `linkedlist.c` in detail, explaining the intent behind each block and the design choices that make this positional cache novel.

---

## A. Header: `linkedlist.h`

### A.1 Configuration and compile-time guarantees

```1:18:C:\Users\lamam\Downloads\MLinked-MList\linkedlist.h
#ifndef LINKEDLIST_H
#define LINKEDLIST_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef CACHE_SIZE
#define CACHE_SIZE 8
#endif
// CACHE_SIZE must be a power of 2 so that (vpos & (CACHE_SIZE-1)) == (vpos % CACHE_SIZE).
// The bitwise form is a single AND instruction vs. an integer division.
_Static_assert((CACHE_SIZE & (CACHE_SIZE - 1)) == 0,
               "CACHE_SIZE must be a power of 2 (4, 8, 16, 32, 64, …)");

// Minimum physical position worth caching (position 0 is always the head).
#define CACHE_THRESHOLD 1
```

- Standard header guard + standard library headers to support allocation and I/O.
- `CACHE_SIZE`:
  - Defaults to 8 but is configurable at compile time (e.g., via `-DCACHE_SIZE=16`).
  - `_Static_assert` enforces that it is a power of 2, which is essential for using `& (CACHE_SIZE - 1)` as a fast modulo.
- `CACHE_THRESHOLD`:
  - Encodes a design choice **not** to cache the head or very shallow positions.
  - This keeps the cache focused on more expensive positions deeper in the list.

### A.2 Core data structures

```19:38:C:\Users\lamam\Downloads\MLinked-MList\linkedlist.h
// ----------------------- Node -----------------------
typedef struct Node {
    int value;
    struct Node *next;
} Node;

// ----------------------- Cache -----------------------
typedef struct {
    unsigned int position;  // Stores the virtual position (= physical_pos + cache->offset)
    Node *behind;
    Node *current;
    unsigned int generation; 
} NodeCacheEntry;

typedef struct {
    NodeCacheEntry entries[CACHE_SIZE];
    unsigned int generation; // Incrementing this invalidates all entries in O(1)
    int offset;              // Virtual offset: virtual_pos = physical_pos + offset
                             // Decrement on push, increment on pop — O(1), no data movement
} ListCache;
```

- `Node` is a minimal singly linked node with an `int` payload and a `next` pointer.
- `NodeCacheEntry` and `ListCache` constitute the positional cache overlay:
  - `position`:
    - Stores a **virtual position** rather than the raw index, allowing head updates via `offset` adjustment.
  - `behind` / `current`:
    - A cached snapshot of the traversal state, so callers get both the node at a position and its predecessor.
  - `generation`:
    - Used to distinguish valid entries from stale ones when the list structure changes.
- `ListCache.entries` is a fixed-size direct-mapped table:
  - A power-of-two array index is computed by ANDing against `CACHE_SIZE - 1`.
  - This is small enough to be cache-friendly at the hardware level and simple to reason about.

### A.3 Inline cache lookup

```40:52:C:\Users\lamam\Downloads\MLinked-MList\linkedlist.h
// ── Internal cache lookup (shared by .c and inline callers) ──────────────────
#ifndef NODECACHE_DISABLE_POSITIONAL_CACHE
static inline NodeCacheEntry *checkCache(ListCache *cache, int position) {
    if (position < CACHE_THRESHOLD) return NULL;
    unsigned int vpos = (unsigned int)(position + cache->offset);
    unsigned int slot = vpos & (CACHE_SIZE - 1);
    if (cache->entries[slot].generation == cache->generation && cache->entries[slot].position   == vpos)
    {
        return &cache->entries[slot];
    }
    return NULL;
}
#endif
```

- `checkCache` is intentionally in the header:
  - It must be shared between hot inline functions and the `.c` file without incurring function call overhead.
- The function:
  - Skips positions `< CACHE_THRESHOLD`.
  - Computes the virtual position and slot index exactly as in `linkedlist.c`.
  - Verifies that the slot:
    - Belongs to the current `generation`.
    - Holds the same `vpos` (to avoid false positives due to aliasing).
- The guarded `#ifndef NODECACHE_DISABLE_POSITIONAL_CACHE` means:
  - When the cache is disabled, all callers automatically fall back to the `.c` implementation’s linear walk, and this helper is omitted entirely.

### A.4 List, stack, and deque types

```54:62:C:\Users\lamam\Downloads\MLinked-MList\linkedlist.h
// ----------------------- List -----------------------
typedef struct {
    Node *head;
    ListCache cache;
} List;

typedef struct { List list; } Stack;
typedef struct { List list; Node *tail; } Deque;
```

- `List` owns both:
  - The user-visible `head` pointer.
  - The internal `ListCache`, kept adjacent in memory for locality.
- `Stack` and `Deque` are thin wrappers around `List`:
  - `Stack` needs nothing beyond a `List`.
  - `Deque` adds a `tail` pointer to support efficient back operations.

### A.5 Public interface and hot-path inlines

```63:80:C:\Users\lamam\Downloads\MLinked-MList\linkedlist.h
// ----------------------- Function Prototypes -----------------------
// Non-hot-path functions are in linkedlist.c.
// Hot-path node/stack/deque ops are static inline below for zero call overhead.
void  listInsert(List *list, int position, Node *nd);
void  listDelete(List *list, int position);
void  listPrint(List *list);
void  traverseList_pub(List *list, int position, Node **outBehind, Node **outCurrent);

// Internal Cache management — optimized versions of your original API
void invalidateCache_pub(ListCache *cache, int mutatedPosition);
void warmCache_pub(ListCache *cache, int position, Node *behind, Node *current);

// O(1) push/pop cache maintenance via offset arithmetic.
// Call cacheOnPush_pub after manually prepending a node to a bare List head.
// Call cacheOnPop_pub  after manually removing the head node of a bare List.
// (The Stack/Deque ops below already handle this internally.)
void cacheOnPush_pub(ListCache *cache);
void cacheOnPop_pub(ListCache *cache);
```

- The header clearly separates:
  - **Non-hot-path functions** (`listInsert`, `listDelete`, `listPrint`, `traverseList_pub`) implemented in `linkedlist.c`.
  - **Hot-path operations** implemented as `static inline` for speed (see below).
- The cache management API is kept public so:
  - Other modules can explicitly invalidate/warm the cache when needed.
  - The core list functions can coordinate cache semantics in a centralised and testable way.

### A.6 Inline node creation

```82:91:C:\Users\lamam\Downloads\MLinked-MList\linkedlist.h
// ── Inline node creation ──────────────────────────────────────────────────────
// Kept inline so the compiler sees malloc + push as one unit and eliminates
// the call overhead for the DFS/BFS inner loops.
static inline Node *createNode(int val) {
    Node *nd = (Node *)malloc(sizeof(Node));
    if (!nd) return NULL;
    nd->value = val;
    nd->next  = NULL;
    return nd;
}
```

- `createNode` is inline by design:
  - Many algorithms call `createNode` immediately followed by a push/enqueue.
  - Inlining lets the compiler optimise across the boundary (e.g., inlining or reordering allocation and linking).
  - It also avoids function call overhead on very hot paths.

### A.7 Inline stack operations

```93:113:C:\Users\lamam\Downloads\MLinked-MList\linkedlist.h
// ── Stack ops (static inline — zero call overhead on the hot path) ────────────
static inline void push(Stack *s, Node *nd) {
    // Head prepend: all cached entries shift one position deeper via offset (O(1)).
    s->list.cache.offset--;
    nd->next     = s->list.head;
    s->list.head = nd;
}

static inline Node *pop(Stack *s) {
    if (!s->list.head) return NULL;
    // Head removal: generation bump invalidates all cached entries in O(1).
    s->list.cache.generation++;
    Node *nd     = s->list.head;
    s->list.head = nd->next;
    nd->next     = NULL;
    return nd;
}

static inline Node *peek(Stack *s)      { return s->list.head; }
static inline int stackIsEmpty(Stack *s) { return s->list.head == NULL; }
```

- `push`:
  - Integrates cache semantics directly by decrementing `offset`.
  - This keeps virtual positions stable even though the physical indices have shifted.
- `pop`:
  - Invalidates all cached entries by incrementing `generation`, which is safe and O(1).
- `peek` and `stackIsEmpty` are trivial inline helpers with no cache side-effects.
- All stack operations are inline to ensure zero function-call overhead in inner loops (e.g., DFS).

### A.8 Inline deque operations

```115:142:C:\Users\lamam\Downloads\MLinked-MList\linkedlist.h
// ── Deque ops (static inline) ─────────────────────────────────────────────────
static inline void enqueueBack(Deque *dq, Node *nd) {
    // Tail append: no existing position changes, so the cache remains valid.
    // Do NOT call invalidateCache — that would destroy entries useful to Trémaux.
    nd->next = NULL;
    if (!dq->list.head) {
        dq->list.head = dq->tail = nd;
    } else {
        dq->tail->next = nd;
        dq->tail       = nd;
    }
}

static inline Node *dequeueFront(Deque *dq) {
    if (!dq->list.head) return NULL;
    // Head removal shifts all positions: bump generation to invalidate.
    dq->list.cache.generation++;
    Node *nd     = dq->list.head;
    dq->list.head = nd->next;
    if (!dq->list.head) dq->tail = NULL;
    nd->next = NULL;
    return nd;
}

static inline int dequeIsEmpty(Deque *dq) { return dq->list.head == NULL; }

// enqueueFront and dequeueBack are infrequently used; they remain in linkedlist.c.
void  enqueueFront(Deque *dq, Node *nd);
Node *dequeueBack(Deque *dq);
```

- `enqueueBack`:
  - Carefully **does not** invalidate the cache:
    - Appending at the tail does not change any existing positions.
    - Deque-based algorithms like Trémaux benefit from keeping cached floors valid across tail appends.
- `dequeueFront`:
  - Head removal changes all positions, so the cache is invalidated by bumping `generation`.
- Less common operations (`enqueueFront` and `dequeueBack`) are declared here but implemented in `linkedlist.c`:
  - This keeps the header focused on the hot path while still exposing the full API.

### A.9 Inline fast-path wrapper

```144:160:C:\Users\lamam\Downloads\MLinked-MList\linkedlist.h
// ── Inline fast-path wrapper around traverseList_pub ─────────────────────────
// Checks the direct cache slot first (O(1)); falls back to traverseList_pub
// (which handles the position-1 fast-path and the general O(CACHE_SIZE) floor
// search) only on a miss.  Use this in hot positional-scan loops.
static inline void traverseList_inline(List *list, int position,
                                       Node **outBehind, Node **outCurrent)
{
#ifndef NODECACHE_DISABLE_POSITIONAL_CACHE
    NodeCacheEntry *hit = checkCache(&list->cache, position);
    if (hit) {
        *outBehind  = hit->behind;
        *outCurrent = hit->current;
        return;
    }
#endif
    traverseList_pub(list, position, outBehind, outCurrent);
}
```

- This wrapper gives algorithms a **cheap first shot** at an O(1) lookup:
  - It checks the direct cache slot for `position` before invoking the full machinery in `traverseList`.
  - On a miss (or when the cache is disabled), it delegates to `traverseList_pub`, which may use the sequential fast path or floor search.
- This split lets high-level code choose the cost model:
  - Use `traverseList_inline` in tight loops where a direct hit is common.
  - Use `traverseList_pub` when a single traversal is performed and cold-start costs are acceptable.

---

## B. Implementation: `linkedlist.c`

### B.1 File header and configuration

```1:2:C:\Users\lamam\Downloads\MLinked-MList\linkedlist.c
#include "linkedlist.h"

// ── Cache internals (disabled under NODECACHE_DISABLE_POSITIONAL_CACHE) ───────
#ifndef NODECACHE_DISABLE_POSITIONAL_CACHE
```

- The implementation depends on the public/interface definitions from `linkedlist.h` (including `Node`, `List`, `ListCache`, and `NodeCacheEntry`).
- All cache-aware functions are compiled only when `NODECACHE_DISABLE_POSITIONAL_CACHE` is **not** defined; otherwise, a simpler, linear implementation is provided (see §3).
- This split allows controlled experiments:
  - With the cache enabled (default).
  - With the cache logic compiled out, so the list behaves as a plain singly linked list while keeping the same API.

---

### 2. Cache internals (enabled mode)

#### 2.1 Generation-based invalidation

```6:7:C:\Users\lamam\Downloads\MLinked-MList\linkedlist.c
static inline void invalidateCache(ListCache *cache) { cache->generation++; }
```

- `generation` is a monotonically increasing counter stored in `ListCache`.
- Each cache entry stores the generation in which it was written.
- Incrementing `generation` immediately renders all old entries obsolete without touching them, which is an **O(1)** bulk invalidation mechanism.

#### 2.2 Shared `checkCache` and `updateCache`

```8:18:C:\Users\lamam\Downloads\MLinked-MList\linkedlist.c
// checkCache is defined as static inline in linkedlist.h (shared with callers).

static inline void updateCache(ListCache *cache, int position, Node *behind, Node *current) {
    if (position < CACHE_THRESHOLD) return;
    unsigned int vpos = (unsigned int)(position + cache->offset);
    unsigned int slot = vpos & (CACHE_SIZE - 1);
    cache->entries[slot].position   = vpos;
    cache->entries[slot].behind     = behind;
    cache->entries[slot].current    = current;
    cache->entries[slot].generation = cache->generation;
}
```

- `checkCache` (in the header) and `updateCache` (here) are the two central primitives for interacting with the positional cache.
- **Virtual positions**:
  - The physical index of a node in the list is translated to a **virtual position**:  
    `vpos = position + cache->offset`.
  - `offset` is adjusted on head insertions/removals so that nodes don’t have to be re-tagged when their physical index changes.
- **Slot selection**:
  - The slot index is computed as `vpos & (CACHE_SIZE - 1)`, relying on `CACHE_SIZE` being a power of two.
  - This avoids expensive modulo operations and compiles down to a single bitwise-AND instruction.
- **CACHE_THRESHOLD**:
  - Very small positions (typically near the head) are not cached:
    - They are cheap to reach by a few `next` pointers.
    - Skipping them avoids polluting the cache with trivial entries.

#### 2.3 Public cache maintenance APIs

```20:27:C:\Users\lamam\Downloads\MLinked-MList\linkedlist.c
void invalidateCache_pub(ListCache *cache, int mutatedPosition) {
    (void)mutatedPosition;
    invalidateCache(cache);
}

void warmCache_pub(ListCache *cache, int position, Node *behind, Node *current) {
    updateCache(cache, position, behind, current);
}
```

- These are the **public** entry points used by other modules (or by the list ops) to:
  - Invalidate the entire cache when a structural change makes old entries unsafe.
  - Seed the cache with a known `(behind, current)` pair after a traversal.
- `mutatedPosition` is currently unused but documented in the signature to express that invalidation is logically tied to a mutation at some position; it also future‑proofs the API.

```29:39:C:\Users\lamam\Downloads\MLinked-MList\linkedlist.c
void cacheOnPush_pub(ListCache *cache) { cache->offset--; }

void cacheOnPop_pub(ListCache *cache) {
    unsigned int stale_vpos = (unsigned int)(1 + cache->offset);
    unsigned int stale_slot = stale_vpos & (CACHE_SIZE - 1);
    if (cache->entries[stale_slot].position == stale_vpos && cache->entries[stale_slot].generation == cache->generation) 
    {
        cache->entries[stale_slot].generation = cache->generation - 1u;
    }
    cache->offset++;
}
```

- These functions encode the most novel aspect of the design: **constant-time head updates using offset arithmetic**.
- `cacheOnPush_pub`:
  - A head push means “every element’s physical index increases by 1”.
  - Instead of rewriting every cached `position`, we simply decrement `offset`, so that `virtual_pos = physical_pos + offset` remains stable.
- `cacheOnPop_pub`:
  - A head pop shifts everything one step towards index 0, so we increment `offset`.
  - Additionally, we compute `stale_vpos` corresponding to the old head and locate its cache slot.
  - If that slot still holds a matching position in the current generation, we mark it explicitly stale by decrementing its `generation` to `generation - 1`.
  - This selective fix avoids “dangling” entries that would otherwise refer to the removed head node.

#### 2.4 Core traversal with fast path

```41:50:C:\Users\lamam\Downloads\MLinked-MList\linkedlist.c
static void traverseList(List *list, int position, Node **outBehind, Node **outCurrent) {
    if (position < 0) { *outBehind = NULL; *outCurrent = NULL; return; }

    NodeCacheEntry *hit = checkCache(&list->cache, position);
    if (hit) {
        *outBehind = hit->behind;
        *outCurrent = hit->current;
        return;
    }
```

- `traverseList` is the **internal** traversal used both by public wrappers and by performance‑critical algorithms.
- For invalid positions (`position < 0`), it reports a miss via `NULL` outputs.
- On a direct cache hit:
  - The `(behind, current)` pair is returned immediately in **O(1)** time.
  - This supports both random-access and repeated positional queries efficiently when the cache is warm.

```51:53:C:\Users\lamam\Downloads\MLinked-MList\linkedlist.c
    int   best_floor = 0;
    Node *floor_node = list->head;
```

- `best_floor` and `floor_node` keep track of the **best cached starting point** strictly before the requested position.
- This is the basis of the “floor search” that avoids always starting from the head.

```54:71:C:\Users\lamam\Downloads\MLinked-MList\linkedlist.c
    // Fast path: check if position-1 is cached — O(1) for sequential-scan
    // patterns (e.g., Trémaux junction search).  Avoids the O(CACHE_SIZE)
    // floor scan in the common case.
    // Disable with -DNODECACHE_DISABLE_FASTPATH to isolate this optimisation's
    // contribution when comparing benchmark variants.
#ifndef NODECACHE_DISABLE_FASTPATH
    if (position > CACHE_THRESHOLD) {
        NodeCacheEntry *prev = checkCache(&list->cache, position - 1);
        if (prev && prev->current) {
            Node *behind  = prev->current;
            Node *current = prev->current->next;
            *outBehind  = behind;
            *outCurrent = current;
            updateCache(&list->cache, position, behind, current);
            return;
        }
    }
#endif  // NODECACHE_DISABLE_FASTPATH
```

- This block implements a **sequential-scan fast path**:
  - Access patterns like Trémaux’s algorithm often ask for `position = 1, 2, 3, ...` in order.
  - If `position - 1` is cached, we can advance one `next` pointer to obtain the new node, instead of scanning the entire cache table or walking from the head.
  - The newly found pair is then cached via `updateCache`, so future access to that position is a direct hit.
- The entire fast path can be compiled out with `NODECACHE_DISABLE_FASTPATH` to measure its incremental benefit separately from the base cache.

```73:83:C:\Users\lamam\Downloads\MLinked-MList\linkedlist.c
    // General path: scan all cache entries for the closest cached floor.
    for (int i = 0; i < CACHE_SIZE; i++) {
        NodeCacheEntry *e = &list->cache.entries[i];
        if (e->generation != list->cache.generation) continue;
        if (!e->current) continue;
        int phys = (int)((unsigned int)e->position - (unsigned int)list->cache.offset);
        if (phys > 0 && phys > best_floor && phys < position) {
            best_floor = phys;
            floor_node = e->current;
        }
    }
```

- When the direct hit and fast path both miss, we fall back to a **floor search**:
  - The loop scans all cache slots looking for entries that:
    - Belong to the current `generation`.
    - Have a valid `current` pointer.
    - Correspond to a physical position `phys` that is both `< position` and as large as possible (closest from below).
  - `phys` is reconstructed by inverting the virtual-position transformation:  
    `phys = (unsigned)e->position - (unsigned)cache.offset`.
- This gives an amortised‑efficient starting point even for random access patterns, while keeping the cache structure extremely simple.

```85:95:C:\Users\lamam\Downloads\MLinked-MList\linkedlist.c
    Node *current = floor_node;
    Node *behind  = NULL;
    for (int i = best_floor; i < position && current; i++) {
        behind  = current;
        current = current->next;
    }
    *outBehind  = behind;
    *outCurrent = current;

    updateCache(&list->cache, position, behind, current);
}
```

- Starting from `floor_node`, we walk forward until we either:
  - Reach the requested `position`, or
  - Hit the end of the list (`current == NULL`).
- Throughout, `behind` trails `current` by one node, so callers can insert or delete at `position` using this `(behind, current)` pair.
- The final result is fed back into the cache via `updateCache`, gradually populating it with useful floors over time.

---

### 3. Cache-disabled variant

```97:116:C:\Users\lamam\Downloads\MLinked-MList\linkedlist.c
#else  // NODECACHE_DISABLE_POSITIONAL_CACHE

// Cache APIs become no-ops
void invalidateCache_pub(ListCache *cache, int mutatedPosition) { (void)cache; (void)mutatedPosition; }
void warmCache_pub(ListCache *cache, int position, Node *behind, Node *current) { (void)cache; (void)position; (void)behind; (void)current; }
void cacheOnPush_pub(ListCache *cache) { (void)cache; }
void cacheOnPop_pub(ListCache *cache) { (void)cache; }

// Traversal becomes plain linear walk from head
static void traverseList(List *list, int position, Node **outBehind, Node **outCurrent) {
    if (position < 0) { *outBehind = NULL; *outCurrent = NULL; return; }
    Node *behind = NULL;
    Node *current = list->head;
    for (int i = 0; i < position && current; i++) {
        behind = current;
        current = current->next;
    }
    *outBehind = behind;
    *outCurrent = current;
}

#endif
```

- When `NODECACHE_DISABLE_POSITIONAL_CACHE` is defined:
  - All cache operations are compiled as **no-ops**, preserving the API shape but removing overhead.
  - `traverseList` is a straightforward linear walk from the head to the requested position.
- This mode is particularly useful in benchmarks:
  - It gives a baseline “Node (cache disabled)” variant for comparison.
  - It makes it easy to attribute speedups or slowdowns specifically to the cache design.

---

### 4. Public traversal wrapper

```120:123:C:\Users\lamam\Downloads\MLinked-MList\linkedlist.c
// ── Public traversal wrapper ───────────────────────────────────────────────────
void traverseList_pub(List *list, int position, Node **outBehind, Node **outCurrent) {
    traverseList(list, position, outBehind, outCurrent);
}
```

- `traverseList_pub` exposes the internal traversal to other modules while keeping the actual cache mechanics private.
- Algorithms that care about positional access (e.g., Trémaux) can call this function without knowing whether the cache is enabled or how it is implemented.

---

### 5. List operations

#### 5.1 Insertion at arbitrary position

```125:149:C:\Users\lamam\Downloads\MLinked-MList\linkedlist.c
// ── List ops ──────────────────────────────────────────────────────────────────
void listInsert(List *list, int position, Node *nd) {
    if (!nd) return;
    if (position == 0) {
#ifndef NODECACHE_DISABLE_POSITIONAL_CACHE
        cacheOnPush_pub(&list->cache);
#endif
        nd->next = list->head;
        list->head = nd;
        return;
    }
#ifndef NODECACHE_DISABLE_POSITIONAL_CACHE
    // For non-head mutations, cached snapshot is unsafe; invalidate.
    // (In cache-disabled mode this doesn't exist / isn't needed.)
    // Keep consistent behavior anyway:
    // Note: invalidateCache_pub is no-op when cache disabled.
    invalidateCache_pub(&list->cache, position);
#endif
    Node *behind, *current;
    traverseList(list, position, &behind, &current);
    if (behind) {
        nd->next = current;
        behind->next = nd;
    }
}
```

- `listInsert` expresses two different mutation patterns:
  1. **Head insertion (`position == 0`)**:
     - Uses `cacheOnPush_pub` to adjust the `offset` and optionally patch out any stale head entry.
     - Prepends `nd` in the usual singly‑linked‑list way.
  2. **Insertion at non-head positions**:
     - Calls `invalidateCache_pub` before modification:
       - Arbitrary middle insertions can potentially invalidate many positional assumptions.
       - Using a generation bump is cheaper than carefully updating each affected entry.
     - Relies on `traverseList` to retrieve `(behind, current)` at `position`.
     - Inserts `nd` between `behind` and `current` if `behind` is non‑NULL.
- This logic keeps the core mutation semantics simple while ensuring cache correctness via coarse‑grained invalidation.

#### 5.2 Deletion at arbitrary position

```151:167:C:\Users\lamam\Downloads\MLinked-MList\linkedlist.c
void listDelete(List *list, int position) {
#ifndef NODECACHE_DISABLE_POSITIONAL_CACHE
    invalidateCache_pub(&list->cache, position);
#endif
    if (position == 0 && list->head) {
        Node *temp = list->head;
        list->head = list->head->next;
        free(temp);
        return;
    }
    Node *behind, *current;
    traverseList(list, position, &behind, &current);
    if (behind && current) {
        behind->next = current->next;
        free(current);
    }
}
```

- Deletion similarly distinguishes between:
  - **Head deletion**: update `head`, free the old head, and let the cache be reset via `invalidateCache_pub` pre-call.
  - **Middle deletion**:
    - Use `traverseList` to find the node and its predecessor.
    - Reroute `behind->next` to skip `current`, then free `current`.
- The pre‑deletion `invalidateCache_pub` ensures that no cached entries rely on indices that become invalid after the structural change.

#### 5.3 Debug printing

```169:173:C:\Users\lamam\Downloads\MLinked-MList\linkedlist.c
void listPrint(List *list) {
    Node *c = list->head;
    while(c) { printf("%d -> ", c->value); c = c->next; }
    printf("NULL\n");
}
```

- A straightforward traversal used for debugging or visualisation:
  - Walks from head to end, printing `value` followed by `->`.
  - Terminates with `NULL` to emphasise that this is a singly linked list.

---

### 6. Deque operations

```175:179:C:\Users\lamam\Downloads\MLinked-MList\linkedlist.c
// ── Deque ops (infrequently used) ─────────────────────────────────────────────
void enqueueFront(Deque *dq, Node *nd) {
    listInsert(&dq->list, 0, nd);
    if (!dq->tail) dq->tail = nd;
}
```

- `enqueueFront` reuses `listInsert` at position 0:
  - This means it automatically benefits from the same cache‑aware head insertion logic.
  - If the deque was empty, it initialises `tail` to the new node.

```181:200:C:\Users\lamam\Downloads\MLinked-MList\linkedlist.c
Node *dequeueBack(Deque *dq) {
    if (!dq->list.head) return NULL;
#ifndef NODECACHE_DISABLE_POSITIONAL_CACHE
    // this was invalidateCache(&dq->list.cache); in original; keep through pub call if desired:
    invalidateCache_pub(&dq->list.cache, -1);
#endif

    Node *curr = dq->list.head;
    if (!curr->next) {
        dq->list.head = dq->tail = NULL;
        return curr;
    }
    while (curr->next && curr->next != dq->tail) {
        curr = curr->next;
    }
    Node *ret = dq->tail;
    dq->tail = curr;
    dq->tail->next = NULL;
    return ret;
}
```

- `dequeueBack` removes from the **back** of the deque:
  - If the list is empty, it returns `NULL`.
  - With the cache enabled, it calls `invalidateCache_pub`:
    - Removing the tail can drastically change the effective length and may invalidate floors near the end.
    - Using generation invalidation is simpler and predictable.
  - If there is only one node, both `head` and `tail` become `NULL` and that node is returned.
  - Otherwise, it walks the list to find the node **just before** the current `tail`, updates `tail`, severs the link, and returns the old tail.
- This operation is marked as “infrequently used” because:
  - It requires a linear walk from the head to reach the predecessor of `tail`.
  - In the benchmark workloads, back‑dequeue is rare enough that optimising it further is unnecessary.

---

### 7. Summary of novel aspects

- **Virtual positions with offset**:
  - Head updates do not require rewriting cache entries; a single integer offset is adjusted instead.
- **Generation-based O(1) invalidation**:
  - Arbitrary middle mutations do not track affected slots individually; a single `generation++` invalidates the entire snapshot.
- **Hybrid traversal strategy**:
  - Direct hits, sequential fast path, and floor search work together to give good performance across both sequential and semi‑random positional access patterns.
- **Configurable feature flags**:
  - `NODECACHE_DISABLE_POSITIONAL_CACHE` and `NODECACHE_DISABLE_FASTPATH` expose well‑defined experimental variants without changing the public API.
- **Tight coupling with algorithms**:
  - `traverseList` and the cache maintenance helpers are designed explicitly to match the access patterns of algorithms like Trémaux, enabling stronger real‑world performance than a purely textbook list.

This commentary is intended to be read alongside `linkedlist.c` and `linkedlist.h` and can serve as a design appendix when turning this implementation into a formal write‑up or paper.
