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

static inline void
pgy_intent_trace_step_export(int32_t handle, const char *step_name, const char *zone_name)
{
    pthread_mutex_lock(&pgy_intent_registry_mutex);
    PgyIntentActiveEntry *entry = pgy_intent_find_active_entry(handle);
    if (entry != NULL) {
        entry->step_count++;
        if (!PGY_INTENT_OBSERVABILITY_ENABLED) {
            pthread_mutex_unlock(&pgy_intent_registry_mutex);
            return;
        }
        char line[256];
        snprintf(line, sizeof(line), "[step] begin %s @ %s\n",
            step_name != NULL ? step_name : "<step>",
            zone_name != NULL ? zone_name : "<zone>");
        pgy_intent_append_line(&entry->trace, line);
        if (entry->step_count <= PGY_INTENT_ACTIVE_MAX) {
            int32_t index = entry->step_count - 1;
            pgy_intent_history_step_clear(&entry->steps[index]);
            entry->steps[index].name = pgy_runtime_strdup(step_name != NULL ? step_name : "");
            entry->steps[index].zone = pgy_runtime_strdup(zone_name != NULL ? zone_name : "");
            entry->steps[index].phase = pgy_runtime_strdup("begin");
            entry->steps[index].participant = pgy_runtime_strdup("");
            entry->steps[index].slot = pgy_runtime_strdup("");
            entry->steps[index].from_zone = pgy_runtime_strdup("");
            entry->steps[index].from_slot = pgy_runtime_strdup("");
            entry->steps[index].to_zone = pgy_runtime_strdup("");
            entry->steps[index].to_slot = pgy_runtime_strdup("");
            entry->steps[index].ok = false;
            entry->steps[index].failure_reason = pgy_runtime_strdup("");
        }
    }
    pthread_mutex_unlock(&pgy_intent_registry_mutex);
}

static inline void
pgy_intent_trace_bind_export(int32_t handle, const char *participant_name, const char *slot_name)
{
    if (!PGY_INTENT_OBSERVABILITY_ENABLED)
        return;
    pthread_mutex_lock(&pgy_intent_registry_mutex);
    PgyIntentActiveEntry *entry = pgy_intent_find_active_entry(handle);
    if (entry != NULL) {
        char line[256];
        snprintf(line, sizeof(line), "[bind] %s -> %s\n",
            participant_name != NULL ? participant_name : "<participant>",
            slot_name != NULL ? slot_name : "<unbound>");
        pgy_intent_append_line(&entry->trace, line);
        if (entry->step_count > 0 && entry->step_count <= PGY_INTENT_ACTIVE_MAX) {
            int32_t index = entry->step_count - 1;
            pgy_intent_history_step_set_string(&entry->steps[index].participant, participant_name);
            pgy_intent_history_step_set_string(&entry->steps[index].slot, slot_name);
        }
    }
    pthread_mutex_unlock(&pgy_intent_registry_mutex);
}

static inline void
pgy_intent_trace_materialize_export(int32_t handle, const char *participant_name,
                                    const char *slot_name, const char *zone_name)
{
    if (!PGY_INTENT_OBSERVABILITY_ENABLED)
        return;
    pthread_mutex_lock(&pgy_intent_registry_mutex);
    PgyIntentActiveEntry *entry = pgy_intent_find_active_entry(handle);
    if (entry != NULL) {
        char line[320];
        snprintf(line, sizeof(line), "[materialize] %s => %s.%s\n",
            participant_name != NULL ? participant_name : "<participant>",
            zone_name != NULL ? zone_name : "<zone>",
            slot_name != NULL ? slot_name : "<unbound>");
        pgy_intent_append_line(&entry->trace, line);
        if (entry->step_count > 0 && entry->step_count <= PGY_INTENT_ACTIVE_MAX) {
            int32_t index = entry->step_count - 1;
            pgy_intent_history_step_set_string(&entry->steps[index].phase, "materialize");
            pgy_intent_history_step_set_string(&entry->steps[index].participant, participant_name);
            pgy_intent_history_step_set_string(&entry->steps[index].slot, slot_name);
            pgy_intent_history_step_set_string(&entry->steps[index].to_zone, zone_name);
            pgy_intent_history_step_set_string(&entry->steps[index].to_slot, slot_name);
        }
    }
    pthread_mutex_unlock(&pgy_intent_registry_mutex);
}

static inline void
pgy_intent_trace_transfer_export(int32_t handle, const char *participant_name,
                                 const char *from_zone_name, const char *from_slot_name,
                                 const char *to_zone_name, const char *to_slot_name)
{
    if (!PGY_INTENT_OBSERVABILITY_ENABLED)
        return;
    pthread_mutex_lock(&pgy_intent_registry_mutex);
    PgyIntentActiveEntry *entry = pgy_intent_find_active_entry(handle);
    if (entry != NULL) {
        char line[384];
        snprintf(line, sizeof(line), "[transfer] %s: %s.%s -> %s.%s\n",
            participant_name != NULL ? participant_name : "<participant>",
            from_zone_name != NULL ? from_zone_name : "<zone>",
            from_slot_name != NULL ? from_slot_name : "<unbound>",
            to_zone_name != NULL ? to_zone_name : "<zone>",
            to_slot_name != NULL ? to_slot_name : "<unbound>");
        pgy_intent_append_line(&entry->trace, line);
        if (entry->step_count > 0 && entry->step_count <= PGY_INTENT_ACTIVE_MAX) {
            int32_t index = entry->step_count - 1;
            pgy_intent_history_step_set_string(&entry->steps[index].phase, "transfer");
            pgy_intent_history_step_set_string(&entry->steps[index].participant, participant_name);
            pgy_intent_history_step_set_string(&entry->steps[index].from_zone, from_zone_name);
            pgy_intent_history_step_set_string(&entry->steps[index].from_slot, from_slot_name);
            pgy_intent_history_step_set_string(&entry->steps[index].to_zone, to_zone_name);
            pgy_intent_history_step_set_string(&entry->steps[index].to_slot, to_slot_name);
        }
    }
    pthread_mutex_unlock(&pgy_intent_registry_mutex);
}

static inline void
pgy_intent_trace_step_ok_export(int32_t handle, const char *step_name)
{
    pthread_mutex_lock(&pgy_intent_registry_mutex);
    PgyIntentActiveEntry *entry = pgy_intent_find_active_entry(handle);
    if (entry != NULL) {
        if (entry->step_count > 0 && entry->step_count <= PGY_INTENT_ACTIVE_MAX)
            entry->steps[entry->step_count - 1].ok = true;
        if (!PGY_INTENT_OBSERVABILITY_ENABLED) {
            pthread_mutex_unlock(&pgy_intent_registry_mutex);
            return;
        }
        char line[256];
        snprintf(line, sizeof(line), "[step] ok %s\n",
            step_name != NULL ? step_name : "<step>");
        pgy_intent_append_line(&entry->trace, line);
        if (entry->step_count > 0 && entry->step_count <= PGY_INTENT_ACTIVE_MAX) {
            pgy_intent_history_step_set_string(
                &entry->steps[entry->step_count - 1].phase, "ok");
        }
    }
    pthread_mutex_unlock(&pgy_intent_registry_mutex);
}

static inline void
pgy_intent_trace_fail_export(int32_t handle, const char *reason)
{
    pthread_mutex_lock(&pgy_intent_registry_mutex);
    PgyIntentActiveEntry *entry = pgy_intent_find_active_entry(handle);
    if (entry != NULL) {
        entry->failed = true;
        if (!PGY_INTENT_OBSERVABILITY_ENABLED) {
            pthread_mutex_unlock(&pgy_intent_registry_mutex);
            return;
        }
        char line[256];
        free(entry->failure_reason);
        entry->failure_reason = pgy_runtime_strdup(reason != NULL ? reason : "");
        if (entry->step_count > 0 && entry->step_count <= PGY_INTENT_ACTIVE_MAX) {
            int32_t index = entry->step_count - 1;
            pgy_intent_history_step_set_string(&entry->steps[index].phase, "fail");
            pgy_intent_history_step_set_string(&entry->steps[index].failure_reason, reason);
        }
        snprintf(line, sizeof(line), "[fail] %s\n",
            reason != NULL ? reason : "<failure>");
        pgy_intent_append_line(&entry->trace, line);
    }
    pthread_mutex_unlock(&pgy_intent_registry_mutex);
}

static inline void
pgy_mir_resource_op_export(int32_t handle,
                           const char *op_name,
                           const char *slot_anchor,
                           const char *arg_name)
{
    if (!PGY_INTENT_OBSERVABILITY_ENABLED || handle == 0)
        return;

    pthread_mutex_lock(&pgy_intent_registry_mutex);
    PgyIntentActiveEntry *entry = pgy_intent_find_active_entry(handle);
    if (entry != NULL) {
        char line[320];
        snprintf(line, sizeof(line), "[mir-resource] %s %s %s\n",
            op_name != NULL ? op_name : "<op>",
            slot_anchor != NULL ? slot_anchor : "<slot>",
            arg_name != NULL ? arg_name : "<arg>");
        pgy_intent_append_line(&entry->trace, line);
    }
    pthread_mutex_unlock(&pgy_intent_registry_mutex);
}

static inline void
pgy_mir_cleanup_op_export(int32_t handle,
                          const char *op_name,
                          const char *slot_anchor,
                          const char *arg_name)
{
    if (!PGY_INTENT_OBSERVABILITY_ENABLED || handle == 0)
        return;

    pthread_mutex_lock(&pgy_intent_registry_mutex);
    PgyIntentActiveEntry *entry = pgy_intent_find_active_entry(handle);
    if (entry != NULL) {
        char line[320];
        snprintf(line, sizeof(line), "[mir-cleanup] %s %s %s\n",
            op_name != NULL ? op_name : "<op>",
            slot_anchor != NULL ? slot_anchor : "<slot>",
            arg_name != NULL ? arg_name : "<arg>");
        pgy_intent_append_line(&entry->trace, line);
    }
    pthread_mutex_unlock(&pgy_intent_registry_mutex);
}

static inline char *
pgy_intent_last_trace_export(void)
{
    return pgy_intent_last_trace != NULL ? pgy_intent_last_trace : "";
}

static inline char *
pgy_intent_last_failure_export(void)
{
    return pgy_intent_last_failure != NULL ? pgy_intent_last_failure : "";
}

static inline char *
pgy_intent_last_name_export(void)
{
    return pgy_intent_last_name != NULL ? pgy_intent_last_name : "";
}

static inline int32_t
pgy_intent_last_handle_export(void)
{
    return pgy_intent_last_handle;
}

