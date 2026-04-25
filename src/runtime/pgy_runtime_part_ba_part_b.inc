    return count;
}

static inline char *
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

static inline int32_t
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

static inline int32_t
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

static inline int32_t
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

static inline bool
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

static inline char *
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

static inline int32_t
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

static inline int32_t
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

static inline int32_t
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

static inline bool
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

static inline char *
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
        pgy_intent_active_step_by_index_export(intent_index, step_index);
    if (step != NULL && step->name != NULL)
        result = step->name;
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

static inline void
pgy_intent_exit_export(int32_t handle)
{
    if (handle == 0)
        return;

    pgy_intent_pop_current_handle(handle);
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

/* =================================================================
 * Stack Memory (Default ??Zero Overhead)
 *
 * All plain variables are stack-allocated by default in C.
 * No special macros needed.
 * ================================================================= */

/* Example:
 *   let x: Int = 42;     ??  int32_t x = 42;
 *   let v: Vec3;         ??  Vec3 v;
 */

/* =================================================================
 * Heap Memory ??Box<T> (Owned Heap Allocation)
 * ================================================================= */

#define PGY_BOX_DEFINE(SuffixName, CType) \
\
typedef struct { \
    CType* ptr; \
} PgyBox_##SuffixName; \
\
static inline PgyBox_##SuffixName \
pgy_box_new_##SuffixName(CType value) \
{ \
    PgyBox_##SuffixName b; \
    b.ptr = (CType*)malloc(sizeof(CType)); \
    if (b.ptr == NULL) { \
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_OOM, \
                          PGY_RUNTIME_PANIC_REASON_ALLOCATION_FAILED); \
    } \
    *b.ptr = value; \
    return b; \
} \
\
static inline CType \
pgy_box_get_##SuffixName(PgyBox_##SuffixName b) \
{ \
    if (b.ptr == NULL) { \
        PGY_PANIC("Box access after move/free"); \
    } \
    return *b.ptr; \
} \
\
static inline void \
pgy_box_set_##SuffixName(PgyBox_##SuffixName* b, CType value) \
{ \
    if (b->ptr == NULL) { \
        PGY_PANIC("Box set after move/free"); \
    } \
    *b->ptr = value; \
} \
\
static inline void \
pgy_box_drop_##SuffixName(PgyBox_##SuffixName* b) \
{ \
    if (b->ptr != NULL) { \
        free(b->ptr); \
        b->ptr = NULL; \
    } \
} \
\
static inline bool \
pgy_box_is_valid_##SuffixName(PgyBox_##SuffixName* b) \
{ \
    return b->ptr != NULL; \
}

/* Box move semantics (transfer ownership) */
#define PGY_BOX_MOVE(dst, src, SuffixName) \
    do { \
        (dst) = (src); \
        (src).ptr = NULL; \
    } while (0)

/* =================================================================
 * Heap Memory ??Arena Allocator (Frame-based)
 * ================================================================= */

typedef struct {
    char*  buffer;
    size_t capacity;
    size_t offset;
} PgyArena;

static inline PgyArena
pgy_arena_create(size_t capacity)
{
    PgyArena arena;
    arena.buffer = (char*)malloc(capacity);
    if (arena.buffer == NULL && capacity > 0) {
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_OOM,
                          PGY_RUNTIME_PANIC_REASON_ALLOCATION_FAILED);
    }
    arena.capacity = capacity;
    arena.offset = 0;
    return arena;
}

static inline void
pgy_arena_destroy(PgyArena* arena)
{
    if (arena->buffer != NULL) {
        free(arena->buffer);
        arena->buffer = NULL;
    }
}

static inline void*
pgy_arena_alloc(PgyArena* arena, size_t size, size_t align)
{
    /* Align the current offset */
    size_t aligned_offset = (arena->offset + align - 1) & ~(align - 1);

    if (aligned_offset + size > arena->capacity) {
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_OOM,
                          PGY_RUNTIME_PANIC_REASON_ARENA_OUT_OF_MEMORY);
    }

    void* ptr = arena->buffer + aligned_offset;
    arena->offset = aligned_offset + size;
    return ptr;
}

#define PGY_ARENA_ALLOC(arena, Type) \
    ((Type*)pgy_arena_alloc((arena), sizeof(Type), _Alignof(Type)))

#define PGY_ARENA_ALLOC_ARRAY(arena, Type, count) \
    ((Type*)pgy_arena_alloc((arena), sizeof(Type) * (count), _Alignof(Type)))

static inline void
pgy_arena_reset(PgyArena* arena)
{
    arena->offset = 0;
}

/* =================================================================
 * Allocator Interface
 * ================================================================= */

typedef enum {
    PGY_ALLOC_SYSTEM,
    PGY_ALLOC_TRACING,
    PGY_ALLOC_DEBUG,
    PGY_ALLOC_POOL
} PgyAllocatorKind;

typedef struct {
    char   *buffer;
    size_t  capacity;
    size_t  offset;
} PgyPoolAllocatorState;

typedef struct {
    PgyAllocatorKind kind;
    bool             trace_enabled;
    bool             debug_enabled;
    size_t           allocations;
    size_t           deallocations;
    size_t           bytes_in_use;
    size_t           peak_bytes;
    PgyPoolAllocatorState *pool;
} PgyAllocator;

