    q.data = (char **)calloc(q.capacity, sizeof(char *));
    if (q.data == NULL) {
        q.capacity = 0;
        pgy_runtime_warn_invalid_collection("queue_new_string", "allocation failed");
    }
    return q;
}

static inline void pgy_queue_push_string(PgyQueue_String *q, const char *val)
{
    if (q == NULL || (q->data == NULL && q->capacity == 0)) {
        pgy_runtime_warn_invalid_collection("queue_push_string", "queue is not initialized");
        return;
    }
    if (q->count >= q->capacity) {
        size_t nc = q->capacity == 0 ? 16 : q->capacity * 2;
        char **nd = (char **)calloc(nc, sizeof(char *));
        if (nd == NULL) {
            pgy_runtime_warn_invalid_collection("queue_push_string", "growth allocation failed");
            return;
        }
        for (size_t i = 0; i < q->count; i++)
            nd[i] = q->data[(q->head + i) % q->capacity];
        free(q->data);
        q->data = nd;
        q->head = 0;
        q->tail = q->count;
        q->capacity = nc;
    }
    q->data[q->tail] = (char *)val;
    q->tail = (q->tail + 1) % q->capacity;
    q->count++;
}

static inline char *pgy_queue_pop_string(PgyQueue_String *q)
{
    char *out = NULL;
    if (q == NULL || q->data == NULL || q->capacity == 0)
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_INTERNAL_INVARIANT,
                          "queue pop on invalid queue");
    if (q->count == 0)
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_OUT_OF_BOUNDS,
                          "queue pop from empty queue");
    out = q->data[q->head];
    q->head = (q->head + 1) % q->capacity;
    q->count--;
    return out;
}

static inline int32_t pgy_queue_size_string(PgyQueue_String *q)
{
    return q != NULL ? (int32_t)q->count : 0;
}

static inline bool pgy_queue_empty_string(PgyQueue_String *q)
{
    return q == NULL || q->count == 0;
}

/* =================================================================
 * Object Pool — fixed-capacity reusable slot pool
 * use pool;
 * ================================================================= */

typedef struct
{
    void    *data;          /* flat array of items */
    uint8_t *alive;         /* alive flags */
    size_t   item_size;
    size_t   capacity;
    size_t   count;
} PgyPool;

static inline PgyPool pgy_pool_create(size_t item_size, size_t capacity)
{
    PgyPool p;
    p.item_size = item_size;
    p.capacity = capacity;
    p.count = 0;
    p.data = calloc(capacity, item_size);
    p.alive = (uint8_t *)calloc(capacity, sizeof(uint8_t));
    return p;
}

static inline int32_t pgy_pool_spawn(PgyPool *p, const void *item)
{
    for (size_t i = 0; i < p->capacity; i++) {
        if (!p->alive[i]) {
            memcpy((char *)p->data + i * p->item_size, item, p->item_size);
            p->alive[i] = 1;
            p->count++;
            return (int32_t)i;
        }
    }
    return -1; /* pool full */
}

static inline void pgy_pool_despawn(PgyPool *p, int32_t index)
{
    if (index >= 0 && (size_t)index < p->capacity && p->alive[index]) {
        p->alive[index] = 0;
        p->count--;
    }
}

static inline void *pgy_pool_get(PgyPool *p, int32_t index)
{
    if (index < 0 || (size_t)index >= p->capacity || !p->alive[index])
        return NULL;
    return (char *)p->data + (size_t)index * p->item_size;
}

static inline bool pgy_pool_alive(PgyPool *p, int32_t index)
{
    return index >= 0 && (size_t)index < p->capacity && p->alive[index];
}

static inline int32_t pgy_pool_count(PgyPool *p) { return (int32_t)p->count; }
static inline int32_t pgy_pool_capacity(PgyPool *p) { return (int32_t)p->capacity; }

/* =================================================================
 * FSM — Finite State Machine
 * use fsm;
 * ================================================================= */

#define PGY_FSM_MAX_STATES 32

typedef struct
{
    int32_t current;
    int32_t transitions[PGY_FSM_MAX_STATES][PGY_FSM_MAX_STATES]; /* transition[from][input] = to */
    char   *state_names[PGY_FSM_MAX_STATES];
    size_t  state_count;
} PgyFsm;

