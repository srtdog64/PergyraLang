#include "pgy_runtime_observability_schema.h"

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

static inline char *
pgy_runtime_strdup(const char *src)
{
    if (src == NULL)
        src = "";

    size_t len = strlen(src);
    char *copy = (char *)malloc(len + 1);
    if (copy == NULL)
        return NULL;

    memcpy(copy, src, len + 1);
    return copy;
}

#define PGY_INTENT_ACTIVE_MAX 256
#define PGY_INTENT_ACTIVE_INDEX_MAX 512
#define PGY_INTENT_ACTIVE_INDEX_TOMBSTONE (-1)
#define PGY_INTENT_RECENT_MAX 16

#ifndef PGY_INTENT_OBSERVABILITY_ENABLED
#define PGY_INTENT_OBSERVABILITY_ENABLED 1
#endif

typedef struct {
    char *name;
    char *zone;
    char *phase;
    char *participant;
    char *slot;
    char *from_zone;
    char *from_slot;
    char *to_zone;
    char *to_slot;
    bool ok;
    char *failure_reason;
} PgyIntentHistoryStep;

#define PGY_INTENT_INLINE_SUBJECT_CAPACITY 4

typedef struct {
    int32_t handle;
    int32_t parent_handle;
    char   *name;
    void  **subjects;
    void   *inline_subjects[PGY_INTENT_INLINE_SUBJECT_CAPACITY];
    int32_t subject_count;
    bool    is_concurrent;
    int32_t priority;
    int32_t trace_id;
    char   *trace;
    size_t  trace_len;
    char   *failure_reason;
    int32_t step_count;
    bool    failed;
    PgyIntentHistoryStep steps[PGY_INTENT_ACTIVE_MAX];
    bool    active;
} PgyIntentActiveEntry;

typedef struct {
    int32_t handle;
    int32_t trace_id;
    int32_t step_count;
    bool    failed;
    char   *name;
    char   *trace;
    char   *failure_reason;
} PgyIntentRecentEntry;

static PgyIntentActiveEntry pgy_intent_active_registry[PGY_INTENT_ACTIVE_MAX];
static int32_t pgy_intent_active_index_handles[PGY_INTENT_ACTIVE_INDEX_MAX];
static int32_t pgy_intent_active_index_slots[PGY_INTENT_ACTIVE_INDEX_MAX];
static pthread_mutex_t pgy_intent_registry_mutex = PTHREAD_MUTEX_INITIALIZER;
static _Thread_local int32_t pgy_intent_current_stack[PGY_INTENT_ACTIVE_MAX];
static _Thread_local int32_t pgy_intent_current_depth = 0;
static int32_t pgy_intent_next_handle = 1;
static int32_t pgy_intent_next_trace_id = 1;
static char *pgy_intent_last_trace = NULL;
static char *pgy_intent_last_failure = NULL;
static char *pgy_intent_last_name = NULL;
static int32_t pgy_intent_last_handle = 0;
static int32_t pgy_intent_last_trace_id = 0;
static int32_t pgy_intent_last_step_count = 0;
static bool pgy_intent_last_failed = false;
static PgyIntentHistoryStep pgy_intent_last_steps[PGY_INTENT_ACTIVE_MAX];
static int32_t pgy_intent_last_history_count = 0;
static PgyIntentRecentEntry pgy_intent_recent_ring[PGY_INTENT_RECENT_MAX];
static int32_t pgy_intent_recent_count = 0;
static int32_t pgy_intent_recent_head = 0;
static int32_t pgy_intent_active_count = 0;

static inline void
pgy_intent_history_step_set_string(char **dst, const char *value)
{
    if (dst == NULL)
        return;
    free(*dst);
    *dst = pgy_runtime_strdup(value != NULL ? value : "");
}

static inline void
pgy_intent_history_step_clear(PgyIntentHistoryStep *step)
{
    if (step == NULL)
        return;
    free(step->name);
    free(step->zone);
    free(step->phase);
    free(step->participant);
    free(step->slot);
    free(step->from_zone);
    free(step->from_slot);
    free(step->to_zone);
    free(step->to_slot);
    free(step->failure_reason);
    step->name = NULL;
    step->zone = NULL;
    step->phase = NULL;
    step->participant = NULL;
    step->slot = NULL;
    step->from_zone = NULL;
    step->from_slot = NULL;
    step->to_zone = NULL;
    step->to_slot = NULL;
    step->failure_reason = NULL;
    step->ok = false;
}

