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
pgy_intent_append_line(char **dst, const char *line)
{
    size_t old_len = 0;
    size_t add_len = 0;
    char *grown;

    if (dst == NULL || line == NULL)
        return;

    if (*dst != NULL)
        old_len = strlen(*dst);
    add_len = strlen(line);
    grown = (char *)realloc(*dst, old_len + add_len + 1);
    if (grown == NULL)
        return;
    memcpy(grown + old_len, line, add_len + 1);
    *dst = grown;
}

static inline PgyIntentActiveEntry *
pgy_intent_find_active_entry(int32_t handle)
{
    for (int i = 0; i < PGY_INTENT_ACTIVE_MAX; i++) {
        if (pgy_intent_active_registry[i].active
            && pgy_intent_active_registry[i].handle == handle) {
            return &pgy_intent_active_registry[i];
        }
    }
    return NULL;
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
    pgy_intent_active_registry[free_index].failure_reason = NULL;
    pgy_intent_active_registry[free_index].step_count = 0;
    pgy_intent_active_registry[free_index].failed = false;
    pgy_intent_active_registry[free_index].active = true;
    if (PGY_INTENT_OBSERVABILITY_ENABLED) {
        char line[256];
        snprintf(line, sizeof(line), "[intent] enter %s\n",
            name != NULL ? name : "<intent>");
        pgy_intent_append_line(&pgy_intent_active_registry[free_index].trace, line);
    }
    pgy_intent_push_current_handle(handle);

    pthread_mutex_unlock(&pgy_intent_registry_mutex);
    return handle;
}

#include "pgy_runtime_intent_trace_events_inline.h"
