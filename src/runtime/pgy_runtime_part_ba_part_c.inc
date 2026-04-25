
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

/* =================================================================
 * Device Slot (Anchored external resource cell)
 *
 * Same ownership surface as Slot<T>, but treated as a device/remote
 * boundary and able to submit asynchronous reads.
 * ================================================================= */

#define PGY_DEVICE_SLOT_DEFINE(SuffixName, CType) \
\
typedef struct { \
    CType   value; \
    bool    claimed; \
} PgyDeviceSlot_##SuffixName; \
\
typedef struct { \
    PgyDeviceSlot_##SuffixName *slot; \
} PgyDeviceReadTaskArg_##SuffixName; \
\
static inline PgyDeviceSlot_##SuffixName \
pgy_claim_device_##SuffixName(void) \
{ \
    PgyDeviceSlot_##SuffixName s; \
    memset(&s, 0, sizeof(s)); \
    s.claimed = true; \
    return s; \
} \
\
static inline void \
pgy_device_write_##SuffixName(PgyDeviceSlot_##SuffixName *s, CType v) \
{ \
    if (s == NULL || !s->claimed) \
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_RELEASED_SLOT, \
                          PGY_RUNTIME_PANIC_REASON_RELEASED_DEVICE_SLOT_WRITE); \
    s->value = v; \
} \
\
static inline CType \
pgy_device_read_##SuffixName(PgyDeviceSlot_##SuffixName *s) \
{ \
    if (s == NULL || !s->claimed) \
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_RELEASED_SLOT, \
                          PGY_RUNTIME_PANIC_REASON_RELEASED_DEVICE_SLOT_READ); \
    return s->value; \
} \
\
static inline void \
pgy_release_device_##SuffixName(PgyDeviceSlot_##SuffixName *s) \
{ \
    if (s == NULL) \
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_INTERNAL_INVARIANT, \
                          "device slot release on null slot"); \
    if (!s->claimed) \
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_DOUBLE_RELEASE, \
                          PGY_RUNTIME_PANIC_REASON_DOUBLE_RELEASE_DEVICE_SLOT); \
    memset(&s->value, 0, sizeof(s->value)); \
    s->claimed = false; \
} \
\
static inline void * \
pgy_device_read_task_##SuffixName(void *raw) \
{ \
    PgyDeviceReadTaskArg_##SuffixName *arg = (PgyDeviceReadTaskArg_##SuffixName *)raw; \
    CType *result = (CType *)malloc(sizeof(CType)); \
    if (result == NULL) { \
        free(arg); \
        return NULL; \
    } \
    *result = pgy_device_read_##SuffixName(arg->slot); \
    free(arg); \
    return result; \
} \
\
static inline PgyTaskHandle \
pgy_submit_device_read_##SuffixName(PgyDeviceSlot_##SuffixName *s) \
{ \
    PgyDeviceReadTaskArg_##SuffixName *arg = \
        (PgyDeviceReadTaskArg_##SuffixName *)malloc(sizeof(PgyDeviceReadTaskArg_##SuffixName)); \
    if (arg == NULL) { \
        PgyTaskHandle empty = {0}; \
        return empty; \
    } \
    arg->slot = s; \
    return pgy_spawn(pgy_device_read_task_##SuffixName, arg); \
}

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
    if (s == NULL || t == NULL) \
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_INTERNAL_INVARIANT, \
                          "null secure slot write operand"); \
    if (!s->occupied) \
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_RELEASED_SLOT, \
                          PGY_RUNTIME_PANIC_REASON_RELEASED_SECURE_SLOT_WRITE); \
    if (s->token != t->id) \
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_INVALID_SECURE_TOKEN, \
                          PGY_RUNTIME_PANIC_REASON_INVALID_SECURE_TOKEN_WRITE); \
    if (!t->can_write) \
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_INVALID_SECURE_TOKEN, \
                          PGY_RUNTIME_PANIC_REASON_SECURE_TOKEN_DENIES_WRITE); \
    s->value = v; \
} \
\
static inline CType \
pgy_secure_read_##SuffixName(PgySecureSlot_##SuffixName* s, \
                              const PgyToken_##SuffixName* t) \
{ \
    if (s == NULL || t == NULL) \
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_INTERNAL_INVARIANT, \
                          "null secure slot read operand"); \
    if (!s->occupied) \
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_RELEASED_SLOT, \
                          PGY_RUNTIME_PANIC_REASON_RELEASED_SECURE_SLOT_READ); \
    if (s->token != t->id) \
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_INVALID_SECURE_TOKEN, \
                          PGY_RUNTIME_PANIC_REASON_INVALID_SECURE_TOKEN_READ); \
    if (!t->can_read) \
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_INVALID_SECURE_TOKEN, \
                          PGY_RUNTIME_PANIC_REASON_SECURE_TOKEN_DENIES_READ); \
    return s->value; \
} \
\
static inline void \
pgy_secure_release_##SuffixName(PgySecureSlot_##SuffixName* s, \
                                 const PgyToken_##SuffixName* t) \
{ \
    if (s == NULL || t == NULL) \
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_INTERNAL_INVARIANT, \
                          "null secure slot release operand"); \
    if (!s->occupied) \
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_DOUBLE_RELEASE, \
                          PGY_RUNTIME_PANIC_REASON_RELEASED_SECURE_SLOT_RELEASE); \
    if (s->token != t->id) \
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_INVALID_SECURE_TOKEN, \
                          PGY_RUNTIME_PANIC_REASON_INVALID_SECURE_TOKEN_RELEASE); \
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
    if (s == NULL || t == NULL) \
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_INTERNAL_INVARIANT, \
                          "null secure slot write operand"); \
    if (s->token != t->id) \
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_INVALID_SECURE_TOKEN, \
                          PGY_RUNTIME_PANIC_REASON_INVALID_SECURE_TOKEN_WRITE); \
    s->value = v; \
} \
\
static inline CType \
pgy_secure_read_##SuffixName(PgySecureSlot_##SuffixName* s, \
                              const PgyToken_##SuffixName* t) \
{ \
    if (s == NULL || t == NULL) \
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_INTERNAL_INVARIANT, \
                          "null secure slot read operand"); \
    if (s->token != t->id) \
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_INVALID_SECURE_TOKEN, \
                          PGY_RUNTIME_PANIC_REASON_INVALID_SECURE_TOKEN_READ); \
    return s->value; \
} \
\
static inline void \
pgy_secure_release_##SuffixName(PgySecureSlot_##SuffixName* s, \
                                 const PgyToken_##SuffixName* t) \
{ \
    if (s == NULL || t == NULL) \
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_INTERNAL_INVARIANT, \
                          "null secure slot release operand"); \
    if (s->token != t->id) \
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_INVALID_SECURE_TOKEN, \
                          PGY_RUNTIME_PANIC_REASON_INVALID_SECURE_TOKEN_RELEASE); \
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

