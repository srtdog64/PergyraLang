#ifndef PGY_RUNTIME_INTENT_ACTIVE_EXPORTS_H
#define PGY_RUNTIME_INTENT_ACTIVE_EXPORTS_H

static inline char *
pgy_intent_last_trace_export(void)
{
    char *result;
    pthread_mutex_lock(&pgy_intent_registry_mutex);
    result = pgy_intent_borrowed_snapshot(pgy_intent_last_trace);
    pthread_mutex_unlock(&pgy_intent_registry_mutex);
    return result;
}

static inline char *
pgy_intent_last_failure_export(void)
{
    char *result;
    pthread_mutex_lock(&pgy_intent_registry_mutex);
    result = pgy_intent_borrowed_snapshot(pgy_intent_last_failure);
    pthread_mutex_unlock(&pgy_intent_registry_mutex);
    return result;
}

static inline char *
pgy_intent_last_name_export(void)
{
    char *result;
    pthread_mutex_lock(&pgy_intent_registry_mutex);
    result = pgy_intent_borrowed_snapshot(pgy_intent_last_name);
    pthread_mutex_unlock(&pgy_intent_registry_mutex);
    return result;
}

static inline int32_t
pgy_intent_last_handle_export(void)
{
    int32_t result;
    pthread_mutex_lock(&pgy_intent_registry_mutex);
    result = pgy_intent_last_handle;
    pthread_mutex_unlock(&pgy_intent_registry_mutex);
    return result;
}

static inline int32_t
pgy_intent_last_trace_id_export(void)
{
    int32_t result;
    pthread_mutex_lock(&pgy_intent_registry_mutex);
    result = pgy_intent_last_trace_id;
    pthread_mutex_unlock(&pgy_intent_registry_mutex);
    return result;
}

static inline int32_t
pgy_intent_last_step_count_export(void)
{
    int32_t result;
    pthread_mutex_lock(&pgy_intent_registry_mutex);
    result = pgy_intent_last_step_count;
    pthread_mutex_unlock(&pgy_intent_registry_mutex);
    return result;
}

static inline bool
pgy_intent_last_failed_export(void)
{
    bool result;
    pthread_mutex_lock(&pgy_intent_registry_mutex);
    result = pgy_intent_last_failed;
    pthread_mutex_unlock(&pgy_intent_registry_mutex);
    return result;
}

static inline int32_t
pgy_intent_history_count_export(void)
{
    int32_t result;
    pthread_mutex_lock(&pgy_intent_registry_mutex);
    result = pgy_intent_last_history_count;
    pthread_mutex_unlock(&pgy_intent_registry_mutex);
    return result;
}

static inline PgyIntentActiveEntry *
pgy_intent_active_entry_by_index_locked_export(int32_t index)
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

static inline PgyIntentRecentEntry *
pgy_intent_recent_entry_by_index_locked_export(int32_t index)
{
    int32_t recent_index;

    if (index < 0 || index >= pgy_intent_recent_count) {
        pgy_runtime_warn_invalid_intent_index("recent_entry_by_index", index,
                                              pgy_intent_recent_count);
        return NULL;
    }

    recent_index = pgy_intent_recent_head - 1 - index;
    while (recent_index < 0)
        recent_index += PGY_INTENT_RECENT_MAX;
    recent_index %= PGY_INTENT_RECENT_MAX;
    return &pgy_intent_recent_ring[recent_index];
}

static inline PgyIntentActiveEntry *
pgy_intent_active_entry_by_handle_locked_export(int32_t handle)
{
    return pgy_intent_find_active_entry_locked(handle);
}

static inline PgyIntentHistoryStep *
pgy_intent_active_step_by_handle_locked_export(int32_t intent_handle, int32_t step_index)
{
    PgyIntentActiveEntry *entry =
        pgy_intent_active_entry_by_handle_locked_export(intent_handle);
    if (entry == NULL)
        return NULL;
    if (step_index < 0 || step_index >= entry->step_count) {
        pgy_runtime_warn_invalid_intent_index("active_step_by_index", step_index,
                                              entry->step_count);
        return NULL;
    }
    return &entry->steps[step_index];
}

static inline int32_t
pgy_intent_active_count_export(void)
{
    int32_t count;

    pthread_mutex_lock(&pgy_intent_registry_mutex);
    count = pgy_intent_active_count;
    pthread_mutex_unlock(&pgy_intent_registry_mutex);

    return count;
}

static inline char *
pgy_intent_active_name_export(int32_t index)
{
    char *result = "";

    pthread_mutex_lock(&pgy_intent_registry_mutex);
    PgyIntentActiveEntry *entry =
        pgy_intent_active_entry_by_index_locked_export(index);
    if (entry != NULL && entry->name != NULL)
        result = pgy_intent_borrowed_snapshot(entry->name);
    pthread_mutex_unlock(&pgy_intent_registry_mutex);
    return result;
}

