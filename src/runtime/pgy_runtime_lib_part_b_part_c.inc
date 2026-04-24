
char *
pgy_intent_last_trace_export(void)
{
    return pgy_intent_last_trace != NULL ? pgy_intent_last_trace : "";
}

char *
pgy_intent_last_failure_export(void)
{
    return pgy_intent_last_failure != NULL ? pgy_intent_last_failure : "";
}

char *
pgy_intent_last_name_export(void)
{
    return pgy_intent_last_name != NULL ? pgy_intent_last_name : "";
}

int32_t
pgy_intent_last_handle_export(void)
{
    return pgy_intent_last_handle;
}

int32_t
pgy_intent_last_trace_id_export(void)
{
    return pgy_intent_last_trace_id;
}

int32_t
pgy_intent_last_step_count_export(void)
{
    return pgy_intent_last_step_count;
}

bool
pgy_intent_last_failed_export(void)
{
    return pgy_intent_last_failed;
}

int32_t
pgy_intent_history_count_export(void)
{
    return pgy_intent_last_history_count;
}

static PgyIntentActiveEntry *
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

static inline PgyIntentActiveEntry *
pgy_intent_active_entry_by_handle_export(int32_t handle)
{
    return pgy_intent_find_active_entry_export(handle);
}

static PgyIntentRecentEntry *
pgy_intent_recent_entry_by_index_export(int32_t index)
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

static PgyIntentHistoryStep *
pgy_intent_active_step_by_index_export(int32_t intent_index, int32_t step_index)
{
    PgyIntentActiveEntry *entry = pgy_intent_active_entry_by_handle_export(intent_index);
    if (entry == NULL)
        return NULL;
    if (step_index < 0 || step_index >= entry->step_count) {
        pgy_runtime_warn_invalid_intent_index("active_step_by_index", step_index,
                                              entry->step_count);
        return NULL;
    }
    return &entry->steps[step_index];
}

char *
pgy_intent_history_step_name_export(int32_t index)
{
    if (index < 0 || index >= pgy_intent_last_history_count) {
        pgy_runtime_warn_invalid_intent_index("history_step_name", index,
                                              pgy_intent_last_history_count);
        return "";
    }
    return pgy_intent_last_steps[index].name != NULL ? pgy_intent_last_steps[index].name : "";
}

char *
pgy_intent_history_step_zone_export(int32_t index)
{
    if (index < 0 || index >= pgy_intent_last_history_count) {
        pgy_runtime_warn_invalid_intent_index("history_step_zone", index,
                                              pgy_intent_last_history_count);
        return "";
    }
    return pgy_intent_last_steps[index].zone != NULL ? pgy_intent_last_steps[index].zone : "";
}

char *
pgy_intent_history_step_phase_export(int32_t index)
{
    if (index < 0 || index >= pgy_intent_last_history_count) {
        pgy_runtime_warn_invalid_intent_index("history_step_phase", index,
                                              pgy_intent_last_history_count);
        return "";
    }
    return pgy_intent_last_steps[index].phase != NULL ? pgy_intent_last_steps[index].phase : "";
}

char *
pgy_intent_history_step_participant_export(int32_t index)
{
    if (index < 0 || index >= pgy_intent_last_history_count) {
        pgy_runtime_warn_invalid_intent_index("history_step_participant", index,
                                              pgy_intent_last_history_count);
        return "";
    }
    return pgy_intent_last_steps[index].participant != NULL ? pgy_intent_last_steps[index].participant : "";
}

char *
pgy_intent_history_step_slot_export(int32_t index)
{
    if (index < 0 || index >= pgy_intent_last_history_count) {
        pgy_runtime_warn_invalid_intent_index("history_step_slot", index,
                                              pgy_intent_last_history_count);
        return "";
    }
    return pgy_intent_last_steps[index].slot != NULL ? pgy_intent_last_steps[index].slot : "";
}

char *
pgy_intent_history_step_from_zone_export(int32_t index)
{
    if (index < 0 || index >= pgy_intent_last_history_count) {
        pgy_runtime_warn_invalid_intent_index("history_step_from_zone", index,
                                              pgy_intent_last_history_count);
        return "";
    }
    return pgy_intent_last_steps[index].from_zone != NULL ? pgy_intent_last_steps[index].from_zone : "";
}