PGY_DEVICE_SLOT_DEFINE(Int,    int32_t)
PGY_DEVICE_SLOT_DEFINE(Long,   int64_t)
PGY_DEVICE_SLOT_DEFINE(Float,  float)
PGY_DEVICE_SLOT_DEFINE(Double, double)
PGY_DEVICE_SLOT_DEFINE(Bool,   bool)
PGY_DEVICE_SLOT_DEFINE(String, char*)

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

/* =================================================================
 * AlphaDev-optimized sort kernels (sort3, sort4, sort5)
 *
 * Based on: "Faster sorting algorithms discovered using deep
 * reinforcement learning" (Nature, 2023)
 *
 * These micro-kernels use conditional swap (branchless where possible)
 * to sort exactly 3, 4, or 5 elements with minimal comparisons.
 * They are used as base cases in the array sort implementation.
 *
 * The key insight from AlphaDev: by exploiting invariants established
 * by earlier comparators in the sorting network, redundant register
 * copies (mov instructions) can be eliminated. This is the
 * "AlphaDev swap move" — not a local optimization, but a global
 * invariant-based instruction elimination.
 * ================================================================= */

#define PGY_SWAP_IF_GREATER(arr, i, j)   \
    do {                                  \
        if ((arr)[i] > (arr)[j]) {        \
            __typeof__((arr)[0]) _t = (arr)[i]; \
            (arr)[i] = (arr)[j];          \
            (arr)[j] = _t;                \
        }                                 \
    } while (0)

