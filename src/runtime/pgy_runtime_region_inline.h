#ifndef PGY_RUNTIME_REGION_INLINE_H
#define PGY_RUNTIME_REGION_INLINE_H

/* =================================================================
 * Region Allocator (chained-block, growable) [WO-REG-1, docs/197]
 *
 * PgyRegion is the runtime backing for the declared `region` lifetime scope.
 * It supersedes the fixed-capacity PgyArena (pgy_runtime_memory_array_slot_
 * inline.h): that frame cannot grow, so a region accumulating an unbounded
 * number of transient allocations (the first consumer is statement-local
 * string temporaries) would have to guess a capacity up front or panic on a
 * correct program. Chained blocks grow by acquiring another block instead of
 * forking into a spill-or-panic decision -- one behaviour, not two. Pointers
 * stay stable (no realloc invalidation).
 *
 * Discipline:
 *   - Header-only static inline: no global state, so all three runtime
 *     materializations (inline / PGY_RUNTIME_LIB_INTERNAL /
 *     PGY_RUNTIME_EXTERN_DEFS) get an identical copy with no export TU.
 *   - Each block acquisition charges the allocation budget (the quantitative
 *     sandbox axis, R6/docs/134); the per-bump fast path carries no atomic.
 *   - OOM is fail-closed panic -- never a silent spill to another allocator.
 *   - Allocations are aligned by their real address, so a block's malloc base
 *     alignment need not match the requested alignment.
 *
 * Include-order dependency: this header is part of the runtime inline chain
 * (pgy_runtime_inline_core.h) and assumes the panic contract
 * (PGY_RUNTIME_PANIC, PGY_RUNTIME_PANIC_REASON_ARENA_OUT_OF_MEMORY) and the
 * budget charge (pgy_budget_charge_export, PGY_BUDGET_ALLOC_*) are already
 * visible -- both are provided by pgy_runtime_panic_checked_inline.h, included
 * earlier in that chain. It also needs <string.h>/<stdlib.h>/<stdint.h>, which
 * the chain pulls in ahead of it.
 * ================================================================= */

#ifndef PGY_REGION_DEFAULT_BLOCK_SIZE
#define PGY_REGION_DEFAULT_BLOCK_SIZE ((size_t)8192)
#endif

typedef struct PgyRegionBlock {
    struct PgyRegionBlock *next;
    size_t                 used;
    size_t                 capacity;
    /* payload follows (flexible array); aligned per-allocation by address */
    char                   data[];
} PgyRegionBlock;

typedef struct {
    PgyRegionBlock *current;         /* most-recent block; NULL until 1st alloc */
    size_t          block_size;      /* size for freshly acquired blocks        */
    size_t          total_allocated; /* diagnostic: payload bytes handed out    */
} PgyRegion;

static inline PgyRegion
pgy_region_create(size_t block_size)
{
    PgyRegion region;
    region.current         = NULL;
    region.block_size      = block_size > 0 ? block_size
                                            : PGY_REGION_DEFAULT_BLOCK_SIZE;
    region.total_allocated = 0;
    return region;
}

static inline void
pgy_region_destroy(PgyRegion* region)
{
    PgyRegionBlock* blk;
    if (region == NULL)
        return;
    blk = region->current;
    while (blk != NULL) {
        PgyRegionBlock* next = blk->next;
        free(blk);
        blk = next;
    }
    region->current         = NULL;
    region->total_allocated = 0;
}

/* Acquire a block with at least `need` payload bytes and push it to the front.
 * Budget is charged here -- once per acquisition, on the block capacity -- so a
 * region under an imposed ceiling fails closed the moment its blocks exceed the
 * bound, without an atomic on every bump. */
static inline PgyRegionBlock*
pgy_region_block_acquire(PgyRegion* region, size_t need)
{
    size_t          cap = region->block_size;
    PgyRegionBlock* blk;

    if (need > cap)
        cap = need;
    if (cap > SIZE_MAX - sizeof(PgyRegionBlock)) {
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_OOM,
                          PGY_RUNTIME_PANIC_REASON_ARENA_OUT_OF_MEMORY);
    }
    blk = (PgyRegionBlock*)malloc(sizeof(PgyRegionBlock) + cap);
    if (blk == NULL) {
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_OOM,
                          PGY_RUNTIME_PANIC_REASON_ALLOCATION_FAILED);
    }
    blk->next        = region->current;
    blk->used        = 0;
    blk->capacity    = cap;
    region->current  = blk;
    pgy_budget_charge_export(PGY_BUDGET_ALLOC_COUNT, 1, "region");
    pgy_budget_charge_export(PGY_BUDGET_ALLOC_BYTES, (uint64_t)cap, "region");
    return blk;
}

