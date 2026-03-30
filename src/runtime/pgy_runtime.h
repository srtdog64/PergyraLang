/*
 * Copyright (c) 2025 Pergyra Language Project
 * All rights reserved.
 *
 * pgy_runtime.h — Hybrid memory model runtime for Pergyra
 *
 * Memory Model:
 *   - Stack: Default for value types (zero overhead)
 *   - Heap: Box<T>, Arena for dynamic allocation
 *   - Slot: Optional safety wrapper with debug checks
 *   - Unsafe: Raw pointers for FFI (inside unsafe { } blocks)
 *
 * Build Modes:
 *   - Debug: PGY_DEBUG defined — safety checks enabled
 *   - Release: PGY_DEBUG not defined — zero-overhead
 */

#ifndef PGY_RUNTIME_H
#define PGY_RUNTIME_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>

/* =================================================================
 * Build Mode Configuration
 * ================================================================= */

/* PGY_DEBUG: Define for debug builds with safety checks */
/* #define PGY_DEBUG */

/* PGY_SAFE_SLOTS: Even in release, keep slot safety (optional) */
/* #define PGY_SAFE_SLOTS */

#ifdef PGY_DEBUG
#  define PGY_DEBUG_ONLY(x) x
#  define PGY_RELEASE_ONLY(x)
#else
#  define PGY_DEBUG_ONLY(x)
#  define PGY_RELEASE_ONLY(x) x
#endif

#if defined(PGY_DEBUG) || defined(PGY_SAFE_SLOTS)
#  define PGY_WITH_SLOT_CHECKS 1
#else
#  define PGY_WITH_SLOT_CHECKS 0
#endif

/* =================================================================
 * Panic — Unrecoverable Error
 * ================================================================= */

#define PGY_PANIC(msg) \
    do { \
        fprintf(stderr, "[PGY PANIC] %s:%d — %s\n", \
                __FILE__, __LINE__, (msg)); \
        abort(); \
    } while (0)

#define PGY_ASSERT(cond, msg) \
    do { \
        if (!(cond)) { \
            PGY_PANIC(msg); \
        } \
    } while (0)

/* =================================================================
 * Stack Memory (Default — Zero Overhead)
 *
 * All plain variables are stack-allocated by default in C.
 * No special macros needed.
 * ================================================================= */

/* Example:
 *   let x: Int = 42;     →   int32_t x = 42;
 *   let v: Vec3;         →   Vec3 v;
 */

/* =================================================================
 * Heap Memory — Box<T> (Owned Heap Allocation)
 * ================================================================= */

#define PGY_BOX_DEFINE(SuffixName, CType) \
\
typedef struct { \
    CType* ptr; \
} PgyBox_##SuffixName; \
\
static inline PgyBox_##SuffixName \
pgy_box_new_##SuffixName(CType value) \
{ \
    PgyBox_##SuffixName b; \
    b.ptr = (CType*)malloc(sizeof(CType)); \
    if (b.ptr == NULL) { \
        PGY_PANIC("Box allocation failed"); \
    } \
    *b.ptr = value; \
    return b; \
} \
\
static inline CType \
pgy_box_get_##SuffixName(PgyBox_##SuffixName b) \
{ \
    if (b.ptr == NULL) { \
        PGY_PANIC("Box access after move/free"); \
    } \
    return *b.ptr; \
} \
\
static inline void \
pgy_box_set_##SuffixName(PgyBox_##SuffixName* b, CType value) \
{ \
    if (b->ptr == NULL) { \
        PGY_PANIC("Box set after move/free"); \
    } \
    *b->ptr = value; \
} \
\
static inline void \
pgy_box_drop_##SuffixName(PgyBox_##SuffixName* b) \
{ \
    if (b->ptr != NULL) { \
        free(b->ptr); \
        b->ptr = NULL; \
    } \
} \
\
static inline bool \
pgy_box_is_valid_##SuffixName(PgyBox_##SuffixName* b) \
{ \
    return b->ptr != NULL; \
}

/* Box move semantics (transfer ownership) */
#define PGY_BOX_MOVE(dst, src, SuffixName) \
    do { \
        (dst) = (src); \
        (src).ptr = NULL; \
    } while (0)

/* =================================================================
 * Heap Memory — Arena Allocator (Frame-based)
 * ================================================================= */

typedef struct {
    char*  buffer;
    size_t capacity;
    size_t offset;
} PgyArena;

static inline PgyArena
pgy_arena_create(size_t capacity)
{
    PgyArena arena;
    arena.buffer = (char*)malloc(capacity);
    if (arena.buffer == NULL) {
        PGY_PANIC("Arena allocation failed");
    }
    arena.capacity = capacity;
    arena.offset = 0;
    return arena;
}

static inline void
pgy_arena_destroy(PgyArena* arena)
{
    if (arena->buffer != NULL) {
        free(arena->buffer);
        arena->buffer = NULL;
    }
}

static inline void*
pgy_arena_alloc(PgyArena* arena, size_t size, size_t align)
{
    /* Align the current offset */
    size_t aligned_offset = (arena->offset + align - 1) & ~(align - 1);

    if (aligned_offset + size > arena->capacity) {
        PGY_PANIC("Arena out of memory");
    }

    void* ptr = arena->buffer + aligned_offset;
    arena->offset = aligned_offset + size;
    return ptr;
}

#define PGY_ARENA_ALLOC(arena, Type) \
    ((Type*)pgy_arena_alloc((arena), sizeof(Type), _Alignof(Type)))

#define PGY_ARENA_ALLOC_ARRAY(arena, Type, count) \
    ((Type*)pgy_arena_alloc((arena), sizeof(Type) * (count), _Alignof(Type)))