static inline PgyFsm pgy_fsm_new(void)
{
    PgyFsm f;
    memset(&f, 0, sizeof(f));
    f.current = 0;
    for (int i = 0; i < PGY_FSM_MAX_STATES; i++)
        for (int j = 0; j < PGY_FSM_MAX_STATES; j++)
            f.transitions[i][j] = -1;
    return f;
}

static inline int32_t pgy_fsm_add_state(PgyFsm *f, const char *name)
{
    if (f->state_count >= PGY_FSM_MAX_STATES) return -1;
    int32_t id = (int32_t)f->state_count;
    f->state_names[id] = pgy_runtime_strdup(name ? name : "");
    f->state_count++;
    return id;
}

static inline void pgy_fsm_add_transition(PgyFsm *f, int32_t from, int32_t input, int32_t to)
{
    if (from >= 0 && from < PGY_FSM_MAX_STATES && input >= 0 && input < PGY_FSM_MAX_STATES)
        f->transitions[from][input] = to;
}

static inline bool pgy_fsm_step(PgyFsm *f, int32_t input)
{
    if (f->current < 0 || f->current >= PGY_FSM_MAX_STATES) return false;
    int32_t next = f->transitions[f->current][input];
    if (next < 0) return false;
    f->current = next;
    return true;
}

static inline int32_t pgy_fsm_current(PgyFsm *f) { return f->current; }

static inline const char *pgy_fsm_current_name(PgyFsm *f)
{
    if (f->current < 0 || (size_t)f->current >= f->state_count) return "";
    return f->state_names[f->current] ? f->state_names[f->current] : "";
}

/* =================================================================
 * Timer / Cooldown
 * use timer;
 * ================================================================= */

typedef struct
{
    int32_t duration;
    int32_t remaining;
    bool    done;
} PgyTimer;

static inline PgyTimer pgy_timer_new(int32_t duration)
{
    PgyTimer t;
    t.duration = duration;
    t.remaining = duration;
    t.done = false;
    return t;
}

static inline void pgy_timer_tick(PgyTimer *t, int32_t delta)
{
    if (t->done) return;
    t->remaining -= delta;
    if (t->remaining <= 0) {
        t->remaining = 0;
        t->done = true;
    }
}

static inline bool pgy_timer_done(PgyTimer *t) { return t->done; }
static inline int32_t pgy_timer_remaining(PgyTimer *t) { return t->remaining; }

static inline void pgy_timer_reset(PgyTimer *t)
{
    t->remaining = t->duration;
    t->done = false;
}

typedef struct
{
    int32_t cooldown;
    int32_t remaining;
} PgyCooldown;

static inline PgyCooldown pgy_cooldown_new(int32_t cooldown)
{
    PgyCooldown c;
    c.cooldown = cooldown;
    c.remaining = 0;
    return c;
}

static inline void pgy_cooldown_tick(PgyCooldown *c, int32_t delta)
{
    c->remaining = (c->remaining > delta) ? c->remaining - delta : 0;
}

static inline bool pgy_cooldown_ready(PgyCooldown *c) { return c->remaining <= 0; }

static inline void pgy_cooldown_trigger(PgyCooldown *c)
{
    c->remaining = c->cooldown;
}

/* =================================================================
 * Parallel Support (OpenMP or Sequential Fallback)
 * ================================================================= */