char *
pgy_intent_history_step_from_slot_export(int32_t index)
{
    if (index < 0 || index >= pgy_intent_last_history_count) {
        pgy_runtime_warn_invalid_intent_index("history_step_from_slot", index,
                                              pgy_intent_last_history_count);
        return "";
    }
    return pgy_intent_last_steps[index].from_slot != NULL ? pgy_intent_last_steps[index].from_slot : "";
}

char *
pgy_intent_history_step_to_zone_export(int32_t index)
{
    if (index < 0 || index >= pgy_intent_last_history_count) {
        pgy_runtime_warn_invalid_intent_index("history_step_to_zone", index,
                                              pgy_intent_last_history_count);
        return "";
    }
    return pgy_intent_last_steps[index].to_zone != NULL ? pgy_intent_last_steps[index].to_zone : "";
}

char *
pgy_intent_history_step_to_slot_export(int32_t index)
{
    if (index < 0 || index >= pgy_intent_last_history_count) {
        pgy_runtime_warn_invalid_intent_index("history_step_to_slot", index,
                                              pgy_intent_last_history_count);
        return "";
    }
    return pgy_intent_last_steps[index].to_slot != NULL ? pgy_intent_last_steps[index].to_slot : "";
}

bool
pgy_intent_history_step_ok_export(int32_t index)
{
    if (index < 0 || index >= pgy_intent_last_history_count) {
        pgy_runtime_warn_invalid_intent_index("history_step_ok", index,
                                              pgy_intent_last_history_count);
        return false;
    }
    return pgy_intent_last_steps[index].ok;
}

char *
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

int32_t
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

char *
pgy_intent_active_name_export(int32_t index)
{
    char *result = "";

    pthread_mutex_lock(&pgy_intent_registry_mutex);
    PgyIntentActiveEntry *entry = pgy_intent_active_entry_by_handle_export(index);
    if (entry != NULL && entry->name != NULL)
        result = entry->name;
    pthread_mutex_unlock(&pgy_intent_registry_mutex);
    return result;
}

int32_t
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

int32_t
pgy_intent_active_priority_export(int32_t index)
{
    int32_t result = 0;

    pthread_mutex_lock(&pgy_intent_registry_mutex);
    PgyIntentActiveEntry *entry = pgy_intent_active_entry_by_handle_export(index);
    if (entry != NULL)
        result = entry->priority;
    pthread_mutex_unlock(&pgy_intent_registry_mutex);
    return result;
}

int32_t
pgy_intent_active_trace_id_export(int32_t index)
{
    int32_t result = 0;

    pthread_mutex_lock(&pgy_intent_registry_mutex);
    PgyIntentActiveEntry *entry = pgy_intent_active_entry_by_handle_export(index);
    if (entry != NULL)
        result = entry->trace_id;
    pthread_mutex_unlock(&pgy_intent_registry_mutex);
    return result;
}

bool
pgy_intent_active_concurrent_export(int32_t index)
{
    bool result = false;

    pthread_mutex_lock(&pgy_intent_registry_mutex);
    PgyIntentActiveEntry *entry = pgy_intent_active_entry_by_handle_export(index);
    if (entry != NULL)
        result = entry->is_concurrent;
    pthread_mutex_unlock(&pgy_intent_registry_mutex);
    return result;
}

char *
pgy_intent_active_trace_export(int32_t index)
{
    char *result = "";

    pthread_mutex_lock(&pgy_intent_registry_mutex);
    PgyIntentActiveEntry *entry = pgy_intent_active_entry_by_handle_export(index);
    if (entry != NULL && entry->trace != NULL)
        result = entry->trace;
    pthread_mutex_unlock(&pgy_intent_registry_mutex);
    return result;
}

int32_t
pgy_intent_active_parent_handle_export(int32_t index)
{
    int32_t result = 0;

    pthread_mutex_lock(&pgy_intent_registry_mutex);
    PgyIntentActiveEntry *entry = pgy_intent_active_entry_by_handle_export(index);
    if (entry != NULL)
        result = entry->parent_handle;
    pthread_mutex_unlock(&pgy_intent_registry_mutex);
    return result;
}

