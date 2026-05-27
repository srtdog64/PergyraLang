#ifndef PGY_RUNTIME_INTENT_HISTORY_H
#define PGY_RUNTIME_INTENT_HISTORY_H

#define PGY_INTENT_HISTORY_STRING_EXPORT(FnName, FieldName, Label)              \
static inline char *                                                            \
FnName(int32_t index)                                                           \
{                                                                               \
    char *result;                                                               \
    pthread_mutex_lock(&pgy_intent_registry_mutex);                             \
    if (index < 0 || index >= pgy_intent_last_history_count) {                  \
        pgy_runtime_warn_invalid_intent_index(Label, index,                     \
                                              pgy_intent_last_history_count);   \
        pthread_mutex_unlock(&pgy_intent_registry_mutex);                       \
        return "";                                                             \
    }                                                                           \
    result = pgy_intent_borrowed_snapshot(                                      \
        pgy_intent_last_steps[index].FieldName);                                \
    pthread_mutex_unlock(&pgy_intent_registry_mutex);                           \
    return result;                                                              \
}

PGY_INTENT_HISTORY_STRING_EXPORT(pgy_intent_history_step_name_export,
                                 name, "history_step_name")
PGY_INTENT_HISTORY_STRING_EXPORT(pgy_intent_history_step_zone_export,
                                 zone, "history_step_zone")
PGY_INTENT_HISTORY_STRING_EXPORT(pgy_intent_history_step_phase_export,
                                 phase, "history_step_phase")
PGY_INTENT_HISTORY_STRING_EXPORT(pgy_intent_history_step_participant_export,
                                 participant, "history_step_participant")
PGY_INTENT_HISTORY_STRING_EXPORT(pgy_intent_history_step_slot_export,
                                 slot, "history_step_slot")
PGY_INTENT_HISTORY_STRING_EXPORT(pgy_intent_history_step_from_zone_export,
                                 from_zone, "history_step_from_zone")
PGY_INTENT_HISTORY_STRING_EXPORT(pgy_intent_history_step_from_slot_export,
                                 from_slot, "history_step_from_slot")
PGY_INTENT_HISTORY_STRING_EXPORT(pgy_intent_history_step_to_zone_export,
                                 to_zone, "history_step_to_zone")
PGY_INTENT_HISTORY_STRING_EXPORT(pgy_intent_history_step_to_slot_export,
                                 to_slot, "history_step_to_slot")
PGY_INTENT_HISTORY_STRING_EXPORT(pgy_intent_history_step_failure_export,
                                 failure_reason, "history_step_failure")

#undef PGY_INTENT_HISTORY_STRING_EXPORT

static inline bool
pgy_intent_history_step_ok_export(int32_t index)
{
    bool result;

    pthread_mutex_lock(&pgy_intent_registry_mutex);
    if (index < 0 || index >= pgy_intent_last_history_count) {
        pgy_runtime_warn_invalid_intent_index("history_step_ok", index,
                                              pgy_intent_last_history_count);
        pthread_mutex_unlock(&pgy_intent_registry_mutex);
        return false;
    }
    result = pgy_intent_last_steps[index].ok;
    pthread_mutex_unlock(&pgy_intent_registry_mutex);
    return result;
}

#endif /* PGY_RUNTIME_INTENT_HISTORY_H */