#ifdef _OPENMP
#  include <omp.h>
#  define PGY_PARALLEL_BEGIN \
       _Pragma("omp parallel sections")  {
#  define PGY_PARALLEL_TASK  _Pragma("omp section")
#  define PGY_PARALLEL_END   }
#else
#  define PGY_PARALLEL_BEGIN {
#  define PGY_PARALLEL_TASK  /* sequential */
#  define PGY_PARALLEL_END   }
#endif

/* =================================================================
 * Zone Concurrency Protection
 *
 * Wraps zone struct access with rwlock when PGY_ZONE_THREADSAFE is
 * defined. Single-threaded builds use no-op macros (zero cost).
 * ================================================================= */

#ifdef PGY_ZONE_THREADSAFE
#include <pthread.h>

typedef pthread_rwlock_t PgyZoneLock;

#define PGY_ZONE_LOCK_FIELD    PgyZoneLock __zone_lock;
#define PGY_ZONE_LOCK_INIT(z)  pthread_rwlock_init(&(z)->__zone_lock, NULL)
#define PGY_ZONE_LOCK_DESTROY(z) pthread_rwlock_destroy(&(z)->__zone_lock)
#define PGY_ZONE_RDLOCK(z)     pthread_rwlock_rdlock(&(z)->__zone_lock)
#define PGY_ZONE_WRLOCK(z)     pthread_rwlock_wrlock(&(z)->__zone_lock)
#define PGY_ZONE_UNLOCK(z)     pthread_rwlock_unlock(&(z)->__zone_lock)

#else /* single-threaded: zero cost */

#define PGY_ZONE_LOCK_FIELD    /* no lock field */
#define PGY_ZONE_LOCK_INIT(z)  ((void)(z))
#define PGY_ZONE_LOCK_DESTROY(z) ((void)(z))
#define PGY_ZONE_RDLOCK(z)     ((void)(z))
#define PGY_ZONE_WRLOCK(z)     ((void)(z))
#define PGY_ZONE_UNLOCK(z)     ((void)(z))

#endif /* PGY_ZONE_THREADSAFE */

/* Generation counter for stale-state detection */
#define PGY_ZONE_GENERATION_FIELD  uint32_t __sync_generation;
#define PGY_ZONE_GENERATION_INC(z) ((z)->__sync_generation++)
static inline void
pgy_zone_generation_warn_if_stale_impl(const char *label,
                                       uint32_t expected,
                                       uint32_t actual)
{
    static unsigned pgy_zone_stale_warn_count = 0;
    static bool pgy_zone_stale_warn_suppressed = false;

    if (actual == expected)
        return;
    if (pgy_zone_stale_warn_count < 8) {
        fprintf(stderr,
            "[pgy][warn] stale zone layer read: %s expected=%u actual=%u\n",
            label,
            (unsigned)expected,
            (unsigned)actual);
        pgy_zone_stale_warn_count++;
        return;
    }
    if (!pgy_zone_stale_warn_suppressed) {
        fprintf(stderr,
            "[pgy][warn] further stale zone layer read warnings suppressed\n");
        pgy_zone_stale_warn_suppressed = true;
    }
}

#define PGY_ZONE_GENERATION_WARN_IF_STALE(z, expected, label) do {                 \
    uint32_t _pgy_expected_gen = (uint32_t)(expected);                             \
    uint32_t _pgy_actual_gen = (z)->__sync_generation;                             \
    pgy_zone_generation_warn_if_stale_impl((label),                                \
        _pgy_expected_gen,                                                          \
        _pgy_actual_gen);                                                           \
} while (0)

/* =================================================================
 * Zone Authority — runtime validation
 *
 * Compile time remains the primary line of defense, but zone-entry
 * code now performs a real runtime contract check instead of a debug
 * placeholder. Generated C uses the inline validator below; LLVM code
 * calls the exported twin in pgy_runtime_lib.c.
 * ================================================================= */
static inline void
pgy_zone_authority_record_last(bool ok,
                               const char *zone_name,
                               const char *participant_name,
                               const char *code,
                               const char *reason)
{
    const char *resolved_zone = zone_name != NULL ? zone_name : "<zone>";
    const char *resolved_participant =
        participant_name != NULL ? participant_name : "<participant>";

    pgy_zone_authority_last_ok = ok;
    snprintf(pgy_zone_authority_last_zone, sizeof(pgy_zone_authority_last_zone),
             "%s", resolved_zone);
    snprintf(pgy_zone_authority_last_participant,
             sizeof(pgy_zone_authority_last_participant),
             "%s", resolved_participant);
    snprintf(pgy_zone_authority_last_code, sizeof(pgy_zone_authority_last_code),
             "%s", code != NULL ? code
                                 : (ok ? PGY_ZONE_AUTHORITY_CODE_OK
                                       : PGY_ZONE_AUTHORITY_CODE_UNKNOWN));
    snprintf(pgy_zone_authority_last_reason, sizeof(pgy_zone_authority_last_reason),
             "%s", reason != NULL ? reason : "");
}

