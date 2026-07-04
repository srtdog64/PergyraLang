
/* =================================================================
 * Parallel Support (OpenMP or Sequential Fallback)
 * ================================================================= */

#include "pgy_runtime_security_log.h"

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
#define PGY_ZONE_LOCK_INIT(z) do { \
    if (pthread_rwlock_init(&(z)->__zone_lock, NULL) != 0) \
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_INTERNAL_INVARIANT, \
                          "zone lock initialization failed"); \
} while (0)
#define PGY_ZONE_LOCK_DESTROY(z) do { \
    if (pthread_rwlock_destroy(&(z)->__zone_lock) != 0) \
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_INTERNAL_INVARIANT, \
                          "zone lock destroy failed"); \
} while (0)
#define PGY_ZONE_RDLOCK(z) do { \
    if (pthread_rwlock_rdlock(&(z)->__zone_lock) != 0) \
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_INTERNAL_INVARIANT, \
                          "zone read lock failed"); \
} while (0)
#define PGY_ZONE_WRLOCK(z) do { \
    if (pthread_rwlock_wrlock(&(z)->__zone_lock) != 0) \
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_INTERNAL_INVARIANT, \
                          "zone write lock failed"); \
} while (0)
#define PGY_ZONE_UNLOCK(z) do { \
    if (pthread_rwlock_unlock(&(z)->__zone_lock) != 0) \
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_INTERNAL_INVARIANT, \
                          "zone unlock failed"); \
} while (0)

#else /* single-threaded: zero cost */

#define PGY_ZONE_LOCK_FIELD    /* no lock field */
#define PGY_ZONE_LOCK_INIT(z)  ((void)(z))
#define PGY_ZONE_LOCK_DESTROY(z) ((void)(z))
#define PGY_ZONE_RDLOCK(z)     ((void)(z))
#define PGY_ZONE_WRLOCK(z)     ((void)(z))
#define PGY_ZONE_UNLOCK(z)     ((void)(z))

#endif /* PGY_ZONE_THREADSAFE */

/*
 * Generation counter for stale-state detection.
 *
 * Stored as _Atomic uint32_t so increment / read are atomic even in the
 * default build where PGY_ZONE_THREADSAFE is not defined and the rwlock
 * macros are no-ops. Parallel/spawn code paths that share a zone pointer
 * no longer race on the counter itself.
 *
 * The rwlock (PGY_ZONE_THREADSAFE) is still the structural lock for the
 * rest of the zone fields. Atomic generation is the *minimum* fix that
 * removes the data race on the gen counter and the use-after-write
 * undefined behaviour on its read path. Other zone-field protection
 * remains the lock's responsibility.
 *
 * Layout: _Atomic uint32_t has the same size/alignment as uint32_t on
 * the platforms Pergyra targets (Itanium C ABI / Windows x64); ABI shape
 * of Zone structs is preserved.
 */
#include <stdatomic.h>

_Static_assert(sizeof(_Atomic uint32_t) == sizeof(uint32_t),
               "Pergyra zone generation atomic must preserve uint32_t size");
_Static_assert(_Alignof(_Atomic uint32_t) == _Alignof(uint32_t),
               "Pergyra zone generation atomic must preserve uint32_t alignment");

#define PGY_ZONE_GENERATION_FIELD  _Atomic uint32_t __sync_generation;
#define PGY_ZONE_GENERATION_INC(z) \
    ((void)atomic_fetch_add_explicit(&(z)->__sync_generation, 1u, \
                                     memory_order_release))
#define PGY_ZONE_GENERATION_LOAD(z) \
    atomic_load_explicit(&(z)->__sync_generation, memory_order_acquire)

static inline void
pgy_zone_generation_warn_if_stale_impl(const char *label,
                                       uint32_t expected,
                                       uint32_t actual)
{
    static _Thread_local unsigned pgy_zone_stale_warn_count = 0;
    static _Thread_local bool pgy_zone_stale_warn_suppressed = false;

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
    uint32_t _pgy_actual_gen = PGY_ZONE_GENERATION_LOAD(z);                        \
    pgy_zone_generation_warn_if_stale_impl((label),                                \
        _pgy_expected_gen,                                                          \
        _pgy_actual_gen);                                                           \
} while (0)