static inline void
pgy_allocator_record_alloc(PgyAllocator *alloc, size_t size)
{
    if (alloc == NULL)
        return;
    alloc->allocations++;
    alloc->bytes_in_use += size;
    if (alloc->bytes_in_use > alloc->peak_bytes)
        alloc->peak_bytes = alloc->bytes_in_use;
    if (alloc->trace_enabled) {
        fprintf(stderr, "[PGY_ALLOC] +%zu bytes (live=%zu)\n",
                size, alloc->bytes_in_use);
    }
}

static inline void
pgy_allocator_record_free(PgyAllocator *alloc, size_t size)
{
    if (alloc == NULL)
        return;
    alloc->deallocations++;
    if (alloc->bytes_in_use >= size)
        alloc->bytes_in_use -= size;
    else
        alloc->bytes_in_use = 0;
    if (alloc->trace_enabled) {
        fprintf(stderr, "[PGY_ALLOC] -%zu bytes (live=%zu)\n",
                size, alloc->bytes_in_use);
    }
}

static inline PgyAllocator
pgy_allocator_system(void)
{
    PgyAllocator alloc;
    memset(&alloc, 0, sizeof(alloc));
    alloc.kind = PGY_ALLOC_SYSTEM;
    return alloc;
}

static inline PgyAllocator
pgy_allocator_tracing(void)
{
    PgyAllocator alloc = pgy_allocator_system();
    alloc.kind = PGY_ALLOC_TRACING;
    alloc.trace_enabled = true;
    return alloc;
}

static inline PgyAllocator
pgy_allocator_debug(void)
{
    PgyAllocator alloc = pgy_allocator_system();
    alloc.kind = PGY_ALLOC_DEBUG;
    alloc.debug_enabled = true;
    return alloc;
}

static inline PgyAllocator
pgy_allocator_pool(size_t capacity)
{
    PgyAllocator alloc = pgy_allocator_system();
    alloc.kind = PGY_ALLOC_POOL;
    alloc.pool = (PgyPoolAllocatorState*)calloc(1, sizeof(PgyPoolAllocatorState));
    if (alloc.pool == NULL)
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_OOM,
                          PGY_RUNTIME_PANIC_REASON_ALLOCATION_FAILED);
    alloc.pool->buffer = (char*)malloc(capacity);
    if (alloc.pool->buffer == NULL && capacity > 0)
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_OOM,
                          PGY_RUNTIME_PANIC_REASON_ALLOCATION_FAILED);
    alloc.pool->capacity = capacity;
    alloc.pool->offset = 0;
    return alloc;
}

static inline void
pgy_allocator_destroy(PgyAllocator *alloc)
{
    if (alloc == NULL)
        return;
    if (alloc->kind == PGY_ALLOC_POOL && alloc->pool != NULL) {
        free(alloc->pool->buffer);
        free(alloc->pool);
        alloc->pool = NULL;
    }
}

static inline void *
pgy_alloc(PgyAllocator *alloc, size_t size, size_t align)
{
    if (alloc != NULL && alloc->kind == PGY_ALLOC_POOL) {
        size_t offset = (alloc->pool->offset + align - 1) & ~(align - 1);
        if (offset + size > alloc->pool->capacity)
            PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_OOM,
                              PGY_RUNTIME_PANIC_REASON_POOL_OUT_OF_MEMORY);
        void *ptr = alloc->pool->buffer + offset;
        alloc->pool->offset = offset + size;
        pgy_allocator_record_alloc(alloc, size);
        if (alloc->debug_enabled)
            memset(ptr, 0xCD, size);
        return ptr;
    }

    void *ptr = malloc(size);
    if (ptr == NULL && size > 0)
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_OOM,
                          PGY_RUNTIME_PANIC_REASON_ALLOCATION_FAILED);
    if (alloc != NULL) {
        pgy_allocator_record_alloc(alloc, size);
        if (alloc->debug_enabled)
            memset(ptr, 0xCD, size);
    }
    return ptr;
}

static inline void *
pgy_realloc(PgyAllocator *alloc, void *ptr, size_t old_size, size_t new_size)
{
    if (alloc != NULL && alloc->kind == PGY_ALLOC_POOL) {
        if (ptr == NULL)
            return pgy_alloc(alloc, new_size, _Alignof(max_align_t));
        PGY_PANIC("Pool allocator does not support realloc");
    }

    void *grown = realloc(ptr, new_size);
    if (grown == NULL && new_size > 0)
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_OOM,
                          PGY_RUNTIME_PANIC_REASON_ALLOCATION_FAILED);

    if (alloc != NULL) {
        pgy_allocator_record_free(alloc, old_size);
        pgy_allocator_record_alloc(alloc, new_size);
        if (alloc->debug_enabled && new_size > old_size) {
            memset((char*)grown + old_size, 0xCD, new_size - old_size);
        }
    }
    return grown;
}

static inline void
pgy_free(PgyAllocator *alloc, void *ptr, size_t size)
{
    if (ptr == NULL)
        return;

    if (alloc != NULL && alloc->kind == PGY_ALLOC_POOL) {
        if (alloc->debug_enabled)
            memset(ptr, 0xDD, size);
        pgy_allocator_record_free(alloc, size);
        return;
    }

    if (alloc != NULL) {
        if (alloc->debug_enabled)
            memset(ptr, 0xDD, size);
        pgy_allocator_record_free(alloc, size);
    }
    free(ptr);
}

/* =================================================================
 * Array / Slice
 * ================================================================= */