int32_t
pgy_intent_active_subject_count_export(int32_t index)
{
    int32_t result = 0;

    pthread_mutex_lock(&pgy_intent_registry_mutex);
    PgyIntentActiveEntry *entry = pgy_intent_active_entry_by_handle_export(index);
    if (entry != NULL)
        result = entry->subject_count;
    pthread_mutex_unlock(&pgy_intent_registry_mutex);
    return result;
}

int32_t
pgy_intent_active_step_count_export(int32_t index)
{
    int32_t result = 0;

    pthread_mutex_lock(&pgy_intent_registry_mutex);
    PgyIntentActiveEntry *entry = pgy_intent_active_entry_by_handle_export(index);
    if (entry != NULL)
        result = entry->step_count;
    pthread_mutex_unlock(&pgy_intent_registry_mutex);
    return result;
}

bool
pgy_intent_active_failed_export(int32_t index)
{
    bool result = false;

    pthread_mutex_lock(&pgy_intent_registry_mutex);
    PgyIntentActiveEntry *entry = pgy_intent_active_entry_by_handle_export(index);
    if (entry != NULL)
        result = entry->failed;
    pthread_mutex_unlock(&pgy_intent_registry_mutex);
    return result;
}

char *
pgy_intent_active_failure_export(int32_t index)
{
    char *result = "";

    pthread_mutex_lock(&pgy_intent_registry_mutex);
    PgyIntentActiveEntry *entry = pgy_intent_active_entry_by_handle_export(index);
    if (entry != NULL && entry->failure_reason != NULL)
        result = entry->failure_reason;
    pthread_mutex_unlock(&pgy_intent_registry_mutex);
    return result;
}

char *
pgy_intent_active_step_name_export(int32_t intent_index, int32_t step_index)
{
    char *result = "";
    pthread_mutex_lock(&pgy_intent_registry_mutex);
    PgyIntentHistoryStep *step =
        pgy_intent_active_step_by_index_export(intent_index, step_index);
    if (step != NULL && step->name != NULL)
        result = step->name;
    pthread_mutex_unlock(&pgy_intent_registry_mutex);
    return result;
}

#define PGY_INTENT_ACTIVE_STEP_STRING_EXPORT(fn_name, field_name) \
char * \
fn_name(int32_t intent_index, int32_t step_index) \
{ \
    char *result = ""; \
    pthread_mutex_lock(&pgy_intent_registry_mutex); \
    PgyIntentHistoryStep *step = \
        pgy_intent_active_step_by_index_export(intent_index, step_index); \
    if (step != NULL && step->field_name != NULL) \
        result = step->field_name; \
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

bool
pgy_intent_active_step_ok_export(int32_t intent_index, int32_t step_index)
{
    bool result = false;
    pthread_mutex_lock(&pgy_intent_registry_mutex);
    PgyIntentHistoryStep *step =
        pgy_intent_active_step_by_index_export(intent_index, step_index);
    if (step != NULL)
        result = step->ok;
    pthread_mutex_unlock(&pgy_intent_registry_mutex);
    return result;
}

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
    PgyIntentRecentEntry *entry = pgy_intent_recent_entry_by_index_export(index);
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
    PgyIntentRecentEntry *entry = pgy_intent_recent_entry_by_index_export(index);
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
    PgyIntentRecentEntry *entry = pgy_intent_recent_entry_by_index_export(index);
    if (entry != NULL && entry->name != NULL)
        result = entry->name;
    pthread_mutex_unlock(&pgy_intent_registry_mutex);
    return result;
}

char *
pgy_intent_recent_trace_export(int32_t index)
{
    char *result = "";

    pthread_mutex_lock(&pgy_intent_registry_mutex);
    PgyIntentRecentEntry *entry = pgy_intent_recent_entry_by_index_export(index);
    if (entry != NULL && entry->trace != NULL)
        result = entry->trace;
    pthread_mutex_unlock(&pgy_intent_registry_mutex);
    return result;
}

