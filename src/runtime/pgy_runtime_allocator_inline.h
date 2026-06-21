/* =================================================================
 * Allocator Interface
 * ================================================================= */

typedef enum {
    PGY_ALLOC_SYSTEM,
    PGY_ALLOC_TRACING,
    PGY_ALLOC_DEBUG,
    PGY_ALLOC_POOL,
    PGY_ALLOC_SCRATCH,
    PGY_ALLOC_RESULT,
    PGY_ALLOC_PERSISTENT
} PgyAllocatorKind;

typedef struct {
    char   *buffer;
    size_t  capacity;
    size_t  offset;
} PgyPoolAllocatorState;

typedef struct {
    PgyAllocatorKind kind;
    bool             trace_enabled;
    bool             debug_enabled;
    size_t           allocations;
    size_t           deallocations;
    size_t           bytes_in_use;
    size_t           peak_bytes;
    PgyPoolAllocatorState *pool;
} PgyAllocator;

static inline void
pgy_allocator_record_alloc(PgyAllocator *alloc, size_t size)
{
    if (alloc == NULL)
        return;
    if (alloc->allocations != SIZE_MAX)
        alloc->allocations++;
    if (alloc->bytes_in_use > SIZE_MAX - size)
        alloc->bytes_in_use = SIZE_MAX;
    else
        alloc->bytes_in_use += size;
    if (alloc->bytes_in_use > alloc->peak_bytes)
        alloc->peak_bytes = alloc->bytes_in_use;
    if (alloc->trace_enabled) {
        fprintf(stderr, "[PGY_ALLOC] +%zu bytes (live=%zu)\n",
                size, alloc->bytes_in_use);
    }
}

static inline void
pgy_allocator_record_free(PgyAllocator *alloc, size_t size)
{
    if (alloc == NULL)
        return;
    if (alloc->deallocations != SIZE_MAX)
        alloc->deallocations++;
    if (alloc->bytes_in_use >= size)
        alloc->bytes_in_use -= size;
    else
        alloc->bytes_in_use = 0;
    if (alloc->trace_enabled) {
        fprintf(stderr, "[PGY_ALLOC] -%zu bytes (live=%zu)\n",
                size, alloc->bytes_in_use);
    }
}

static inline PgyAllocator
pgy_allocator_system(void)
{
    PgyAllocator alloc;
    memset(&alloc, 0, sizeof(alloc));
    alloc.kind = PGY_ALLOC_SYSTEM;
    return alloc;
}

static inline PgyAllocator
pgy_allocator_tracing(void)
{
    PgyAllocator alloc = pgy_allocator_system();
    alloc.kind = PGY_ALLOC_TRACING;
    alloc.trace_enabled = true;
    return alloc;
}

static inline PgyAllocator
pgy_allocator_debug(void)
{
    PgyAllocator alloc = pgy_allocator_system();
    alloc.kind = PGY_ALLOC_DEBUG;
    alloc.debug_enabled = true;
    return alloc;
}

static inline PgyAllocator
pgy_allocator_pool(size_t capacity)
{
    PgyAllocator alloc = pgy_allocator_system();
    alloc.kind = PGY_ALLOC_POOL;
    alloc.pool = (PgyPoolAllocatorState*)calloc(1, sizeof(PgyPoolAllocatorState));
    if (alloc.pool == NULL)
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_OOM,
                          PGY_RUNTIME_PANIC_REASON_ALLOCATION_FAILED);
    alloc.pool->buffer = (char*)malloc(capacity);
    if (alloc.pool->buffer == NULL && capacity > 0)
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_OOM,
                          PGY_RUNTIME_PANIC_REASON_ALLOCATION_FAILED);
    alloc.pool->capacity = capacity;
    alloc.pool->offset = 0;
    return alloc;
}

static inline PgyAllocator
pgy_allocator_scratch(void)
{
    PgyAllocator alloc = pgy_allocator_system();
    alloc.kind = PGY_ALLOC_SCRATCH;
    return alloc;
}

static inline PgyAllocator
pgy_allocator_result(void)
{
    PgyAllocator alloc = pgy_allocator_system();
    alloc.kind = PGY_ALLOC_RESULT;
    return alloc;
}

static inline PgyAllocator
pgy_allocator_persistent(void)
{
    PgyAllocator alloc = pgy_allocator_system();
    alloc.kind = PGY_ALLOC_PERSISTENT;
    return alloc;
}

