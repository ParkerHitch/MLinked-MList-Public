## Node+cache Linked List and Maze Benchmark Suite

This project contains a custom singly linked list with a **position-aware cache** (“Node+cache”) and a small benchmark harness that compares it against more conventional list representations inside several maze-search algorithms.

The code is written in C and builds a single executable, `maze_bench`, which generates random mazes and times multiple search algorithms under different list implementations.

---

### 1. Data structure overview (`linkedlist.h`, `linkedlist.c`)

- **Core types**
  - `Node`: a standard singly-linked list node (`value`, `next`).
  - `List`: owns a `head` pointer plus a `ListCache` instance.
  - `Stack` and `Deque`: thin wrappers over `List`, used by the algorithms.
- **ListCache design**
  - `ListCache` stores an array of `NodeCacheEntry { position, behind, current, generation }` plus a global `generation` counter and an `offset`.
  - `CACHE_SIZE` (power of two) controls the number of cache slots and is used with bit-masked indexing (`vpos & (CACHE_SIZE - 1)`) instead of integer modulo for performance.
  - Each entry’s `position` is a **virtual position**: `virtual_pos = physical_pos + offset`. Only the `offset` changes on head insertions / removals; node contents are never moved for cache maintenance.
  - `generation` is bumped on structural changes that invalidate positional assumptions, so a single increment makes all previous entries logically stale in O(1).

#### 1.1 Traversal and cache invariants

- `traverseList` (in `linkedlist.c`) is the central traversal routine. It guarantees:
  - For `position < 0`, outputs are `NULL`.
  - Otherwise, it either:
    - returns a cached `(behind, current)` pair for the requested logical position, or
    - walks the list from a cached “best floor” node (closest cached position `< position`) or from `head` if no usable cache entry exists.
- **Fast-path optimisation**
  - For **sequential scans** (e.g., Trémaux’s junction search), the most common pattern is to request positions `1, 2, 3, ...` in order.
  - When `NODECACHE_DISABLE_FASTPATH` is **not** defined, `traverseList` first checks the cache slot for `position - 1`. If that entry is valid, it advances one step and returns the result immediately, updating the cache for `position`.
  - When `NODECACHE_DISABLE_FASTPATH` is defined, this short-circuit is disabled so that benchmarks can isolate the benefit of the fast-path separately from the base cache.
- **Cache maintenance**
  - Head insert (`push` / `enqueueFront`):
    - The physical positions of all nodes increase by 1.
    - Instead of recomputing cache entries, code decrements `offset` (logical shift) and updates the head pointer.
  - Head removal (`pop` / `dequeueFront`):
    - Physical positions decrease by 1.
    - `generation` is incremented so that all cached entries from the old generation are ignored on future lookups.
  - General mutation at non-head positions (`listInsert`, `listDelete`):
    - These operations call `invalidateCache_pub`, which bumps `generation` and ensures no stale positional entries survive an arbitrary structural change.

This design lets the list behave like a normal singly linked list from the caller’s perspective while providing **amortised O(1)** access to recently used positions under typical scan patterns, at the cost of a small, fixed auxiliary structure per list.

---

### 2. Algorithm variants and fairness of comparisons

The benchmark compares several algorithms implemented over different list representations:

- **Node+cache (“cache” variant)**
  - Uses `Node` + `ListCache` and the cached positional traversal API:
    - `traverseList_inline` / `traverseList_pub`
    - `Stack` / `Deque` built on top of `List`
  - Algorithms are defined in `algorithms_cache.h`:
    - `cache_dfs`
    - `cache_bfs`
    - `cache_iddfs`
    - `cache_bibfs`
    - `cache_gbfs` (Greedy Best-First Search)
    - `cache_astar`
- **PSLL (“Node+nocache”)**
  - Same node size as `Node+cache` but **no cache**.
  - Exposes `psll_get_behind_current` as a baseline positional API.
  - Used in `psll_tremaux` as a “same node, no cache” comparison.
- **SLL / DLL**
  - `SLLNode` / `DLLNode` are conventional singly- and doubly-linked-list nodes.
  - Used in:
    - `sll_dfs`, `dll_dfs`, etc. (algorithm variants in `algorithms_sll.h`, `algorithms_dll.h`).
    - `sll_tremaux`, `dll_tremaux` (in `tremaux.h`).

#### 2.1 Why these comparisons?

- **Isolating the cache effect**:
  - `Node+cache` vs **Node+nocache (PSLL)** keeps the node layout constant and changes only traversal strategy.
  - Any speedup here is attributable primarily to positional caching and the fast-path optimisation, not node size or pointer count.
- **Separating data-structure overhead from algorithmic work**:
  - `Node+nocache` vs **SLL**: same algorithm, different representation.
  - `SLL` vs **DLL**: measures the overhead of extra pointers and node size.
- **Algorithm-level comparisons**:
  - For DFS/BFS/GBFS/A*, each algorithm is implemented on top of each list representation with as few differences as possible (stack vs queue semantics, but structurally equivalent operations).
  - This helps ensure that measured differences reflect **data structure costs**, not different search logic.

---

### 3. Benchmark methodology (`main.c`)

