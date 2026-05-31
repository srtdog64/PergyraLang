
void
pgy_intent_exit_export(int32_t handle)
{
    if (handle == 0)
        return;

    pgy_intent_pop_current_handle_export(handle);
    pthread_mutex_lock(&pgy_intent_registry_mutex);

    {
        int32_t active_slot = pgy_intent_find_active_registry_slot_export(handle);
        if (active_slot < 0) {
            pthread_mutex_unlock(&pgy_intent_registry_mutex);
            return;
        }
        PgyIntentActiveEntry *entry = &pgy_intent_active_registry[active_slot];
        if (PGY_INTENT_OBSERVABILITY_ENABLED) {
            free(pgy_intent_last_trace);
            free(pgy_intent_last_failure);
            free(pgy_intent_last_name);
            for (int32_t j = 0; j < pgy_intent_last_history_count; j++) {
                free(pgy_intent_last_steps[j].name);
                free(pgy_intent_last_steps[j].zone);
                free(pgy_intent_last_steps[j].failure_reason);
                pgy_intent_last_steps[j].name = NULL;
                pgy_intent_last_steps[j].zone = NULL;
                pgy_intent_last_steps[j].failure_reason = NULL;
                pgy_intent_last_steps[j].ok = false;
            }
            pgy_intent_last_trace = entry->trace != NULL
                ? pgy_runtime_strdup_export(entry->trace) : pgy_runtime_strdup_export("");
            pgy_intent_last_failure = entry->failure_reason != NULL
                ? pgy_runtime_strdup_export(entry->failure_reason) : pgy_runtime_strdup_export("");
            pgy_intent_last_name = entry->name != NULL
                ? pgy_runtime_strdup_export(entry->name) : pgy_runtime_strdup_export("");
            pgy_intent_last_handle = entry->handle;
            pgy_intent_last_trace_id = entry->trace_id;
            pgy_intent_last_step_count = entry->step_count;
            pgy_intent_last_failed = entry->failed;
            pgy_intent_last_history_count = entry->step_count;
            if (pgy_intent_last_history_count > PGY_INTENT_ACTIVE_MAX)
                pgy_intent_last_history_count = PGY_INTENT_ACTIVE_MAX;
            for (int32_t j = 0; j < pgy_intent_last_history_count; j++) {
                pgy_intent_history_step_clear_export(&pgy_intent_last_steps[j]);
                pgy_intent_last_steps[j].name = pgy_runtime_strdup_export(
                    entry->steps[j].name != NULL ? entry->steps[j].name : "");
                pgy_intent_last_steps[j].zone = pgy_runtime_strdup_export(
                    entry->steps[j].zone != NULL ? entry->steps[j].zone : "");
                pgy_intent_last_steps[j].phase = pgy_runtime_strdup_export(
                    entry->steps[j].phase != NULL ? entry->steps[j].phase : "");
                pgy_intent_last_steps[j].participant = pgy_runtime_strdup_export(
                    entry->steps[j].participant != NULL ? entry->steps[j].participant : "");
                pgy_intent_last_steps[j].slot = pgy_runtime_strdup_export(
                    entry->steps[j].slot != NULL ? entry->steps[j].slot : "");
                pgy_intent_last_steps[j].from_zone = pgy_runtime_strdup_export(
                    entry->steps[j].from_zone != NULL ? entry->steps[j].from_zone : "");
                pgy_intent_last_steps[j].from_slot = pgy_runtime_strdup_export(
                    entry->steps[j].from_slot != NULL ? entry->steps[j].from_slot : "");
                pgy_intent_last_steps[j].to_zone = pgy_runtime_strdup_export(
                    entry->steps[j].to_zone != NULL ? entry->steps[j].to_zone : "");
                pgy_intent_last_steps[j].to_slot = pgy_runtime_strdup_export(
                    entry->steps[j].to_slot != NULL ? entry->steps[j].to_slot : "");
                pgy_intent_last_steps[j].ok = entry->steps[j].ok;
                pgy_intent_last_steps[j].failure_reason = pgy_runtime_strdup_export(
                    entry->steps[j].failure_reason != NULL ? entry->steps[j].failure_reason : "");
            }
            pgy_intent_recent_entry_copy_from_active_export(
                &pgy_intent_recent_ring[pgy_intent_recent_head], entry);
            pgy_intent_recent_head = (pgy_intent_recent_head + 1) % PGY_INTENT_RECENT_MAX;
            if (pgy_intent_recent_count < PGY_INTENT_RECENT_MAX)
                pgy_intent_recent_count++;
        } else {
            free(pgy_intent_last_trace);
            free(pgy_intent_last_failure);
            free(pgy_intent_last_name);
            pgy_intent_last_trace = NULL;
            pgy_intent_last_failure = NULL;
            pgy_intent_last_name = NULL;
            pgy_intent_last_handle = entry->handle;
            pgy_intent_last_trace_id = 0;
            pgy_intent_last_step_count = entry->step_count;
            pgy_intent_last_failed = entry->failed;
            pgy_intent_last_history_count = 0;
        }
        if (PGY_INTENT_OBSERVABILITY_ENABLED)
            free(entry->name);
        if (entry->subjects != entry->inline_subjects)
            free(entry->subjects);
        free(entry->trace);
        free(entry->failure_reason);
        if (PGY_INTENT_OBSERVABILITY_ENABLED) {
            for (int32_t j = 0; j < entry->step_count && j < PGY_INTENT_ACTIVE_MAX; j++) {
                pgy_intent_history_step_clear_export(&entry->steps[j]);
            }
        }
        entry->handle = 0;
        entry->parent_handle = 0;
        entry->name = NULL;
        entry->subjects = NULL;
        entry->subject_count = 0;
        entry->subject_fingerprint = 0;
        entry->is_concurrent = false;
        entry->priority = 0;
        entry->trace_id = 0;
        entry->trace = NULL;
        entry->trace_len = 0;
        entry->failure_reason = NULL;
        entry->step_count = 0;
        entry->failed = false;
        pgy_intent_active_index_clear_export(handle);
        entry->active = false;
        pgy_intent_note_free_active_slot_export(active_slot);
        if (pgy_intent_active_count_value > 0)
            pgy_intent_active_count_value--;
    }

    pthread_mutex_unlock(&pgy_intent_registry_mutex);
}