static inline bool
pgy_zone_authority_validate_impl(void *zone_ptr, void *participant_ptr,
                                 const char *zone_name,
                                 const char *participant_name,
                                 bool emit_stderr)
{
    const char *resolved_zone = zone_name != NULL ? zone_name : "<zone>";
    const char *resolved_participant =
        participant_name != NULL ? participant_name : "<participant>";

    if (zone_ptr == NULL) {
        pgy_zone_authority_record_last(false, resolved_zone, resolved_participant,
            PGY_ZONE_AUTHORITY_CODE_MISSING_ZONE,
            PGY_ZONE_AUTHORITY_REASON_MISSING_ZONE);
        if (emit_stderr) {
            fprintf(stderr, PGY_ZONE_AUTHORITY_STDERR_MISSING_ZONE,
                resolved_zone, resolved_participant);
        }
        return false;
    }
    if (participant_ptr == NULL) {
        pgy_zone_authority_record_last(false, resolved_zone, resolved_participant,
            PGY_ZONE_AUTHORITY_CODE_MISSING_PARTICIPANT,
            PGY_ZONE_AUTHORITY_REASON_MISSING_PARTICIPANT);
        if (emit_stderr) {
            fprintf(stderr, PGY_ZONE_AUTHORITY_STDERR_MISSING_PARTICIPANT,
                resolved_zone, resolved_participant);
        }
        return false;
    }
    pgy_zone_authority_record_last(true, resolved_zone, resolved_participant,
                                   PGY_ZONE_AUTHORITY_CODE_OK, "");
    return true;
}

static inline bool
pgy_zone_authority_validate_token_impl(void *zone_ptr,
                                       void *participant_ptr,
                                       int64_t expected_token,
                                       int64_t provided_token,
                                       const char *zone_name,
                                       const char *participant_name,
                                       bool emit_stderr)
{
    const char *resolved_zone = zone_name != NULL ? zone_name : "<zone>";
    const char *resolved_participant =
        participant_name != NULL ? participant_name : "<participant>";

    if (!pgy_zone_authority_validate_impl(zone_ptr, participant_ptr,
            resolved_zone, resolved_participant, emit_stderr)) {
        return false;
    }

    if (expected_token <= 0 || provided_token <= 0
        || expected_token != provided_token) {
        pgy_zone_authority_record_last(false, resolved_zone,
            resolved_participant, PGY_ZONE_AUTHORITY_CODE_TOKEN_MISMATCH,
            PGY_ZONE_AUTHORITY_REASON_TOKEN_MISMATCH);
        if (emit_stderr) {
            fprintf(stderr, PGY_ZONE_AUTHORITY_STDERR_TOKEN_MISMATCH,
                resolved_zone, resolved_participant);
        }
        return false;
    }

    pgy_zone_authority_record_last(true, resolved_zone, resolved_participant,
                                   PGY_ZONE_AUTHORITY_CODE_OK, "");
    return true;
}

static inline bool
pgy_zone_authority_validate(void *zone_ptr, void *participant_ptr,
                            const char *zone_name, const char *participant_name)
{
    return pgy_zone_authority_validate_impl(zone_ptr, participant_ptr,
                                            zone_name, participant_name, true);
}

static inline bool
pgy_zone_authority_validate_flags_export(bool has_zone,
                                         bool has_participant,
                                         char *zone_name,
                                         char *participant_name)
{
    void *zone_ptr = has_zone ? (void *)&pgy_zone_authority_last_ok : NULL;
    void *participant_ptr =
        has_participant ? (void *)pgy_zone_authority_last_zone : NULL;
    return pgy_zone_authority_validate_impl(zone_ptr, participant_ptr,
                                            zone_name, participant_name, false);
}

static inline bool
pgy_zone_authority_validate_token_flags_export(bool has_zone,
                                               bool has_participant,
                                               int64_t expected_token,
                                               int64_t provided_token,
                                               char *zone_name,
                                               char *participant_name)
{
    void *zone_ptr = has_zone ? (void *)&pgy_zone_authority_last_ok : NULL;
    void *participant_ptr =
        has_participant ? (void *)pgy_zone_authority_last_zone : NULL;
    return pgy_zone_authority_validate_token_impl(zone_ptr, participant_ptr,
        expected_token, provided_token, zone_name, participant_name, false);
}

static inline bool
pgy_zone_authority_last_ok_export(void)
{
    return pgy_zone_authority_last_ok;
}

static inline char *
pgy_zone_authority_last_zone_export(void)
{
    return pgy_zone_authority_last_zone;
}

static inline char *
pgy_zone_authority_last_participant_export(void)
{
    return pgy_zone_authority_last_participant;
}

static inline char *
pgy_zone_authority_last_code_export(void)
{
    return pgy_zone_authority_last_code;
}

static inline char *
pgy_zone_authority_last_reason_export(void)
{
    return pgy_zone_authority_last_reason;
}