char *
pgy_intent_recent_failure_export(int32_t index)
{
    char *result = "";

    pthread_mutex_lock(&pgy_intent_registry_mutex);
    PgyIntentRecentEntry *entry = pgy_intent_recent_entry_by_index_export(index);
    if (entry != NULL && entry->failure_reason != NULL)
        result = entry->failure_reason;
    pthread_mutex_unlock(&pgy_intent_registry_mutex);
    return result;
}

int32_t
pgy_intent_recent_step_count_export(int32_t index)
{
    int32_t result = 0;

    pthread_mutex_lock(&pgy_intent_registry_mutex);
    PgyIntentRecentEntry *entry = pgy_intent_recent_entry_by_index_export(index);
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
    PgyIntentRecentEntry *entry = pgy_intent_recent_entry_by_index_export(index);
    if (entry != NULL)
        result = entry->failed;
    pthread_mutex_unlock(&pgy_intent_registry_mutex);
    return result;
}

void
pgy_intent_exit_export(int32_t handle)
{
    if (handle == 0)
        return;

    pgy_intent_pop_current_handle_export(handle);
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
                free(pgy_intent_last_steps[j].name);
                free(pgy_intent_last_steps[j].zone);
                free(pgy_intent_last_steps[j].failure_reason);
                pgy_intent_last_steps[j].name = NULL;
                pgy_intent_last_steps[j].zone = NULL;
                pgy_intent_last_steps[j].failure_reason = NULL;
                pgy_intent_last_steps[j].ok = false;
            }
            pgy_intent_last_trace = entry->trace != NULL
                ? pgy_runtime_strdup_export(entry->trace) : pgy_runtime_strdup_export("");
            pgy_intent_last_failure = entry->failure_reason != NULL
                ? pgy_runtime_strdup_export(entry->failure_reason) : pgy_runtime_strdup_export("");
            pgy_intent_last_name = entry->name != NULL
                ? pgy_runtime_strdup_export(entry->name) : pgy_runtime_strdup_export("");
            pgy_intent_last_handle = entry->handle;
            pgy_intent_last_trace_id = entry->trace_id;
            pgy_intent_last_step_count = entry->step_count;
            pgy_intent_last_failed = entry->failed;
            pgy_intent_last_history_count = entry->step_count;
            if (pgy_intent_last_history_count > PGY_INTENT_ACTIVE_MAX)
                pgy_intent_last_history_count = PGY_INTENT_ACTIVE_MAX;
            for (int32_t j = 0; j < pgy_intent_last_history_count; j++) {
                pgy_intent_history_step_clear_export(&pgy_intent_last_steps[j]);
                pgy_intent_last_steps[j].name = pgy_runtime_strdup_export(
                    entry->steps[j].name != NULL ? entry->steps[j].name : "");
                pgy_intent_last_steps[j].zone = pgy_runtime_strdup_export(
                    entry->steps[j].zone != NULL ? entry->steps[j].zone : "");
                pgy_intent_last_steps[j].phase = pgy_runtime_strdup_export(
                    entry->steps[j].phase != NULL ? entry->steps[j].phase : "");
                pgy_intent_last_steps[j].participant = pgy_runtime_strdup_export(
                    entry->steps[j].participant != NULL ? entry->steps[j].participant : "");
                pgy_intent_last_steps[j].slot = pgy_runtime_strdup_export(
                    entry->steps[j].slot != NULL ? entry->steps[j].slot : "");
                pgy_intent_last_steps[j].from_zone = pgy_runtime_strdup_export(
                    entry->steps[j].from_zone != NULL ? entry->steps[j].from_zone : "");
                pgy_intent_last_steps[j].from_slot = pgy_runtime_strdup_export(
                    entry->steps[j].from_slot != NULL ? entry->steps[j].from_slot : "");
                pgy_intent_last_steps[j].to_zone = pgy_runtime_strdup_export(
                    entry->steps[j].to_zone != NULL ? entry->steps[j].to_zone : "");
                pgy_intent_last_steps[j].to_slot = pgy_runtime_strdup_export(
                    entry->steps[j].to_slot != NULL ? entry->steps[j].to_slot : "");
                pgy_intent_last_steps[j].ok = entry->steps[j].ok;
                pgy_intent_last_steps[j].failure_reason = pgy_runtime_strdup_export(
                    entry->steps[j].failure_reason != NULL ? entry->steps[j].failure_reason : "");
            }
            pgy_intent_recent_entry_copy_from_active_export(
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
        if (PGY_INTENT_OBSERVABILITY_ENABLED)
            free(entry->name);
        if (entry->subjects != entry->inline_subjects)
            free(entry->subjects);
        free(entry->trace);
        free(entry->failure_reason);
        if (PGY_INTENT_OBSERVABILITY_ENABLED) {
            for (int32_t j = 0; j < entry->step_count && j < PGY_INTENT_ACTIVE_MAX; j++) {
                pgy_intent_history_step_clear_export(&entry->steps[j]);
            }
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

static struct timespec
pgy_runtime_deadline_after_ns(uint64_t timeout_ns)
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

/* =================================================================
 * Slot types (must match pgy_runtime.h layout)
 * ================================================================= */

typedef struct {
    int32_t value;
    bool    claimed;
} PgySlot_Int;

typedef struct {
    int64_t value;
    bool    claimed;
} PgySlot_Long;

typedef struct {
    float   value;
    bool    claimed;
} PgySlot_Float;

typedef struct {
    double  value;
    bool    claimed;
} PgySlot_Double;

typedef struct {
    bool    value;
    bool    claimed;
} PgySlot_Bool;

typedef struct {
    char   *value;
    bool    claimed;
} PgySlot_String;

/* =================================================================
 * Slot operations — Int
 * ================================================================= */

PgySlot_Int pgy_claim_Int(void)
{
    PgySlot_Int s;
    s.value = 0;
    s.claimed = true;
    return s;
}

void pgy_write_Int(PgySlot_Int *s, int32_t v)
{
    if (s == NULL) return;
    if (!s->claimed) {
        fprintf(stderr, "[pgy] slot write after release (Int)\n");
        return;
    }
    s->value = v;
}

int32_t pgy_read_Int(PgySlot_Int *s)
{
    if (s == NULL) return 0;
    if (!s->claimed) {
        fprintf(stderr, "[pgy] slot read after release (Int)\n");
        return 0;
    }
    return s->value;
}

void pgy_release_Int(PgySlot_Int *s)
{
    if (s != NULL) {
        s->value = 0;
        s->claimed = false;
    }
}

/* =================================================================
 * Slot operations — Long
 * ================================================================= */

PgySlot_Long pgy_claim_Long(void)
{
    PgySlot_Long s;
    s.value = 0;
    s.claimed = true;
    return s;
}

void pgy_write_Long(PgySlot_Long *s, int64_t v)
{
    if (s == NULL) return;
    if (!s->claimed) { fprintf(stderr, "[pgy] slot write after release (Long)\n"); return; }
    s->value = v;
}

int64_t pgy_read_Long(PgySlot_Long *s)
{
    if (s == NULL) return 0;
    if (!s->claimed) { fprintf(stderr, "[pgy] slot read after release (Long)\n"); return 0; }
    return s->value;
}

void pgy_release_Long(PgySlot_Long *s)
{
    if (s != NULL) { s->value = 0; s->claimed = false; }
}

/* =================================================================
 * Slot operations — Float
 * ================================================================= */

PgySlot_Float pgy_claim_Float(void)
{
    PgySlot_Float s;
    s.value = 0.0f;
    s.claimed = true;
    return s;
}

void pgy_write_Float(PgySlot_Float *s, float v)
{
    if (s == NULL) return;
    if (!s->claimed) { fprintf(stderr, "[pgy] slot write after release (Float)\n"); return; }
    s->value = v;
}

float pgy_read_Float(PgySlot_Float *s)
{
    if (s == NULL) return 0.0f;
    if (!s->claimed) { fprintf(stderr, "[pgy] slot read after release (Float)\n"); return 0.0f; }
    return s->value;
}

void pgy_release_Float(PgySlot_Float *s)
{
    if (s != NULL) { s->value = 0.0f; s->claimed = false; }
}
