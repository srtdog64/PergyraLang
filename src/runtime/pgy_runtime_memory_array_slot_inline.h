
/* =================================================================
 * Stack Memory (Default - Zero Overhead)
 *
 * All plain variables are stack-allocated by default in C.
 * No special macros needed.
 * ================================================================= */

/* Example:
 *   let x: Int = 42;     ->  int32_t x = 42;
 *   let v: Vec3;         ->  Vec3 v;
 */

/* =================================================================
 * Heap Memory - Box<T> (Owned Heap Allocation)
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
 * Heap Memory - Arena Allocator (Frame-based)
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
    if (arena == NULL)
        return;
    if (arena->buffer != NULL) {
        free(arena->buffer);
        arena->buffer = NULL;
    }
    arena->capacity = 0;
    arena->offset = 0;
}

static inline void*
pgy_arena_alloc(PgyArena* arena, size_t size, size_t align)
{
    size_t align_mask;
    size_t aligned_offset;

    if (arena == NULL || align == 0 || (align & (align - 1)) != 0) {
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_INTERNAL_INVARIANT,
                          "invalid arena or alignment");
    }

    align_mask = align - 1;
    if (arena->offset > SIZE_MAX - align_mask) {
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_OOM,
                          PGY_RUNTIME_PANIC_REASON_ARENA_OUT_OF_MEMORY);
    }

    aligned_offset = (arena->offset + align_mask) & ~align_mask;
    if (aligned_offset > arena->capacity ||
        size > arena->capacity - aligned_offset) {
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_OOM,
                          PGY_RUNTIME_PANIC_REASON_ARENA_OUT_OF_MEMORY);
    }
    if (arena->buffer == NULL && size > 0) {
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_INTERNAL_INVARIANT,
                          "arena has no backing buffer");
    }

    void* ptr = size == 0 ? NULL : arena->buffer + aligned_offset;
    arena->offset = aligned_offset + size;
    return ptr;
}

static inline void*
pgy_arena_alloc_array(PgyArena* arena, size_t elem_size, size_t align,
                      size_t count)
{
    if (count > 0 && elem_size > SIZE_MAX / count) {
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_OOM,
                          PGY_RUNTIME_PANIC_REASON_ARENA_OUT_OF_MEMORY);
    }
    return pgy_arena_alloc(arena, elem_size * count, align);
}

#define PGY_ARENA_ALLOC(arena, Type) \
    ((Type*)pgy_arena_alloc((arena), sizeof(Type), _Alignof(Type)))

#define PGY_ARENA_ALLOC_ARRAY(arena, Type, count) \
    ((Type*)pgy_arena_alloc_array((arena), sizeof(Type), _Alignof(Type), (count)))

static inline void
pgy_arena_reset(PgyArena* arena)
{
    if (arena == NULL)
        return;
    arena->offset = 0;
}

#include "pgy_runtime_allocator_inline.h"

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
        size_t next; \
        if (arr->capacity == 0) { \
            next = 4; \
        } else { \
            if (arr->capacity > SIZE_MAX / 2) \
                PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_OOM, \
                                  PGY_RUNTIME_PANIC_REASON_ALLOCATION_FAILED); \
            next = arr->capacity * 2; \
        } \
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
static inline void \
pgy_array_pop_##SuffixName(PgyArray_##SuffixName *arr) \
{ \
    if (arr == NULL) \
        return; \
    if (arr->length > 0) \
        arr->length--; \
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
    if (arr == NULL) \
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_INTERNAL_INVARIANT, \
                          "slice on null array"); \
    if (start > arr->length || len > arr->length - start) \
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_OUT_OF_BOUNDS, \
                          PGY_RUNTIME_PANIC_REASON_SLICE_OUT_OF_BOUNDS); \
    if (len > 0 && arr->data == NULL) \
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_INTERNAL_INVARIANT, \
                          "slice on array without backing storage"); \
    slice.data = len == 0 ? NULL : arr->data + start; \
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
    size_t bytes; \
    if (capacity > (SIZE_MAX - sizeof(PgyBoxArrayStorage_##SuffixName)) / sizeof(CType)) \
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_OOM, \
                          PGY_RUNTIME_PANIC_REASON_ALLOCATION_FAILED); \
    bytes = sizeof(PgyBoxArrayStorage_##SuffixName) + sizeof(CType) * capacity; \
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