static inline bool
pgy_zone_authority_last_ok_rt_export(void)
{
    return pgy_zone_authority_last_ok_export();
}

static inline char *
pgy_zone_authority_last_zone_rt_export(void)
{
    return pgy_zone_authority_last_zone_export();
}

static inline char *
pgy_zone_authority_last_participant_rt_export(void)
{
    return pgy_zone_authority_last_participant_export();
}

static inline char *
pgy_zone_authority_last_code_rt_export(void)
{
    return pgy_zone_authority_last_code_export();
}

static inline char *
pgy_zone_authority_last_reason_rt_export(void)
{
    return pgy_zone_authority_last_reason_export();
}

void pgy_zone_authority_check_export(void *zone_ptr, void *participant_ptr,
                                     const char *zone_name,
                                     const char *participant_name);
void pgy_zone_authority_check_token_export(void *zone_ptr, void *participant_ptr,
                                           int64_t expected_token,
                                           int64_t provided_token,
                                           const char *zone_name,
                                           const char *participant_name);

#define PGY_ZONE_AUTHORITY_CHECK(zone_ptr, participant_ptr, zone_name, participant_name) do { \
    if (!pgy_zone_authority_validate((void *)(zone_ptr), (void *)(participant_ptr),           \
                                     (zone_name), (participant_name))) {                      \
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_AUTHORITY_MISMATCH,                         \
                          PGY_RUNTIME_PANIC_REASON_AUTHORITY_MISMATCH);                       \
    }                                                                                          \
} while (0)

/* =================================================================
 * Effect Pool — multiple instances of the same effect type
 *
 * Usage in generated zone struct:
 *   PGY_EFFECT_POOL_DEFINE(DamageEffect, 8)
 *   -> typedef struct { DamageEffect items[8]; bool active[8]; uint8_t count; uint8_t cap; } PgyEffectPool_DamageEffect_8;
 *
 * API:
 *   PGY_EFFECT_POOL_APPLY(pool, instance)  — activate next slot
 *   PGY_EFFECT_POOL_DETACH(pool, index)    — deactivate slot
 *   PGY_EFFECT_POOL_ACTIVE_COUNT(pool)     — number of active instances
 *   PGY_EFFECT_POOL_FOR_EACH(pool, i, item) — iterate active instances
 * ================================================================= */

#define PGY_EFFECT_POOL_DEFINE(Type, Cap)                              \
typedef struct {                                                        \
    Type items[Cap];                                                    \
    bool active[Cap];                                                   \
    uint8_t count;                                                      \
    uint8_t cap;                                                        \
} PgyEffectPool_##Type##_##Cap;

#define PGY_EFFECT_POOL_INIT(pool) do {                                \
    memset(&(pool), 0, sizeof(pool));                                   \
    (pool).cap = sizeof((pool).items) / sizeof((pool).items[0]);        \
} while(0)

#define PGY_EFFECT_POOL_APPLY(pool, instance) do {                     \
    for (uint8_t _pi = 0; _pi < (pool).cap; _pi++) {                  \
        if (!(pool).active[_pi]) {                                      \
            (pool).items[_pi] = (instance);                             \
            (pool).active[_pi] = true;                                  \
            (pool).count++;                                             \
            break;                                                      \
        }                                                               \
    }                                                                   \
} while(0)

#define PGY_EFFECT_POOL_DETACH(pool, index) do {                       \
    if ((index) < (pool).cap && (pool).active[(index)]) {              \
        (pool).active[(index)] = false;                                 \
        (pool).count--;                                                 \
    }                                                                   \
} while(0)

#define PGY_EFFECT_POOL_DETACH_ALL(pool) do {                          \
    for (uint8_t _pi = 0; _pi < (pool).cap; _pi++) {                  \
        (pool).active[_pi] = false;                                     \
    }                                                                   \
    (pool).count = 0;                                                   \
} while(0)

#define PGY_EFFECT_POOL_ACTIVE_COUNT(pool) ((pool).count)

#define PGY_EFFECT_POOL_FOR_EACH(pool, idx_var, item_var)              \
    for (uint8_t idx_var = 0; idx_var < (pool).cap; idx_var++)         \
        if ((pool).active[idx_var])                                     \
            for (int _once = 1; _once; _once = 0)                      \
                for (__typeof__((pool).items[0]) *item_var = &(pool).items[idx_var]; _once; _once = 0)