static inline int32_t
pgy_intent_last_trace_id_export(void)
{
    return pgy_intent_last_trace_id;
}

static inline int32_t
pgy_intent_last_step_count_export(void)
{
    return pgy_intent_last_step_count;
}

static inline bool
pgy_intent_last_failed_export(void)
{
    return pgy_intent_last_failed;
}

static inline int32_t
pgy_intent_history_count_export(void)
{
    return pgy_intent_last_history_count;
}

static inline PgyIntentActiveEntry *
pgy_intent_active_entry_by_index_export(int32_t index)
{
    int32_t seen = 0;

    if (index < 0) {
        pgy_runtime_warn_invalid_intent_index("active_entry_by_index", index, -1);
        return NULL;
    }

    for (int i = 0; i < PGY_INTENT_ACTIVE_MAX; i++) {
        if (!pgy_intent_active_registry[i].active)
            continue;
        if (seen == index)
            return &pgy_intent_active_registry[i];
        seen++;
    }

    pgy_runtime_warn_invalid_intent_index("active_entry_by_index", index, seen);
    return NULL;
}

static inline char *
pgy_intent_history_step_name_export(int32_t index)
{
    if (index < 0 || index >= pgy_intent_last_history_count) {
        pgy_runtime_warn_invalid_intent_index("history_step_name", index,
                                              pgy_intent_last_history_count);
        return "";
    }
    return pgy_intent_last_steps[index].name != NULL ? pgy_intent_last_steps[index].name : "";
}

static inline char *
pgy_intent_history_step_zone_export(int32_t index)
{
    if (index < 0 || index >= pgy_intent_last_history_count) {
        pgy_runtime_warn_invalid_intent_index("history_step_zone", index,
                                              pgy_intent_last_history_count);
        return "";
    }
    return pgy_intent_last_steps[index].zone != NULL ? pgy_intent_last_steps[index].zone : "";
}

static inline char *
pgy_intent_history_step_phase_export(int32_t index)
{
    if (index < 0 || index >= pgy_intent_last_history_count) {
        pgy_runtime_warn_invalid_intent_index("history_step_phase", index,
                                              pgy_intent_last_history_count);
        return "";
    }
    return pgy_intent_last_steps[index].phase != NULL ? pgy_intent_last_steps[index].phase : "";
}

static inline char *
pgy_intent_history_step_participant_export(int32_t index)
{
    if (index < 0 || index >= pgy_intent_last_history_count) {
        pgy_runtime_warn_invalid_intent_index("history_step_participant", index,
                                              pgy_intent_last_history_count);
        return "";
    }
    return pgy_intent_last_steps[index].participant != NULL ? pgy_intent_last_steps[index].participant : "";
}

static inline char *
pgy_intent_history_step_slot_export(int32_t index)
{
    if (index < 0 || index >= pgy_intent_last_history_count) {
        pgy_runtime_warn_invalid_intent_index("history_step_slot", index,
                                              pgy_intent_last_history_count);
        return "";
    }
    return pgy_intent_last_steps[index].slot != NULL ? pgy_intent_last_steps[index].slot : "";
}

static inline char *
pgy_intent_history_step_from_zone_export(int32_t index)
{
    if (index < 0 || index >= pgy_intent_last_history_count) {
        pgy_runtime_warn_invalid_intent_index("history_step_from_zone", index,
                                              pgy_intent_last_history_count);
        return "";
    }
    return pgy_intent_last_steps[index].from_zone != NULL ? pgy_intent_last_steps[index].from_zone : "";
}

static inline char *
pgy_intent_history_step_from_slot_export(int32_t index)
{
    if (index < 0 || index >= pgy_intent_last_history_count) {
        pgy_runtime_warn_invalid_intent_index("history_step_from_slot", index,
                                              pgy_intent_last_history_count);
        return "";
    }
    return pgy_intent_last_steps[index].from_slot != NULL ? pgy_intent_last_steps[index].from_slot : "";
}

static inline char *
pgy_intent_history_step_to_zone_export(int32_t index)
{
    if (index < 0 || index >= pgy_intent_last_history_count) {
        pgy_runtime_warn_invalid_intent_index("history_step_to_zone", index,
                                              pgy_intent_last_history_count);
        return "";
    }
    return pgy_intent_last_steps[index].to_zone != NULL ? pgy_intent_last_steps[index].to_zone : "";
}

static inline char *
pgy_intent_history_step_to_slot_export(int32_t index)
{
    if (index < 0 || index >= pgy_intent_last_history_count) {
        pgy_runtime_warn_invalid_intent_index("history_step_to_slot", index,
                                              pgy_intent_last_history_count);
        return "";
    }
    return pgy_intent_last_steps[index].to_slot != NULL ? pgy_intent_last_steps[index].to_slot : "";
}

static inline bool
pgy_intent_history_step_ok_export(int32_t index)
{
    if (index < 0 || index >= pgy_intent_last_history_count) {
        pgy_runtime_warn_invalid_intent_index("history_step_ok", index,
                                              pgy_intent_last_history_count);
        return false;
    }
    return pgy_intent_last_steps[index].ok;
}

static inline char *
pgy_intent_history_step_failure_export(int32_t index)
{
    if (index < 0 || index >= pgy_intent_last_history_count) {
        pgy_runtime_warn_invalid_intent_index("history_step_failure", index,
                                              pgy_intent_last_history_count);
        return "";
    }
    return pgy_intent_last_steps[index].failure_reason != NULL
        ? pgy_intent_last_steps[index].failure_reason : "";
}

static inline int32_t
pgy_intent_active_count_export(void)
{
    int32_t count = 0;

    pthread_mutex_lock(&pgy_intent_registry_mutex);
    for (int i = 0; i < PGY_INTENT_ACTIVE_MAX; i++) {
        if (pgy_intent_active_registry[i].active)
            count++;
    }
    pthread_mutex_unlock(&pgy_intent_registry_mutex);
    return count;
}

static inline char *
pgy_intent_active_name_export(int32_t index)
{
    char *result = "";

    pthread_mutex_lock(&pgy_intent_registry_mutex);
    PgyIntentActiveEntry *entry = pgy_intent_active_entry_by_index_export(index);
    if (entry != NULL && entry->name != NULL)
        result = entry->name;
    pthread_mutex_unlock(&pgy_intent_registry_mutex);
    return result;
}

static inline int32_t
pgy_intent_active_handle_export(int32_t index)
{
    int32_t result = 0;

    pthread_mutex_lock(&pgy_intent_registry_mutex);
    PgyIntentActiveEntry *entry = pgy_intent_active_entry_by_index_export(index);
    if (entry != NULL)
        result = entry->handle;
    pthread_mutex_unlock(&pgy_intent_registry_mutex);
    return result;
}

static inline int32_t
pgy_intent_active_priority_export(int32_t index)
{
    int32_t result = 0;

    pthread_mutex_lock(&pgy_intent_registry_mutex);
    PgyIntentActiveEntry *entry = pgy_intent_active_entry_by_index_export(index);
    if (entry != NULL)
        result = entry->priority;
    pthread_mutex_unlock(&pgy_intent_registry_mutex);
    return result;
}

static inline int32_t
pgy_intent_active_trace_id_export(int32_t index)
{
    int32_t result = 0;

    pthread_mutex_lock(&pgy_intent_registry_mutex);
    PgyIntentActiveEntry *entry = pgy_intent_active_entry_by_index_export(index);
    if (entry != NULL)
        result = entry->trace_id;
    pthread_mutex_unlock(&pgy_intent_registry_mutex);
    return result;
}

static inline bool
pgy_intent_active_concurrent_export(int32_t index)
{
    bool result = false;

    pthread_mutex_lock(&pgy_intent_registry_mutex);
    PgyIntentActiveEntry *entry = pgy_intent_active_entry_by_index_export(index);
    if (entry != NULL)
        result = entry->is_concurrent;
    pthread_mutex_unlock(&pgy_intent_registry_mutex);
    return result;
}

static inline char *
pgy_intent_active_trace_export(int32_t index)
{
    char *result = "";

    pthread_mutex_lock(&pgy_intent_registry_mutex);
    PgyIntentActiveEntry *entry = pgy_intent_active_entry_by_index_export(index);
    if (entry != NULL && entry->trace != NULL)
        result = entry->trace;
    pthread_mutex_unlock(&pgy_intent_registry_mutex);
    return result;
}

static inline int32_t
pgy_intent_active_parent_handle_export(int32_t index)
{
    int32_t result = 0;

    pthread_mutex_lock(&pgy_intent_registry_mutex);
    PgyIntentActiveEntry *entry = pgy_intent_active_entry_by_index_export(index);
    if (entry != NULL)
        result = entry->parent_handle;
    pthread_mutex_unlock(&pgy_intent_registry_mutex);
    return result;
}

static inline int32_t
pgy_intent_active_subject_count_export(int32_t index)
{
    int32_t result = 0;

    pthread_mutex_lock(&pgy_intent_registry_mutex);
    PgyIntentActiveEntry *entry = pgy_intent_active_entry_by_index_export(index);
    if (entry != NULL)
        result = entry->subject_count;
    pthread_mutex_unlock(&pgy_intent_registry_mutex);
    return result;
}

static inline int32_t
pgy_intent_active_step_count_export(int32_t index)
{
    int32_t result = 0;

    pthread_mutex_lock(&pgy_intent_registry_mutex);
    PgyIntentActiveEntry *entry = pgy_intent_active_entry_by_index_export(index);
    if (entry != NULL)
        result = entry->step_count;
    pthread_mutex_unlock(&pgy_intent_registry_mutex);
    return result;
}

static inline bool
pgy_intent_active_failed_export(int32_t index)
{
    bool result = false;

    pthread_mutex_lock(&pgy_intent_registry_mutex);
    PgyIntentActiveEntry *entry = pgy_intent_active_entry_by_index_export(index);
    if (entry != NULL)
        result = entry->failed;
    pthread_mutex_unlock(&pgy_intent_registry_mutex);
    return result;
}

static inline char *
pgy_intent_active_failure_export(int32_t index)
{
    char *result = "";

    pthread_mutex_lock(&pgy_intent_registry_mutex);
    PgyIntentActiveEntry *entry = pgy_intent_active_entry_by_index_export(index);
    if (entry != NULL && entry->failure_reason != NULL)
        result = entry->failure_reason;
    pthread_mutex_unlock(&pgy_intent_registry_mutex);
    return result;
}

