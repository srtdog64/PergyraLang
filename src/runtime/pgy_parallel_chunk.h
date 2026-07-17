#ifndef PERGYRA_RUNTIME_PGY_PARALLEL_CHUNK_H
#define PERGYRA_RUNTIME_PGY_PARALLEL_CHUNK_H

/* Policy SoT is src/self_hosted/parallel/chunk_policy_owner.pgy. */
#define PGY_PARALLEL_CHUNK_FACTOR 4

static inline size_t
pgy_parallel_chunk_count(size_t n)
{
    size_t workers;
    size_t chunks;

    if (n <= 1)
        return n;
    workers = atomic_load_explicit(&g_pgy_pool_active, memory_order_acquire)
        ? g_pgy_pool.worker_count
        : pgy_default_worker_count();
    if (workers == 0)
        workers = 1;
    chunks = workers * PGY_PARALLEL_CHUNK_FACTOR;
    return n < chunks ? n : chunks;
}

typedef struct {
    void *(*body)(void *);
    unsigned char *ctxs;
    size_t         elem_size;
    size_t         lo;
    size_t         hi;
} PgyParallelChunkCtx;

static void *
pgy_parallel_chunk_driver(void *raw)
{
    PgyParallelChunkCtx *chunk = (PgyParallelChunkCtx *)raw;

    for (size_t i = chunk->lo; i < chunk->hi; i++) {
        if (pgy_cancel_is_requested(pgy_current_cancel_node()))
            break;
        chunk->body(chunk->ctxs + i * chunk->elem_size);
    }
    return NULL;
}

static inline void *
pgy_parallel_chunk_ctxs_alloc(size_t chunk_count)
{
    void *contexts;

    if (!pgy_parallel_array_fits(chunk_count ? chunk_count : 1,
                                 sizeof(PgyParallelChunkCtx))) {
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_OOM,
                          PGY_RUNTIME_PANIC_REASON_ALLOCATION_FAILED);
    }
    contexts = malloc(sizeof(PgyParallelChunkCtx)
                      * (chunk_count ? chunk_count : 1));
    if (contexts == NULL) {
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_OOM,
                          PGY_RUNTIME_PANIC_REASON_ALLOCATION_FAILED);
    }
    return contexts;
}

static inline PgyTaskHandle
pgy_parallel_spawn_chunk_at(void *contexts, size_t index, size_t chunk_count,
                            void *(*body)(void *), void *items,
                            size_t elem_size, size_t item_count)
{
    PgyParallelChunkCtx *slots = (PgyParallelChunkCtx *)contexts;
    size_t base;
    size_t remainder;
    size_t lo;

    if (contexts == NULL || body == NULL || chunk_count == 0
        || index >= chunk_count) {
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_INTERNAL_INVARIANT,
                          "parallel chunk spawn out of range");
    }
    base = item_count / chunk_count;
    remainder = item_count % chunk_count;
    lo = base * index + (index < remainder ? index : remainder);
    slots[index].body = body;
    slots[index].ctxs = (unsigned char *)items;
    slots[index].elem_size = elem_size;
    slots[index].lo = lo;
    slots[index].hi = lo + base + (index < remainder ? 1 : 0);
    return pgy_spawn(pgy_parallel_chunk_driver, &slots[index]);
}

#endif