/* =================================================================
 * Zone Authority runtime validation
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
            pgy_runtime_log_authority_failure(stderr,
                PGY_ZONE_AUTHORITY_CODE_MISSING_ZONE,
                PGY_ZONE_AUTHORITY_REASON_MISSING_ZONE,
                resolved_zone, resolved_participant);
        }
        return false;
    }
    if (participant_ptr == NULL) {
        pgy_zone_authority_record_last(false, resolved_zone, resolved_participant,
            PGY_ZONE_AUTHORITY_CODE_MISSING_PARTICIPANT,
            PGY_ZONE_AUTHORITY_REASON_MISSING_PARTICIPANT);
        if (emit_stderr) {
            pgy_runtime_log_authority_failure(stderr,
                PGY_ZONE_AUTHORITY_CODE_MISSING_PARTICIPANT,
                PGY_ZONE_AUTHORITY_REASON_MISSING_PARTICIPANT,
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
            pgy_runtime_log_authority_failure(stderr,
                PGY_ZONE_AUTHORITY_CODE_TOKEN_MISMATCH,
                PGY_ZONE_AUTHORITY_REASON_TOKEN_MISMATCH,
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
 * Effect Pool - multiple instances of the same effect type
 *
 * Usage in generated zone struct:
 *   PGY_EFFECT_POOL_DEFINE(DamageEffect, 8)
 *   -> typedef struct { DamageEffect items[8]; bool active[8]; uint8_t count; uint8_t cap; } PgyEffectPool_DamageEffect_8;
 *
 * API:
 *   PGY_EFFECT_POOL_APPLY(pool, instance)   -> activate next slot
 *   PGY_EFFECT_POOL_DETACH(pool, index)     -> deactivate slot
 *   PGY_EFFECT_POOL_ACTIVE_COUNT(pool)      -> number of active instances
 *   PGY_EFFECT_POOL_FOR_EACH(pool, i, item) -> iterate active instances
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
 * Unsafe Block Marker
 *
 * In generated C, this is only a documentation marker for a source lexical
 * boundary. It does not grant raw, FFI, layout, runtime, or concurrency
 * capability by itself.
 * ================================================================= */

#define PGY_UNSAFE_BEGIN \
    /* BEGIN UNSAFE_BLOCK */
#define PGY_UNSAFE_END \
    /* END UNSAFE_BLOCK */

/*
 * Runtime-internal raw pointer helpers.
 *
 * These macros are not the user-facing raw escape surface. Source-level raw
 * access remains reserved for the future scoped unsafe capability contract
 * (`unsafe(raw) { ... }`) and must pass through semantic/AIR/ABI gates first.
 */
static inline void* pgy_ptr_new_impl(size_t size, const char *file, int line)
{
    void *p = malloc(size);
    if (p == NULL && size > 0) {
        PGY_RUNTIME_PANIC_AT(PGY_RUNTIME_PANIC_CLASS_OOM,
                             PGY_RUNTIME_PANIC_REASON_ALLOCATION_FAILED,
                             file, line);
    }
    return p;
}

static inline void*
pgy_ptr_new_array_impl(size_t elem_size, size_t count, const char *file, int line)
{
    if (elem_size != 0 && count > ((size_t)-1) / elem_size) {
        PGY_RUNTIME_PANIC_AT(PGY_RUNTIME_PANIC_CLASS_OOM,
                             PGY_RUNTIME_PANIC_REASON_ALLOCATION_FAILED,
                             file, line);
    }
    return pgy_ptr_new_impl(elem_size * count, file, line);
}

#define PGY_PTR_NEW(Type) \
    ((Type*)pgy_ptr_new_impl(sizeof(Type), __FILE__, __LINE__))

#define PGY_PTR_NEW_ARRAY(Type, count) \
    ((Type*)pgy_ptr_new_array_impl(sizeof(Type), (count), __FILE__, __LINE__))

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

#include "pgy_runtime_result_option_inline.h"
