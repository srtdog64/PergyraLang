#ifndef PGY_RUNTIME_LIB_INTENT_RECENT_EXPORTS_H
#define PGY_RUNTIME_LIB_INTENT_RECENT_EXPORTS_H

int32_t
pgy_intent_recent_count_export(void)
{
    int32_t count;

    pthread_mutex_lock(&pgy_intent_registry_mutex);
    count = pgy_intent_recent_count;
    pthread_mutex_unlock(&pgy_intent_registry_mutex);
    return count;
}

int32_t
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

int32_t
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

char *
pgy_intent_recent_name_export(int32_t index)
{
    char *result = "";

    pthread_mutex_lock(&pgy_intent_registry_mutex);
    PgyIntentRecentEntry *entry =
        pgy_intent_recent_entry_by_index_locked_export(index);
    if (entry != NULL && entry->name != NULL)
        result = pgy_intent_borrowed_snapshot_export(entry->name);
    pthread_mutex_unlock(&pgy_intent_registry_mutex);
    return result;
}

char *
pgy_intent_recent_trace_export(int32_t index)
{
    char *result = "";

    pthread_mutex_lock(&pgy_intent_registry_mutex);
    PgyIntentRecentEntry *entry =
        pgy_intent_recent_entry_by_index_locked_export(index);
    if (entry != NULL && entry->trace != NULL)
        result = pgy_intent_borrowed_snapshot_export(entry->trace);
    pthread_mutex_unlock(&pgy_intent_registry_mutex);
    return result;
}

char *
pgy_intent_recent_failure_export(int32_t index)
{
    char *result = "";

    pthread_mutex_lock(&pgy_intent_registry_mutex);
    PgyIntentRecentEntry *entry =
        pgy_intent_recent_entry_by_index_locked_export(index);
    if (entry != NULL && entry->failure_reason != NULL)
        result = pgy_intent_borrowed_snapshot_export(entry->failure_reason);
    pthread_mutex_unlock(&pgy_intent_registry_mutex);
    return result;
}

int32_t
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

bool
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

#endif /* PGY_RUNTIME_LIB_INTENT_RECENT_EXPORTS_H */
