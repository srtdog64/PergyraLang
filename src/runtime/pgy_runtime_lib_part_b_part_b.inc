bool
pgy_set_has_raw_export(void *set_ptr, void *elem_ptr, int64_t elem_size)
{
    PgySetRaw *set = (PgySetRaw *)set_ptr;
    if (set == NULL) {
        pgy_runtime_warn_invalid_collection("set_has", "null set");
        return false;
    }
    if (elem_ptr == NULL) {
        pgy_runtime_warn_invalid_collection("set_has", "null element");
        return false;
    }
    if (elem_size <= 0) {
        pgy_runtime_warn_invalid_collection("set_has", "non-positive element size");
        return false;
    }
    if (set->capacity == 0 || set->data == NULL || set->occupied == NULL) {
        pgy_runtime_warn_invalid_collection("set_has", "set is not initialized");
        return false;
    }
    if (set->count == 0)
        return false;
    uint32_t h = pgy_set_raw_hash(elem_ptr, elem_size) % (uint32_t)set->capacity;
    size_t p = 0;
    while (set->occupied[h] && p < set->capacity) {
        if (pgy_set_raw_elem_eq(SET_RAW_ELEM(set, h, elem_size), elem_ptr, elem_size))
            return true;
        h = (h + 1) % (uint32_t)set->capacity; p++;
    }
    return false;
}

void
pgy_set_remove_raw_export(void *set_ptr, void *elem_ptr, int64_t elem_size)
{
    PgySetRaw *set = (PgySetRaw *)set_ptr;
    if (set == NULL) {
        pgy_runtime_warn_invalid_collection("set_remove", "null set");
        return;
    }
    if (elem_ptr == NULL) {
        pgy_runtime_warn_invalid_collection("set_remove", "null element");
        return;
    }
    if (elem_size <= 0) {
        pgy_runtime_warn_invalid_collection("set_remove", "non-positive element size");
        return;
    }
    if (set->capacity == 0 || set->data == NULL || set->occupied == NULL) {
        pgy_runtime_warn_invalid_collection("set_remove", "set is not initialized");
        return;
    }
    if (set->count == 0)
        return;
    uint32_t h = pgy_set_raw_hash(elem_ptr, elem_size) % (uint32_t)set->capacity;
    size_t p = 0;
    while (set->occupied[h] && p < set->capacity) {
        if (pgy_set_raw_elem_eq(SET_RAW_ELEM(set, h, elem_size), elem_ptr, elem_size)) {
            memset(SET_RAW_ELEM(set, h, elem_size), 0, (size_t)elem_size);
            set->occupied[h] = 0;
            set->count--;
            return;
        }
        h = (h + 1) % (uint32_t)set->capacity; p++;
    }
}

int32_t
pgy_set_size_raw_export(void *set_ptr)
{
    PgySetRaw *set = (PgySetRaw *)set_ptr;
    if (set == NULL) {
        pgy_runtime_warn_invalid_collection("set_size", "null set");
        return 0;
    }
    return (int32_t)set->count;
}

#define PGY_INTENT_ACTIVE_MAX 256
#define PGY_INTENT_RECENT_MAX 16

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

static char *
pgy_runtime_strdup_export(const char *src)
{
    size_t len;
    char *copy;

    if (src == NULL)
        src = "";
    len = strlen(src);
    copy = (char *)malloc(len + 1);
    if (copy == NULL)
        return NULL;
    memcpy(copy, src, len + 1);
    return copy;
}

static void
pgy_intent_history_step_set_string_export(char **dst, const char *value)
{
    if (dst == NULL)
        return;
    free(*dst);
    *dst = pgy_runtime_strdup_export(value != NULL ? value : "");
}

static void
pgy_intent_history_step_clear_export(PgyIntentHistoryStep *step)
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

static void
pgy_intent_recent_entry_clear_export(PgyIntentRecentEntry *entry)
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