/* =================================================================
 * Unsafe Block Marker (for FFI)
 *
 * In C, this is just a documentation marker. Future versions may
 * add static analysis tools to check unsafe block boundaries.
 * ================================================================= */

#define PGY_UNSAFE_BEGIN \
    /* BEGIN UNSAFE_BLOCK */
#define PGY_UNSAFE_END \
    /* END UNSAFE_BLOCK */

/* Raw pointer operations (use inside unsafe blocks) */
static inline void* pgy_ptr_new_impl(size_t size, const char *file, int line)
{
    void *p = malloc(size);
    if (p == NULL && size > 0) {
        pgy_runtime_panic_emit(PGY_RUNTIME_PANIC_CLASS_OOM,
                               PGY_RUNTIME_PANIC_REASON_ALLOCATION_FAILED,
                               file, line);
    }
    return p;
}

#define PGY_PTR_NEW(Type) \
    ((Type*)pgy_ptr_new_impl(sizeof(Type), __FILE__, __LINE__))

#define PGY_PTR_NEW_ARRAY(Type, count) \
    ((Type*)pgy_ptr_new_impl(sizeof(Type) * (count), __FILE__, __LINE__))

#define PGY_PTR_FREE(ptr) \
    do { \
        free(ptr); \
        (ptr) = NULL; \
    } while (0)

#define PGY_PTR_READ(ptr) \
    (*(ptr))

#define PGY_PTR_WRITE(ptr, val) \
    do { \
        (*(ptr)) = (val); \
    } while (0)

/* =================================================================
 * Result Type (Error Handling)
 * ================================================================= */

typedef enum {
    PgyResultOk,
    PgyResultErr
} PgyResultTag;

#define PGY_RESULT_DEFINE(SuffixName, CType, ErrType) \
\
typedef struct { \
    PgyResultTag tag; \
    union { \
        CType ok; \
        ErrType err; \
    }; \
} PgyResult_##SuffixName; \
\
static inline PgyResult_##SuffixName \
pgy_result_ok_##SuffixName(CType value) \
{ \
    PgyResult_##SuffixName r; \
    r.tag = PgyResultOk; \
    r.ok = value; \
    return r; \
} \
\
static inline PgyResult_##SuffixName \
pgy_result_err_##SuffixName(ErrType err) \
{ \
    PgyResult_##SuffixName r; \
    r.tag = PgyResultErr; \
    r.err = err; \
    return r; \
} \
\
static inline bool \
pgy_result_is_ok_##SuffixName(PgyResult_##SuffixName* r) \
{ \
    return r->tag == PgyResultOk; \
} \
\
static inline CType \
pgy_result_unwrap_##SuffixName(PgyResult_##SuffixName* r) \
{ \
    if (r->tag != PgyResultOk) { \
        PGY_PANIC(PGY_RUNTIME_PANIC_REASON_RESULT_UNWRAP_ERR); \
    } \
    return r->ok; \
} \
\
static inline ErrType \
pgy_result_unwrap_err_##SuffixName(PgyResult_##SuffixName* r) \
{ \
    if (r->tag != PgyResultErr) { \
        PGY_PANIC("Result unwrap_err on Ok value"); \
    } \
    return r->err; \
}

/* Result types for common error types */
typedef const char* PgyError;

PGY_RESULT_DEFINE(Int, int32_t, PgyError)
PGY_RESULT_DEFINE(Bool, bool, PgyError)
PGY_RESULT_DEFINE(String, char*, PgyError)

/* Convenience wrappers for Pergyra language syntax:
 *   Ok(val), Err(msg), IsOk(r), IsErr(r), Unwrap(r), UnwrapOr(r, fallback) */
#define Ok_Int(v)           pgy_result_ok_Int(v)
#define Err_Int(m)          pgy_result_err_Int(m)
#define IsOk_Int(r)         ((r).tag == PgyResultOk)
#define IsErr_Int(r)        ((r).tag == PgyResultErr)
#define Unwrap_Int(r)       pgy_result_unwrap_Int(&(PgyResult_Int){(r).tag, {.ok=(r).ok}})
#define UnwrapOr_Int(r, f)  ((r).tag == PgyResultOk ? (r).ok : (f))