static inline void
pgy_arena_reset(PgyArena* arena)
{
    arena->offset = 0;
}

/* =================================================================
 * Allocator Interface
 * ================================================================= */

typedef enum {
    PGY_ALLOC_SYSTEM,
    PGY_ALLOC_TRACING,
    PGY_ALLOC_DEBUG,
    PGY_ALLOC_POOL
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
    alloc->allocations++;
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
        PGY_PANIC("Pool allocator state allocation failed");
    alloc.pool->buffer = (char*)malloc(capacity);
    if (alloc.pool->buffer == NULL)
        PGY_PANIC("Pool allocator buffer allocation failed");
    alloc.pool->capacity = capacity;
    alloc.pool->offset = 0;
    return alloc;
}

static inline void
pgy_allocator_destroy(PgyAllocator *alloc)
{
    if (alloc == NULL)
        return;
    if (alloc->kind == PGY_ALLOC_POOL && alloc->pool != NULL) {
        free(alloc->pool->buffer);
        free(alloc->pool);
        alloc->pool = NULL;
    }
}

static inline void *
pgy_alloc(PgyAllocator *alloc, size_t size, size_t align)
{
    if (alloc != NULL && alloc->kind == PGY_ALLOC_POOL) {
        size_t offset = (alloc->pool->offset + align - 1) & ~(align - 1);
        if (offset + size > alloc->pool->capacity)
            PGY_PANIC("Pool allocator out of memory");
        void *ptr = alloc->pool->buffer + offset;
        alloc->pool->offset = offset + size;
        pgy_allocator_record_alloc(alloc, size);
        if (alloc->debug_enabled)
            memset(ptr, 0xCD, size);
        return ptr;
    }

    void *ptr = malloc(size);
    if (ptr == NULL)
        PGY_PANIC("Allocator allocation failed");
    if (alloc != NULL) {
        pgy_allocator_record_alloc(alloc, size);
        if (alloc->debug_enabled)
            memset(ptr, 0xCD, size);
    }
    return ptr;
}

