#ifndef PGY_RUNTIME_PLAIN_SLOT_INLINE_H
#define PGY_RUNTIME_PLAIN_SLOT_INLINE_H

/* =================================================================
 * Slot Memory (Checked By Default)
 *
 * Checked mode: Full safety checks (occupied flag, panic on invalid access)
 * Raw mode: Whole-program PGY_RAW_SLOTS opt-out (no occupied flag)
 * ================================================================= */

/* Checked slot with safety checks. */
#define PGY_SLOT_DEFINE_CHECKED(SuffixName, CType) \
\
typedef struct { \
    CType   value; \
    bool    occupied; \
} PgySlot_##SuffixName; \
\
PGY_RUNTIME_SLOT_RESULT_DEFINE(SuffixName, CType) \
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
} \
\
static inline PgyRuntimeSlotStatus \
__attribute__((unused)) \
pgy_try_write_##SuffixName(PgySlot_##SuffixName* s, CType v) \
{ \
    if (s == NULL) \
        return PGY_RUNTIME_SLOT_STATUS_NULL_SLOT; \
    if (!s->occupied) \
        return PGY_RUNTIME_SLOT_STATUS_RELEASED_SLOT; \
    s->value = v; \
    return PGY_RUNTIME_SLOT_STATUS_OK; \
} \
\
static inline PgyRuntimeSlotStatus \
__attribute__((unused)) \
pgy_try_read_##SuffixName(PgySlot_##SuffixName* s, CType* out) \
{ \
    if (out == NULL) \
        return PGY_RUNTIME_SLOT_STATUS_NULL_OUTPUT; \
    if (s == NULL) \
        return PGY_RUNTIME_SLOT_STATUS_NULL_SLOT; \
    if (!s->occupied) \
        return PGY_RUNTIME_SLOT_STATUS_RELEASED_SLOT; \
    *out = s->value; \
    return PGY_RUNTIME_SLOT_STATUS_OK; \
} \
\
static inline PgyRuntimeSlotResult_##SuffixName \
__attribute__((unused)) \
pgy_try_read_result_##SuffixName(PgySlot_##SuffixName* s) \
{ \
    CType value; \
    PgyRuntimeSlotStatus status; \
    memset(&value, 0, sizeof(value)); \
    status = pgy_try_read_##SuffixName(s, &value); \
    if (status == PGY_RUNTIME_SLOT_STATUS_OK) \
        return pgy_runtime_slot_result_ok_##SuffixName(value); \
    return pgy_runtime_slot_result_err_##SuffixName( \
        pgy_runtime_slot_failure_from_status(status, "slot-boundary", "read")); \
} \
\
static inline PgyRuntimeSlotStatus \
__attribute__((unused)) \
pgy_try_release_##SuffixName(PgySlot_##SuffixName* s) \
{ \
    if (s == NULL) \
        return PGY_RUNTIME_SLOT_STATUS_NULL_SLOT; \
    if (!s->occupied) \
        return PGY_RUNTIME_SLOT_STATUS_DOUBLE_RELEASE; \
    s->occupied = false; \
    return PGY_RUNTIME_SLOT_STATUS_OK; \
} \
\
typedef struct { \
    PgySlot_##SuffixName *slot; \
    bool                 active; \
    bool                 can_write; \
} PgyPinnedSlotView_##SuffixName; \
\
static inline PgyPinnedSlotView_##SuffixName \
__attribute__((unused)) \
pgy_pin_read_##SuffixName(PgySlot_##SuffixName* s) \
{ \
    if (s == NULL) \
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_INTERNAL_INVARIANT, \
                          "null slot pin read"); \
    if (!s->occupied) \
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_RELEASED_SLOT, \
                          PGY_RUNTIME_PANIC_REASON_RELEASED_SLOT_READ); \
    PgyPinnedSlotView_##SuffixName view; \
    view.slot = s; \
    view.active = true; \
    view.can_write = false; \
    return view; \
} \
\
static inline PgyPinnedSlotView_##SuffixName \
__attribute__((unused)) \
pgy_pin_write_##SuffixName(PgySlot_##SuffixName* s) \
{ \
    if (s == NULL) \
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_INTERNAL_INVARIANT, \
                          "null slot pin write"); \
    if (!s->occupied) \
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_RELEASED_SLOT, \
                          PGY_RUNTIME_PANIC_REASON_RELEASED_SLOT_WRITE); \
    PgyPinnedSlotView_##SuffixName view; \
    view.slot = s; \
    view.active = true; \
    view.can_write = true; \
    return view; \
} \
\
static inline void \
__attribute__((unused)) \
pgy_unpin_##SuffixName(PgyPinnedSlotView_##SuffixName* view) \
{ \
    if (view == NULL) \
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_INTERNAL_INVARIANT, \
                          "null slot unpin"); \
    if (!view->active || view->slot == NULL) \
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_INTERNAL_INVARIANT, \
                          "inactive slot unpin"); \
    view->active = false; \
    view->slot = NULL; \
} \
\
static inline void \
__attribute__((unused)) \
pgy_unpin_cleanup_##SuffixName(PgyPinnedSlotView_##SuffixName* view) \
{ \
    if (view != NULL && view->active) \
        pgy_unpin_##SuffixName(view); \
}

