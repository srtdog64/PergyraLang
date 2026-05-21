#ifndef PGY_RUNTIME_INTENT_EXIT_H
#define PGY_RUNTIME_INTENT_EXIT_H

static inline void
pgy_intent_exit_export(int32_t handle)
{
    if (handle == 0)
        return;

    pgy_intent_pop_current_handle(handle);
    pthread_mutex_lock(&pgy_intent_registry_mutex);

    {
        int32_t active_slot = pgy_intent_find_active_registry_slot(handle);
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
            pgy_intent_recent_entry_copy_from_active(
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
        entry->subject_fingerprint = 0;
        entry->is_concurrent = false;
        entry->priority = 0;
        entry->trace_id = 0;
        entry->trace = NULL;
        entry->trace_len = 0;
        entry->failure_reason = NULL;
        entry->step_count = 0;
        entry->failed = false;
        pgy_intent_active_index_clear(handle);
        entry->active = false;
        pgy_intent_note_free_active_slot(active_slot);
        if (pgy_intent_active_count > 0)
            pgy_intent_active_count--;
    }

    pthread_mutex_unlock(&pgy_intent_registry_mutex);
}

#endif /* PGY_RUNTIME_INTENT_EXIT_H */