static struct timespec
pgy_runtime_deadline_after_ns(uint64_t timeout_ns)
{
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    ts.tv_sec += (time_t)(timeout_ns / 1000000000ull);
    ts.tv_nsec += (long)(timeout_ns % 1000000000ull);
    if (ts.tv_nsec >= 1000000000l) {
        ts.tv_sec += 1;
        ts.tv_nsec -= 1000000000l;
    }
    return ts;
}

#include "pgy_runtime_slot_status.h"

/* =================================================================
 * Slot types (must match pgy_runtime.h layout)
 * ================================================================= */

typedef struct {
    int32_t value;
    bool    claimed;
} PgySlot_Int;

typedef struct {
    int64_t value;
    bool    claimed;
} PgySlot_Long;

typedef struct {
    float   value;
    bool    claimed;
} PgySlot_Float;

typedef struct {
    double  value;
    bool    claimed;
} PgySlot_Double;

typedef struct {
    bool    value;
    bool    claimed;
} PgySlot_Bool;

typedef struct {
    char   *value;
    bool    claimed;
} PgySlot_String;

#define PGY_SLOT_TRY_EXPORT_DEFINE(SuffixName, CType, ZeroValue) \
PGY_RUNTIME_SLOT_RESULT_DEFINE(SuffixName, CType) \
\
PgyRuntimeSlotStatus pgy_try_write_##SuffixName(PgySlot_##SuffixName *s, CType v) \
{ \
    if (s == NULL) \
        return PGY_RUNTIME_SLOT_STATUS_NULL_SLOT; \
    if (!s->claimed) \
        return PGY_RUNTIME_SLOT_STATUS_RELEASED_SLOT; \
    s->value = v; \
    return PGY_RUNTIME_SLOT_STATUS_OK; \
} \
\
PgyRuntimeSlotStatus pgy_try_read_##SuffixName(PgySlot_##SuffixName *s, CType *out) \
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
PgyRuntimeSlotResult_##SuffixName pgy_try_read_result_##SuffixName(PgySlot_##SuffixName *s) \
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
PgyRuntimeSlotStatus pgy_try_release_##SuffixName(PgySlot_##SuffixName *s) \
{ \
    if (s == NULL) \
        return PGY_RUNTIME_SLOT_STATUS_NULL_SLOT; \
    if (!s->claimed) \
        return PGY_RUNTIME_SLOT_STATUS_DOUBLE_RELEASE; \
    s->value = (ZeroValue); \
    s->claimed = false; \
    return PGY_RUNTIME_SLOT_STATUS_OK; \
}

