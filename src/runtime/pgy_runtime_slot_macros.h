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
static inline PgyRuntimeSlotStatus \
pgy_try_device_write_##SuffixName(PgyDeviceSlot_##SuffixName *s, CType v) \
{ \
    if (s == NULL) \
        return PGY_RUNTIME_SLOT_STATUS_NULL_SLOT; \
    if (!s->claimed) \
        return PGY_RUNTIME_SLOT_STATUS_RELEASED_SLOT; \
    s->value = v; \
    return PGY_RUNTIME_SLOT_STATUS_OK; \
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
static inline PgyRuntimeSlotStatus \
pgy_try_device_read_##SuffixName(PgyDeviceSlot_##SuffixName *s, CType *out) \
{ \
    if (out == NULL) \
        return PGY_RUNTIME_SLOT_STATUS_NULL_OUTPUT; \
    if (s == NULL) \
        return PGY_RUNTIME_SLOT_STATUS_NULL_SLOT; \
    if (!s->claimed) \
        return PGY_RUNTIME_SLOT_STATUS_RELEASED_SLOT; \
    *out = s->value; \
    return PGY_RUNTIME_SLOT_STATUS_OK; \
} \
\
static inline PgyRuntimeSlotResult_##SuffixName \
pgy_try_device_read_result_##SuffixName(PgyDeviceSlot_##SuffixName *s) \
{ \
    CType value; \
    PgyRuntimeSlotStatus status; \
    memset(&value, 0, sizeof(value)); \
    status = pgy_try_device_read_##SuffixName(s, &value); \
    if (status == PGY_RUNTIME_SLOT_STATUS_OK) \
        return pgy_runtime_slot_result_ok_##SuffixName(value); \
    return pgy_runtime_slot_result_err_##SuffixName( \
        pgy_runtime_slot_failure_from_status(status, \
                                            "device-slot-boundary", "read")); \
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
static inline PgyRuntimeSlotStatus \
pgy_try_release_device_##SuffixName(PgyDeviceSlot_##SuffixName *s) \
{ \
    if (s == NULL) \
        return PGY_RUNTIME_SLOT_STATUS_NULL_SLOT; \
    if (!s->claimed) \
        return PGY_RUNTIME_SLOT_STATUS_DOUBLE_RELEASE; \
    memset(&s->value, 0, sizeof(s->value)); \
    s->claimed = false; \
    return PGY_RUNTIME_SLOT_STATUS_OK; \
} \
\
static inline void * \
pgy_device_read_task_##SuffixName(void *raw) \
{ \
    PgyDeviceReadTaskArg_##SuffixName *arg = (PgyDeviceReadTaskArg_##SuffixName *)raw; \
    PgyRuntimeSlotResult_##SuffixName read_result; \
    if (arg == NULL) \
        return NULL; \
    read_result = pgy_try_device_read_result_##SuffixName(arg->slot); \
    if (read_result.tag != PGY_RUNTIME_SLOT_RESULT_OK) { \
        free(arg); \
        return NULL; \
    } \
    CType *payload = (CType *)malloc(sizeof(CType)); \
    if (payload == NULL) { \
        free(arg); \
        return NULL; \
    } \
    *payload = read_result.ok; \
    free(arg); \
    return payload; \
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
static inline PgyRuntimeSlotStatus \
pgy_try_secure_write_##SuffixName(PgySecureSlot_##SuffixName* s, \
                                  CType v, \
                                  const PgyToken_##SuffixName* t) \
{ \
    if (s == NULL) \
        return PGY_RUNTIME_SLOT_STATUS_NULL_SLOT; \
    if (t == NULL) \
        return PGY_RUNTIME_SLOT_STATUS_NULL_TOKEN; \
    if (!s->occupied) \
        return PGY_RUNTIME_SLOT_STATUS_RELEASED_SLOT; \
    if (s->token != t->id) \
        return PGY_RUNTIME_SLOT_STATUS_INVALID_TOKEN; \
    if (!t->can_write) \
        return PGY_RUNTIME_SLOT_STATUS_TOKEN_DENIES_WRITE; \
    s->value = v; \
    return PGY_RUNTIME_SLOT_STATUS_OK; \
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
static inline PgyRuntimeSlotStatus \
pgy_try_secure_read_##SuffixName(PgySecureSlot_##SuffixName* s, \
                                 const PgyToken_##SuffixName* t, \
                                 CType *out) \
{ \
    if (out == NULL) \
        return PGY_RUNTIME_SLOT_STATUS_NULL_OUTPUT; \
    if (s == NULL) \
        return PGY_RUNTIME_SLOT_STATUS_NULL_SLOT; \
    if (t == NULL) \
        return PGY_RUNTIME_SLOT_STATUS_NULL_TOKEN; \
    if (!s->occupied) \
        return PGY_RUNTIME_SLOT_STATUS_RELEASED_SLOT; \
    if (s->token != t->id) \
        return PGY_RUNTIME_SLOT_STATUS_INVALID_TOKEN; \
    if (!t->can_read) \
        return PGY_RUNTIME_SLOT_STATUS_TOKEN_DENIES_READ; \
    *out = s->value; \
    return PGY_RUNTIME_SLOT_STATUS_OK; \
} \
\
static inline PgyRuntimeSlotResult_##SuffixName \
pgy_try_secure_read_result_##SuffixName(PgySecureSlot_##SuffixName* s, \
                                        const PgyToken_##SuffixName* t) \
{ \
    CType value; \
    PgyRuntimeSlotStatus status; \
    memset(&value, 0, sizeof(value)); \
    status = pgy_try_secure_read_##SuffixName(s, t, &value); \
    if (status == PGY_RUNTIME_SLOT_STATUS_OK) \
        return pgy_runtime_slot_result_ok_##SuffixName(value); \
    return pgy_runtime_slot_result_err_##SuffixName( \
        pgy_runtime_slot_failure_from_status(status, \
                                            "secure-slot-boundary", "read")); \
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
} \
\
static inline PgyRuntimeSlotStatus \
pgy_try_secure_release_##SuffixName(PgySecureSlot_##SuffixName* s, \
                                    const PgyToken_##SuffixName* t) \
{ \
    if (s == NULL) \
        return PGY_RUNTIME_SLOT_STATUS_NULL_SLOT; \
    if (t == NULL) \
        return PGY_RUNTIME_SLOT_STATUS_NULL_TOKEN; \
    if (!s->occupied) \
        return PGY_RUNTIME_SLOT_STATUS_DOUBLE_RELEASE; \
    if (s->token != t->id) \
        return PGY_RUNTIME_SLOT_STATUS_INVALID_TOKEN; \
    s->occupied = false; \
    s->token = 0; \
    return PGY_RUNTIME_SLOT_STATUS_OK; \
} \
\
typedef struct { \
    PgySecureSlot_##SuffixName      *slot; \
    const PgyToken_##SuffixName     *token; \
    bool                            active; \
    bool                            can_write; \
} PgyPinnedSecureSlotView_##SuffixName; \
\
static inline PgyPinnedSecureSlotView_##SuffixName \
pgy_secure_pin_read_##SuffixName(PgySecureSlot_##SuffixName* s, \
                                  const PgyToken_##SuffixName* t) \
{ \
    if (s == NULL || t == NULL) \
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_INTERNAL_INVARIANT, \
                          "null secure slot pin read operand"); \
    if (!s->occupied) \
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_RELEASED_SLOT, \
                          PGY_RUNTIME_PANIC_REASON_RELEASED_SECURE_SLOT_READ); \
    if (s->token != t->id || !t->can_read) \
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_INVALID_SECURE_TOKEN, \
                          PGY_RUNTIME_PANIC_REASON_INVALID_SECURE_TOKEN_READ); \
    PgyPinnedSecureSlotView_##SuffixName view; \
    view.slot = s; \
    view.token = t; \
    view.active = true; \
    view.can_write = false; \
    return view; \
} \
\
static inline PgyPinnedSecureSlotView_##SuffixName \
pgy_secure_pin_write_##SuffixName(PgySecureSlot_##SuffixName* s, \
                                   const PgyToken_##SuffixName* t) \
{ \
    if (s == NULL || t == NULL) \
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_INTERNAL_INVARIANT, \
                          "null secure slot pin write operand"); \
    if (!s->occupied) \
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_RELEASED_SLOT, \
                          PGY_RUNTIME_PANIC_REASON_RELEASED_SECURE_SLOT_WRITE); \
    if (s->token != t->id || !t->can_write) \
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_INVALID_SECURE_TOKEN, \
                          PGY_RUNTIME_PANIC_REASON_INVALID_SECURE_TOKEN_WRITE); \
    PgyPinnedSecureSlotView_##SuffixName view; \
    view.slot = s; \
    view.token = t; \
    view.active = true; \
    view.can_write = true; \
    return view; \
} \
\
static inline void \
pgy_secure_unpin_##SuffixName(PgyPinnedSecureSlotView_##SuffixName* view) \
{ \
    if (view == NULL) \
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_INTERNAL_INVARIANT, \
                          "null secure slot unpin"); \
    if (!view->active || view->slot == NULL || view->token == NULL) \
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_INTERNAL_INVARIANT, \
                          "inactive secure slot unpin"); \
    view->active = false; \
    view->slot = NULL; \
    view->token = NULL; \
} \
\
static inline void \
pgy_secure_unpin_cleanup_##SuffixName(PgyPinnedSecureSlotView_##SuffixName* view) \
{ \
    if (view != NULL && view->active) \
        pgy_secure_unpin_##SuffixName(view); \
}

#define PGY_SECURE_SLOT_DEFINE(SuffixName, CType) \
    PGY_SECURE_SLOT_DEFINE_DEBUG(SuffixName, CType)

#endif /* PGY_RUNTIME_SLOT_MACROS_H */
