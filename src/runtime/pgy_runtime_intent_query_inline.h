/* Runtime intent active-step and recent observability query exports. */

#ifndef PGY_RUNTIME_INTENT_QUERY_INLINE_H
#define PGY_RUNTIME_INTENT_QUERY_INLINE_H

static inline bool
pgy_intent_active_step_ok_export(int32_t intent_index, int32_t step_index)
{
    bool result = false;
    pthread_mutex_lock(&pgy_intent_registry_mutex);
    PgyIntentHistoryStep *step =
        pgy_intent_active_step_by_handle_locked_export(intent_index, step_index);
    if (step != NULL)
        result = step->ok;
    pthread_mutex_unlock(&pgy_intent_registry_mutex);
    return result;
}

static inline int32_t
pgy_intent_recent_count_export(void)
{
    int32_t count;

    pthread_mutex_lock(&pgy_intent_registry_mutex);
    count = pgy_intent_recent_count;
    pthread_mutex_unlock(&pgy_intent_registry_mutex);
    return count;
}

static inline int32_t
pgy_intent_recent_handle_export(int32_t index)
{
    int32_t result = 0;
    pthread_mutex_lock(&pgy_intent_registry_mutex);
    PgyIntentRecentEntry *entry =
        pgy_intent_recent_entry_by_index_locked_export(index);
    if (entry != NULL)
        result = entry->handle;
    pthread_mutex_unlock(&pgy_intent_registry_mutex);
    return result;
}

static inline int32_t
pgy_intent_recent_trace_id_export(int32_t index)
{
    int32_t result = 0;
    pthread_mutex_lock(&pgy_intent_registry_mutex);
    PgyIntentRecentEntry *entry =
        pgy_intent_recent_entry_by_index_locked_export(index);
    if (entry != NULL)
        result = entry->trace_id;
    pthread_mutex_unlock(&pgy_intent_registry_mutex);
    return result;
}

static inline char *
pgy_intent_recent_name_export(int32_t index)
{
    char *result = "";

    pthread_mutex_lock(&pgy_intent_registry_mutex);
    PgyIntentRecentEntry *entry =
        pgy_intent_recent_entry_by_index_locked_export(index);
    if (entry != NULL && entry->name != NULL)
        result = pgy_intent_borrowed_snapshot(entry->name);
    pthread_mutex_unlock(&pgy_intent_registry_mutex);
    return result;
}

static inline char *
pgy_intent_recent_trace_export(int32_t index)
{
    char *result = "";

    pthread_mutex_lock(&pgy_intent_registry_mutex);
    PgyIntentRecentEntry *entry =
        pgy_intent_recent_entry_by_index_locked_export(index);
    if (entry != NULL && entry->trace != NULL)
        result = pgy_intent_borrowed_snapshot(entry->trace);
    pthread_mutex_unlock(&pgy_intent_registry_mutex);
    return result;
}

static inline char *
pgy_intent_recent_failure_export(int32_t index)
{
    char *result = "";

    pthread_mutex_lock(&pgy_intent_registry_mutex);
    PgyIntentRecentEntry *entry =
        pgy_intent_recent_entry_by_index_locked_export(index);
    if (entry != NULL && entry->failure_reason != NULL)
        result = pgy_intent_borrowed_snapshot(entry->failure_reason);
    pthread_mutex_unlock(&pgy_intent_registry_mutex);
    return result;
}

static inline int32_t
pgy_intent_recent_step_count_export(int32_t index)
{
    int32_t result = 0;

    pthread_mutex_lock(&pgy_intent_registry_mutex);
    PgyIntentRecentEntry *entry =
        pgy_intent_recent_entry_by_index_locked_export(index);
    if (entry != NULL)
        result = entry->step_count;
    pthread_mutex_unlock(&pgy_intent_registry_mutex);
    return result;
}

static inline bool
pgy_intent_recent_failed_export(int32_t index)
{
    bool result = false;

    pthread_mutex_lock(&pgy_intent_registry_mutex);
    PgyIntentRecentEntry *entry =
        pgy_intent_recent_entry_by_index_locked_export(index);
    if (entry != NULL)
        result = entry->failed;
    pthread_mutex_unlock(&pgy_intent_registry_mutex);
    return result;
}

#endif /* PGY_RUNTIME_INTENT_QUERY_INLINE_H */