static inline void
pgy_allocator_destroy(PgyAllocator *alloc)
{
    if (alloc == NULL)
        return;
    if (alloc->pool != NULL) {
        free(alloc->pool->buffer);
        free(alloc->pool);
        alloc->pool = NULL;
    }
}

/*
 * Single charged choke point for the resource budget. Every runtime heap
 * allocation primitive -- pgy_alloc, but also the collection calloc/realloc
 * paths that bypass pgy_alloc -- routes its byte count through here BEFORE
 * allocating, so an over-budget request is denied fail-closed before it
 * consumes anything. The fast-path skips when no budget is imposed, so trusted
 * programs pay nothing. This is the backend-uniform fix: both C and LLVM call
 * the same runtime primitives, which all funnel through this one charge, so a
 * memory-exhaustion bound holds identically across backends regardless of how
 * each backend lowers the allocation. Resolves to the inline budget twin in C
 * output, the extern twin in the linked .bc -- one atomic counter per context. */
static inline void
pgy_budget_charge_alloc(size_t bytes)
{
    if (pgy_budget_is_imposed_export()) {
        pgy_budget_charge_export(PGY_BUDGET_ALLOC_COUNT, 1, "alloc");
        pgy_budget_charge_export(PGY_BUDGET_ALLOC_BYTES, (uint64_t)bytes, "alloc");
    }
}

static inline void *
pgy_alloc(PgyAllocator *alloc, size_t size, size_t align)
{
    pgy_budget_charge_alloc(size);
    if (alloc != NULL && alloc->pool != NULL) {
        size_t align_mask;
        size_t offset;
        void *ptr;
        if (alloc->pool == NULL || align == 0 || (align & (align - 1)) != 0)
            PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_INTERNAL_INVARIANT,
                              "invalid pool allocator state or alignment");
        align_mask = align - 1;
        if (alloc->pool->offset > SIZE_MAX - align_mask)
            PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_OOM,
                              PGY_RUNTIME_PANIC_REASON_POOL_OUT_OF_MEMORY);
        offset = (alloc->pool->offset + align_mask) & ~align_mask;
        if (offset > alloc->pool->capacity
            || size > alloc->pool->capacity - offset)
            PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_OOM,
                              PGY_RUNTIME_PANIC_REASON_POOL_OUT_OF_MEMORY);
        if (alloc->pool->buffer == NULL && size > 0)
            PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_INTERNAL_INVARIANT,
                              "pool allocator has no backing buffer");
        ptr = size == 0 ? NULL : alloc->pool->buffer + offset;
        alloc->pool->offset = offset + size;
        pgy_allocator_record_alloc(alloc, size);
        if (alloc->debug_enabled && ptr != NULL && size > 0)
            memset(ptr, 0xCD, size);
        return ptr;
    }

    void *ptr = malloc(size);
    if (ptr == NULL && size > 0)
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_OOM,
                          PGY_RUNTIME_PANIC_REASON_ALLOCATION_FAILED);
    if (alloc != NULL) {
        pgy_allocator_record_alloc(alloc, size);
        if (alloc->debug_enabled && ptr != NULL && size > 0)
            memset(ptr, 0xCD, size);
    }
    return ptr;
}

static inline void *
pgy_realloc(PgyAllocator *alloc, void *ptr, size_t old_size, size_t new_size)
{
    if (alloc != NULL && alloc->pool != NULL) {
        if (ptr == NULL)
            return pgy_alloc(alloc, new_size, _Alignof(max_align_t));
        PGY_PANIC("Pool allocator does not support realloc");
    }

    void *grown = realloc(ptr, new_size);
    if (grown == NULL && new_size > 0)
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_OOM,
                          PGY_RUNTIME_PANIC_REASON_ALLOCATION_FAILED);

    if (alloc != NULL) {
        pgy_allocator_record_free(alloc, old_size);
        pgy_allocator_record_alloc(alloc, new_size);
        if (alloc->debug_enabled && new_size > old_size) {
            memset((char*)grown + old_size, 0xCD, new_size - old_size);
        }
    }
    return grown;
}

static inline void
pgy_free(PgyAllocator *alloc, void *ptr, size_t size)
{
    if (ptr == NULL)
        return;

    if (alloc != NULL && alloc->pool != NULL) {
        if (alloc->debug_enabled)
            memset(ptr, 0xDD, size);
        pgy_allocator_record_free(alloc, size);
        return;
    }

    if (alloc != NULL) {
        if (alloc->debug_enabled)
            memset(ptr, 0xDD, size);
        pgy_allocator_record_free(alloc, size);
    }
    free(ptr);
}