PGY_SLOT_TRY_EXPORT_DEFINE(Int, int32_t, 0)
PGY_SLOT_TRY_EXPORT_DEFINE(Long, int64_t, 0)
PGY_SLOT_TRY_EXPORT_DEFINE(Float, float, 0.0f)
PGY_SLOT_TRY_EXPORT_DEFINE(Double, double, 0.0)
PGY_SLOT_TRY_EXPORT_DEFINE(Bool, bool, false)
PGY_SLOT_TRY_EXPORT_DEFINE(String, char *, NULL)

#undef PGY_SLOT_TRY_EXPORT_DEFINE

#define PGY_INLINE_PIN_EXPORT_DEFINE(SuffixName) \
typedef struct { \
    PgySlot_##SuffixName *slot; \
    bool                 active; \
    bool                 can_write; \
} PgyPinnedSlotView_##SuffixName; \
\
PgyPinnedSlotView_##SuffixName pgy_pin_read_##SuffixName(PgySlot_##SuffixName *s) \
{ \
    if (s == NULL) { \
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_INTERNAL_INVARIANT, \
                          "null " #SuffixName " slot pin read"); \
    } \
    if (!s->claimed) { \
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_RELEASED_SLOT, \
                          PGY_RUNTIME_PANIC_REASON_RELEASED_SLOT_READ); \
    } \
    PgyPinnedSlotView_##SuffixName view; \
    view.slot = s; \
    view.active = true; \
    view.can_write = false; \
    return view; \
} \
\
PgyPinnedSlotView_##SuffixName pgy_pin_write_##SuffixName(PgySlot_##SuffixName *s) \
{ \
    if (s == NULL) { \
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_INTERNAL_INVARIANT, \
                          "null " #SuffixName " slot pin write"); \
    } \
    if (!s->claimed) { \
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_RELEASED_SLOT, \
                          PGY_RUNTIME_PANIC_REASON_RELEASED_SLOT_WRITE); \
    } \
    PgyPinnedSlotView_##SuffixName view; \
    view.slot = s; \
    view.active = true; \
    view.can_write = true; \
    return view; \
} \
\
void pgy_pin_read_init_##SuffixName(PgyPinnedSlotView_##SuffixName *out, \
                                    PgySlot_##SuffixName *s) \
{ \
    if (out == NULL) { \
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_INTERNAL_INVARIANT, \
                          "null " #SuffixName " slot pin read out"); \
    } \
    *out = pgy_pin_read_##SuffixName(s); \
} \
\
void pgy_pin_write_init_##SuffixName(PgyPinnedSlotView_##SuffixName *out, \
                                     PgySlot_##SuffixName *s) \
{ \
    if (out == NULL) { \
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_INTERNAL_INVARIANT, \
                          "null " #SuffixName " slot pin write out"); \
    } \
    *out = pgy_pin_write_##SuffixName(s); \
} \
\
void pgy_unpin_##SuffixName(PgyPinnedSlotView_##SuffixName *view) \
{ \
    if (view == NULL) { \
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_INTERNAL_INVARIANT, \
                          "null " #SuffixName " slot unpin"); \
    } \
    if (!view->active || view->slot == NULL) { \
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_INTERNAL_INVARIANT, \
                          "inactive " #SuffixName " slot unpin"); \
    } \
    view->active = false; \
    view->slot = NULL; \
}

PGY_INLINE_PIN_EXPORT_DEFINE(Int)
PGY_INLINE_PIN_EXPORT_DEFINE(Long)
PGY_INLINE_PIN_EXPORT_DEFINE(Float)
PGY_INLINE_PIN_EXPORT_DEFINE(Double)
PGY_INLINE_PIN_EXPORT_DEFINE(Bool)
PGY_INLINE_PIN_EXPORT_DEFINE(String)