static void
pgy_intent_recent_entry_copy_from_active_export(PgyIntentRecentEntry *dst,
                                                const PgyIntentActiveEntry *src)
{
    if (dst == NULL || src == NULL)
        return;
    pgy_intent_recent_entry_clear_export(dst);
    dst->handle = src->handle;
    dst->trace_id = src->trace_id;
    dst->step_count = src->step_count;
    dst->failed = src->failed;
    dst->name = pgy_runtime_strdup_export(src->name != NULL ? src->name : "");
    dst->trace = pgy_runtime_strdup_export(src->trace != NULL ? src->trace : "");
    dst->failure_reason = pgy_runtime_strdup_export(
        src->failure_reason != NULL ? src->failure_reason : "");
}

static void
pgy_intent_append_line_export(char **dst, const char *line)
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

static PgyIntentActiveEntry *
pgy_intent_find_active_entry_export(int32_t handle)
{
    for (int i = 0; i < PGY_INTENT_ACTIVE_MAX; i++) {
        if (pgy_intent_active_registry[i].active
            && pgy_intent_active_registry[i].handle == handle) {
            return &pgy_intent_active_registry[i];
        }
    }
    return NULL;
}

int32_t
pgy_intent_current_handle_export(void)
{
    if (pgy_intent_current_depth <= 0)
        return 0;
    return pgy_intent_current_stack[pgy_intent_current_depth - 1];
}

static bool
pgy_intent_handle_is_current_ancestor_export(int32_t handle)
{
    int32_t cursor = pgy_intent_current_handle_export();

    while (cursor != 0) {
        PgyIntentActiveEntry *entry;

        if (cursor == handle)
            return true;
        entry = pgy_intent_find_active_entry_export(cursor);
        if (entry == NULL || entry->parent_handle == cursor)
            break;
        cursor = entry->parent_handle;
    }
    return false;
}

static void
pgy_intent_push_current_handle_export(int32_t handle)
{
    if (handle == 0 || pgy_intent_current_depth >= PGY_INTENT_ACTIVE_MAX)
        return;
    pgy_intent_current_stack[pgy_intent_current_depth++] = handle;
}

static void
pgy_intent_pop_current_handle_export(int32_t handle)
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

static bool
pgy_intent_subjects_overlap_export(void **lhs, int32_t lhs_count,
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

int32_t
pgy_intent_enter_export(char *name, void **subjects, int32_t subject_count,
                        bool is_concurrent, int32_t priority)
{
    int free_index = -1;
    int32_t handle = 0;
    int32_t parent_handle = 0;
    void **subject_copy = NULL;

    pthread_mutex_lock(&pgy_intent_registry_mutex);
    parent_handle = pgy_intent_current_handle_export();

    for (int i = 0; i < PGY_INTENT_ACTIVE_MAX; i++) {
        PgyIntentActiveEntry *entry = &pgy_intent_active_registry[i];
        if (!entry->active)
            continue;
        if (!pgy_intent_subjects_overlap_export(entry->subjects, entry->subject_count,
                                                subjects, subject_count))
            continue;
        if (pgy_intent_handle_is_current_ancestor_export(entry->handle))
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
    pgy_intent_active_registry[free_index].name = PGY_INTENT_OBSERVABILITY_ENABLED
        ? pgy_runtime_strdup_export(name)
        : NULL;
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
        pgy_intent_append_line_export(&pgy_intent_active_registry[free_index].trace, line);
    }
    pgy_intent_push_current_handle_export(handle);

    pthread_mutex_unlock(&pgy_intent_registry_mutex);
    return handle;
}

void
pgy_intent_trace_step_export(int32_t handle, char *step_name, char *zone_name)
{
    pthread_mutex_lock(&pgy_intent_registry_mutex);
    PgyIntentActiveEntry *entry = pgy_intent_find_active_entry_export(handle);
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
        pgy_intent_append_line_export(&entry->trace, line);
        if (entry->step_count <= PGY_INTENT_ACTIVE_MAX) {
            int32_t index = entry->step_count - 1;
            pgy_intent_history_step_clear_export(&entry->steps[index]);
            entry->steps[index].name = pgy_runtime_strdup_export(step_name != NULL ? step_name : "");
            entry->steps[index].zone = pgy_runtime_strdup_export(zone_name != NULL ? zone_name : "");
            entry->steps[index].phase = pgy_runtime_strdup_export("begin");
            entry->steps[index].participant = pgy_runtime_strdup_export("");
            entry->steps[index].slot = pgy_runtime_strdup_export("");
            entry->steps[index].from_zone = pgy_runtime_strdup_export("");
            entry->steps[index].from_slot = pgy_runtime_strdup_export("");
            entry->steps[index].to_zone = pgy_runtime_strdup_export("");
            entry->steps[index].to_slot = pgy_runtime_strdup_export("");
            entry->steps[index].ok = false;
            entry->steps[index].failure_reason = pgy_runtime_strdup_export("");
        }
    }
    pthread_mutex_unlock(&pgy_intent_registry_mutex);
}