The benchmark harness in `main.c` is written to emphasise **fairness and reproducibility**.

- **Maze generation**
  - `maze_create` / `maze_generate` (from `maze.h`) construct rectangular mazes of various sizes.
  - A single `MAZE_SEED` (either provided at compile time or derived from `time(NULL)`) seeds the generator.
  - All algorithm variants run over the **same maze instances** for a given run, ensuring they solve identical problems.

- **Allocator warming**
  - `warm_allocator()` performs a batch of allocations and frees before benchmarking.
  - Purpose: reduce noise from first-use page faults and OS-level allocator setup that could otherwise bias the first variant run.

- **Cold-cache protocol**
  - Before each individual timing, `flush_cpu_cache()` touches a large, contiguous buffer in steps of typical cache-line size (64 bytes).
  - This approximates a cold CPU cache for each run, so no algorithm benefits disproportionately from data left in cache by a previous variant.

- **Run interleaving and rotation**
  - `RUNS` is chosen so that:
    - `bench3` (3 algorithms) and `bench4_tremaux` (4 algorithms) each complete full rotations of call order.
  - In `bench3`:
    - For iteration `i`, the call order to `(cache, sll, dll)` rotates as `cache → sll → dll`, then `sll → dll → cache`, then `dll → cache → sll`, and so on.
  - In `bench4_tremaux`:
    - The four algorithms rotate deterministically, using a helper macro `TIME_CALL` to ensure identical timing structure per call.
  - **Rationale**: rotation ensures that no algorithm is systematically favoured by being run first or last (e.g., benefitting from a better warmed instruction cache).

- **Averaging and reporting**
  - For each algorithm/variant pair, all measured times are accumulated and then divided by `RUNS` to obtain an average in milliseconds.
  - `printHeader3`, `printRow3`, `printHeaderTremaux4`, and `printRowTremaux4` format these averages into tables, including:
    - A ranking column (1st / 2nd / 3rd) with a `★` flag for the variant that wins within its row.
    - Clear column labels such as `"Node+cache"`, `"Node+nocache"`, `"SLLNode"`, `"DLLNode"`.

---

### 4. Build variants and controlled experiments (`Makefile`)

The `Makefile` provides several build modes to help isolate specific design choices:

- **Default build**
  - `make` or `make all` builds `maze_bench` with the full Node+cache implementation enabled.
  - The banner reflects the active configuration, e.g.:
    - `MAZE BENCHMARK: Node+cache vs. Node+nocache / SLLNode / DLLNode`

- **Cache-disabled build**
  - `make nocache`
  - Compiles with `-DNODECACHE_DISABLE_POSITIONAL_CACHE`, replacing cached positional traversal with a simple linear walk.
  - Used to quantify the **baseline cost** of the cache machinery when it is present in the struct but logically turned off.

- **Fast-path-disabled build**
  - `make nofastpath`
  - Compiles with `-DNODECACHE_DISABLE_FASTPATH`, keeping the cache but disabling the “position-1” fast-path optimisation.
  - Used to separate the effect of the **cache table** from the incremental benefit of the sequential-scan fast path.

- **Variants and sweeps**
  - `make variants`:
    - Runs three builds back-to-back:
      1. Node+cache (full: cache + fast-path)
      2. Node+cache (cache only, fast-path disabled)
      3. Node (cache disabled)
    - Each variant runs identical benchmarks so that differences can be directly attributed to the enabled/disabled features.
  - `make sweep`:
    - Sweeps over several `CACHE_SIZE` values and builds a dedicated Trémaux-only binary for each.
    - This allows analysis of how cache table size affects performance.

- **Reproducibility**
  - `SEED` is a make-time parameter:
    - `make SEED=123` will define `MAZE_SEED=123` and print it at runtime.
    - Omitting `SEED` or leaving it at `0` falls back to `time(NULL)`, which is also printed.
  - All printed banners include:
    - `CACHE_SIZE`
    - `RUNS`
    - Build notes about which compile-time flags were used.

---

### 5. Interpreting results (for a write-up)

When turning this into a paper or write-up, it is helpful to structure the discussion around three axes:

1. **Data structure complexity**  
   - Describe the asymptotic performance of:
     - Insertion/deletion at head vs arbitrary positions.
     - Positional access with and without the cache.
   - Emphasise that the positional cache is designed to exploit **locality in access patterns** (especially sequential scans).

2. **Micro-architectural considerations**  
   - Explain the use of:
     - Bit-masked indexing instead of modulo.
     - Generation counters for O(1) invalidation.
     - Cold-cache runs and CPU cache flushing.
   - Discuss how these design choices aim to minimise confounding hardware effects when comparing variants.

3. **Experimental methodology**  
   - Justify the maze sizes, number of runs, and rotation scheme.
   - Explicitly state that:
     - Each row in the output table represents the **same maze instance**.
     - Each timing is an average over multiple cold-cache runs.
     - Build flags are used to keep all other conditions equal while varying a single feature at a time.

Together, these components provide a solid foundation for a formal write-up that explains both **why** the Node+cache design is interesting and **how** the experimental results were obtained in a controlled and reproducible way.

