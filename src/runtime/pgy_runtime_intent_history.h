#ifndef PGY_RUNTIME_INTENT_HISTORY_H
#define PGY_RUNTIME_INTENT_HISTORY_H

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

#endif /* PGY_RUNTIME_INTENT_HISTORY_H */
