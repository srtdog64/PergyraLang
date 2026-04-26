
/* =================================================================
 * Stack Memory (Default ??Zero Overhead)
 *
 * All plain variables are stack-allocated by default in C.
 * No special macros needed.
 * ================================================================= */

/* Example:
 *   let x: Int = 42;     ??  int32_t x = 42;
 *   let v: Vec3;         ??  Vec3 v;
 */

/* =================================================================
 * Heap Memory ??Box<T> (Owned Heap Allocation)
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
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_OOM, \
                          PGY_RUNTIME_PANIC_REASON_ALLOCATION_FAILED); \
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
 * Heap Memory ??Arena Allocator (Frame-based)
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
    if (arena.buffer == NULL && capacity > 0) {
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_OOM,
                          PGY_RUNTIME_PANIC_REASON_ALLOCATION_FAILED);
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
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_OOM,
                          PGY_RUNTIME_PANIC_REASON_ARENA_OUT_OF_MEMORY);
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
            PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_OOM,
                              PGY_RUNTIME_PANIC_REASON_POOL_OUT_OF_MEMORY);
        void *ptr = alloc->pool->buffer + offset;
        alloc->pool->offset = offset + size;
        pgy_allocator_record_alloc(alloc, size);
        if (alloc->debug_enabled)
            memset(ptr, 0xCD, size);
        return ptr;
    }

    void *ptr = malloc(size);
    if (ptr == NULL && size > 0)
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_OOM,
                          PGY_RUNTIME_PANIC_REASON_ALLOCATION_FAILED);
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
    if (capacity > 0 && capacity > SIZE_MAX / sizeof(CType)) \
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_OOM, \
                          PGY_RUNTIME_PANIC_REASON_ALLOCATION_FAILED); \
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
    if (new_capacity > SIZE_MAX / sizeof(CType)) \
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_OOM, \
                          PGY_RUNTIME_PANIC_REASON_ALLOCATION_FAILED); \
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
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_OUT_OF_BOUNDS, \
                          PGY_RUNTIME_PANIC_REASON_ARRAY_INDEX_OUT_OF_BOUNDS); \
    return arr->data[index]; \
} \
\
static inline void \
pgy_array_set_##SuffixName(PgyArray_##SuffixName *arr, size_t index, CType value) \
{ \
    if (index >= arr->length) \
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_OUT_OF_BOUNDS, \
                          PGY_RUNTIME_PANIC_REASON_ARRAY_INDEX_OUT_OF_BOUNDS); \
    arr->data[index] = value; \
} \
\
static inline CType \
pgy_slice_get_##SuffixName(PgySlice_##SuffixName *slice, size_t index) \
{ \
    if (slice == NULL) \
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_INTERNAL_INVARIANT, \
                          "slice get on null slice"); \
    if (index >= slice->length) \
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_OUT_OF_BOUNDS, \
                          PGY_RUNTIME_PANIC_REASON_SLICE_OUT_OF_BOUNDS); \
    return slice->data[index]; \
} \
\
static inline PgySlice_##SuffixName \
pgy_array_slice_##SuffixName(PgyArray_##SuffixName *arr, size_t start, size_t len) \
{ \
    PgySlice_##SuffixName slice; \
    if (start + len > arr->length) \
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_OUT_OF_BOUNDS, \
                          PGY_RUNTIME_PANIC_REASON_SLICE_OUT_OF_BOUNDS); \
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
__attribute__((unused)) \
pgy_claim_##SuffixName(void) \
{ \
    PgySlot_##SuffixName s; \
    memset(&s, 0, sizeof(s)); \
    s.occupied = true; \
    return s; \
} \
\
static inline void \
__attribute__((unused)) \
pgy_write_##SuffixName(PgySlot_##SuffixName* s, CType v) \
{ \
    if (s == NULL) \
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_INTERNAL_INVARIANT, \
                          "null slot write"); \
    if (!s->occupied) \
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_RELEASED_SLOT, \
                          PGY_RUNTIME_PANIC_REASON_RELEASED_SLOT_WRITE); \
    s->value = v; \
} \
\
static inline CType \
__attribute__((unused)) \
pgy_read_##SuffixName(PgySlot_##SuffixName* s) \
{ \
    if (s == NULL) \
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_INTERNAL_INVARIANT, \
                          "null slot read"); \
    if (!s->occupied) \
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_RELEASED_SLOT, \
                          PGY_RUNTIME_PANIC_REASON_RELEASED_SLOT_READ); \
    return s->value; \
} \
\
static inline void \
__attribute__((unused)) \
pgy_release_##SuffixName(PgySlot_##SuffixName* s) \
{ \
    if (s == NULL) \
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_INTERNAL_INVARIANT, \
                          "null slot release"); \
    if (!s->occupied) \
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_DOUBLE_RELEASE, \
                          PGY_RUNTIME_PANIC_REASON_DOUBLE_RELEASE_SLOT); \
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
__attribute__((unused)) \
pgy_claim_##SuffixName(void) \
{ \
    PgySlot_##SuffixName s; \
    memset(&s, 0, sizeof(s)); \
    return s; \
} \
\
static inline void \
__attribute__((unused)) \
pgy_write_##SuffixName(PgySlot_##SuffixName* s, CType v) \
{ \
    s->value = v; \
} \
\
static inline CType \
__attribute__((unused)) \
pgy_read_##SuffixName(PgySlot_##SuffixName* s) \
{ \
    return s->value; \
} \
\
static inline void \
__attribute__((unused)) \
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