void
pgy_intent_trace_bind_export(int32_t handle, char *participant_name, char *slot_name)
{
    if (!PGY_INTENT_OBSERVABILITY_ENABLED)
        return;
    pthread_mutex_lock(&pgy_intent_registry_mutex);
    PgyIntentActiveEntry *entry = pgy_intent_find_active_entry_export(handle);
    if (entry != NULL) {
        char line[256];
        snprintf(line, sizeof(line), "[bind] %s -> %s\n",
            participant_name != NULL ? participant_name : "<participant>",
            slot_name != NULL ? slot_name : "<unbound>");
        pgy_intent_append_line_export(&entry->trace, line);
        if (entry->step_count > 0 && entry->step_count <= PGY_INTENT_ACTIVE_MAX) {
            int32_t index = entry->step_count - 1;
            pgy_intent_history_step_set_string_export(&entry->steps[index].participant, participant_name);
            pgy_intent_history_step_set_string_export(&entry->steps[index].slot, slot_name);
        }
    }
    pthread_mutex_unlock(&pgy_intent_registry_mutex);
}

void
pgy_intent_trace_materialize_export(int32_t handle, char *participant_name,
                                    char *slot_name, char *zone_name)
{
    if (!PGY_INTENT_OBSERVABILITY_ENABLED)
        return;
    pthread_mutex_lock(&pgy_intent_registry_mutex);
    PgyIntentActiveEntry *entry = pgy_intent_find_active_entry_export(handle);
    if (entry != NULL) {
        char line[320];
        snprintf(line, sizeof(line), "[materialize] %s => %s.%s\n",
            participant_name != NULL ? participant_name : "<participant>",
            zone_name != NULL ? zone_name : "<zone>",
            slot_name != NULL ? slot_name : "<unbound>");
        pgy_intent_append_line_export(&entry->trace, line);
        if (entry->step_count > 0 && entry->step_count <= PGY_INTENT_ACTIVE_MAX) {
            int32_t index = entry->step_count - 1;
            pgy_intent_history_step_set_string_export(&entry->steps[index].phase, "materialize");
            pgy_intent_history_step_set_string_export(&entry->steps[index].participant, participant_name);
            pgy_intent_history_step_set_string_export(&entry->steps[index].slot, slot_name);
            pgy_intent_history_step_set_string_export(&entry->steps[index].to_zone, zone_name);
            pgy_intent_history_step_set_string_export(&entry->steps[index].to_slot, slot_name);
        }
    }
    pthread_mutex_unlock(&pgy_intent_registry_mutex);
}