#undef PGY_INLINE_PIN_EXPORT_DEFINE

/* =================================================================
 * Slot operations - Int
 * ================================================================= */

PgySlot_Int pgy_claim_Int(void)
{
    PgySlot_Int s;
    s.value = 0;
    s.claimed = true;
    return s;
}

void pgy_write_Int(PgySlot_Int *s, int32_t v)
{
    if (s == NULL) {
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_INTERNAL_INVARIANT,
                          "null Int slot write");
    }
    if (!s->claimed) {
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_RELEASED_SLOT,
                          PGY_RUNTIME_PANIC_REASON_RELEASED_SLOT_WRITE);
    }
    s->value = v;
}

int32_t pgy_read_Int(PgySlot_Int *s)
{
    if (s == NULL) {
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_INTERNAL_INVARIANT,
                          "null Int slot read");
    }
    if (!s->claimed) {
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_RELEASED_SLOT,
                          PGY_RUNTIME_PANIC_REASON_RELEASED_SLOT_READ);
    }
    return s->value;
}

void pgy_release_Int(PgySlot_Int *s)
{
    if (s == NULL) {
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_INTERNAL_INVARIANT,
                          "null Int slot release");
    }
    if (!s->claimed) {
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_DOUBLE_RELEASE,
                          PGY_RUNTIME_PANIC_REASON_DOUBLE_RELEASE_SLOT);
    }
    s->value = 0;
    s->claimed = false;
}

/* =================================================================
 * Slot operations - Long
 * ================================================================= */

PgySlot_Long pgy_claim_Long(void)
{
    PgySlot_Long s;
    s.value = 0;
    s.claimed = true;
    return s;
}

void pgy_write_Long(PgySlot_Long *s, int64_t v)
{
    if (s == NULL) {
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_INTERNAL_INVARIANT,
                          "null Long slot write");
    }
    if (!s->claimed) {
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_RELEASED_SLOT,
                          PGY_RUNTIME_PANIC_REASON_RELEASED_SLOT_WRITE);
    }
    s->value = v;
}

int64_t pgy_read_Long(PgySlot_Long *s)
{
    if (s == NULL) {
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_INTERNAL_INVARIANT,
                          "null Long slot read");
    }
    if (!s->claimed) {
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_RELEASED_SLOT,
                          PGY_RUNTIME_PANIC_REASON_RELEASED_SLOT_READ);
    }
    return s->value;
}

void pgy_release_Long(PgySlot_Long *s)
{
    if (s == NULL) {
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_INTERNAL_INVARIANT,
                          "null Long slot release");
    }
    if (!s->claimed) {
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_DOUBLE_RELEASE,
                          PGY_RUNTIME_PANIC_REASON_DOUBLE_RELEASE_SLOT);
    }
    s->value = 0;
    s->claimed = false;
}

/* =================================================================
 * Slot operations - Float
 * ================================================================= */

PgySlot_Float pgy_claim_Float(void)
{
    PgySlot_Float s;
    s.value = 0.0f;
    s.claimed = true;
    return s;
}

void pgy_write_Float(PgySlot_Float *s, float v)
{
    if (s == NULL) {
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_INTERNAL_INVARIANT,
                          "null Float slot write");
    }
    if (!s->claimed) {
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_RELEASED_SLOT,
                          PGY_RUNTIME_PANIC_REASON_RELEASED_SLOT_WRITE);
    }
    s->value = v;
}

float pgy_read_Float(PgySlot_Float *s)
{
    if (s == NULL) {
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_INTERNAL_INVARIANT,
                          "null Float slot read");
    }
    if (!s->claimed) {
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_RELEASED_SLOT,
                          PGY_RUNTIME_PANIC_REASON_RELEASED_SLOT_READ);
    }
    return s->value;
}

void pgy_release_Float(PgySlot_Float *s)
{
    if (s == NULL) {
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_INTERNAL_INVARIANT,
                          "null Float slot release");
    }
    if (!s->claimed) {
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_DOUBLE_RELEASE,
                          PGY_RUNTIME_PANIC_REASON_DOUBLE_RELEASE_SLOT);
    }
    s->value = 0.0f;
    s->claimed = false;
}