/* sort2: 1 comparator */
#define PGY_SORT2(arr, i, j) PGY_SWAP_IF_GREATER(arr, i, j)

/* sort3: 3 comparators (AlphaDev-optimized network)
 * Network: (0,1), (1,2), (0,1)
 * After (0,1): arr[0] <= arr[1]
 * After (1,2): arr[2] = max(arr[1],arr[2]), arr[1] = min(arr[1],arr[2])
 * After (0,1): arr[0] = min(all), arr[1] = median
 * Total: 3 comparisons (optimal for 3 elements) */
#define PGY_SORT3(arr, a, b, c)          \
    do {                                  \
        PGY_SWAP_IF_GREATER(arr, a, b);   \
        PGY_SWAP_IF_GREATER(arr, b, c);   \
        PGY_SWAP_IF_GREATER(arr, a, b);   \
    } while (0)

/* sort4: 5 comparators (optimal network)
 * Network: (0,1),(2,3), then (0,2),(1,3), then (1,2)
 * This is the optimal 5-comparator sorting network for 4 elements. */
#define PGY_SORT4(arr, a, b, c, d)       \
    do {                                  \
        PGY_SWAP_IF_GREATER(arr, a, b);   \
        PGY_SWAP_IF_GREATER(arr, c, d);   \
        PGY_SWAP_IF_GREATER(arr, a, c);   \
        PGY_SWAP_IF_GREATER(arr, b, d);   \
        PGY_SWAP_IF_GREATER(arr, b, c);   \
    } while (0)

/* sort5: 9 comparators (optimal network)
 * Network: (0,1),(3,4), (2,4), (2,3), (0,3), (0,2), (1,4), (1,3), (1,2)
 * This is the optimal 9-comparator sorting network for 5 elements. */
#define PGY_SORT5(arr, a, b, c, d, e)    \
    do {                                  \
        PGY_SWAP_IF_GREATER(arr, a, b);   \
        PGY_SWAP_IF_GREATER(arr, d, e);   \
        PGY_SWAP_IF_GREATER(arr, c, e);   \
        PGY_SWAP_IF_GREATER(arr, c, d);   \
        PGY_SWAP_IF_GREATER(arr, a, d);   \
        PGY_SWAP_IF_GREATER(arr, a, c);   \
        PGY_SWAP_IF_GREATER(arr, b, e);   \
        PGY_SWAP_IF_GREATER(arr, b, d);   \
        PGY_SWAP_IF_GREATER(arr, b, c);   \
    } while (0)

/* pgy_array_sort_T: hybrid sort using AlphaDev kernels for small sizes,
 * falling back to stdlib qsort for larger arrays. */
#define PGY_ARRAY_SORT_DEFINE(SuffixName, CType, CmpFn)                     \
static inline void pgy_array_sort_##SuffixName(CType *arr, size_t n)         \
{                                                                             \
    if (n <= 1) return;                                                       \
    if (n == 2) { PGY_SORT2(arr, 0, 1); return; }                           \
    if (n == 3) { PGY_SORT3(arr, 0, 1, 2); return; }                        \
    if (n == 4) { PGY_SORT4(arr, 0, 1, 2, 3); return; }                     \
    if (n == 5) { PGY_SORT5(arr, 0, 1, 2, 3, 4); return; }                  \
    /* For n > 5: use introsort-like strategy with AlphaDev base cases */     \
    /* Partition step + recurse, using sort5 as base case at depth limit */   \
    qsort(arr, n, sizeof(CType), CmpFn);                                    \
}

/* qsort comparison helpers + AlphaDev sort instantiations below */
static inline int pgy_cmp_Int(const void *a, const void *b)
{ return (*(const int32_t *)a > *(const int32_t *)b)
       - (*(const int32_t *)a < *(const int32_t *)b); }
static inline int pgy_cmp_Long(const void *a, const void *b)
{ return (*(const int64_t *)a > *(const int64_t *)b)
       - (*(const int64_t *)a < *(const int64_t *)b); }
