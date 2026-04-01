#include "linkedlist.h"

// ── Cache internals (disabled under NODECACHE_DISABLE_POSITIONAL_CACHE) ───────
#ifndef NODECACHE_DISABLE_POSITIONAL_CACHE

static inline void invalidateCache(ListCache *cache) { cache->generation++; }

// checkCache is defined as static inline in linkedlist.h (shared with callers).

static inline void updateCache(ListCache *cache, int position, Node *behind, Node *current) 
{
    if (position < CACHE_THRESHOLD) return;
    unsigned int vpos = (unsigned int)(position + cache->offset);
    unsigned int slot = vpos & (CACHE_SIZE - 1);
    cache->entries[slot].position   = vpos;
    cache->entries[slot].behind     = behind;
    cache->entries[slot].current    = current;
    cache->entries[slot].generation = cache->generation;
}

void invalidateCache_pub(ListCache *cache, int mutatedPosition)
{
    (void)mutatedPosition;
    invalidateCache(cache);
}

void warmCache_pub(ListCache *cache, int position, Node *behind, Node *current)
{
    updateCache(cache, position, behind, current);
}

void cacheOnPush_pub(ListCache *cache) { cache->offset--; }

void cacheOnPop_pub(ListCache *cache) {
    unsigned int stale_vpos = (unsigned int)(1 + cache->offset);
    unsigned int stale_slot = stale_vpos & (CACHE_SIZE - 1);
    if (cache->entries[stale_slot].position == stale_vpos &&
        cache->entries[stale_slot].generation == cache->generation) {
        cache->entries[stale_slot].generation = cache->generation - 1u;
    }
    cache->offset++;
}

static void traverseList(List *list, int position, Node **outBehind, Node **outCurrent) {
    if (position < 0) { *outBehind = NULL; *outCurrent = NULL; return; }

    NodeCacheEntry *hit = checkCache(&list->cache, position);
    if (hit) 
    {
        *outBehind = hit->behind;
        *outCurrent = hit->current;
        return;
    }

    int   best_floor = 0;
    Node *floor_node = list->head;

    // Fast path: check if position-1 is cached — O(1) for sequential-scan
    // patterns (e.g., Trémaux junction search).  Avoids the O(CACHE_SIZE)
    // floor scan in the common case.
    // Disable with -DNODECACHE_DISABLE_FASTPATH to isolate this optimisation's
    // contribution when comparing benchmark variants.
#ifndef NODECACHE_DISABLE_FASTPATH
    if (position > CACHE_THRESHOLD) 
    {
        NodeCacheEntry *prev = checkCache(&list->cache, position - 1);
        if (prev && prev->current) 
        {
            Node *behind  = prev->current;
            Node *current = prev->current->next;
            *outBehind  = behind;
            *outCurrent = current;
            updateCache(&list->cache, position, behind, current);
            return;
        }
    }
#endif  // NODECACHE_DISABLE_FASTPATH

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

// ── Public traversal wrapper ───────────────────────────────────────────────────
void traverseList_pub(List *list, int position, Node **outBehind, Node **outCurrent) 
{
    traverseList(list, position, outBehind, outCurrent);
}

// ── List ops ──────────────────────────────────────────────────────────────────
void listInsert(List *list, int position, Node *nd) 
{
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

void listDelete(List *list, int position) 
{
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

void listPrint(List *list) 
{
    Node *c = list->head;
    while(c) { printf("%d -> ", c->value); c = c->next; }
    printf("NULL\n");
}

// ── Deque ops (infrequently used) ─────────────────────────────────────────────
void enqueueFront(Deque *dq, Node *nd) {
    listInsert(&dq->list, 0, nd);
    if (!dq->tail) dq->tail = nd;
}

Node *dequeueBack(Deque *dq) 
{
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