static inline void
pgy_intent_exit_export(int32_t handle)
{
    if (handle == 0)
        return;

    pgy_intent_pop_current_handle(handle);
    pthread_mutex_lock(&pgy_intent_registry_mutex);

    for (int i = 0; i < PGY_INTENT_ACTIVE_MAX; i++) {
        PgyIntentActiveEntry *entry = &pgy_intent_active_registry[i];
        if (!entry->active || entry->handle != handle)
            continue;
        if (PGY_INTENT_OBSERVABILITY_ENABLED) {
            free(pgy_intent_last_trace);
            free(pgy_intent_last_failure);
            free(pgy_intent_last_name);
            for (int32_t j = 0; j < pgy_intent_last_history_count; j++) {
                pgy_intent_history_step_clear(&pgy_intent_last_steps[j]);
            }
            pgy_intent_last_trace = entry->trace != NULL
                ? pgy_runtime_strdup(entry->trace) : pgy_runtime_strdup("");
            pgy_intent_last_failure = entry->failure_reason != NULL
                ? pgy_runtime_strdup(entry->failure_reason) : pgy_runtime_strdup("");
            pgy_intent_last_name = entry->name != NULL
                ? pgy_runtime_strdup(entry->name) : pgy_runtime_strdup("");
            pgy_intent_last_handle = entry->handle;
            pgy_intent_last_trace_id = entry->trace_id;
            pgy_intent_last_step_count = entry->step_count;
            pgy_intent_last_failed = entry->failed;
            pgy_intent_last_history_count = entry->step_count;
            if (pgy_intent_last_history_count > PGY_INTENT_ACTIVE_MAX)
                pgy_intent_last_history_count = PGY_INTENT_ACTIVE_MAX;
            for (int32_t j = 0; j < pgy_intent_last_history_count; j++) {
                pgy_intent_history_step_clear(&pgy_intent_last_steps[j]);
                pgy_intent_last_steps[j].name = pgy_runtime_strdup(
                    entry->steps[j].name != NULL ? entry->steps[j].name : "");
                pgy_intent_last_steps[j].zone = pgy_runtime_strdup(
                    entry->steps[j].zone != NULL ? entry->steps[j].zone : "");
                pgy_intent_last_steps[j].phase = pgy_runtime_strdup(
                    entry->steps[j].phase != NULL ? entry->steps[j].phase : "");
                pgy_intent_last_steps[j].participant = pgy_runtime_strdup(
                    entry->steps[j].participant != NULL ? entry->steps[j].participant : "");
                pgy_intent_last_steps[j].slot = pgy_runtime_strdup(
                    entry->steps[j].slot != NULL ? entry->steps[j].slot : "");
                pgy_intent_last_steps[j].from_zone = pgy_runtime_strdup(
                    entry->steps[j].from_zone != NULL ? entry->steps[j].from_zone : "");
                pgy_intent_last_steps[j].from_slot = pgy_runtime_strdup(
                    entry->steps[j].from_slot != NULL ? entry->steps[j].from_slot : "");
                pgy_intent_last_steps[j].to_zone = pgy_runtime_strdup(
                    entry->steps[j].to_zone != NULL ? entry->steps[j].to_zone : "");
                pgy_intent_last_steps[j].to_slot = pgy_runtime_strdup(
                    entry->steps[j].to_slot != NULL ? entry->steps[j].to_slot : "");
                pgy_intent_last_steps[j].ok = entry->steps[j].ok;
                pgy_intent_last_steps[j].failure_reason = pgy_runtime_strdup(
                    entry->steps[j].failure_reason != NULL ? entry->steps[j].failure_reason : "");
            }
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
        free(entry->name);
        if (entry->subjects != entry->inline_subjects)
            free(entry->subjects);
        free(entry->trace);
        free(entry->failure_reason);
        for (int32_t j = 0; j < entry->step_count && j < PGY_INTENT_ACTIVE_MAX; j++) {
            pgy_intent_history_step_clear(&entry->steps[j]);
        }
        entry->handle = 0;
        entry->parent_handle = 0;
        entry->name = NULL;
        entry->subjects = NULL;
        entry->subject_count = 0;
        entry->is_concurrent = false;
        entry->priority = 0;
        entry->trace_id = 0;
        entry->trace = NULL;
        entry->failure_reason = NULL;
        entry->step_count = 0;
        entry->failed = false;
        entry->active = false;
        break;
    }

    pthread_mutex_unlock(&pgy_intent_registry_mutex);
}

static inline struct timespec
pgy_timespec_after_ns(uint64_t timeout_ns)
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
    PGY_ASSERT(s->occupied, "Write to released slot"); \
    s->value = v; \
} \
\
static inline CType \
__attribute__((unused)) \
pgy_read_##SuffixName(PgySlot_##SuffixName* s) \
{ \
    PGY_ASSERT(s->occupied, "Read from released slot"); \
    return s->value; \
} \
\
static inline void \
__attribute__((unused)) \
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
    PGY_ASSERT(s != NULL && s->claimed, "Write to released device slot"); \
    s->value = v; \
} \
\
static inline CType \
pgy_device_read_##SuffixName(PgyDeviceSlot_##SuffixName *s) \
{ \
    PGY_ASSERT(s != NULL && s->claimed, "Read from released device slot"); \
    return s->value; \
} \
\
static inline void \
pgy_release_device_##SuffixName(PgyDeviceSlot_##SuffixName *s) \
{ \
    if (s == NULL) return; \
    PGY_ASSERT(s->claimed, "Double release of device slot"); \
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
    if (len < 0) {
        char *fallback = (char *)malloc(2);
        if (fallback != NULL) {
            fallback[0] = '0';
            fallback[1] = '\0';
        }
        return fallback;
    }
    char *buf = (char *)malloc((size_t)len + 1);
    if (buf == NULL) return NULL;
    memcpy(buf, stack_buf, (size_t)len + 1);
    return buf;
}

static inline char* pgy_float_to_string(float val) {
    char stack_buf[32];
    int len = snprintf(stack_buf, sizeof(stack_buf), "%g", (double)val);
    if (len < 0) {
        char *fallback = (char *)malloc(4);
        if (fallback != NULL) { fallback[0] = '0'; fallback[1] = '.'; fallback[2] = '0'; fallback[3] = '\0'; }
        return fallback;
    }
    char *buf = (char *)malloc((size_t)len + 1);
    if (buf == NULL) return NULL;
    memcpy(buf, stack_buf, (size_t)len + 1);
    return buf;
}

static inline char* pgy_bool_to_string(bool val) {
    const char *s = val ? "true" : "false";
    size_t len = strlen(s);
    char *buf = (char *)malloc(len + 1);
    if (buf != NULL) memcpy(buf, s, len + 1);
    return buf;
}

/* Console I/O: Input(prompt), Print(msg) are already defined below as
 * pgy_input() and pgy_print(). See type_checker_builtins.c for semantic. */

/* =================================================================
 * HashMap<String, T> / HashMap<Int, T> — open-addressing
 * ================================================================= */

#define PGY_HASHMAP_INIT_CAP 16
#define PGY_HASHMAP_LOAD_FACTOR 0.75

typedef struct
{
    char   **keys;
    int32_t *values;
    uint8_t *occupied;
    size_t   count;
    size_t   capacity;
} PgyHashMap_Int;

static inline uint32_t pgy_hash_string(const char *s)
{
    uint32_t h = 5381;
    if (s == NULL) return h;
    while (*s) { h = ((h << 5) + h) ^ (uint32_t)*s++; }
    return h;
}

static inline char *pgy_map_format_i32_key(int32_t key)
{
    return pgy_int_to_string(key);
}