static inline void
pgy_intent_recent_entry_clear(PgyIntentRecentEntry *entry)
{
    if (entry == NULL)
        return;
    free(entry->name);
    free(entry->trace);
    free(entry->failure_reason);
    entry->handle = 0;
    entry->trace_id = 0;
    entry->step_count = 0;
    entry->failed = false;
    entry->name = NULL;
    entry->trace = NULL;
    entry->failure_reason = NULL;
}

static inline void
pgy_intent_recent_entry_copy_from_active(PgyIntentRecentEntry *dst,
                                         const PgyIntentActiveEntry *src)
{
    if (dst == NULL || src == NULL)
        return;
    pgy_intent_recent_entry_clear(dst);
    dst->handle = src->handle;
    dst->trace_id = src->trace_id;
    dst->step_count = src->step_count;
    dst->failed = src->failed;
    dst->name = pgy_runtime_strdup(src->name != NULL ? src->name : "");
    dst->trace = pgy_runtime_strdup(src->trace != NULL ? src->trace : "");
    dst->failure_reason = pgy_runtime_strdup(
        src->failure_reason != NULL ? src->failure_reason : "");
}

static inline void
pgy_intent_append_line_len(char **dst, size_t *dst_len, const char *line)
{
    size_t old_len;
    size_t add_len;
    char *grown;

    if (dst == NULL || dst_len == NULL || line == NULL)
        return;

    old_len = *dst_len;
    if (*dst != NULL && old_len == 0)
        old_len = strlen(*dst);
    add_len = strlen(line);
    if (add_len > ((size_t)-1) - old_len - 1)
        return;
    grown = (char *)realloc(*dst, old_len + add_len + 1);
    if (grown == NULL)
        return;
    memcpy(grown + old_len, line, add_len + 1);
    *dst = grown;
    *dst_len = old_len + add_len;
}

static inline void
pgy_intent_append_line(char **dst, const char *line)
{
    size_t ignored_len = 0;

    pgy_intent_append_line_len(dst, &ignored_len, line);
}

static inline PgyIntentActiveEntry *
pgy_intent_find_active_entry_linear(int32_t handle)
{
    for (int i = 0; i < PGY_INTENT_ACTIVE_MAX; i++) {
        if (pgy_intent_active_registry[i].active
            && pgy_intent_active_registry[i].handle == handle) {
            return &pgy_intent_active_registry[i];
        }
    }
    return NULL;
}

static inline uint32_t
pgy_intent_handle_hash(int32_t handle)
{
    uint32_t value = (uint32_t)handle;
    value ^= value >> 16;
    value *= 0x7feb352dU;
    value ^= value >> 15;
    return value;
}

static inline int32_t
pgy_intent_active_index_find_slot(int32_t handle)
{
    uint32_t base;

    if (handle <= 0)
        return -1;
    base = pgy_intent_handle_hash(handle) & (PGY_INTENT_ACTIVE_INDEX_MAX - 1);
    for (int32_t probe = 0; probe < PGY_INTENT_ACTIVE_INDEX_MAX; probe++) {
        int32_t slot = (int32_t)((base + (uint32_t)probe)
            & (PGY_INTENT_ACTIVE_INDEX_MAX - 1));
        int32_t indexed_handle = pgy_intent_active_index_handles[slot];
        if (indexed_handle == 0)
            return -1;
        if (indexed_handle == handle)
            return slot;
    }
    return -1;
}