static inline void *
pgy_realloc(PgyAllocator *alloc, void *ptr, size_t old_size, size_t new_size)
{
    if (alloc != NULL && alloc->kind == PGY_ALLOC_POOL) {
        if (ptr == NULL)
            return pgy_alloc(alloc, new_size, _Alignof(max_align_t));
        PGY_PANIC("Pool allocator does not support realloc");
    }

    void *grown = realloc(ptr, new_size);
    if (grown == NULL)
        PGY_PANIC("Allocator reallocation failed");

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

    if (alloc != NULL && alloc->kind == PGY_ALLOC_POOL) {
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

/* =================================================================
 * Array / Slice
 * ================================================================= */

#define PGY_ARRAY_DEFINE(SuffixName, CType) \
typedef struct { \
    CType        *data; \
    size_t        length; \
    size_t        capacity; \
    PgyAllocator *allocator; \
} PgyArray_##SuffixName; \
\
typedef struct { \
    CType  *data; \
    size_t  length; \
} PgySlice_##SuffixName; \
\
static inline PgyArray_##SuffixName \
pgy_array_new_in_##SuffixName(PgyAllocator *alloc, size_t capacity) \
{ \
    PgyArray_##SuffixName arr; \
    arr.length = 0; \
    arr.capacity = capacity; \
    arr.allocator = alloc; \
    arr.data = capacity > 0 \
        ? (CType*)pgy_alloc(alloc, sizeof(CType) * capacity, _Alignof(CType)) \
        : NULL; \
    return arr; \
} \
\
static inline PgyArray_##SuffixName \
pgy_array_new_##SuffixName(size_t capacity) \
{ \
    return pgy_array_new_in_##SuffixName(NULL, capacity); \
} \
\
static inline void \
pgy_array_drop_##SuffixName(PgyArray_##SuffixName *arr) \
{ \
    if (arr->data != NULL) { \
        pgy_free(arr->allocator, arr->data, sizeof(CType) * arr->capacity); \
        arr->data = NULL; \
    } \
    arr->length = 0; \
    arr->capacity = 0; \
} \
\
static inline void \
pgy_array_reserve_##SuffixName(PgyArray_##SuffixName *arr, size_t new_capacity) \
{ \
    if (new_capacity <= arr->capacity) \
        return; \
    size_t old_size = sizeof(CType) * arr->capacity; \
    size_t new_size = sizeof(CType) * new_capacity; \
    arr->data = arr->data == NULL \
        ? (CType*)pgy_alloc(arr->allocator, new_size, _Alignof(CType)) \
        : (CType*)pgy_realloc(arr->allocator, arr->data, old_size, new_size); \
    arr->capacity = new_capacity; \
} \
\
static inline void \
pgy_array_push_##SuffixName(PgyArray_##SuffixName *arr, CType value) \
{ \
    if (arr->length == arr->capacity) { \
        size_t next = arr->capacity == 0 ? 4 : arr->capacity * 2; \
        pgy_array_reserve_##SuffixName(arr, next); \
    } \
    arr->data[arr->length++] = value; \
} \
\
static inline CType \
pgy_array_get_##SuffixName(PgyArray_##SuffixName *arr, size_t index) \
{ \
    if (index >= arr->length) \
        PGY_PANIC("Array index out of bounds"); \
    return arr->data[index]; \
} \
\
static inline PgySlice_##SuffixName \
pgy_array_slice_##SuffixName(PgyArray_##SuffixName *arr, size_t start, size_t len) \
{ \
    PgySlice_##SuffixName slice; \
    if (start + len > arr->length) \
        PGY_PANIC("Slice out of bounds"); \
    slice.data = arr->data + start; \
    slice.length = len; \
    return slice; \
}

/* =================================================================
 * Rc / Weak
 * ================================================================= */

#define PGY_RC_DEFINE(SuffixName, CType) \
typedef struct { \
    uint32_t strong_count; \
    uint32_t weak_count; \
    bool     alive; \
    CType    value; \
} PgyRcControl_##SuffixName; \
\
typedef struct { \
    PgyRcControl_##SuffixName *ctrl; \
} PgyRc_##SuffixName; \
\
typedef struct { \
    PgyRcControl_##SuffixName *ctrl; \
} PgyWeak_##SuffixName; \
\
static inline PgyRc_##SuffixName \
pgy_rc_new_##SuffixName(CType value) \
{ \
    PgyRc_##SuffixName rc; \
    rc.ctrl = (PgyRcControl_##SuffixName*)malloc(sizeof(PgyRcControl_##SuffixName)); \
    if (rc.ctrl == NULL) \
        PGY_PANIC("Rc allocation failed"); \
    rc.ctrl->strong_count = 1; \
    rc.ctrl->weak_count = 0; \
    rc.ctrl->alive = true; \
    rc.ctrl->value = value; \
    return rc; \
} \
\
static inline PgyRc_##SuffixName \
pgy_rc_clone_##SuffixName(PgyRc_##SuffixName rc) \
{ \
    if (rc.ctrl == NULL || !rc.ctrl->alive || rc.ctrl->strong_count == 0) \
        PGY_PANIC("RcClone on invalid Rc"); \
    rc.ctrl->strong_count++; \
    return rc; \
} \
\
static inline CType * \
pgy_rc_get_##SuffixName(PgyRc_##SuffixName *rc) \
{ \
    if (rc->ctrl == NULL || !rc->ctrl->alive || rc->ctrl->strong_count == 0) \
        PGY_PANIC("RcGet on invalid Rc"); \
    return &rc->ctrl->value; \
} \
\
static inline PgyWeak_##SuffixName \
pgy_rc_downgrade_##SuffixName(PgyRc_##SuffixName rc) \
{ \
    PgyWeak_##SuffixName weak; \
    if (rc.ctrl == NULL) \
        PGY_PANIC("RcDowngrade on null Rc"); \
    rc.ctrl->weak_count++; \
    weak.ctrl = rc.ctrl; \
    return weak; \
} \
\
static inline void \
pgy_rc_drop_##SuffixName(PgyRc_##SuffixName *rc) \
{ \
    if (rc->ctrl == NULL) \
        return; \
    if (rc->ctrl->strong_count == 0) \
        PGY_PANIC("RcDrop on empty Rc"); \
    rc->ctrl->strong_count--; \
    if (rc->ctrl->strong_count == 0) { \
        rc->ctrl->alive = false; \
        if (rc->ctrl->weak_count == 0) \
            free(rc->ctrl); \
    } \
    rc->ctrl = NULL; \
} \
\
static inline PgyRc_##SuffixName \
pgy_weak_upgrade_##SuffixName(PgyWeak_##SuffixName weak) \
{ \
    if (weak.ctrl == NULL || !weak.ctrl->alive || weak.ctrl->strong_count == 0) \
        PGY_PANIC("WeakUpgrade on expired Weak"); \
    weak.ctrl->strong_count++; \
    PgyRc_##SuffixName rc; \
    rc.ctrl = weak.ctrl; \
    return rc; \
} \
\
static inline void \
pgy_weak_drop_##SuffixName(PgyWeak_##SuffixName *weak) \
{ \
    if (weak->ctrl == NULL) \
        return; \
    if (weak->ctrl->weak_count == 0) \
        PGY_PANIC("WeakDrop on empty Weak"); \
    weak->ctrl->weak_count--; \
    if (weak->ctrl->weak_count == 0 && weak->ctrl->strong_count == 0) \
        free(weak->ctrl); \
    weak->ctrl = NULL; \
}

/* =================================================================
 * Box<Array<T>> fused allocation
 * ================================================================= */

#define PGY_BOX_ARRAY_DEFINE(SuffixName, CType) \
typedef struct { \
    PgyArray_##SuffixName *ptr; \
} PgyBoxArray_##SuffixName; \
\
typedef struct { \
    PgyArray_##SuffixName array; \
    CType storage[]; \
} PgyBoxArrayStorage_##SuffixName; \
\
static inline PgyBoxArray_##SuffixName \
pgy_box_array_new_##SuffixName(size_t capacity, PgyAllocator *alloc) \
{ \
    size_t bytes = sizeof(PgyBoxArrayStorage_##SuffixName) + sizeof(CType) * capacity; \
    PgyBoxArrayStorage_##SuffixName *storage = \
        (PgyBoxArrayStorage_##SuffixName*)pgy_alloc(alloc, bytes, _Alignof(PgyBoxArrayStorage_##SuffixName)); \
    storage->array.data = storage->storage; \
    storage->array.length = 0; \
    storage->array.capacity = capacity; \
    storage->array.allocator = alloc; \
    PgyBoxArray_##SuffixName box; \
    box.ptr = &storage->array; \
    return box; \
} \
\
static inline PgyArray_##SuffixName * \
pgy_box_array_get_##SuffixName(PgyBoxArray_##SuffixName *box) \
{ \
    if (box->ptr == NULL) \
        PGY_PANIC("BoxArray access after drop"); \
    return box->ptr; \
} \
\
static inline void \
pgy_box_array_drop_##SuffixName(PgyBoxArray_##SuffixName *box) \
{ \
    if (box->ptr == NULL) \
        return; \
    PgyBoxArrayStorage_##SuffixName *storage = \
        (PgyBoxArrayStorage_##SuffixName*)((char*)box->ptr - offsetof(PgyBoxArrayStorage_##SuffixName, array)); \
    size_t bytes = sizeof(PgyBoxArrayStorage_##SuffixName) + sizeof(CType) * box->ptr->capacity; \
    pgy_free(box->ptr->allocator, storage, bytes); \
    box->ptr = NULL; \
}

/* =================================================================
 * Slot Memory (Optional Safety Wrapper)
 *
 * Debug mode: Full safety checks (occupied flag, panic on invalid access)
 * Release mode: Zero-overhead passthrough (no occupied flag)
 * ================================================================= */

/* Debug mode slot — with safety checks */
#define PGY_SLOT_DEFINE_DEBUG(SuffixName, CType) \
\
typedef struct { \
    CType   value; \
    bool    occupied; \
} PgySlot_##SuffixName; \
\
static inline PgySlot_##SuffixName \
pgy_claim_##SuffixName(void) \
{ \
    PgySlot_##SuffixName s; \
    memset(&s, 0, sizeof(s)); \
    s.occupied = true; \
    return s; \
} \
\
static inline void \
pgy_write_##SuffixName(PgySlot_##SuffixName* s, CType v) \
{ \
    PGY_ASSERT(s->occupied, "Write to released slot"); \
    s->value = v; \
} \
\
static inline CType \
pgy_read_##SuffixName(PgySlot_##SuffixName* s) \
{ \
    PGY_ASSERT(s->occupied, "Read from released slot"); \
    return s->value; \
} \
\
static inline void \
pgy_release_##SuffixName(PgySlot_##SuffixName* s) \
{ \
    PGY_ASSERT(s->occupied, "Double release of slot"); \
    s->occupied = false; \
}

/* Release mode slot — zero overhead (just a wrapper around the value) */
#define PGY_SLOT_DEFINE_RELEASE(SuffixName, CType) \
\
typedef struct { \
    CType   value; \
} PgySlot_##SuffixName; \
\
static inline PgySlot_##SuffixName \
pgy_claim_##SuffixName(void) \
{ \
    PgySlot_##SuffixName s; \
    memset(&s, 0, sizeof(s)); \
    return s; \
} \
\
static inline void \
pgy_write_##SuffixName(PgySlot_##SuffixName* s, CType v) \
{ \
    s->value = v; \
} \
\
static inline CType \
pgy_read_##SuffixName(PgySlot_##SuffixName* s) \
{ \
    return s->value; \
} \
\
static inline void \
pgy_release_##SuffixName(PgySlot_##SuffixName* s) \
{ \
    (void)s; /* no-op in release mode */ \
}

/* Conditional definition based on build mode */
#if PGY_WITH_SLOT_CHECKS
#  define PGY_SLOT_DEFINE(SuffixName, CType) \
       PGY_SLOT_DEFINE_DEBUG(SuffixName, CType)
#else
#  define PGY_SLOT_DEFINE(SuffixName, CType) \
       PGY_SLOT_DEFINE_RELEASE(SuffixName, CType)
#endif

/* =================================================================
 * Secure Slot (Token-based Access Control)
 *
 * Always includes checks (security feature), but can be disabled
 * in release mode for non-security-critical builds.
 * ================================================================= */

#define PGY_SECURE_SLOT_DEFINE_DEBUG(SuffixName, CType) \
\
typedef struct { \
    CType    value; \
    bool     occupied; \
    uint64_t token; \
} PgySecureSlot_##SuffixName; \
\
typedef struct { \
    uint64_t id; \
    bool     can_write; \
    bool     can_read; \
} PgyToken_##SuffixName; \
\
static inline void \
pgy_make_token_##SuffixName(PgySecureSlot_##SuffixName* s, \
                             PgyToken_##SuffixName* t) \
{ \
    uint64_t id = (uint64_t)(uintptr_t)s ^ 0xDEADBEEFCAFEBABEULL; \
    s->token    = id; \
    t->id       = id; \
    t->can_write = true; \
    t->can_read  = true; \
} \
\
static inline PgySecureSlot_##SuffixName \
pgy_claim_secure_##SuffixName(PgyToken_##SuffixName* out_token) \
{ \
    PgySecureSlot_##SuffixName s; \
    memset(&s, 0, sizeof(s)); \
    s.occupied = true; \
    pgy_make_token_##SuffixName(&s, out_token); \
    return s; \
} \
\
static inline void \
pgy_secure_write_##SuffixName(PgySecureSlot_##SuffixName* s, \
                               CType v, \
                               const PgyToken_##SuffixName* t) \
{ \
    PGY_ASSERT(s->occupied, "Write to released secure slot"); \
    PGY_ASSERT(s->token == t->id, "Invalid token on write"); \
    PGY_ASSERT(t->can_write, "Token does not allow write"); \
    s->value = v; \
} \
\
static inline CType \
pgy_secure_read_##SuffixName(PgySecureSlot_##SuffixName* s, \
                              const PgyToken_##SuffixName* t) \
{ \
    PGY_ASSERT(s->occupied, "Read from released secure slot"); \
    PGY_ASSERT(s->token == t->id, "Invalid token on read"); \
    PGY_ASSERT(t->can_read, "Token does not allow read"); \
    return s->value; \
} \
\
static inline void \
pgy_secure_release_##SuffixName(PgySecureSlot_##SuffixName* s, \
                                 const PgyToken_##SuffixName* t) \
{ \
    PGY_ASSERT(s->occupied, "Double release of secure slot"); \
    PGY_ASSERT(s->token == t->id, "Invalid token on release"); \
    s->occupied = false; \
    s->token    = 0; \
}

/* Release mode secure slot — minimal checks */
#define PGY_SECURE_SLOT_DEFINE_RELEASE(SuffixName, CType) \
\
typedef struct { \
    CType    value; \
    bool     occupied; \
    uint64_t token; \
} PgySecureSlot_##SuffixName; \
\
typedef struct { \
    uint64_t id; \
} PgyToken_##SuffixName; \
\
static inline void \
pgy_make_token_##SuffixName(PgySecureSlot_##SuffixName* s, \
                             PgyToken_##SuffixName* t) \
{ \
    uint64_t id = (uint64_t)(uintptr_t)s ^ 0xDEADBEEFCAFEBABEULL; \
    s->token    = id; \
    t->id       = id; \
} \
\
static inline PgySecureSlot_##SuffixName \
pgy_claim_secure_##SuffixName(PgyToken_##SuffixName* out_token) \
{ \
    PgySecureSlot_##SuffixName s; \
    memset(&s, 0, sizeof(s)); \
    s.occupied = true; \
    pgy_make_token_##SuffixName(&s, out_token); \
    return s; \
} \
\
static inline void \
pgy_secure_write_##SuffixName(PgySecureSlot_##SuffixName* s, \
                               CType v, \
                               const PgyToken_##SuffixName* t) \
{ \
    if (s->token != t->id) PGY_PANIC("Invalid token"); \
    s->value = v; \
} \
\
static inline CType \
pgy_secure_read_##SuffixName(PgySecureSlot_##SuffixName* s, \
                              const PgyToken_##SuffixName* t) \
{ \
    if (s->token != t->id) PGY_PANIC("Invalid token"); \
    return s->value; \
} \
\
static inline void \
pgy_secure_release_##SuffixName(PgySecureSlot_##SuffixName* s, \
                                 const PgyToken_##SuffixName* t) \
{ \
    if (s->token != t->id) PGY_PANIC("Invalid token"); \
    s->occupied = false; \
}

#if PGY_WITH_SLOT_CHECKS
#  define PGY_SECURE_SLOT_DEFINE(SuffixName, CType) \
       PGY_SECURE_SLOT_DEFINE_DEBUG(SuffixName, CType)
#else
#  define PGY_SECURE_SLOT_DEFINE(SuffixName, CType) \
       PGY_SECURE_SLOT_DEFINE_RELEASE(SuffixName, CType)
#endif

/* =================================================================
 * Instantiate Built-in Slot Types
 * ================================================================= */

PGY_SLOT_DEFINE(Int,    int32_t)
PGY_SLOT_DEFINE(Long,   int64_t)
PGY_SLOT_DEFINE(Float,  float)
PGY_SLOT_DEFINE(Double, double)
PGY_SLOT_DEFINE(Bool,   bool)
PGY_SLOT_DEFINE(String, char*)

PGY_SECURE_SLOT_DEFINE(Int,    int32_t)
PGY_SECURE_SLOT_DEFINE(Long,   int64_t)
PGY_SECURE_SLOT_DEFINE(Float,  float)
PGY_SECURE_SLOT_DEFINE(Double, double)
PGY_SECURE_SLOT_DEFINE(Bool,   bool)
PGY_SECURE_SLOT_DEFINE(String, char*)

/* =================================================================
 * Instantiate Box Types for Built-ins
 * ================================================================= */

PGY_BOX_DEFINE(Int,    int32_t)
PGY_BOX_DEFINE(Long,   int64_t)
PGY_BOX_DEFINE(Float,  float)
PGY_BOX_DEFINE(Double, double)
PGY_BOX_DEFINE(Bool,   bool)
PGY_BOX_DEFINE(String, char*)

/* =================================================================
 * Instantiate Array / Slice / Rc / Weak / BoxArray for Built-ins
 * ================================================================= */

PGY_ARRAY_DEFINE(Int,    int32_t)
PGY_ARRAY_DEFINE(Long,   int64_t)
PGY_ARRAY_DEFINE(Float,  float)
PGY_ARRAY_DEFINE(Double, double)
PGY_ARRAY_DEFINE(Bool,   bool)
PGY_ARRAY_DEFINE(String, char*)

PGY_RC_DEFINE(Int,    int32_t)
PGY_RC_DEFINE(Long,   int64_t)
PGY_RC_DEFINE(Float,  float)
PGY_RC_DEFINE(Double, double)
PGY_RC_DEFINE(Bool,   bool)
PGY_RC_DEFINE(String, char*)

PGY_BOX_ARRAY_DEFINE(Int,    int32_t)
PGY_BOX_ARRAY_DEFINE(Long,   int64_t)
PGY_BOX_ARRAY_DEFINE(Float,  float)
PGY_BOX_ARRAY_DEFINE(Double, double)
PGY_BOX_ARRAY_DEFINE(Bool,   bool)
PGY_BOX_ARRAY_DEFINE(String, char*)

/* =================================================================
 * Log — Type-safe Logging
 * ================================================================= */

static inline void pgy_log_int(int32_t v)    { printf("%d\n", v); }
static inline void pgy_log_long(int64_t v)   { printf("%lld\n", (long long)v); }
static inline void pgy_log_float(float v)    { printf("%f\n", v); }
static inline void pgy_log_double(double v)  { printf("%lf\n", v); }
static inline void pgy_log_bool(bool v)      { printf("%s\n", v ? "true" : "false"); }
static inline void pgy_log_string(char* v)   { printf("%s\n", v ? v : "(null)"); }

#define pgy_log(x) _Generic((x), \
    int32_t:  pgy_log_int,    \
    int64_t:  pgy_log_long,   \
    float:    pgy_log_float,  \
    double:   pgy_log_double, \
    bool:     pgy_log_bool,   \
    char*:    pgy_log_string  \
)(x)

/* =================================================================
 * Standard Library Helpers
 * ================================================================= */

static inline char* pgy_int_to_string(int32_t val) {
    static char buf[32];
    snprintf(buf, sizeof(buf), "%d", val);
    return buf;
}

/* =================================================================
 * Parallel Support (OpenMP or Sequential Fallback)
 * ================================================================= */

#ifdef _OPENMP
#  include <omp.h>
#  define PGY_PARALLEL_BEGIN \
       _Pragma("omp parallel sections")  {
#  define PGY_PARALLEL_TASK  _Pragma("omp section")
#  define PGY_PARALLEL_END   }
#else
#  define PGY_PARALLEL_BEGIN {
#  define PGY_PARALLEL_TASK  /* sequential */
#  define PGY_PARALLEL_END   }
#endif

/* =================================================================
 * Unsafe Block Marker (for FFI)
 *
 * In C, this is just a documentation marker. Future versions may
 * add static analysis tools to check unsafe block boundaries.
 * ================================================================= */

#define PGY_UNSAFE_BEGIN \
    /* BEGIN UNSAFE_BLOCK */
#define PGY_UNSAFE_END \
    /* END UNSAFE_BLOCK */

/* Raw pointer operations (use inside unsafe blocks) */
#define PGY_PTR_NEW(Type) \
    ((Type*)malloc(sizeof(Type)))

#define PGY_PTR_NEW_ARRAY(Type, count) \
    ((Type*)malloc(sizeof(Type) * (count)))

#define PGY_PTR_FREE(ptr) \
    do { \
        free(ptr); \
        (ptr) = NULL; \
    } while (0)

#define PGY_PTR_READ(ptr) \
    (*(ptr))

#define PGY_PTR_WRITE(ptr, val) \
    do { \
        (*(ptr)) = (val); \
    } while (0)

/* =================================================================
 * Result Type (Error Handling)
 * ================================================================= */

typedef enum {
    PgyResultOk,
    PgyResultErr
} PgyResultTag;

#define PGY_RESULT_DEFINE(SuffixName, CType, ErrType) \
\
typedef struct { \
    PgyResultTag tag; \
    union { \
        CType ok; \
        ErrType err; \
    }; \
} PgyResult_##SuffixName; \
\
static inline PgyResult_##SuffixName \
pgy_result_ok_##SuffixName(CType value) \
{ \
    PgyResult_##SuffixName r; \
    r.tag = PgyResultOk; \
    r.ok = value; \
    return r; \
} \
\
static inline PgyResult_##SuffixName \
pgy_result_err_##SuffixName(ErrType err) \
{ \
    PgyResult_##SuffixName r; \
    r.tag = PgyResultErr; \
    r.err = err; \
    return r; \
} \
\
static inline bool \
pgy_result_is_ok_##SuffixName(PgyResult_##SuffixName* r) \
{ \
    return r->tag == PgyResultOk; \
} \
\
static inline CType \
pgy_result_unwrap_##SuffixName(PgyResult_##SuffixName* r) \
{ \
    if (r->tag != PgyResultOk) { \
        PGY_PANIC("Result unwrap on Err value"); \
    } \
    return r->ok; \
} \
\
static inline ErrType \
pgy_result_unwrap_err_##SuffixName(PgyResult_##SuffixName* r) \
{ \
    if (r->tag != PgyResultErr) { \
        PGY_PANIC("Result unwrap_err on Ok value"); \
    } \
    return r->err; \
}

/* Result types for common error types */
typedef const char* PgyError;

PGY_RESULT_DEFINE(Int, int32_t, PgyError)
PGY_RESULT_DEFINE(Bool, bool, PgyError)
PGY_RESULT_DEFINE(String, char*, PgyError)

/* Convenience wrappers for Pergyra language syntax:
 *   Ok(val), Err(msg), IsOk(r), IsErr(r), Unwrap(r), UnwrapOr(r, fallback) */
#define Ok_Int(v)           pgy_result_ok_Int(v)
#define Err_Int(m)          pgy_result_err_Int(m)
#define IsOk_Int(r)         ((r).tag == PgyResultOk)
#define IsErr_Int(r)        ((r).tag == PgyResultErr)
#define Unwrap_Int(r)       pgy_result_unwrap_Int(&(PgyResult_Int){(r).tag, {.ok=(r).ok}})
#define UnwrapOr_Int(r, f)  ((r).tag == PgyResultOk ? (r).ok : (f))

#define Ok_Bool(v)          pgy_result_ok_Bool(v)
#define Err_Bool(m)         pgy_result_err_Bool(m)
#define IsOk_Bool(r)        ((r).tag == PgyResultOk)
#define IsErr_Bool(r)       ((r).tag == PgyResultErr)
#define Unwrap_Bool(r)      pgy_result_unwrap_Bool(&(PgyResult_Bool){(r).tag, {.ok=(r).ok}})
#define UnwrapOr_Bool(r, f) ((r).tag == PgyResultOk ? (r).ok : (f))

/* Result helper macros (similar to Rust's ? operator)
 * ResultType: the concrete result struct type (e.g. PgyResult_Int)
 */
#define PGY_RESULT_TRY(ResultType, result_expr, ok_var, err_handler) \
    do { \
        ResultType pgy__try_tmp_ = (result_expr); \
        if (pgy__try_tmp_.tag != PgyResultOk) { \
            err_handler(pgy__try_tmp_.err); \
        } \
        (ok_var) = pgy__try_tmp_.ok; \
    } while (0)

/* =================================================================
 * Option Type (Nullable Values)
 * ================================================================= */

typedef enum {
    PgyOptionSome,
    PgyOptionNone
} PgyOptionTag;

#define PGY_OPTION_DEFINE(SuffixName, CType) \
\
typedef struct { \
    PgyOptionTag tag; \
    CType value; \
} PgyOption_##SuffixName; \
\
static inline PgyOption_##SuffixName \
pgy_option_some_##SuffixName(CType value) \
{ \
    PgyOption_##SuffixName o; \
    o.tag = PgyOptionSome; \
    o.value = value; \
    return o; \
} \
\
static inline PgyOption_##SuffixName \
pgy_option_none_##SuffixName(void) \
{ \
    PgyOption_##SuffixName o; \
    o.tag = PgyOptionNone; \
    return o; \
} \
\
static inline bool \
pgy_option_is_some_##SuffixName(PgyOption_##SuffixName* o) \
{ \
    return o->tag == PgyOptionSome; \
} \
\
static inline CType \
pgy_option_unwrap_##SuffixName(PgyOption_##SuffixName* o) \
{ \
    if (o->tag != PgyOptionSome) { \
        PGY_PANIC("Option unwrap on None value"); \
    } \
    return o->value; \
}

PGY_OPTION_DEFINE(Int, int32_t)
PGY_OPTION_DEFINE(String, char*)

/* =================================================================
 * Channel (thread-safe bounded ring buffer)
 *
 * Blocking send/recv with mutex + condvar.
 * Send blocks when full; recv blocks when empty.
 *
 * Usage:
 *   PGY_CHANNEL_DEFINE(Int, int32_t)
 *   PgyChannel_Int ch; pgy_channel_init_Int(&ch, 16);
 *   pgy_channel_send_Int(&ch, 42);    // blocks if full
 *   int32_t v = pgy_channel_recv_Int(&ch);  // blocks if empty
 *   pgy_channel_close_Int(&ch);
 *   pgy_channel_destroy_Int(&ch);
 * ================================================================= */

#define PGY_CHANNEL_DEFINE(SuffixName, CType) \
typedef struct \
{ \
    CType           *buf; \
    size_t           cap; \
    size_t           head; \
    size_t           tail; \
    size_t           count; \
    bool             closed; \
    pthread_mutex_t  mutex; \
    pthread_cond_t   cond_not_full; \
    pthread_cond_t   cond_not_empty; \
} PgyChannel_##SuffixName; \
\
static inline void \
pgy_channel_init_##SuffixName(PgyChannel_##SuffixName *ch, size_t capacity) \
{ \
    ch->buf    = (CType *)calloc(capacity, sizeof(CType)); \
    ch->cap    = capacity; \
    ch->head   = 0; \
    ch->tail   = 0; \
    ch->count  = 0; \
    ch->closed = false; \
    pthread_mutex_init(&ch->mutex, NULL); \
    pthread_cond_init(&ch->cond_not_full, NULL); \
    pthread_cond_init(&ch->cond_not_empty, NULL); \
} \
\
static inline void \
pgy_channel_close_##SuffixName(PgyChannel_##SuffixName *ch) \
{ \
    pthread_mutex_lock(&ch->mutex); \
    ch->closed = true; \
    pthread_cond_broadcast(&ch->cond_not_full); \
    pthread_cond_broadcast(&ch->cond_not_empty); \
    pthread_mutex_unlock(&ch->mutex); \
} \
\
static inline void \
pgy_channel_destroy_##SuffixName(PgyChannel_##SuffixName *ch) \
{ \
    pthread_mutex_destroy(&ch->mutex); \
    pthread_cond_destroy(&ch->cond_not_full); \
    pthread_cond_destroy(&ch->cond_not_empty); \
    free(ch->buf); \
    ch->buf = NULL; \
} \
\
/* Blocking send. Returns false if channel closed. */ \
static inline bool \
pgy_channel_send_##SuffixName(PgyChannel_##SuffixName *ch, CType value) \
{ \
    pthread_mutex_lock(&ch->mutex); \
    while (ch->count >= ch->cap && !ch->closed) \
        pthread_cond_wait(&ch->cond_not_full, &ch->mutex); \
    if (ch->closed) { \
        pthread_mutex_unlock(&ch->mutex); \
        return false; \
    } \
    ch->buf[ch->tail] = value; \
    ch->tail = (ch->tail + 1) % ch->cap; \
    ch->count++; \
    pthread_cond_signal(&ch->cond_not_empty); \
    pthread_mutex_unlock(&ch->mutex); \
    return true; \
} \
\
/* Blocking recv. Returns false if channel closed and empty. */ \
static inline bool \
pgy_channel_recv_##SuffixName(PgyChannel_##SuffixName *ch, CType *out) \
{ \
    pthread_mutex_lock(&ch->mutex); \
    while (ch->count == 0 && !ch->closed) \
        pthread_cond_wait(&ch->cond_not_empty, &ch->mutex); \
    if (ch->count == 0 && ch->closed) { \
        pthread_mutex_unlock(&ch->mutex); \
        return false; \
    } \
    *out = ch->buf[ch->head]; \
    ch->head = (ch->head + 1) % ch->cap; \
    ch->count--; \
    pthread_cond_signal(&ch->cond_not_full); \
    pthread_mutex_unlock(&ch->mutex); \
    return true; \
} \
\
/* Non-blocking try_recv. Returns false if empty. */ \
static inline bool \
pgy_channel_try_recv_##SuffixName(PgyChannel_##SuffixName *ch, CType *out) \
{ \
    pthread_mutex_lock(&ch->mutex); \
    if (ch->count == 0) { \
        pthread_mutex_unlock(&ch->mutex); \
        return false; \
    } \
    *out = ch->buf[ch->head]; \
    ch->head = (ch->head + 1) % ch->cap; \
    ch->count--; \
    pthread_cond_signal(&ch->cond_not_full); \
    pthread_mutex_unlock(&ch->mutex); \
    return true; \
} \
\
/* Non-blocking readiness check */ \
static inline bool \
pgy_channel_ready_##SuffixName(PgyChannel_##SuffixName *ch) \
{ \
    pthread_mutex_lock(&ch->mutex); \
    bool ready = ch->count > 0; \
    pthread_mutex_unlock(&ch->mutex); \
    return ready; \
} \
\
/* Blocking recv that returns the value (convenience wrapper). \
 * Returns zero-initialized value if channel closed and empty. */ \
static inline CType \
pgy_channel_recv_val_##SuffixName(PgyChannel_##SuffixName *ch) \
{ \
    CType out; \
    memset(&out, 0, sizeof(CType)); \
    pgy_channel_recv_##SuffixName(ch, &out); \
    return out; \
}

PGY_CHANNEL_DEFINE(Int, int32_t)
PGY_CHANNEL_DEFINE(String, char*)

/* =================================================================
 * I/O Built-ins (platform-independent via C stdio)
 *
 * File handles use an internal table mapping Int fd → FILE*.
 * fd 0/1/2 are reserved for stdin/stdout/stderr.
 * ================================================================= */

#define PGY_MAX_OPEN_FILES 256

static FILE *_pgy_ftable[PGY_MAX_OPEN_FILES];
static int   _pgy_ftable_next = 3; /* 0=stdin,1=stdout,2=stderr */

static inline void
_pgy_io_init(void)
{
    _pgy_ftable[0] = stdin;
    _pgy_ftable[1] = stdout;
    _pgy_ftable[2] = stderr;
}

/* FileOpen(path, mode) → fd (-1 on error) */
static inline int32_t
pgy_file_open(const char *path, const char *mode)
{
    if (_pgy_ftable[0] == NULL) _pgy_io_init();
    FILE *fp = fopen(path, mode);
    if (fp == NULL) return -1;
    if (_pgy_ftable_next >= PGY_MAX_OPEN_FILES) { fclose(fp); return -1; }
    int fd = _pgy_ftable_next++;
    _pgy_ftable[fd] = fp;
    return (int32_t)fd;
}

/* FileRead(fd) → read one line (heap-allocated copy) */
static inline char *
pgy_file_read(int32_t fd)
{
    char tmp[4096];
    tmp[0] = '\0';
    if (fd < 0 || fd >= PGY_MAX_OPEN_FILES || _pgy_ftable[fd] == NULL)
        return strdup("");
    if (fgets(tmp, sizeof(tmp), _pgy_ftable[fd]) == NULL)
        return strdup("");
    size_t len = strlen(tmp);
    if (len > 0 && tmp[len - 1] == '\n')
        tmp[len - 1] = '\0';
    return strdup(tmp);
}

/* FileWrite(fd, data) */
static inline void
pgy_file_write(int32_t fd, const char *data)
{
    if (fd < 0 || fd >= PGY_MAX_OPEN_FILES || _pgy_ftable[fd] == NULL) return;
    if (data != NULL)
        fwrite(data, 1, strlen(data), _pgy_ftable[fd]);
}

/* FileClose(fd) */
static inline void
pgy_file_close(int32_t fd)
{
    if (fd < 3 || fd >= PGY_MAX_OPEN_FILES || _pgy_ftable[fd] == NULL) return;
    fclose(_pgy_ftable[fd]);
    _pgy_ftable[fd] = NULL;
}

/* ReadFile(path) → entire file as heap-allocated string */
static inline char *
pgy_read_file(const char *path)
{
    FILE *fp = fopen(path, "rb");
    if (fp == NULL) return "";
    fseek(fp, 0, SEEK_END);
    long len = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    if (len < 0) { fclose(fp); return ""; }
    char *buf = (char *)malloc((size_t)len + 1);
    if (buf == NULL) { fclose(fp); return ""; }
    fread(buf, 1, (size_t)len, fp);
    buf[len] = '\0';
    fclose(fp);
    return buf;
}

/* WriteFile(path, data) → write entire string to file */
static inline void
pgy_write_file(const char *path, const char *data)
{
    FILE *fp = fopen(path, "wb");
    if (fp == NULL) return;
    if (data != NULL)
        fwrite(data, 1, strlen(data), fp);
    fclose(fp);
}

/* Input(prompt) → read line from stdin */
static inline char *
pgy_input(const char *prompt)
{
    if (prompt != NULL && prompt[0] != '\0')
        printf("%s", prompt);
    fflush(stdout);
    static char _pgy_input_buf[4096];
    _pgy_input_buf[0] = '\0';
    if (fgets(_pgy_input_buf, sizeof(_pgy_input_buf), stdin) == NULL)
        return _pgy_input_buf;
    size_t len = strlen(_pgy_input_buf);
    if (len > 0 && _pgy_input_buf[len - 1] == '\n')
        _pgy_input_buf[len - 1] = '\0';
    return _pgy_input_buf;
}

/* Print(msg) → stdout without newline */
static inline void
pgy_print(const char *msg)
{
    if (msg != NULL) printf("%s", msg);
    fflush(stdout);
}

#endif /* PGY_RUNTIME_H */