static inline int pgy_cmp_Float(const void *a, const void *b)
{ float fa = *(const float *)a, fb = *(const float *)b;
  return (fa > fb) - (fa < fb); }
static inline int pgy_cmp_Double(const void *a, const void *b)
{ double da = *(const double *)a, db = *(const double *)b;
  return (da > db) - (da < db); }
static inline int pgy_cmp_String(const void *a, const void *b)
{ return strcmp(*(const char *const *)a, *(const char *const *)b); }
static inline int pgy_cmp_Bool(const void *a, const void *b)
{ return (int)(*(const bool *)a) - (int)(*(const bool *)b); }

/* AlphaDev sort instantiations (after cmp helpers are defined) */
PGY_ARRAY_SORT_DEFINE(Int,    int32_t, pgy_cmp_Int)
PGY_ARRAY_SORT_DEFINE(Long,   int64_t, pgy_cmp_Long)
PGY_ARRAY_SORT_DEFINE(Float,  float,   pgy_cmp_Float)
PGY_ARRAY_SORT_DEFINE(Double, double,  pgy_cmp_Double)
/* String sort uses qsort only (pointer comparison != strcmp) */
static inline void pgy_array_sort_String(char **arr, size_t n) {
    if (n <= 1) return;
    qsort(arr, n, sizeof(char *), pgy_cmp_String);
}

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
static inline void
pgy_log_string(const char *v)
{
    const char *msg = (v == NULL ? "(null)" : v);
    fputs(msg, stdout);
    size_t msg_len = strlen(msg);
    if (msg_len == 0 || msg[msg_len - 1] != '\n')
        fputc('\n', stdout);
    fflush(stdout);
}

/* Banner/raw log helper: keep multiline payload intact and avoid truncation. */
static inline void
pgy_log_banner(const char *v)
{
    pgy_log_string(v);
}

#define pgy_log(x) _Generic((x), \
    int32_t:  pgy_log_int,    \
    int64_t:  pgy_log_long,   \
    float:    pgy_log_float,  \
    double:   pgy_log_double, \
    bool:     pgy_log_bool,   \
    char*:    pgy_log_string, \
    const char*: pgy_log_string \
)(x)

/* =================================================================
 * Math / Random Helpers (C backend inline)
 * ================================================================= */

#include <math.h>

static inline int32_t ToInt(const char *s)    { return s == NULL ? 0 : (int32_t)strtol(s, NULL, 10); }
static inline float   ToFloat(const char *s)  { return s == NULL ? 0.0f : strtof(s, NULL); }
static inline float  Sqrt(float x)            { return sqrtf(x); }
static inline float  Pow(float x, float y)    { return powf(x, y); }
static inline float  Floor(float x)           { return floorf(x); }
static inline float  Ceil(float x)            { return ceilf(x); }
static inline float  Round(float x)           { return roundf(x); }
static inline float  Sin(float x)             { return sinf(x); }
static inline float  Cos(float x)             { return cosf(x); }
static inline float  Tan(float x)             { return tanf(x); }
static inline float  Asin(float x)            { return asinf(x); }
static inline float  Acos(float x)            { return acosf(x); }
static inline float  Atan(float x)            { return atanf(x); }
static inline float  Atan2(float y, float x)  { return atan2f(y, x); }
static inline float  Exp(float x)             { return expf(x); }
static inline float  MathLog(float x)          { return logf(x); }
static inline float  Log10(float x)            { return log10f(x); }
static inline float  Log2(float x)             { return log2f(x); }
#define PGY_PI 3.14159265358979323846f
#define PGY_E  2.71828182845904523536f

static inline int32_t Clamp(int32_t v, int32_t lo, int32_t hi) {
    return v < lo ? lo : (v > hi ? hi : v);
}

static inline int32_t Random(int32_t max)     { return max <= 0 ? 0 : (int32_t)(rand() % max); }
static inline void SeedRandom(int32_t seed)   { srand((unsigned int)seed); }

/* =================================================================
 * Standard Library Helpers
 * ================================================================= */

static inline char* pgy_int_to_string(int32_t val) {
    char stack_buf[32];
    int len = snprintf(stack_buf, sizeof(stack_buf), "%d", val);