static inline void
pgy_intent_active_index_set(int32_t handle, int32_t active_slot)
{
    uint32_t base;
    int32_t first_tombstone = -1;

    if (handle <= 0 || active_slot < 0 || active_slot >= PGY_INTENT_ACTIVE_MAX)
        return;
    base = pgy_intent_handle_hash(handle) & (PGY_INTENT_ACTIVE_INDEX_MAX - 1);
    for (int32_t probe = 0; probe < PGY_INTENT_ACTIVE_INDEX_MAX; probe++) {
        int32_t slot = (int32_t)((base + (uint32_t)probe)
            & (PGY_INTENT_ACTIVE_INDEX_MAX - 1));
        int32_t indexed_handle = pgy_intent_active_index_handles[slot];
        if (indexed_handle == PGY_INTENT_ACTIVE_INDEX_TOMBSTONE) {
            if (first_tombstone < 0)
                first_tombstone = slot;
            continue;
        }
        if (indexed_handle == 0) {
            if (first_tombstone >= 0)
                slot = first_tombstone;
            pgy_intent_active_index_handles[slot] = handle;
            pgy_intent_active_index_slots[slot] = active_slot;
            return;
        }
        if (indexed_handle == handle) {
            pgy_intent_active_index_handles[slot] = handle;
            pgy_intent_active_index_slots[slot] = active_slot;
            return;
        }
    }
}

static inline void
pgy_intent_active_index_clear(int32_t handle)
{
    int32_t slot = pgy_intent_active_index_find_slot(handle);
    if (slot < 0)
        return;
    pgy_intent_active_index_handles[slot] =
        PGY_INTENT_ACTIVE_INDEX_TOMBSTONE;
    pgy_intent_active_index_slots[slot] = 0;
}

static inline PgyIntentActiveEntry *
pgy_intent_find_active_entry(int32_t handle)
{
    int32_t slot = pgy_intent_active_index_find_slot(handle);

    if (slot >= 0) {
        int32_t active_slot = pgy_intent_active_index_slots[slot];
        if (active_slot >= 0 && active_slot < PGY_INTENT_ACTIVE_MAX) {
            PgyIntentActiveEntry *entry =
                &pgy_intent_active_registry[active_slot];
            if (entry->active && entry->handle == handle)
                return entry;
        }
    }
    return pgy_intent_find_active_entry_linear(handle);
}

static inline int32_t
pgy_intent_find_active_registry_slot(int32_t handle)
{
    int32_t slot = pgy_intent_active_index_find_slot(handle);

    if (slot >= 0) {
        int32_t active_slot = pgy_intent_active_index_slots[slot];
        if (active_slot >= 0 && active_slot < PGY_INTENT_ACTIVE_MAX) {
            PgyIntentActiveEntry *entry =
                &pgy_intent_active_registry[active_slot];
            if (entry->active && entry->handle == handle)
                return active_slot;
        }
    }

    for (int32_t i = 0; i < PGY_INTENT_ACTIVE_MAX; i++) {
        if (pgy_intent_active_registry[i].active
            && pgy_intent_active_registry[i].handle == handle) {
            return i;
        }
    }
    return -1;
}

static inline int32_t
pgy_intent_current_handle(void)
{
    if (pgy_intent_current_depth <= 0)
        return 0;
    return pgy_intent_current_stack[pgy_intent_current_depth - 1];
}

static inline bool
pgy_intent_handle_is_current_ancestor(int32_t handle)
{
    int32_t cursor = pgy_intent_current_handle();

    while (cursor != 0) {
        PgyIntentActiveEntry *entry;

        if (cursor == handle)
            return true;
        entry = pgy_intent_find_active_entry(cursor);
        if (entry == NULL || entry->parent_handle == cursor)
            break;
        cursor = entry->parent_handle;
    }
    return false;
}

static inline void
pgy_intent_push_current_handle(int32_t handle)
{
    if (handle == 0 || pgy_intent_current_depth >= PGY_INTENT_ACTIVE_MAX)
        return;
    pgy_intent_current_stack[pgy_intent_current_depth++] = handle;
}

static inline void
pgy_intent_pop_current_handle(int32_t handle)
{
    for (int32_t i = pgy_intent_current_depth - 1; i >= 0; i--) {
        if (pgy_intent_current_stack[i] != handle)
            continue;
        for (int32_t j = i; j + 1 < pgy_intent_current_depth; j++)
            pgy_intent_current_stack[j] = pgy_intent_current_stack[j + 1];
        pgy_intent_current_depth--;
        return;
    }
}

static inline bool
pgy_intent_subjects_overlap(void **lhs, int32_t lhs_count,
                            void **rhs, int32_t rhs_count)
{
    for (int32_t i = 0; i < lhs_count; i++) {
        if (lhs == NULL || lhs[i] == NULL)
            continue;
        for (int32_t j = 0; j < rhs_count; j++) {
            if (rhs == NULL || rhs[j] == NULL)
                continue;
            if (lhs[i] == rhs[j])
                return true;
        }
    }
    return false;
}