#define Ok_Bool(v)          pgy_result_ok_Bool(v)
#define Err_Bool(m)         pgy_result_err_Bool(m)
#define IsOk_Bool(r)        ((r).tag == PgyResultOk)
#define IsErr_Bool(r)       ((r).tag == PgyResultErr)
#define Unwrap_Bool(r)      pgy_result_unwrap_Bool(&(PgyResult_Bool){(r).tag, {.ok=(r).ok}})
#define UnwrapOr_Bool(r, f) ((r).tag == PgyResultOk ? (r).ok : (f))

/* Result helper macros (similar to Rust's ? operator)
 * ResultType: the concrete result struct type (e.g. PgyResult_Int)
 */
#define PGY_RESULT_TRY(ResultType, result_expr, ok_var, err_handler) \
    do { \
        ResultType pgy__try_tmp_ = (result_expr); \
        if (pgy__try_tmp_.tag != PgyResultOk) { \
            err_handler(pgy__try_tmp_.err); \
        } \
        (ok_var) = pgy__try_tmp_.ok; \
    } while (0)

/* RemoteFuture<T> → Result<T>: wraps the raw await pointer in a
 * PgyResult.  NULL result → Err("remote operation failed"),
 * non-NULL  → Ok(value).  SuffixName must match PGY_RESULT_DEFINE
 * (e.g. Int, Bool, String).  RawCType is the C storage type. */
#define pgy_await_result_take(handle, SuffixName, RawCType) \
    ({ \
        void *_pgy_raw = pgy_await((handle)); \
        PgyResult_##SuffixName _pgy_r; \
        if (_pgy_raw != NULL) { \
            _pgy_r.tag = PgyResultOk; \
            _pgy_r.ok  = *(RawCType *)_pgy_raw; \
            free(_pgy_raw); \
        } else { \
            _pgy_r.tag = PgyResultErr; \
            _pgy_r.err = "remote operation failed"; \
        } \
        _pgy_r; \
    })

/* =================================================================
 * Option Type (Nullable Values)
 * ================================================================= */

typedef enum {
    PgyOptionSome,
    PgyOptionNone
} PgyOptionTag;

#define PGY_OPTION_DEFINE(SuffixName, CType) \
\
typedef struct { \
    PgyOptionTag tag; \
    CType value; \
} PgyOption_##SuffixName; \
\
static inline PgyOption_##SuffixName \
pgy_option_some_##SuffixName(CType value) \
{ \
    PgyOption_##SuffixName o; \
    o.tag = PgyOptionSome; \
    o.value = value; \
    return o; \
} \
\
static inline PgyOption_##SuffixName \
pgy_option_none_##SuffixName(void) \
{ \
    PgyOption_##SuffixName o; \
    o.tag = PgyOptionNone; \
    return o; \
} \
\
static inline bool \
pgy_option_is_some_##SuffixName(PgyOption_##SuffixName* o) \
{ \
    return o->tag == PgyOptionSome; \
} \
\
static inline CType \
pgy_option_unwrap_##SuffixName(PgyOption_##SuffixName* o) \
{ \
    if (o->tag != PgyOptionSome) { \
        PGY_PANIC(PGY_RUNTIME_PANIC_REASON_OPTION_UNWRAP_NONE); \
    } \
    return o->value; \
}

PGY_OPTION_DEFINE(Int, int32_t)
PGY_OPTION_DEFINE(Bool, bool)
PGY_OPTION_DEFINE(String, char*)

#define Some_Int(v)             pgy_option_some_Int(v)
#define None_Int()              pgy_option_none_Int()
#define IsSome_Int(o)           ((o).tag == PgyOptionSome)
#define IsNone_Int(o)           ((o).tag == PgyOptionNone)
#define UnwrapOption_Int(o)     pgy_option_unwrap_Int(&(PgyOption_Int){(o).tag, (o).value})

#define Some_Bool(v)            pgy_option_some_Bool(v)
#define None_Bool()             pgy_option_none_Bool()
#define IsSome_Bool(o)          ((o).tag == PgyOptionSome)
#define IsNone_Bool(o)          ((o).tag == PgyOptionNone)
#define UnwrapOption_Bool(o)    pgy_option_unwrap_Bool(&(PgyOption_Bool){(o).tag, (o).value})

#define Some_String(v)          pgy_option_some_String(v)
#define None_String()           pgy_option_none_String()
#define IsSome_String(o)        ((o).tag == PgyOptionSome)
#define IsNone_String(o)        ((o).tag == PgyOptionNone)
#define UnwrapOption_String(o)  pgy_option_unwrap_String(&(PgyOption_String){(o).tag, (o).value})