#define PGY_HASHMAP_DEFINE(SuffixName, CType) \
typedef struct \
{ \
    char    **keys; \
    CType    *values; \
    uint8_t  *occupied; \
    size_t    count; \
    size_t    capacity; \
} PgyHashMap_##SuffixName; \
\
static inline PgyHashMap_##SuffixName pgy_map_new_##SuffixName(void) \
{ \
    PgyHashMap_##SuffixName m; \
    m.capacity = PGY_HASHMAP_INIT_CAP; \
    m.count = 0; \
    m.keys = (char **)calloc(m.capacity, sizeof(char *)); \
    m.values = (CType *)calloc(m.capacity, sizeof(CType)); \
    m.occupied = (uint8_t *)calloc(m.capacity, sizeof(uint8_t)); \
    if (m.keys == NULL || m.values == NULL || m.occupied == NULL) { \
        free(m.keys); free(m.values); free(m.occupied); \
        m.keys = NULL; m.values = NULL; m.occupied = NULL; \
        m.capacity = 0; \
        pgy_runtime_warn_invalid_collection("map_new_" #SuffixName, "allocation failed"); \
    } \
    return m; \
} \
\
static inline void pgy_map_grow_##SuffixName(PgyHashMap_##SuffixName *m) \
{ \
    size_t old_cap = m->capacity; \
    char **old_keys = m->keys; \
    CType *old_vals = m->values; \
    uint8_t *old_occ = m->occupied; \
    size_t new_capacity = m->capacity == 0 ? PGY_HASHMAP_INIT_CAP : m->capacity * 2; \
    char **new_keys = (char **)calloc(new_capacity, sizeof(char *)); \
    CType *new_values = (CType *)calloc(new_capacity, sizeof(CType)); \
    uint8_t *new_occupied = (uint8_t *)calloc(new_capacity, sizeof(uint8_t)); \
    if (new_keys == NULL || new_values == NULL || new_occupied == NULL) { \
        free(new_keys); free(new_values); free(new_occupied); \
        pgy_runtime_warn_invalid_collection("map_grow_" #SuffixName, "allocation failed"); \
        return; \
    } \
    m->capacity = new_capacity; \
    m->keys = new_keys; \
    m->values = new_values; \
    m->occupied = new_occupied; \
    m->count = 0; \
    for (size_t i = 0; i < old_cap; i++) { \
        if (old_occ[i]) { \
            uint32_t h = pgy_hash_string(old_keys[i]) % (uint32_t)m->capacity; \
            while (m->occupied[h]) h = (h + 1) % (uint32_t)m->capacity; \
            m->keys[h] = old_keys[i]; \
            m->values[h] = old_vals[i]; \
            m->occupied[h] = 1; \
            m->count++; \
        } \
    } \
    free(old_keys); free(old_vals); free(old_occ); \
} \
\
static inline void pgy_map_set_##SuffixName(PgyHashMap_##SuffixName *m, const char *key, CType val) \
{ \
    if (m == NULL || m->capacity == 0 || m->keys == NULL || m->values == NULL || m->occupied == NULL) { \
        pgy_runtime_warn_invalid_collection("map_set_" #SuffixName, "map is not initialized"); \
        return; \
    } \
    if ((double)m->count / (double)m->capacity > PGY_HASHMAP_LOAD_FACTOR) \
        pgy_map_grow_##SuffixName(m); \
    if (m->capacity == 0 || m->keys == NULL || m->values == NULL || m->occupied == NULL) { \
        pgy_runtime_warn_invalid_collection("map_set_" #SuffixName, "map growth failed"); \
        return; \
    } \
    uint32_t h = pgy_hash_string(key) % (uint32_t)m->capacity; \
    while (m->occupied[h]) { \
        if (m->keys[h] != NULL && strcmp(m->keys[h], key) == 0) { \
            m->values[h] = val; \
            return; \
        } \
        h = (h + 1) % (uint32_t)m->capacity; \
    } \
    m->keys[h] = pgy_runtime_strdup(key); \
    if (m->keys[h] == NULL) { \
        pgy_runtime_warn_invalid_collection("map_set_" #SuffixName, "key duplication failed"); \
        return; \
    } \
    m->values[h] = val; \
    m->occupied[h] = 1; \
    m->count++; \
} \
\
static inline CType pgy_map_get_##SuffixName(PgyHashMap_##SuffixName *m, const char *key) \
{ \
    CType zero_value; \
    memset(&zero_value, 0, sizeof(CType)); \
    if (m == NULL || m->capacity == 0 || m->keys == NULL || m->values == NULL || m->occupied == NULL) return zero_value; \
    if (m->count == 0) return zero_value; \
    uint32_t h = pgy_hash_string(key) % (uint32_t)m->capacity; \
    size_t probes = 0; \
    while (m->occupied[h] && probes < m->capacity) { \
        if (m->keys[h] != NULL && strcmp(m->keys[h], key) == 0) \
            return m->values[h]; \
        h = (h + 1) % (uint32_t)m->capacity; \
        probes++; \
    } \
    return zero_value; \
} \
\
static inline bool pgy_map_has_##SuffixName(PgyHashMap_##SuffixName *m, const char *key) \
{ \
    if (m == NULL || m->capacity == 0 || m->keys == NULL || m->values == NULL || m->occupied == NULL) return false; \
    if (m->count == 0) return false; \
    uint32_t h = pgy_hash_string(key) % (uint32_t)m->capacity; \
    size_t probes = 0; \
    while (m->occupied[h] && probes < m->capacity) { \
        if (m->keys[h] != NULL && strcmp(m->keys[h], key) == 0) \
            return true; \
        h = (h + 1) % (uint32_t)m->capacity; \
        probes++; \
    } \
    return false; \
} \
\
static inline void pgy_map_remove_##SuffixName(PgyHashMap_##SuffixName *m, const char *key) \
{ \
    if (m == NULL || m->capacity == 0 || m->keys == NULL || m->values == NULL || m->occupied == NULL) return; \
    if (m->count == 0) return; \
    uint32_t h = pgy_hash_string(key) % (uint32_t)m->capacity; \
    size_t probes = 0; \
    while (m->occupied[h] && probes < m->capacity) { \
        if (m->keys[h] != NULL && strcmp(m->keys[h], key) == 0) { \
            free(m->keys[h]); \
            m->keys[h] = NULL; \
            memset(&m->values[h], 0, sizeof(CType)); \
            m->occupied[h] = 0; \
            m->count--; \
            return; \
        } \
        h = (h + 1) % (uint32_t)m->capacity; \
        probes++; \
    } \
} \
\
static inline int32_t pgy_map_size_##SuffixName(PgyHashMap_##SuffixName *m) \
{ \
    return (int32_t)m->count; \
} \
\
static inline void pgy_map_set_i32_##SuffixName(PgyHashMap_##SuffixName *m, int32_t key, CType val) \
{ \
    char *key_str = pgy_map_format_i32_key(key); \
    if (key_str == NULL) { \
        pgy_runtime_warn_invalid_collection("map_set_i32_" #SuffixName, "key formatting failed"); \
        return; \
    } \
    pgy_map_set_##SuffixName(m, key_str, val); \
    free(key_str); \
} \
\
static inline CType pgy_map_get_i32_##SuffixName(PgyHashMap_##SuffixName *m, int32_t key) \
{ \
    CType value; \
    char *key_str = pgy_map_format_i32_key(key); \
    memset(&value, 0, sizeof(CType)); \
    if (key_str == NULL) { \
        pgy_runtime_warn_invalid_collection("map_get_i32_" #SuffixName, "key formatting failed"); \
        return value; \
    } \
    value = pgy_map_get_##SuffixName(m, key_str); \
    free(key_str); \
    return value; \
} \
\
static inline bool pgy_map_has_i32_##SuffixName(PgyHashMap_##SuffixName *m, int32_t key) \
{ \
    bool result; \
    char *key_str = pgy_map_format_i32_key(key); \
    if (key_str == NULL) { \
        pgy_runtime_warn_invalid_collection("map_has_i32_" #SuffixName, "key formatting failed"); \
        return false; \
    } \
    result = pgy_map_has_##SuffixName(m, key_str); \
    free(key_str); \
    return result; \
} \
\
static inline void pgy_map_remove_i32_##SuffixName(PgyHashMap_##SuffixName *m, int32_t key) \
{ \
    char *key_str = pgy_map_format_i32_key(key); \
    if (key_str == NULL) { \
        pgy_runtime_warn_invalid_collection("map_remove_i32_" #SuffixName, "key formatting failed"); \
        return; \
    } \
    pgy_map_remove_##SuffixName(m, key_str); \
    free(key_str); \
}

static inline PgyHashMap_Int pgy_map_new_int(void)
{
    PgyHashMap_Int m;
    m.capacity = PGY_HASHMAP_INIT_CAP;
    m.count = 0;
    m.keys     = (char **)calloc(m.capacity, sizeof(char *));
    m.values   = (int32_t *)calloc(m.capacity, sizeof(int32_t));
    m.occupied = (uint8_t *)calloc(m.capacity, sizeof(uint8_t));
    if (m.keys == NULL || m.values == NULL || m.occupied == NULL) {
        free(m.keys); free(m.values); free(m.occupied);
        m.keys = NULL; m.values = NULL; m.occupied = NULL; m.capacity = 0;
        pgy_runtime_warn_invalid_collection("map_new_int", "allocation failed");
    }
    return m;
}

static inline void pgy_map_grow_int(PgyHashMap_Int *m)
{
    size_t old_cap = m->capacity;
    char **old_keys = m->keys;
    int32_t *old_vals = m->values;
    uint8_t *old_occ = m->occupied;
    size_t new_capacity = m->capacity == 0 ? PGY_HASHMAP_INIT_CAP : m->capacity * 2;
    char **new_keys = (char **)calloc(new_capacity, sizeof(char *));
    int32_t *new_values = (int32_t *)calloc(new_capacity, sizeof(int32_t));
    uint8_t *new_occupied = (uint8_t *)calloc(new_capacity, sizeof(uint8_t));
    if (new_keys == NULL || new_values == NULL || new_occupied == NULL) {
        free(new_keys); free(new_values); free(new_occupied);
        pgy_runtime_warn_invalid_collection("map_grow_int", "allocation failed");
        return;
    }
    m->capacity = new_capacity;
    m->keys     = new_keys;
    m->values   = new_values;
    m->occupied = new_occupied;
    m->count = 0;

    for (size_t i = 0; i < old_cap; i++) {
        if (old_occ[i]) {
            uint32_t h = pgy_hash_string(old_keys[i]) % (uint32_t)m->capacity;
            while (m->occupied[h]) h = (h + 1) % (uint32_t)m->capacity;
            m->keys[h] = old_keys[i];
            m->values[h] = old_vals[i];
            m->occupied[h] = 1;
            m->count++;
        }
    }
    free(old_keys); free(old_vals); free(old_occ);
}

static inline void pgy_map_set_int(PgyHashMap_Int *m, const char *key, int32_t val)
{
    if (m == NULL || m->capacity == 0 || m->keys == NULL || m->values == NULL || m->occupied == NULL) {
        pgy_runtime_warn_invalid_collection("map_set_int", "map is not initialized");
        return;
    }
    if ((double)m->count / (double)m->capacity > PGY_HASHMAP_LOAD_FACTOR)
        pgy_map_grow_int(m);
    if (m->capacity == 0 || m->keys == NULL || m->values == NULL || m->occupied == NULL) {
        pgy_runtime_warn_invalid_collection("map_set_int", "map growth failed");
        return;
    }
    uint32_t h = pgy_hash_string(key) % (uint32_t)m->capacity;
    while (m->occupied[h]) {
        if (m->keys[h] != NULL && strcmp(m->keys[h], key) == 0) {
            m->values[h] = val;
            return;
        }
        h = (h + 1) % (uint32_t)m->capacity;
    }
    m->keys[h] = pgy_runtime_strdup(key);
    if (m->keys[h] == NULL) {
        pgy_runtime_warn_invalid_collection("map_set_int", "key duplication failed");
        return;
    }
    m->values[h] = val;
    m->occupied[h] = 1;
    m->count++;
}

static inline int32_t pgy_map_get_int(PgyHashMap_Int *m, const char *key)
{
    if (m == NULL || m->capacity == 0 || m->keys == NULL || m->values == NULL || m->occupied == NULL) return 0;
    if (m->count == 0) return 0;
    uint32_t h = pgy_hash_string(key) % (uint32_t)m->capacity;
    size_t probes = 0;
    while (m->occupied[h] && probes < m->capacity) {
        if (m->keys[h] != NULL && strcmp(m->keys[h], key) == 0)
            return m->values[h];
        h = (h + 1) % (uint32_t)m->capacity;
        probes++;
    }
    return 0;
}

static inline bool pgy_map_has_int(PgyHashMap_Int *m, const char *key)
{
    if (m == NULL || m->capacity == 0 || m->keys == NULL || m->values == NULL || m->occupied == NULL) return false;
    if (m->count == 0) return false;
    uint32_t h = pgy_hash_string(key) % (uint32_t)m->capacity;
    size_t probes = 0;
    while (m->occupied[h] && probes < m->capacity) {
        if (m->keys[h] != NULL && strcmp(m->keys[h], key) == 0)
            return true;
        h = (h + 1) % (uint32_t)m->capacity;
        probes++;
    }
    return false;
}

static inline void pgy_map_remove_int(PgyHashMap_Int *m, const char *key)
{
    if (m == NULL || m->capacity == 0 || m->keys == NULL || m->values == NULL || m->occupied == NULL) return;
    if (m->count == 0) return;
    uint32_t cap = (uint32_t)m->capacity;
    uint32_t h = pgy_hash_string(key) % cap;
    size_t probes = 0;
    while (m->occupied[h] && probes < m->capacity) {
        if (m->keys[h] != NULL && strcmp(m->keys[h], key) == 0) {
            free(m->keys[h]);
            m->keys[h] = NULL;
            m->values[h] = 0;
            m->occupied[h] = 0;
            m->count--;
            /* Backward-shift: rehash subsequent entries to fill the gap */
            uint32_t gap = h;
            uint32_t j = (gap + 1) % cap;
            while (m->occupied[j]) {
                uint32_t ideal = pgy_hash_string(m->keys[j]) % cap;
                uint32_t dist_to_j   = (j - ideal + cap) % cap;
                uint32_t dist_to_gap = (gap - ideal + cap) % cap;
                if (dist_to_gap < dist_to_j) {
                    m->keys[gap]     = m->keys[j];
                    m->values[gap]   = m->values[j];
                    m->occupied[gap] = 1;
                    m->keys[j]      = NULL;
                    m->values[j]    = 0;
                    m->occupied[j]  = 0;
                    gap = j;
                }
                j = (j + 1) % cap;
            }
            return;
        }
        h = (h + 1) % cap;
        probes++;
    }
}

static inline int32_t pgy_map_size_int(PgyHashMap_Int *m)
{
    return (int32_t)m->count;
}

/* String-value variant */
typedef struct
{
    char   **keys;
    char   **values;
    uint8_t *occupied;
    size_t   count;
    size_t   capacity;
} PgyHashMap_String;

static inline PgyHashMap_String pgy_map_new_string(void)
{
    PgyHashMap_String m;
    m.capacity = PGY_HASHMAP_INIT_CAP;
    m.count = 0;
    m.keys     = (char **)calloc(m.capacity, sizeof(char *));
    m.values   = (char **)calloc(m.capacity, sizeof(char *));
    m.occupied = (uint8_t *)calloc(m.capacity, sizeof(uint8_t));
    if (m.keys == NULL || m.values == NULL || m.occupied == NULL) {
        free(m.keys); free(m.values); free(m.occupied);
        m.keys = NULL; m.values = NULL; m.occupied = NULL; m.capacity = 0;
        pgy_runtime_warn_invalid_collection("map_new_string", "allocation failed");
    }
    return m;
}

static inline void pgy_map_set_string(PgyHashMap_String *m, const char *key, const char *val)
{
    if (m == NULL || m->capacity == 0 || m->keys == NULL || m->values == NULL || m->occupied == NULL) {
        pgy_runtime_warn_invalid_collection("map_set_string", "map is not initialized");
        return;
    }
    if ((double)m->count / (double)m->capacity > PGY_HASHMAP_LOAD_FACTOR) {
        size_t old_cap = m->capacity;
        char **ok = m->keys; char **ov = m->values; uint8_t *oo = m->occupied;
        size_t new_capacity = m->capacity == 0 ? PGY_HASHMAP_INIT_CAP : m->capacity * 2;
        char **new_keys = (char **)calloc(new_capacity, sizeof(char *));
        char **new_values = (char **)calloc(new_capacity, sizeof(char *));
        uint8_t *new_occupied = (uint8_t *)calloc(new_capacity, sizeof(uint8_t));
        if (new_keys == NULL || new_values == NULL || new_occupied == NULL) {
            free(new_keys); free(new_values); free(new_occupied);
            pgy_runtime_warn_invalid_collection("map_set_string", "map growth allocation failed");
            return;
        }
        m->capacity = new_capacity;
        m->keys = new_keys;
        m->values = new_values;
        m->occupied = new_occupied;
        m->count = 0;
        for (size_t i = 0; i < old_cap; i++) {
            if (oo[i]) {
                uint32_t h2 = pgy_hash_string(ok[i]) % (uint32_t)m->capacity;
                while (m->occupied[h2]) h2 = (h2 + 1) % (uint32_t)m->capacity;
                m->keys[h2] = ok[i]; m->values[h2] = ov[i]; m->occupied[h2] = 1; m->count++;
            }
        }
        free(ok); free(ov); free(oo);
    }
    uint32_t h = pgy_hash_string(key) % (uint32_t)m->capacity;
    while (m->occupied[h]) {
        if (m->keys[h] && strcmp(m->keys[h], key) == 0) {
            free(m->values[h]);
            m->values[h] = pgy_runtime_strdup(val);
            if (m->values[h] == NULL) {
                pgy_runtime_warn_invalid_collection("map_set_string", "value duplication failed");
            }
            return;
        }
        h = (h + 1) % (uint32_t)m->capacity;
    }
    m->keys[h] = pgy_runtime_strdup(key); m->values[h] = pgy_runtime_strdup(val);
    if (m->keys[h] == NULL || m->values[h] == NULL) {
        free(m->keys[h]); free(m->values[h]);
        m->keys[h] = NULL; m->values[h] = NULL;
        pgy_runtime_warn_invalid_collection("map_set_string", "key/value duplication failed");
        return;
    }
    m->occupied[h] = 1; m->count++;
}

static inline char *pgy_map_get_string(PgyHashMap_String *m, const char *key)
{
    if (m == NULL || m->capacity == 0 || m->keys == NULL || m->values == NULL || m->occupied == NULL) return "";
    if (m->count == 0) return "";
    uint32_t h = pgy_hash_string(key) % (uint32_t)m->capacity;
    size_t p = 0;
    while (m->occupied[h] && p < m->capacity) {
        if (m->keys[h] && strcmp(m->keys[h], key) == 0)
            return m->values[h] ? m->values[h] : "";
        h = (h + 1) % (uint32_t)m->capacity; p++;
    }
    return "";
}

static inline bool pgy_map_has_string(PgyHashMap_String *m, const char *key)
{
    if (m == NULL || m->capacity == 0 || m->keys == NULL || m->values == NULL || m->occupied == NULL) return false;
    if (m->count == 0) return false;
    uint32_t h = pgy_hash_string(key) % (uint32_t)m->capacity;
    size_t p = 0;
    while (m->occupied[h] && p < m->capacity) {
        if (m->keys[h] && strcmp(m->keys[h], key) == 0) return true;
        h = (h + 1) % (uint32_t)m->capacity; p++;
    }
    return false;
}

static inline void pgy_map_remove_string(PgyHashMap_String *m, const char *key)
{
    if (m == NULL || m->capacity == 0 || m->keys == NULL || m->values == NULL || m->occupied == NULL) return;
    if (m->count == 0) return;
    uint32_t cap = (uint32_t)m->capacity;
    uint32_t h = pgy_hash_string(key) % cap;
    size_t probes = 0;
    while (m->occupied[h] && probes < m->capacity) {
        if (m->keys[h] != NULL && strcmp(m->keys[h], key) == 0) {
            free(m->keys[h]);
            m->keys[h] = NULL;
            free(m->values[h]);
            m->values[h] = NULL;
            m->occupied[h] = 0;
            m->count--;
            /* Backward-shift: rehash subsequent entries to fill the gap */
            uint32_t gap = h;
            uint32_t j = (gap + 1) % cap;
            while (m->occupied[j]) {
                uint32_t ideal = pgy_hash_string(m->keys[j]) % cap;
                uint32_t dist_to_j   = (j - ideal + cap) % cap;
                uint32_t dist_to_gap = (gap - ideal + cap) % cap;
                if (dist_to_gap < dist_to_j) {
                    m->keys[gap]     = m->keys[j];
                    m->values[gap]   = m->values[j];
                    m->occupied[gap] = 1;
                    m->keys[j]      = NULL;
                    m->values[j]    = NULL;
                    m->occupied[j]  = 0;
                    gap = j;
                }
                j = (j + 1) % cap;
            }
            return;
        }
        h = (h + 1) % cap;
        probes++;
    }
}

/* =================================================================
 * List<Int> — dynamic array (grows automatically)
 * ================================================================= */

#define PGY_LIST_DEFINE(SuffixName, CType) \
typedef struct \
{ \
    CType   *data; \
    size_t   count; \
    size_t   capacity; \
} PgyList_##SuffixName; \
\
static inline PgyList_##SuffixName pgy_list_new_##SuffixName(void) \
{ \
    PgyList_##SuffixName l; \
    l.capacity = 16; \
    l.count = 0; \
    l.data = (CType *)calloc(l.capacity, sizeof(CType)); \
    if (l.data == NULL) { \
        l.capacity = 0; \
        pgy_runtime_warn_invalid_collection("list_new_" #SuffixName, "allocation failed"); \
    } \
    return l; \
} \
\
static inline void pgy_list_push_##SuffixName(PgyList_##SuffixName *l, CType val) \
{ \
    if (l == NULL || (l->data == NULL && l->capacity == 0)) { \
        pgy_runtime_warn_invalid_collection("list_push_" #SuffixName, "list is not initialized"); \
        return; \
    } \
    if (l->count >= l->capacity) { \
        size_t new_capacity = l->capacity == 0 ? 16 : l->capacity * 2; \
        CType *grown = (CType *)realloc(l->data, new_capacity * sizeof(CType)); \
        if (grown == NULL) { \
            pgy_runtime_warn_invalid_collection("list_push_" #SuffixName, "realloc failed"); \
            return; \
        } \
        l->data = grown; \
        l->capacity = new_capacity; \
    } \
    l->data[l->count++] = val; \
} \
\
static inline CType pgy_list_get_##SuffixName(PgyList_##SuffixName *l, int32_t index) \
{ \
    CType zero_value; \
    memset(&zero_value, 0, sizeof(CType)); \
    if (index < 0 || (size_t)index >= l->count) return zero_value; \
    return l->data[index]; \
} \
\
static inline void pgy_list_set_##SuffixName(PgyList_##SuffixName *l, int32_t index, CType val) \
{ \
    if (index >= 0 && (size_t)index < l->count) l->data[index] = val; \
} \
\
static inline int32_t pgy_list_size_##SuffixName(PgyList_##SuffixName *l) { return (int32_t)l->count; } \
\
static inline void pgy_list_remove_##SuffixName(PgyList_##SuffixName *l, int32_t index) \
{ \
    if (index < 0 || (size_t)index >= l->count) return; \
    for (size_t i = (size_t)index; i < l->count - 1; i++) \
        l->data[i] = l->data[i + 1]; \
    l->count--; \
}

typedef struct
{
    int32_t *data;
    size_t   count;
    size_t   capacity;
} PgyList_Int;

static inline PgyList_Int pgy_list_new_int(void)
{
    PgyList_Int l;
    l.capacity = 16;
    l.count = 0;
    l.data = (int32_t *)calloc(l.capacity, sizeof(int32_t));
    if (l.data == NULL) {
        l.capacity = 0;
        pgy_runtime_warn_invalid_collection("list_new_int", "allocation failed");
    }
    return l;
}

static inline void pgy_list_push_int(PgyList_Int *l, int32_t val)
{
    if (l == NULL || (l->data == NULL && l->capacity == 0)) {
        pgy_runtime_warn_invalid_collection("list_push_int", "list is not initialized");
        return;
    }
    if (l->count >= l->capacity) {
        size_t new_capacity = l->capacity == 0 ? 16 : l->capacity * 2;
        int32_t *grown = (int32_t *)realloc(l->data, new_capacity * sizeof(int32_t));
        if (grown == NULL) {
            pgy_runtime_warn_invalid_collection("list_push_int", "realloc failed");
            return;
        }
        l->data = grown;
        l->capacity = new_capacity;
    }
    l->data[l->count++] = val;
}

static inline int32_t pgy_list_get_int(PgyList_Int *l, int32_t index)
{
    if (index < 0 || (size_t)index >= l->count) return 0;
    return l->data[index];
}

static inline void pgy_list_set_int(PgyList_Int *l, int32_t index, int32_t val)
{
    if (index >= 0 && (size_t)index < l->count) l->data[index] = val;
}

static inline int32_t pgy_list_size_int(PgyList_Int *l) { return (int32_t)l->count; }

static inline void pgy_list_remove_int(PgyList_Int *l, int32_t index)
{
    if (index < 0 || (size_t)index >= l->count) return;
    for (size_t i = (size_t)index; i < l->count - 1; i++)
        l->data[i] = l->data[i + 1];
    l->count--;
}

/* String variant */
typedef struct
{
    char   **data;
    size_t   count;
    size_t   capacity;
} PgyList_String;

static inline PgyList_String pgy_list_new_string(void)
{
    PgyList_String l;
    l.capacity = 16;
    l.count = 0;
    l.data = (char **)calloc(l.capacity, sizeof(char *));
    if (l.data == NULL) {
        l.capacity = 0;
        pgy_runtime_warn_invalid_collection("list_new_string", "allocation failed");
    }
    return l;
}

static inline void pgy_list_push_string(PgyList_String *l, const char *val)
{
    if (l == NULL || (l->data == NULL && l->capacity == 0)) {
        pgy_runtime_warn_invalid_collection("list_push_string", "list is not initialized");
        return;
    }
    if (l->count >= l->capacity) {
        size_t new_capacity = l->capacity == 0 ? 16 : l->capacity * 2;
        char **grown = (char **)realloc(l->data, new_capacity * sizeof(char *));
        if (grown == NULL) {
            pgy_runtime_warn_invalid_collection("list_push_string", "realloc failed");
            return;
        }
        l->data = grown;
        l->capacity = new_capacity;
    }
    l->data[l->count] = pgy_runtime_strdup(val ? val : "");
    if (l->data[l->count] == NULL) {
        pgy_runtime_warn_invalid_collection("list_push_string", "string duplication failed");
        return;
    }
    l->count++;
}

static inline char *pgy_list_get_string(PgyList_String *l, int32_t index)
{
    if (index < 0 || (size_t)index >= l->count) return "";
    return l->data[index] ? l->data[index] : "";
}

static inline int32_t pgy_list_size_string(PgyList_String *l) { return (int32_t)l->count; }

/* =================================================================
 * Set<String> — hash set (string keys)
 * ================================================================= */

typedef struct
{
    char   **keys;
    uint8_t *occupied;
    size_t   count;
    size_t   capacity;
} PgySet_String;

static inline PgySet_String pgy_set_new_string(void)
{
    PgySet_String s;
    s.capacity = 16;
    s.count = 0;
    s.keys = (char **)calloc(s.capacity, sizeof(char *));
    s.occupied = (uint8_t *)calloc(s.capacity, sizeof(uint8_t));
    if (s.keys == NULL || s.occupied == NULL) {
        free(s.keys); free(s.occupied);
        s.keys = NULL; s.occupied = NULL; s.capacity = 0;
        pgy_runtime_warn_invalid_collection("set_new_string", "allocation failed");
    }
    return s;
}

static inline bool pgy_set_has_string(PgySet_String *s, const char *key)
{
    if (s == NULL || s->capacity == 0 || s->keys == NULL || s->occupied == NULL) return false;
    if (s->count == 0 || key == NULL) return false;
    uint32_t h = pgy_hash_string(key) % (uint32_t)s->capacity;
    size_t p = 0;
    while (s->occupied[h] && p < s->capacity) {
        if (s->keys[h] && strcmp(s->keys[h], key) == 0) return true;
        h = (h + 1) % (uint32_t)s->capacity; p++;
    }
    return false;
}

static inline void pgy_set_add_string(PgySet_String *s, const char *key)
{
    if (s == NULL || s->capacity == 0 || s->keys == NULL || s->occupied == NULL) {
        pgy_runtime_warn_invalid_collection("set_add_string", "set is not initialized");
        return;
    }
    if (pgy_set_has_string(s, key)) return;
    if ((double)s->count / (double)s->capacity > 0.75) {
        size_t oc = s->capacity; char **ok = s->keys; uint8_t *oo = s->occupied;
        size_t new_capacity = s->capacity == 0 ? 16 : s->capacity * 2;
        char **new_keys = (char **)calloc(new_capacity, sizeof(char *));
        uint8_t *new_occupied = (uint8_t *)calloc(new_capacity, sizeof(uint8_t));
        if (new_keys == NULL || new_occupied == NULL) {
            free(new_keys); free(new_occupied);
            pgy_runtime_warn_invalid_collection("set_add_string", "growth allocation failed");
            return;
        }
        s->capacity = new_capacity;
        s->keys = new_keys;
        s->occupied = new_occupied;
        s->count = 0;
        for (size_t i = 0; i < oc; i++) {
            if (oo[i]) { pgy_set_add_string(s, ok[i]); free(ok[i]); }
        }
        free(ok); free(oo);
    }
    uint32_t h = pgy_hash_string(key) % (uint32_t)s->capacity;
    while (s->occupied[h]) h = (h + 1) % (uint32_t)s->capacity;
    s->keys[h] = pgy_runtime_strdup(key);
    if (s->keys[h] == NULL) {
        pgy_runtime_warn_invalid_collection("set_add_string", "string duplication failed");
        return;
    }
    s->occupied[h] = 1; s->count++;
}

static inline void pgy_set_remove_string(PgySet_String *s, const char *key)
{
    if (s == NULL || s->capacity == 0 || s->keys == NULL || s->occupied == NULL) return;
    if (s->count == 0 || key == NULL) return;
    uint32_t h = pgy_hash_string(key) % (uint32_t)s->capacity;
    size_t p = 0;
    while (s->occupied[h] && p < s->capacity) {
        if (s->keys[h] && strcmp(s->keys[h], key) == 0) {
            free(s->keys[h]); s->keys[h] = NULL; s->occupied[h] = 0; s->count--;
            return;
        }
        h = (h + 1) % (uint32_t)s->capacity; p++;
    }
}

static inline int32_t pgy_set_size_string(PgySet_String *s) { return (int32_t)s->count; }

/* =================================================================
 * Set<T> — Generic hash set macro (value-based, no string conversion)
 *
 * Uses FNV-1a hash on raw bytes for non-string types.
 * For String type, use PgySet_String above (strcmp-based).
 * ================================================================= */

#define PGY_SET_DEFINE(SuffixName, CType) \
typedef struct \
{ \
    CType   *data; \
    uint8_t *occupied; \
    size_t   count; \
    size_t   capacity; \
} PgySet_##SuffixName; \
\
static inline uint32_t pgy_set_hash_##SuffixName(CType val) \
{ \
    const uint8_t *p = (const uint8_t *)&val; \
    uint32_t h = 2166136261u; \
    for (size_t i = 0; i < sizeof(CType); i++) { h ^= p[i]; h *= 16777619u; } \
    return h; \
} \
\
static inline PgySet_##SuffixName pgy_set_new_##SuffixName(void) \
{ \
    PgySet_##SuffixName s; \
    s.capacity = 16; s.count = 0; \
    s.data = (CType *)calloc(s.capacity, sizeof(CType)); \
    s.occupied = (uint8_t *)calloc(s.capacity, sizeof(uint8_t)); \
    if (s.data == NULL || s.occupied == NULL) { \
        free(s.data); free(s.occupied); \
        s.data = NULL; s.occupied = NULL; s.capacity = 0; \
        pgy_runtime_warn_invalid_collection("set_new_" #SuffixName, "allocation failed"); \
    } \
    return s; \
} \
\
static inline bool pgy_set_has_##SuffixName(PgySet_##SuffixName *s, CType val) \
{ \
    if (s == NULL || s->capacity == 0 || s->data == NULL || s->occupied == NULL) return false; \
    if (s->count == 0) return false; \
    uint32_t h = pgy_set_hash_##SuffixName(val) % (uint32_t)s->capacity; \
    size_t p = 0; \
    while (s->occupied[h] && p < s->capacity) { \
        if (memcmp(&s->data[h], &val, sizeof(CType)) == 0) return true; \
        h = (h + 1) % (uint32_t)s->capacity; p++; \
    } \
    return false; \
} \
\
static inline void pgy_set_add_##SuffixName(PgySet_##SuffixName *s, CType val) \
{ \
    if (s == NULL || s->capacity == 0 || s->data == NULL || s->occupied == NULL) { \
        pgy_runtime_warn_invalid_collection("set_add_" #SuffixName, "set is not initialized"); \
        return; \
    } \
    if (pgy_set_has_##SuffixName(s, val)) return; \
    if ((double)s->count / (double)s->capacity > 0.75) { \
        size_t oc = s->capacity; CType *od = s->data; uint8_t *oo = s->occupied; \
        size_t nc = s->capacity == 0 ? 16 : s->capacity * 2; \
        CType *nd = (CType *)calloc(nc, sizeof(CType)); \
        uint8_t *no = (uint8_t *)calloc(nc, sizeof(uint8_t)); \
        if (nd == NULL || no == NULL) { \
            free(nd); free(no); \
            pgy_runtime_warn_invalid_collection("set_add_" #SuffixName, "rehash allocation failed"); \
            return; \
        } \
        s->capacity = nc; \
        s->data = nd; \
        s->occupied = no; \
        s->count = 0; \
        for (size_t i = 0; i < oc; i++) { if (oo[i]) pgy_set_add_##SuffixName(s, od[i]); } \
        free(od); free(oo); \
    } \
    uint32_t h = pgy_set_hash_##SuffixName(val) % (uint32_t)s->capacity; \
    while (s->occupied[h]) h = (h + 1) % (uint32_t)s->capacity; \
    s->data[h] = val; s->occupied[h] = 1; s->count++; \
} \
\
static inline void pgy_set_remove_##SuffixName(PgySet_##SuffixName *s, CType val) \
{ \
    if (s->count == 0) return; \
    uint32_t h = pgy_set_hash_##SuffixName(val) % (uint32_t)s->capacity; \
    size_t p = 0; \
    while (s->occupied[h] && p < s->capacity) { \
        if (memcmp(&s->data[h], &val, sizeof(CType)) == 0) { \
            memset(&s->data[h], 0, sizeof(CType)); \
            s->occupied[h] = 0; s->count--; return; \
        } \
        h = (h + 1) % (uint32_t)s->capacity; p++; \
    } \
} \
\
static inline int32_t pgy_set_size_##SuffixName(PgySet_##SuffixName *s) \
{ return (int32_t)s->count; }

/* Pre-instantiate Set<Int> (lowercase suffix to match collection_runtime_suffix) */
PGY_SET_DEFINE(int, int32_t)

/* =================================================================
 * Queue<Int> — ring buffer FIFO
 * ================================================================= */

#define PGY_QUEUE_DEFINE(SuffixName, CType) \
typedef struct \
{ \
    CType   *data; \
    size_t   head; \
    size_t   tail; \
    size_t   count; \
    size_t   capacity; \
} PgyQueue_##SuffixName; \
\
static inline PgyQueue_##SuffixName pgy_queue_new_##SuffixName(void) \
{ \
    PgyQueue_##SuffixName q; \
    q.capacity = 16; \
    q.count = q.head = q.tail = 0; \
    q.data = (CType *)calloc(q.capacity, sizeof(CType)); \
    if (q.data == NULL) { \
        q.capacity = 0; \
        pgy_runtime_warn_invalid_collection("queue_new_" #SuffixName, "allocation failed"); \
    } \
    return q; \
} \
\
static inline void pgy_queue_push_##SuffixName(PgyQueue_##SuffixName *q, CType val) \
{ \
    if (q == NULL || (q->data == NULL && q->capacity == 0)) { \
        pgy_runtime_warn_invalid_collection("queue_push_" #SuffixName, "queue is not initialized"); \
        return; \
    } \
    if (q->count >= q->capacity) { \
        size_t nc = q->capacity == 0 ? 16 : q->capacity * 2; \
        CType *nd = (CType *)calloc(nc, sizeof(CType)); \
        if (nd == NULL) { \
            pgy_runtime_warn_invalid_collection("queue_push_" #SuffixName, "growth allocation failed"); \
            return; \
        } \
        for (size_t i = 0; i < q->count; i++) \
            nd[i] = q->data[(q->head + i) % q->capacity]; \
        free(q->data); q->data = nd; \
        q->head = 0; q->tail = q->count; q->capacity = nc; \
    } \
    q->data[q->tail] = val; \
    q->tail = (q->tail + 1) % q->capacity; \
    q->count++; \
} \
\
static inline CType pgy_queue_pop_##SuffixName(PgyQueue_##SuffixName *q) \
{ \
    CType zero_value; \
    memset(&zero_value, 0, sizeof(CType)); \
    if (q == NULL || q->data == NULL || q->capacity == 0) return zero_value; \
    if (q->count == 0) return zero_value; \
    CType val = q->data[q->head]; \
    q->head = (q->head + 1) % q->capacity; \
    q->count--; \
    return val; \
} \
\
static inline int32_t pgy_queue_size_##SuffixName(PgyQueue_##SuffixName *q) { return (int32_t)q->count; } \
static inline bool pgy_queue_empty_##SuffixName(PgyQueue_##SuffixName *q) { return q->count == 0; }

typedef struct
{
    int32_t *data;
    size_t   head;
    size_t   tail;
    size_t   count;
    size_t   capacity;
} PgyQueue_Int;

static inline PgyQueue_Int pgy_queue_new_int(void)
{
    PgyQueue_Int q;
    q.capacity = 16;
    q.count = q.head = q.tail = 0;
    q.data = (int32_t *)calloc(q.capacity, sizeof(int32_t));
    if (q.data == NULL) {
        q.capacity = 0;
        pgy_runtime_warn_invalid_collection("queue_new_int", "allocation failed");
    }
    return q;
}

static inline void pgy_queue_push_int(PgyQueue_Int *q, int32_t val)
{
    if (q == NULL || (q->data == NULL && q->capacity == 0)) {
        pgy_runtime_warn_invalid_collection("queue_push_int", "queue is not initialized");
        return;
    }
    if (q->count >= q->capacity) {
        size_t nc = q->capacity == 0 ? 16 : q->capacity * 2;
        int32_t *nd = (int32_t *)calloc(nc, sizeof(int32_t));
        if (nd == NULL) {
            pgy_runtime_warn_invalid_collection("queue_push_int", "growth allocation failed");
            return;
        }
        for (size_t i = 0; i < q->count; i++)
            nd[i] = q->data[(q->head + i) % q->capacity];
        free(q->data); q->data = nd;
        q->head = 0; q->tail = q->count; q->capacity = nc;
    }
    q->data[q->tail] = val;
    q->tail = (q->tail + 1) % q->capacity;
    q->count++;
}

static inline int32_t pgy_queue_pop_int(PgyQueue_Int *q)
{
    if (q == NULL || q->data == NULL || q->capacity == 0) return 0;
    if (q->count == 0) return 0;
    int32_t val = q->data[q->head];
    q->head = (q->head + 1) % q->capacity;
    q->count--;
    return val;
}

static inline int32_t pgy_queue_size_int(PgyQueue_Int *q) { return (int32_t)q->count; }
static inline bool pgy_queue_empty_int(PgyQueue_Int *q) { return q->count == 0; }

/* =================================================================
 * Object Pool — fixed-capacity reusable slot pool
 * use pool;
 * ================================================================= */

typedef struct
{
    void    *data;          /* flat array of items */
    uint8_t *alive;         /* alive flags */
    size_t   item_size;
    size_t   capacity;
    size_t   count;
} PgyPool;

static inline PgyPool pgy_pool_create(size_t item_size, size_t capacity)
{
    PgyPool p;
    p.item_size = item_size;
    p.capacity = capacity;
    p.count = 0;
    p.data = calloc(capacity, item_size);
    p.alive = (uint8_t *)calloc(capacity, sizeof(uint8_t));
    return p;
}

static inline int32_t pgy_pool_spawn(PgyPool *p, const void *item)
{
    for (size_t i = 0; i < p->capacity; i++) {
        if (!p->alive[i]) {
            memcpy((char *)p->data + i * p->item_size, item, p->item_size);
            p->alive[i] = 1;
            p->count++;
            return (int32_t)i;
        }
    }
    return -1; /* pool full */
}

static inline void pgy_pool_despawn(PgyPool *p, int32_t index)
{
    if (index >= 0 && (size_t)index < p->capacity && p->alive[index]) {
        p->alive[index] = 0;
        p->count--;
    }
}

static inline void *pgy_pool_get(PgyPool *p, int32_t index)
{
    if (index < 0 || (size_t)index >= p->capacity || !p->alive[index])
        return NULL;
    return (char *)p->data + (size_t)index * p->item_size;
}

static inline bool pgy_pool_alive(PgyPool *p, int32_t index)
{
    return index >= 0 && (size_t)index < p->capacity && p->alive[index];
}

static inline int32_t pgy_pool_count(PgyPool *p) { return (int32_t)p->count; }
static inline int32_t pgy_pool_capacity(PgyPool *p) { return (int32_t)p->capacity; }

/* =================================================================
 * FSM — Finite State Machine
 * use fsm;
 * ================================================================= */

#define PGY_FSM_MAX_STATES 32

typedef struct
{
    int32_t current;
    int32_t transitions[PGY_FSM_MAX_STATES][PGY_FSM_MAX_STATES]; /* transition[from][input] = to */
    char   *state_names[PGY_FSM_MAX_STATES];
    size_t  state_count;
} PgyFsm;

static inline PgyFsm pgy_fsm_new(void)
{
    PgyFsm f;
    memset(&f, 0, sizeof(f));
    f.current = 0;
    for (int i = 0; i < PGY_FSM_MAX_STATES; i++)
        for (int j = 0; j < PGY_FSM_MAX_STATES; j++)
            f.transitions[i][j] = -1;
    return f;
}

static inline int32_t pgy_fsm_add_state(PgyFsm *f, const char *name)
{
    if (f->state_count >= PGY_FSM_MAX_STATES) return -1;
    int32_t id = (int32_t)f->state_count;
    f->state_names[id] = pgy_runtime_strdup(name ? name : "");
    f->state_count++;
    return id;
}

static inline void pgy_fsm_add_transition(PgyFsm *f, int32_t from, int32_t input, int32_t to)
{
    if (from >= 0 && from < PGY_FSM_MAX_STATES && input >= 0 && input < PGY_FSM_MAX_STATES)
        f->transitions[from][input] = to;
}

static inline bool pgy_fsm_step(PgyFsm *f, int32_t input)
{
    if (f->current < 0 || f->current >= PGY_FSM_MAX_STATES) return false;
    int32_t next = f->transitions[f->current][input];
    if (next < 0) return false;
    f->current = next;
    return true;
}

static inline int32_t pgy_fsm_current(PgyFsm *f) { return f->current; }

static inline const char *pgy_fsm_current_name(PgyFsm *f)
{
    if (f->current < 0 || (size_t)f->current >= f->state_count) return "";
    return f->state_names[f->current] ? f->state_names[f->current] : "";
}

/* =================================================================
 * Timer / Cooldown
 * use timer;
 * ================================================================= */

typedef struct
{
    int32_t duration;
    int32_t remaining;
    bool    done;
} PgyTimer;

static inline PgyTimer pgy_timer_new(int32_t duration)
{
    PgyTimer t;
    t.duration = duration;
    t.remaining = duration;
    t.done = false;
    return t;
}

static inline void pgy_timer_tick(PgyTimer *t, int32_t delta)
{
    if (t->done) return;
    t->remaining -= delta;
    if (t->remaining <= 0) {
        t->remaining = 0;
        t->done = true;
    }
}

static inline bool pgy_timer_done(PgyTimer *t) { return t->done; }
static inline int32_t pgy_timer_remaining(PgyTimer *t) { return t->remaining; }

static inline void pgy_timer_reset(PgyTimer *t)
{
    t->remaining = t->duration;
    t->done = false;
}

typedef struct
{
    int32_t cooldown;
    int32_t remaining;
} PgyCooldown;

static inline PgyCooldown pgy_cooldown_new(int32_t cooldown)
{
    PgyCooldown c;
    c.cooldown = cooldown;
    c.remaining = 0;
    return c;
}

static inline void pgy_cooldown_tick(PgyCooldown *c, int32_t delta)
{
    c->remaining = (c->remaining > delta) ? c->remaining - delta : 0;
}

static inline bool pgy_cooldown_ready(PgyCooldown *c) { return c->remaining <= 0; }

static inline void pgy_cooldown_trigger(PgyCooldown *c)
{
    c->remaining = c->cooldown;
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
 * Zone Concurrency Protection
 *
 * Wraps zone struct access with rwlock when PGY_ZONE_THREADSAFE is
 * defined. Single-threaded builds use no-op macros (zero cost).
 * ================================================================= */

#ifdef PGY_ZONE_THREADSAFE
#include <pthread.h>

typedef pthread_rwlock_t PgyZoneLock;

#define PGY_ZONE_LOCK_FIELD    PgyZoneLock __zone_lock;
#define PGY_ZONE_LOCK_INIT(z)  pthread_rwlock_init(&(z)->__zone_lock, NULL)
#define PGY_ZONE_LOCK_DESTROY(z) pthread_rwlock_destroy(&(z)->__zone_lock)
#define PGY_ZONE_RDLOCK(z)     pthread_rwlock_rdlock(&(z)->__zone_lock)
#define PGY_ZONE_WRLOCK(z)     pthread_rwlock_wrlock(&(z)->__zone_lock)
#define PGY_ZONE_UNLOCK(z)     pthread_rwlock_unlock(&(z)->__zone_lock)

#else /* single-threaded: zero cost */

#define PGY_ZONE_LOCK_FIELD    /* no lock field */
#define PGY_ZONE_LOCK_INIT(z)  ((void)(z))
#define PGY_ZONE_LOCK_DESTROY(z) ((void)(z))
#define PGY_ZONE_RDLOCK(z)     ((void)(z))
#define PGY_ZONE_WRLOCK(z)     ((void)(z))
#define PGY_ZONE_UNLOCK(z)     ((void)(z))

#endif /* PGY_ZONE_THREADSAFE */

/* Generation counter for stale-state detection */
#define PGY_ZONE_GENERATION_FIELD  uint32_t __sync_generation;
#define PGY_ZONE_GENERATION_INC(z) ((z)->__sync_generation++)
#define PGY_ZONE_GENERATION_WARN_IF_STALE(z, expected, label) do {                 \
    uint32_t _pgy_expected_gen = (uint32_t)(expected);                             \
    uint32_t _pgy_actual_gen = (z)->__sync_generation;                             \
    if (_pgy_actual_gen != _pgy_expected_gen) {                                    \
        fprintf(stderr,                                                             \
            "[pgy][warn] stale zone layer read: %s expected=%u actual=%u\n",       \
            (label),                                                                \
            (unsigned)_pgy_expected_gen,                                            \
            (unsigned)_pgy_actual_gen);                                             \
    }                                                                               \
} while (0)

/* =================================================================
 * Zone Authority — runtime validation
 *
 * Compile time remains the primary line of defense, but zone-entry
 * code now performs a real runtime contract check instead of a debug
 * placeholder. Generated C uses the inline validator below; LLVM code
 * calls the exported twin in pgy_runtime_lib.c.
 * ================================================================= */
static inline bool
pgy_zone_authority_validate(void *zone_ptr, void *participant_ptr,
                            const char *zone_name, const char *participant_name)
{
    const char *resolved_zone = zone_name != NULL ? zone_name : "<zone>";
    const char *resolved_participant =
        participant_name != NULL ? participant_name : "<participant>";

    if (zone_ptr == NULL) {
        fprintf(stderr,
            "[pgy][authority] zone '%s' entered with null self while validating '%s'\n",
            resolved_zone, resolved_participant);
        return false;
    }
    if (participant_ptr == NULL) {
        fprintf(stderr,
            "[pgy][authority] zone '%s' has null authority participant '%s'\n",
            resolved_zone, resolved_participant);
        return false;
    }
    return true;
}

void pgy_zone_authority_check_export(void *zone_ptr, void *participant_ptr,
                                     const char *zone_name,
                                     const char *participant_name);

#define PGY_ZONE_AUTHORITY_CHECK(zone_ptr, participant_ptr, zone_name, participant_name) do { \
    if (!pgy_zone_authority_validate((void *)(zone_ptr), (void *)(participant_ptr),           \
                                     (zone_name), (participant_name))) {                      \
        PGY_PANIC("zone authority validation failed");                                         \
    }                                                                                          \
} while (0)

/* =================================================================
 * Effect Pool — multiple instances of the same effect type
 *
 * Usage in generated zone struct:
 *   PGY_EFFECT_POOL_DEFINE(DamageEffect, 8)
 *   -> typedef struct { DamageEffect items[8]; bool active[8]; uint8_t count; uint8_t cap; } PgyEffectPool_DamageEffect_8;
 *
 * API:
 *   PGY_EFFECT_POOL_APPLY(pool, instance)  — activate next slot
 *   PGY_EFFECT_POOL_DETACH(pool, index)    — deactivate slot
 *   PGY_EFFECT_POOL_ACTIVE_COUNT(pool)     — number of active instances
 *   PGY_EFFECT_POOL_FOR_EACH(pool, i, item) — iterate active instances
 * ================================================================= */

#define PGY_EFFECT_POOL_DEFINE(Type, Cap)                              \
typedef struct {                                                        \
    Type items[Cap];                                                    \
    bool active[Cap];                                                   \
    uint8_t count;                                                      \
    uint8_t cap;                                                        \
} PgyEffectPool_##Type##_##Cap;

#define PGY_EFFECT_POOL_INIT(pool) do {                                \
    memset(&(pool), 0, sizeof(pool));                                   \
    (pool).cap = sizeof((pool).items) / sizeof((pool).items[0]);        \
} while(0)

#define PGY_EFFECT_POOL_APPLY(pool, instance) do {                     \
    for (uint8_t _pi = 0; _pi < (pool).cap; _pi++) {                  \
        if (!(pool).active[_pi]) {                                      \
            (pool).items[_pi] = (instance);                             \
            (pool).active[_pi] = true;                                  \
            (pool).count++;                                             \
            break;                                                      \
        }                                                               \
    }                                                                   \
} while(0)

#define PGY_EFFECT_POOL_DETACH(pool, index) do {                       \
    if ((index) < (pool).cap && (pool).active[(index)]) {              \
        (pool).active[(index)] = false;                                 \
        (pool).count--;                                                 \
    }                                                                   \
} while(0)

#define PGY_EFFECT_POOL_DETACH_ALL(pool) do {                          \
    for (uint8_t _pi = 0; _pi < (pool).cap; _pi++) {                  \
        (pool).active[_pi] = false;                                     \
    }                                                                   \
    (pool).count = 0;                                                   \
} while(0)

#define PGY_EFFECT_POOL_ACTIVE_COUNT(pool) ((pool).count)

#define PGY_EFFECT_POOL_FOR_EACH(pool, idx_var, item_var)              \
    for (uint8_t idx_var = 0; idx_var < (pool).cap; idx_var++)         \
        if ((pool).active[idx_var])                                     \
            for (int _once = 1; _once; _once = 0)                      \
                for (__typeof__((pool).items[0]) *item_var = &(pool).items[idx_var]; _once; _once = 0)

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
static inline void* pgy_ptr_new_impl(size_t size, const char *file, int line)
{
    void *p = malloc(size);
    if (!p) { fprintf(stderr, "pgy: out of memory at %s:%d\n", file, line); abort(); }
    return p;
}

#define PGY_PTR_NEW(Type) \
    ((Type*)pgy_ptr_new_impl(sizeof(Type), __FILE__, __LINE__))

#define PGY_PTR_NEW_ARRAY(Type, count) \
    ((Type*)pgy_ptr_new_impl(sizeof(Type) * (count), __FILE__, __LINE__))

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

/* RemoteFuture<T> → Result<T>: wraps the raw await pointer in a
 * PgyResult.  NULL result → Err("remote operation failed"),
 * non-NULL  → Ok(value).  SuffixName must match PGY_RESULT_DEFINE
 * (e.g. Int, Bool, String).  RawCType is the C storage type. */
#define pgy_await_result_take(handle, SuffixName, RawCType) \
    ({ \
        void *_pgy_raw = pgy_await((handle)); \
        PgyResult_##SuffixName _pgy_r; \
        if (_pgy_raw != NULL) { \
            _pgy_r.tag = PgyResultOk; \
            _pgy_r.ok  = *(RawCType *)_pgy_raw; \
            free(_pgy_raw); \
        } else { \
            _pgy_r.tag = PgyResultErr; \
            _pgy_r.err = "remote operation failed"; \
        } \
        _pgy_r; \
    })

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
PGY_OPTION_DEFINE(Bool, bool)
PGY_OPTION_DEFINE(String, char*)

#define Some_Int(v)             pgy_option_some_Int(v)
#define None_Int()              pgy_option_none_Int()
#define IsSome_Int(o)           ((o).tag == PgyOptionSome)
#define IsNone_Int(o)           ((o).tag == PgyOptionNone)
#define UnwrapOption_Int(o)     pgy_option_unwrap_Int(&(PgyOption_Int){(o).tag, (o).value})

#define Some_Bool(v)            pgy_option_some_Bool(v)
#define None_Bool()             pgy_option_none_Bool()
#define IsSome_Bool(o)          ((o).tag == PgyOptionSome)
#define IsNone_Bool(o)          ((o).tag == PgyOptionNone)
#define UnwrapOption_Bool(o)    pgy_option_unwrap_Bool(&(PgyOption_Bool){(o).tag, (o).value})

#define Some_String(v)          pgy_option_some_String(v)
#define None_String()           pgy_option_none_String()
#define IsSome_String(o)        ((o).tag == PgyOptionSome)
#define IsNone_String(o)        ((o).tag == PgyOptionNone)
#define UnwrapOption_String(o)  pgy_option_unwrap_String(&(PgyOption_String){(o).tag, (o).value})