static inline int32_t
pgy_intent_enter_export(char *name, void **subjects, int32_t subject_count,
                        bool is_concurrent, int32_t priority)
{
    int free_index = -1;
    int32_t handle = 0;
    int32_t parent_handle = 0;
    void **subject_copy = NULL;

    pthread_mutex_lock(&pgy_intent_registry_mutex);
    parent_handle = pgy_intent_current_handle();

    for (int i = 0; i < PGY_INTENT_ACTIVE_MAX; i++) {
        PgyIntentActiveEntry *entry = &pgy_intent_active_registry[i];
        if (!entry->active)
            continue;
        if (!pgy_intent_subjects_overlap(entry->subjects, entry->subject_count,
                                         subjects, subject_count))
            continue;
        if (pgy_intent_handle_is_current_ancestor(entry->handle))
            continue;
        if (entry->is_concurrent && is_concurrent)
            continue;
        if (priority > entry->priority)
            continue;
        pgy_runtime_warn_intent_enter_failure(name,
            "same-subject conflict with active intent",
            priority, is_concurrent);
        pthread_mutex_unlock(&pgy_intent_registry_mutex);
        return 0;
    }

    for (int i = 0; i < PGY_INTENT_ACTIVE_MAX; i++) {
        if (!pgy_intent_active_registry[i].active) {
            free_index = i;
            break;
        }
    }

    if (free_index < 0) {
        pgy_runtime_warn_intent_enter_failure(name,
            "active registry capacity exhausted",
            priority, is_concurrent);
        pthread_mutex_unlock(&pgy_intent_registry_mutex);
        return 0;
    }

    if (subject_count > 0) {
        if (subject_count <= PGY_INTENT_INLINE_SUBJECT_CAPACITY) {
            subject_copy = pgy_intent_active_registry[free_index].inline_subjects;
        } else {
            subject_copy = (void **)malloc(sizeof(void *) * (size_t)subject_count);
            if (subject_copy == NULL) {
                pgy_runtime_warn_intent_enter_failure(name,
                    "subject registry allocation failed",
                    priority, is_concurrent);
                pthread_mutex_unlock(&pgy_intent_registry_mutex);
                return 0;
            }
        }
        memcpy(subject_copy, subjects, sizeof(void *) * (size_t)subject_count);
    }

    handle = pgy_intent_next_handle++;
    pgy_intent_active_registry[free_index].handle = handle;
    pgy_intent_active_registry[free_index].parent_handle = parent_handle;
    pgy_intent_active_registry[free_index].name = pgy_runtime_strdup(name);
    pgy_intent_active_registry[free_index].subjects = subject_copy;
    pgy_intent_active_registry[free_index].subject_count = subject_count;
    pgy_intent_active_registry[free_index].is_concurrent = is_concurrent;
    pgy_intent_active_registry[free_index].priority = priority;
    pgy_intent_active_registry[free_index].trace_id = PGY_INTENT_OBSERVABILITY_ENABLED
        ? pgy_intent_next_trace_id++ : 0;
    pgy_intent_active_registry[free_index].trace = NULL;
    pgy_intent_active_registry[free_index].trace_len = 0;
    pgy_intent_active_registry[free_index].failure_reason = NULL;
    pgy_intent_active_registry[free_index].step_count = 0;
    pgy_intent_active_registry[free_index].failed = false;
    pgy_intent_active_registry[free_index].active = true;
    pgy_intent_active_count++;
    pgy_intent_active_index_set(handle, free_index);
    if (PGY_INTENT_OBSERVABILITY_ENABLED) {
        char line[256];
        snprintf(line, sizeof(line), "[intent] enter %s\n",
            name != NULL ? name : "<intent>");
        pgy_intent_append_line_len(&pgy_intent_active_registry[free_index].trace,
                                   &pgy_intent_active_registry[free_index].trace_len,
                                   line);
    }
    pgy_intent_push_current_handle(handle);

    pthread_mutex_unlock(&pgy_intent_registry_mutex);
    return handle;
}

#include "pgy_runtime_intent_trace_events_inline.h"