void
pgy_intent_trace_transfer_export(int32_t handle, char *participant_name,
                                 char *from_zone_name, char *from_slot_name,
                                 char *to_zone_name, char *to_slot_name)
{
    if (!PGY_INTENT_OBSERVABILITY_ENABLED)
        return;
    pthread_mutex_lock(&pgy_intent_registry_mutex);
    PgyIntentActiveEntry *entry = pgy_intent_find_active_entry_export(handle);
    if (entry != NULL) {
        char line[384];
        snprintf(line, sizeof(line), "[transfer] %s: %s.%s -> %s.%s\n",
            participant_name != NULL ? participant_name : "<participant>",
            from_zone_name != NULL ? from_zone_name : "<zone>",
            from_slot_name != NULL ? from_slot_name : "<unbound>",
            to_zone_name != NULL ? to_zone_name : "<zone>",
            to_slot_name != NULL ? to_slot_name : "<unbound>");
        pgy_intent_append_line_export(&entry->trace, line);
        if (entry->step_count > 0 && entry->step_count <= PGY_INTENT_ACTIVE_MAX) {
            int32_t index = entry->step_count - 1;
            pgy_intent_history_step_set_string_export(&entry->steps[index].phase, "transfer");
            pgy_intent_history_step_set_string_export(&entry->steps[index].participant, participant_name);
            pgy_intent_history_step_set_string_export(&entry->steps[index].from_zone, from_zone_name);
            pgy_intent_history_step_set_string_export(&entry->steps[index].from_slot, from_slot_name);
            pgy_intent_history_step_set_string_export(&entry->steps[index].to_zone, to_zone_name);
            pgy_intent_history_step_set_string_export(&entry->steps[index].to_slot, to_slot_name);
        }
    }
    pthread_mutex_unlock(&pgy_intent_registry_mutex);
}

void
pgy_intent_trace_step_ok_export(int32_t handle, char *step_name)
{
    pthread_mutex_lock(&pgy_intent_registry_mutex);
    PgyIntentActiveEntry *entry = pgy_intent_find_active_entry_export(handle);
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
        pgy_intent_append_line_export(&entry->trace, line);
        if (entry->step_count > 0 && entry->step_count <= PGY_INTENT_ACTIVE_MAX) {
            pgy_intent_history_step_set_string_export(
                &entry->steps[entry->step_count - 1].phase, "ok");
        }
    }
    pthread_mutex_unlock(&pgy_intent_registry_mutex);
}

void
pgy_intent_trace_fail_export(int32_t handle, char *reason)
{
    pthread_mutex_lock(&pgy_intent_registry_mutex);
    PgyIntentActiveEntry *entry = pgy_intent_find_active_entry_export(handle);
    if (entry != NULL) {
        entry->failed = true;
        if (!PGY_INTENT_OBSERVABILITY_ENABLED) {
            pthread_mutex_unlock(&pgy_intent_registry_mutex);
            return;
        }
        char line[256];
        free(entry->failure_reason);
        entry->failure_reason = pgy_runtime_strdup_export(reason != NULL ? reason : "");
        if (entry->step_count > 0 && entry->step_count <= PGY_INTENT_ACTIVE_MAX) {
            int32_t index = entry->step_count - 1;
            pgy_intent_history_step_set_string_export(&entry->steps[index].phase, "fail");
            pgy_intent_history_step_set_string_export(&entry->steps[index].failure_reason, reason);
        }
        snprintf(line, sizeof(line), "[fail] %s\n",
            reason != NULL ? reason : "<failure>");
        pgy_intent_append_line_export(&entry->trace, line);
    }
    pthread_mutex_unlock(&pgy_intent_registry_mutex);
}

void
pgy_mir_resource_op_export(int32_t handle,
                           const char *op_name,
                           const char *slot_anchor,
                           const char *arg_name)
{
#ifdef PGY_MIR_TRACE
    fprintf(stderr, "[MIR resource-op] handle=%d op=%s slot=%s arg=%s\n",
            handle,
            op_name      != NULL ? op_name      : "-",
            slot_anchor  != NULL ? slot_anchor  : "-",
            arg_name     != NULL ? arg_name     : "-");
#else
    (void)handle;
    (void)op_name;
    (void)slot_anchor;
    (void)arg_name;
#endif
}

void
pgy_mir_cleanup_op_export(int32_t handle,
                          const char *op_name,
                          const char *slot_anchor,
                          const char *arg_name)
{
#ifdef PGY_MIR_TRACE
    fprintf(stderr, "[MIR cleanup-op] handle=%d op=%s slot=%s arg=%s\n",
            handle,
            op_name      != NULL ? op_name      : "-",
            slot_anchor  != NULL ? slot_anchor  : "-",
            arg_name     != NULL ? arg_name     : "-");
#else
    (void)handle;
    (void)op_name;
    (void)slot_anchor;
    (void)arg_name;
#endif
}