/* Raw slot opt-out: zero overhead, not the canonical beta ABI. */
#define PGY_SLOT_DEFINE_RAW(SuffixName, CType) \
\
typedef struct { \
    CType   value; \
} PgySlot_##SuffixName; \
\
PGY_RUNTIME_SLOT_RESULT_DEFINE(SuffixName, CType) \
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
    (void)s; /* no-op in raw slot mode */ \
} \
\
static inline PgyRuntimeSlotStatus \
__attribute__((unused)) \
pgy_try_write_##SuffixName(PgySlot_##SuffixName* s, CType v) \
{ \
    if (s == NULL) \
        return PGY_RUNTIME_SLOT_STATUS_NULL_SLOT; \
    s->value = v; \
    return PGY_RUNTIME_SLOT_STATUS_OK; \
} \
\
static inline PgyRuntimeSlotStatus \
__attribute__((unused)) \
pgy_try_read_##SuffixName(PgySlot_##SuffixName* s, CType* out) \
{ \
    if (out == NULL) \
        return PGY_RUNTIME_SLOT_STATUS_NULL_OUTPUT; \
    if (s == NULL) \
        return PGY_RUNTIME_SLOT_STATUS_NULL_SLOT; \
    *out = s->value; \
    return PGY_RUNTIME_SLOT_STATUS_OK; \
} \
\
static inline PgyRuntimeSlotResult_##SuffixName \
__attribute__((unused)) \
pgy_try_read_result_##SuffixName(PgySlot_##SuffixName* s) \
{ \
    CType value; \
    PgyRuntimeSlotStatus status; \
    memset(&value, 0, sizeof(value)); \
    status = pgy_try_read_##SuffixName(s, &value); \
    if (status == PGY_RUNTIME_SLOT_STATUS_OK) \
        return pgy_runtime_slot_result_ok_##SuffixName(value); \
    return pgy_runtime_slot_result_err_##SuffixName( \
        pgy_runtime_slot_failure_from_status(status, "slot-boundary", "read")); \
} \
\
static inline PgyRuntimeSlotStatus \
__attribute__((unused)) \
pgy_try_release_##SuffixName(PgySlot_##SuffixName* s) \
{ \
    if (s == NULL) \
        return PGY_RUNTIME_SLOT_STATUS_NULL_SLOT; \
    pgy_release_##SuffixName(s); \
    return PGY_RUNTIME_SLOT_STATUS_OK; \
} \
\
typedef struct { \
    PgySlot_##SuffixName *slot; \
    bool                 active; \
    bool                 can_write; \
} PgyPinnedSlotView_##SuffixName; \
\
static inline PgyPinnedSlotView_##SuffixName \
__attribute__((unused)) \
pgy_pin_read_##SuffixName(PgySlot_##SuffixName* s) \
{ \
    PgyPinnedSlotView_##SuffixName view; \
    view.slot = s; \
    view.active = true; \
    view.can_write = false; \
    return view; \
} \
\
static inline PgyPinnedSlotView_##SuffixName \
__attribute__((unused)) \
pgy_pin_write_##SuffixName(PgySlot_##SuffixName* s) \
{ \
    PgyPinnedSlotView_##SuffixName view; \
    view.slot = s; \
    view.active = true; \
    view.can_write = true; \
    return view; \
} \
\
static inline void \
__attribute__((unused)) \
pgy_unpin_##SuffixName(PgyPinnedSlotView_##SuffixName* view) \
{ \
    if (view != NULL) { \
        view->active = false; \
        view->slot = NULL; \
    } \
} \
\
static inline void \
__attribute__((unused)) \
pgy_unpin_cleanup_##SuffixName(PgyPinnedSlotView_##SuffixName* view) \
{ \
    pgy_unpin_##SuffixName(view); \
}

/* Conditional definition based on slot safety mode. */
#if PGY_WITH_SLOT_CHECKS
#  define PGY_SLOT_DEFINE(SuffixName, CType) \
       PGY_SLOT_DEFINE_CHECKED(SuffixName, CType)
#else
#  define PGY_SLOT_DEFINE(SuffixName, CType) \
       PGY_SLOT_DEFINE_RAW(SuffixName, CType)
#endif

#endif /* PGY_RUNTIME_PLAIN_SLOT_INLINE_H */