static inline int32_t
pgy_intent_active_handle_export(int32_t index)
{
    int32_t result = 0;

    pthread_mutex_lock(&pgy_intent_registry_mutex);
    PgyIntentActiveEntry *entry =
        pgy_intent_active_entry_by_index_locked_export(index);
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
    PgyIntentActiveEntry *entry =
        pgy_intent_active_entry_by_index_locked_export(index);
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
    PgyIntentActiveEntry *entry =
        pgy_intent_active_entry_by_index_locked_export(index);
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
    PgyIntentActiveEntry *entry =
        pgy_intent_active_entry_by_index_locked_export(index);
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
    PgyIntentActiveEntry *entry =
        pgy_intent_active_entry_by_index_locked_export(index);
    if (entry != NULL && entry->trace != NULL)
        result = pgy_intent_borrowed_snapshot(entry->trace);
    pthread_mutex_unlock(&pgy_intent_registry_mutex);
    return result;
}

static inline int32_t
pgy_intent_active_parent_handle_export(int32_t index)
{
    int32_t result = 0;

    pthread_mutex_lock(&pgy_intent_registry_mutex);
    PgyIntentActiveEntry *entry =
        pgy_intent_active_entry_by_index_locked_export(index);
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
    PgyIntentActiveEntry *entry =
        pgy_intent_active_entry_by_index_locked_export(index);
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
    PgyIntentActiveEntry *entry =
        pgy_intent_active_entry_by_index_locked_export(index);
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
    PgyIntentActiveEntry *entry =
        pgy_intent_active_entry_by_index_locked_export(index);
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
    PgyIntentActiveEntry *entry =
        pgy_intent_active_entry_by_index_locked_export(index);
    if (entry != NULL && entry->failure_reason != NULL)
        result = pgy_intent_borrowed_snapshot(entry->failure_reason);
    pthread_mutex_unlock(&pgy_intent_registry_mutex);
    return result;
}

static inline int32_t
pgy_intent_current_handle_export(void)
{
    return pgy_intent_current_handle();
}

static inline char *
pgy_intent_active_step_name_export(int32_t intent_index, int32_t step_index)
{
    char *result = "";
    pthread_mutex_lock(&pgy_intent_registry_mutex);
    PgyIntentHistoryStep *step =
        pgy_intent_active_step_by_handle_locked_export(intent_index, step_index);
    if (step != NULL && step->name != NULL)
        result = pgy_intent_borrowed_snapshot(step->name);
    pthread_mutex_unlock(&pgy_intent_registry_mutex);
    return result;
}

#define PGY_INTENT_ACTIVE_STEP_STRING_EXPORT(fn_name, field_name) \
static inline char * \
fn_name(int32_t intent_index, int32_t step_index) \
{ \
    char *result = ""; \
    pthread_mutex_lock(&pgy_intent_registry_mutex); \
    PgyIntentHistoryStep *step = \
        pgy_intent_active_step_by_handle_locked_export(intent_index, step_index); \
    if (step != NULL && step->field_name != NULL) \
        result = pgy_intent_borrowed_snapshot(step->field_name); \
    pthread_mutex_unlock(&pgy_intent_registry_mutex); \
    return result; \
}

PGY_INTENT_ACTIVE_STEP_STRING_EXPORT(pgy_intent_active_step_zone_export, zone)
PGY_INTENT_ACTIVE_STEP_STRING_EXPORT(pgy_intent_active_step_phase_export, phase)
PGY_INTENT_ACTIVE_STEP_STRING_EXPORT(pgy_intent_active_step_participant_export, participant)
PGY_INTENT_ACTIVE_STEP_STRING_EXPORT(pgy_intent_active_step_slot_export, slot)
PGY_INTENT_ACTIVE_STEP_STRING_EXPORT(pgy_intent_active_step_from_zone_export, from_zone)
PGY_INTENT_ACTIVE_STEP_STRING_EXPORT(pgy_intent_active_step_from_slot_export, from_slot)
PGY_INTENT_ACTIVE_STEP_STRING_EXPORT(pgy_intent_active_step_to_zone_export, to_zone)
PGY_INTENT_ACTIVE_STEP_STRING_EXPORT(pgy_intent_active_step_to_slot_export, to_slot)
PGY_INTENT_ACTIVE_STEP_STRING_EXPORT(pgy_intent_active_step_failure_export, failure_reason)

#endif /* PGY_RUNTIME_INTENT_ACTIVE_EXPORTS_H */