static inline void*
pgy_region_alloc(PgyRegion* region, size_t size, size_t align)
{
    PgyRegionBlock* blk;
    uintptr_t       base;
    uintptr_t       aligned;
    size_t          pad;

    if (region == NULL || align == 0 || (align & (align - 1)) != 0) {
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_INTERNAL_INVARIANT,
                          "invalid region or alignment");
    }
    if (size == 0)
        return NULL;

    blk = region->current;
    if (blk != NULL) {
        base    = (uintptr_t)(blk->data + blk->used);
        aligned = (base + (align - 1)) & ~(uintptr_t)(align - 1);
        pad     = (size_t)(aligned - base);
        if (pad <= blk->capacity - blk->used
            && size <= blk->capacity - blk->used - pad) {
            blk->used += pad + size;
            if (region->total_allocated <= SIZE_MAX - size)
                region->total_allocated += size;
            return (void*)aligned;
        }
    }

    /* Fresh block. Reserve size + (align-1) so alignment padding always fits
     * regardless of the new block's base address. */
    if (size > SIZE_MAX - (align - 1)) {
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_OOM,
                          PGY_RUNTIME_PANIC_REASON_ARENA_OUT_OF_MEMORY);
    }
    blk     = pgy_region_block_acquire(region, size + (align - 1));
    base    = (uintptr_t)(blk->data + blk->used);
    aligned = (base + (align - 1)) & ~(uintptr_t)(align - 1);
    pad     = (size_t)(aligned - base);
    blk->used += pad + size;
    if (region->total_allocated <= SIZE_MAX - size)
        region->total_allocated += size;
    return (void*)aligned;
}

static inline void*
pgy_region_alloc_array(PgyRegion* region, size_t elem_size, size_t align,
                       size_t count)
{
    if (count > 0 && elem_size > SIZE_MAX / count) {
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_OOM,
                          PGY_RUNTIME_PANIC_REASON_ARENA_OUT_OF_MEMORY);
    }
    return pgy_region_alloc(region, elem_size * count, align);
}

#define PGY_REGION_ALLOC(region, Type) \
    ((Type*)pgy_region_alloc((region), sizeof(Type), _Alignof(Type)))

#define PGY_REGION_ALLOC_ARRAY(region, Type, count) \
    ((Type*)pgy_region_alloc_array((region), sizeof(Type), _Alignof(Type), (count)))

/* Copy a NUL-terminated string into the region. NULL source yields NULL. */
static inline char*
pgy_region_strdup(PgyRegion* region, const char* s)
{
    size_t len;
    char*  dup;
    if (s == NULL)
        return NULL;
    len = strlen(s);
    if (len > SIZE_MAX - 1) {
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_OOM,
                          PGY_RUNTIME_PANIC_REASON_ARENA_OUT_OF_MEMORY);
    }
    dup = (char*)pgy_region_alloc(region, len + 1, 1);
    memcpy(dup, s, len + 1);
    return dup;
}

/* Region-backed string concatenation: the arena-allocating twin of the
 * heap-allocating StringConcat (pgy_runtime_string_builtin_inline.h). The
 * region owns the result -- no per-temporary free, no leak when a chained
 * concat's intermediates are never named. OOM fails closed (panic), unlike
 * StringConcat's silent-empty fallback, because a region site is only emitted
 * where the escape analysis certified the whole tree region-safe. */
static inline char*
pgy_region_string_concat(PgyRegion* region, const char* a, const char* b)
{
    size_t la, lb;
    char*  buf;
    if (a == NULL) a = "";
    if (b == NULL) b = "";
    la = strlen(a);
    lb = strlen(b);
    if (la > SIZE_MAX - lb || la + lb > SIZE_MAX - 1) {
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_OOM,
                          PGY_RUNTIME_PANIC_REASON_ARENA_OUT_OF_MEMORY);
    }
    buf = (char*)pgy_region_alloc(region, la + lb + 1, 1);
    memcpy(buf, a, la);
    memcpy(buf + la, b, lb + 1);
    return buf;
}

/* Reset for reuse (frame-arena pattern): keep the blocks, drop their contents.
 * Retained blocks are not re-charged on reuse -- the charge was at acquisition.
 * Not on the function-scope-region path (that create/destroys per call). */
static inline void
pgy_region_reset(PgyRegion* region)
{
    PgyRegionBlock* blk;
    if (region == NULL)
        return;
    for (blk = region->current; blk != NULL; blk = blk->next)
        blk->used = 0;
    region->total_allocated = 0;
}

#endif /* PGY_RUNTIME_REGION_INLINE_H */
