/* Runtime intent recent accessors, panic helpers, and checked arithmetic exports. */

static inline bool
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
    PgyIntentRecentEntry *entry = pgy_intent_recent_entry_by_index_export(index);
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
    PgyIntentRecentEntry *entry = pgy_intent_recent_entry_by_index_export(index);
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
    PgyIntentRecentEntry *entry = pgy_intent_recent_entry_by_index_export(index);
    if (entry != NULL && entry->name != NULL)
        result = entry->name;
    pthread_mutex_unlock(&pgy_intent_registry_mutex);
    return result;
}

static inline char *
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

static inline char *
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

static inline int32_t
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

static inline bool
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
 * Panic ??Unrecoverable Error
 * ================================================================= */

#define PGY_PANIC(msg) \
    PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_INTERNAL_INVARIANT, (msg))

#define PGY_ASSERT(cond, msg) \
    do { \
        if (!(cond)) { \
            PGY_PANIC(msg); \
        } \
    } while (0)

static inline void
pgy_runtime_panic_internal_invariant_export(const char *reason)
{
    PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_INTERNAL_INVARIANT,
                      reason != NULL ? reason : "runtime invariant failed");
}

static inline int32_t
pgy_checked_div_i32_export(int32_t lhs, int32_t rhs)
{
    if (rhs == 0)
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_DIVIDE_BY_ZERO,
                          PGY_RUNTIME_PANIC_REASON_DIVIDE_BY_ZERO);
    return lhs / rhs;
}

static inline int64_t
pgy_checked_div_i64_export(int64_t lhs, int64_t rhs)
{
    if (rhs == 0)
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_DIVIDE_BY_ZERO,
                          PGY_RUNTIME_PANIC_REASON_DIVIDE_BY_ZERO);
    return lhs / rhs;
}

static inline int32_t
pgy_checked_mod_i32_export(int32_t lhs, int32_t rhs)
{
    if (rhs == 0)
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_DIVIDE_BY_ZERO,
                          PGY_RUNTIME_PANIC_REASON_DIVIDE_BY_ZERO);
    return lhs % rhs;
}

static inline int64_t
pgy_checked_mod_i64_export(int64_t lhs, int64_t rhs)
{
    if (rhs == 0)
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_DIVIDE_BY_ZERO,
                          PGY_RUNTIME_PANIC_REASON_DIVIDE_BY_ZERO);
    return lhs % rhs;
}
