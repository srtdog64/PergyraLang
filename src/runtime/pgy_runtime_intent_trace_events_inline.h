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
        pgy_intent_append_line_len(&entry->trace, &entry->trace_len, line);
        if (entry->step_count <= PGY_INTENT_ACTIVE_MAX) {
            int32_t index = entry->step_count - 1;
            pgy_intent_history_step_clear(&entry->steps[index]);
            entry->steps[index].name = pgy_runtime_strdup(step_name != NULL ? step_name : "");
            entry->steps[index].zone = pgy_runtime_strdup(zone_name != NULL ? zone_name : "");
            entry->steps[index].phase = pgy_runtime_strdup("begin");
            entry->steps[index].ok = false;
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
        pgy_intent_append_line_len(&entry->trace, &entry->trace_len, line);
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
        pgy_intent_append_line_len(&entry->trace, &entry->trace_len, line);
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
        pgy_intent_append_line_len(&entry->trace, &entry->trace_len, line);
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
        pgy_intent_append_line_len(&entry->trace, &entry->trace_len, line);
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
        pgy_intent_append_line_len(&entry->trace, &entry->trace_len, line);
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
        pgy_intent_append_line_len(&entry->trace, &entry->trace_len, line);
    }
    pthread_mutex_unlock(&pgy_intent_registry_mutex);
}

static inline void
pgy_mir_cleanup_op_export(int32_t handle,
                          const char *op_name,
                          const char *slot_anchor,
                          const char *arg_name)
{
    (void)handle;
    (void)op_name;
    (void)slot_anchor;
    (void)arg_name;
}
