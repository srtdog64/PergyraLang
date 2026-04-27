#ifndef PGY_RUNTIME_SLOT_MACROS_H
#define PGY_RUNTIME_SLOT_MACROS_H

/*
 * Inline Slot ABI note:
 *
 * These structs are the current C backend representation, not the language
 * identity of Slot<T>. Source semantics observe a source-level resource boundary
 * with claim/read/write/release/pin contracts; pointer/address ownership stays
 * below this layer and may be replaced by another backend handle.
 */

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
 * Always includes checks because this is a security boundary, not a
 * zero-overhead Slot<T> wrapper.
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

#define PGY_SECURE_SLOT_DEFINE(SuffixName, CType) \
    PGY_SECURE_SLOT_DEFINE_DEBUG(SuffixName, CType)

#endif /* PGY_RUNTIME_SLOT_MACROS_H */
