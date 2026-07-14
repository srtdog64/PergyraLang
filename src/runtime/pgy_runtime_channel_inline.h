/* =================================================================
 * Channel (thread-safe bounded ring buffer)
 *
 * Blocking send/recv with mutex + condvar.
 * Send blocks when full; recv blocks when empty.
 *
 * Usage:
 *   PGY_CHANNEL_DEFINE(Int, int32_t)
 *   PgyChannel_Int ch; pgy_channel_init_Int(&ch, 16);
 *   pgy_channel_send_Int(&ch, 42);    // blocks if full
 *   int32_t v = pgy_channel_recv_Int(&ch);  // blocks if empty
 *   pgy_channel_close_Int(&ch);
 *   pgy_channel_destroy_Int(&ch);
 *
 * Destroy is quiescent-only: close/drain/join producers and consumers before
 * freeing the backing buffer and destroying mutex/condvar state.
 * ================================================================= */

#include "pgy_runtime_channel_status.h"
#include "pgy_runtime_channel_lane_inline.h"
#include "pgy_runtime_channel_lifecycle_inline.h"
#include "pgy_runtime_channel_result_inline.h"
#include "pgy_runtime_cancel_probe.h"

/* Cancellable parked waits (docs/182 SS2.2): the unbounded blocking
 * ops sleep in short quanta and re-check the cancellation probe, so a
 * cancelled task parked on a channel retires promptly instead of
 * deadlocking its join. A cancelled exit is a contract OUTCOME (plain
 * false, like closed), never a violation: any-join losers then reach
 * their give, lose the CAS, and retire. Timeout variants already
 * self-unblock by deadline and keep their exact shape. */
#define PGY_CHANNEL_CANCEL_WAIT_QUANTUM_NS 10000000ULL

/* Lifecycle guard (docs/179 3a; V1 bug class #8-R): an operation on a
 * NULL or uninitialized channel is a contract VIOLATION, never a contract
 * outcome, and it panics fail-closed. The prior warn-and-continue let a
 * pre-fix binary retry a silently-lost send forever and write a 4.7GB
 * warn log (2026-07-12 incident). Closed/full/empty/timeout stay
 * data-carrying contract outcomes, and the *_result variants stay the
 * failure-as-data tier: their own guards return ERR before ever reaching
 * these raw operations. */
#define PGY_CHANNEL_CORE_DEFINE(SuffixName, CType, PGY_CH_STORAGE) \
typedef struct \
{ \
    CType           *buf; \
    size_t           cap; \
    size_t           head; \
    size_t           tail; \
    size_t           count; \
    bool             closed; \
    pthread_mutex_t  mutex; \
    pthread_cond_t   cond_not_full; \
    pthread_cond_t   cond_not_empty; \
} PgyChannel_##SuffixName; \
\
PGY_CH_STORAGE void \
pgy_channel_init_##SuffixName(PgyChannel_##SuffixName *ch, size_t capacity) \
{ \
    if (ch == NULL) { \
        pgy_runtime_warn_invalid_channel("init_" #SuffixName, "null channel"); \
        return; \
    } \
    /* CHANNEL_COUNT budget charge (R6): each channel created counts toward the \
     * host's ceiling; deny-before-allocate, behind the imposed fast-path. Both \
     * backends call pgy_channel_init_<T> (LLVM emits the same shared runtime fn, \
     * see llvm_mir_source_resource_defs.c), so this one charge covers both. */ \
    if (pgy_budget_is_imposed_export()) \
        pgy_budget_charge_export(PGY_BUDGET_CHANNEL_COUNT, 1, "channel"); \
    capacity = pgy_runtime_channel_capacity_or_default("init_" #SuffixName, capacity); \
    if (capacity > SIZE_MAX / sizeof(CType)) { \
        pgy_runtime_warn_invalid_channel("init_" #SuffixName, "capacity overflows buffer size"); \
        ch->buf = NULL; \
        ch->cap = 0; \
        return; \
    } \
    ch->buf    = (CType *)calloc(capacity, sizeof(CType)); \
    if (ch->buf == NULL) { \
        pgy_runtime_warn_invalid_channel("init_" #SuffixName, "buffer allocation failed"); \
        ch->cap = 0; \
        return; \
    } \
    ch->cap    = capacity; \
    ch->head   = 0; \
    ch->tail   = 0; \
    ch->count  = 0; \
    ch->closed = false; \
    if (pthread_mutex_init(&ch->mutex, NULL) != 0) { \
        pgy_runtime_warn_invalid_channel("init_" #SuffixName, "mutex initialization failed"); \
        free(ch->buf); \
        ch->buf = NULL; \
        ch->cap = 0; \
        return; \
    } \
    if (pthread_cond_init(&ch->cond_not_full, NULL) != 0) { \
        pgy_runtime_warn_invalid_channel("init_" #SuffixName, "not-full condition initialization failed"); \
        pthread_mutex_destroy(&ch->mutex); \
        free(ch->buf); \
        ch->buf = NULL; \
        ch->cap = 0; \
        return; \
    } \
    if (pthread_cond_init(&ch->cond_not_empty, NULL) != 0) { \
        pgy_runtime_warn_invalid_channel("init_" #SuffixName, "not-empty condition initialization failed"); \
        pthread_cond_destroy(&ch->cond_not_full); \
        pthread_mutex_destroy(&ch->mutex); \
        free(ch->buf); \
        ch->buf = NULL; \
        ch->cap = 0; \
        return; \
    } \
} \
\
PGY_CH_STORAGE void \
pgy_channel_close_##SuffixName(PgyChannel_##SuffixName *ch) \
{ \
    if (ch == NULL || ch->buf == NULL || ch->cap == 0) \
        return; \
    pthread_mutex_lock(&ch->mutex); \
    ch->closed = true; \
    pthread_cond_broadcast(&ch->cond_not_full); \
    pthread_cond_broadcast(&ch->cond_not_empty); \
    pthread_mutex_unlock(&ch->mutex); \
} \
\
PGY_CH_STORAGE void \
pgy_channel_destroy_##SuffixName(PgyChannel_##SuffixName *ch) \
{ \
    if (ch == NULL || ch->buf == NULL || ch->cap == 0) \
        return; \
    pthread_mutex_destroy(&ch->mutex); \
    pthread_cond_destroy(&ch->cond_not_full); \
    pthread_cond_destroy(&ch->cond_not_empty); \
    free(ch->buf); \
    ch->buf = NULL; \
    ch->cap = 0; \
    ch->head = 0; \
    ch->tail = 0; \
    ch->count = 0; \
    ch->closed = true; \
} \
\
/* Blocking send. Returns false if channel closed. */ \
PGY_CH_STORAGE bool \
pgy_channel_send_##SuffixName(PgyChannel_##SuffixName *ch, CType value) \
{ \
    pgy_channel_require_operable(ch == NULL, \
        ch != NULL && (ch->buf == NULL || ch->cap == 0), false, \
        "send_" #SuffixName); \
    pthread_mutex_lock(&ch->mutex); \
    while (ch->count >= ch->cap && !ch->closed \
           && !pgy_cancel_probe_cancelled()) { \
        if (pgy_async_in_coroutine()) { \
            pthread_mutex_unlock(&ch->mutex); \
            pgy_async_yield(); \
            pthread_mutex_lock(&ch->mutex); \
        } else { \
            struct timespec _pgy_q = \
                pgy_timespec_after_ns(PGY_CHANNEL_CANCEL_WAIT_QUANTUM_NS); \
            int _pgy_ws = pthread_cond_timedwait(&ch->cond_not_full, \
                                                 &ch->mutex, &_pgy_q); \
            if (_pgy_ws != 0 && _pgy_ws != ETIMEDOUT) { \
                pgy_runtime_warn_invalid_channel("send_" #SuffixName, \
                    "not-full condition wait failed"); \
                pthread_mutex_unlock(&ch->mutex); \
                return false; \
            } \
        } \
    } \
    if (ch->count >= ch->cap && !ch->closed) { \
        /* cancelled while still full: contract outcome, no warn */ \
        pthread_mutex_unlock(&ch->mutex); \
        return false; \
    } \
    if (ch->closed) { \
        pgy_runtime_warn_invalid_channel("send_" #SuffixName, "channel is closed"); \
        pthread_mutex_unlock(&ch->mutex); \
        return false; \
    } \
    ch->buf[ch->tail] = value; \
    ch->tail = (ch->tail + 1) % ch->cap; \
    ch->count++; \
    pthread_cond_signal(&ch->cond_not_empty); \
    pthread_mutex_unlock(&ch->mutex); \
    return true; \
} \
\
/* Non-blocking try_send. Returns false if full or closed. */ \
PGY_CH_STORAGE bool \
pgy_channel_try_send_##SuffixName(PgyChannel_##SuffixName *ch, CType value) \
{ \
    pgy_channel_require_operable(ch == NULL, \
        ch != NULL && (ch->buf == NULL || ch->cap == 0), false, \
        "try_send_" #SuffixName); \
    pthread_mutex_lock(&ch->mutex); \
    if (ch->closed || ch->count >= ch->cap) { \
        pgy_runtime_warn_invalid_channel("try_send_" #SuffixName, \
            ch->closed ? "channel is closed" : "channel is full"); \
        pthread_mutex_unlock(&ch->mutex); \
        return false; \
    } \
    ch->buf[ch->tail] = value; \
    ch->tail = (ch->tail + 1) % ch->cap; \
    ch->count++; \
    pthread_cond_signal(&ch->cond_not_empty); \
    pthread_mutex_unlock(&ch->mutex); \
    return true; \
} \
\
/* Non-blocking try_send with status. Some(true)=sent, Some(false)=closed, None=full. */ \
PGY_CH_STORAGE PgyOption_Bool \
pgy_channel_try_send_status_##SuffixName(PgyChannel_##SuffixName *ch, CType value) \
{ \
    /* Silently reporting an uninitialized channel as "closed" was the
     * worst tier of the incident class -- not even a warn. */ \
    pgy_channel_require_operable(ch == NULL, \
        ch != NULL && (ch->buf == NULL || ch->cap == 0), false, \
        "try_send_status_" #SuffixName); \
    pthread_mutex_lock(&ch->mutex); \
    if (ch->closed) { \
        pthread_mutex_unlock(&ch->mutex); \
        return Some_Bool(false); \
    } \
    if (ch->count >= ch->cap) { \
        pthread_mutex_unlock(&ch->mutex); \
        return None_Bool(); \
    } \
    ch->buf[ch->tail] = value; \
    ch->tail = (ch->tail + 1) % ch->cap; \
    ch->count++; \
    pthread_cond_signal(&ch->cond_not_empty); \
    pthread_mutex_unlock(&ch->mutex); \
    return Some_Bool(true); \
} \
\
/* Blocking send with timeout. Returns false on timeout or closed. */ \
PGY_CH_STORAGE bool \
pgy_channel_send_timeout_##SuffixName(PgyChannel_##SuffixName *ch, \
                                      CType value, uint64_t timeout_ns) \
{ \
    pgy_channel_require_operable(ch == NULL, \
        ch != NULL && (ch->buf == NULL || ch->cap == 0), false, \
        "send_timeout_" #SuffixName); \
    struct timespec deadline = pgy_timespec_after_ns(timeout_ns); \
    pthread_mutex_lock(&ch->mutex); \
    while (ch->count >= ch->cap && !ch->closed) { \
        int wait_status = \
            pthread_cond_timedwait(&ch->cond_not_full, &ch->mutex, &deadline); \
        if (wait_status == ETIMEDOUT && ch->count >= ch->cap && !ch->closed) { \
            pgy_runtime_warn_invalid_channel("send_timeout_" #SuffixName, \
                "deadline reached while channel remained full"); \
            pthread_mutex_unlock(&ch->mutex); \
            return false; \
        } \
        if (wait_status != 0) { \
            pgy_runtime_warn_invalid_channel("send_timeout_" #SuffixName, \
                "not-full condition timed wait failed"); \
            pthread_mutex_unlock(&ch->mutex); \
            return false; \
        } \
    } \
    if (ch->closed) { \
        pgy_runtime_warn_invalid_channel("send_timeout_" #SuffixName, "channel is closed"); \
        pthread_mutex_unlock(&ch->mutex); \
        return false; \
    } \
    ch->buf[ch->tail] = value; \
    ch->tail = (ch->tail + 1) % ch->cap; \
    ch->count++; \
    pthread_cond_signal(&ch->cond_not_empty); \
    pthread_mutex_unlock(&ch->mutex); \
    return true; \
} \
\
/* Timed send with status. Some(true)=sent, Some(false)=closed, None=timeout. */ \
PGY_CH_STORAGE PgyOption_Bool \
pgy_channel_send_timeout_status_##SuffixName(PgyChannel_##SuffixName *ch, \
                                             CType value, uint64_t timeout_ns) \
{ \
    pgy_channel_require_operable(ch == NULL, \
        ch != NULL && (ch->buf == NULL || ch->cap == 0), false, \
        "send_timeout_status_" #SuffixName); \
    struct timespec deadline = pgy_timespec_after_ns(timeout_ns); \
    pthread_mutex_lock(&ch->mutex); \
    while (ch->count >= ch->cap && !ch->closed) { \
        int wait_status = \
            pthread_cond_timedwait(&ch->cond_not_full, &ch->mutex, &deadline); \
        if (wait_status == ETIMEDOUT && ch->count >= ch->cap && !ch->closed) { \
            pthread_mutex_unlock(&ch->mutex); \
            return None_Bool(); \
        } \
        if (wait_status != 0) { \
            pgy_runtime_warn_invalid_channel("send_timeout_status_" #SuffixName, \
                "not-full condition timed wait failed"); \
            pthread_mutex_unlock(&ch->mutex); \
            return Some_Bool(false); \
        } \
    } \
    if (ch->closed) { \
        pthread_mutex_unlock(&ch->mutex); \
        return Some_Bool(false); \
    } \
    ch->buf[ch->tail] = value; \
    ch->tail = (ch->tail + 1) % ch->cap; \
    ch->count++; \
    pthread_cond_signal(&ch->cond_not_empty); \
    pthread_mutex_unlock(&ch->mutex); \
    return Some_Bool(true); \
} \
\
/* Blocking recv. Returns false if channel closed and empty. */ \
PGY_CH_STORAGE bool \
pgy_channel_recv_##SuffixName(PgyChannel_##SuffixName *ch, CType *out) \
{ \
    pgy_channel_require_operable(ch == NULL, \
        ch != NULL && (ch->buf == NULL || ch->cap == 0), out == NULL, \
        "recv_" #SuffixName); \
    pthread_mutex_lock(&ch->mutex); \
    while (ch->count == 0 && !ch->closed \
           && !pgy_cancel_probe_cancelled()) { \
        if (pgy_async_in_coroutine()) { \
            pthread_mutex_unlock(&ch->mutex); \
            pgy_async_yield(); \
            pthread_mutex_lock(&ch->mutex); \
        } else { \
            pthread_mutex_unlock(&ch->mutex); \
            if (!pgy_async_progress_one()) { \
                pthread_mutex_lock(&ch->mutex); \
                struct timespec _pgy_q = \
                    pgy_timespec_after_ns(PGY_CHANNEL_CANCEL_WAIT_QUANTUM_NS); \
                int _pgy_ws = pthread_cond_timedwait(&ch->cond_not_empty, \
                                                     &ch->mutex, &_pgy_q); \
                if (_pgy_ws != 0 && _pgy_ws != ETIMEDOUT) { \
                    pgy_runtime_warn_invalid_channel("recv_" #SuffixName, \
                        "not-empty condition wait failed"); \
                    pthread_mutex_unlock(&ch->mutex); \
                    return false; \
                } \
            } else { \
                pthread_mutex_lock(&ch->mutex); \
            } \
        } \
    } \
    if (ch->count == 0) { \
        if (ch->closed) \
            pgy_runtime_warn_invalid_channel("recv_" #SuffixName, \
                "channel is closed and empty"); \
        /* cancelled while still empty: contract outcome, no warn */ \
        pthread_mutex_unlock(&ch->mutex); \
        return false; \
    } \
    *out = ch->buf[ch->head]; \
    ch->head = (ch->head + 1) % ch->cap; \
    ch->count--; \
    pthread_cond_signal(&ch->cond_not_full); \
    pthread_mutex_unlock(&ch->mutex); \
    return true; \
} \
\
/* Blocking recv with timeout. Returns false on timeout or closed+empty. */ \
PGY_CH_STORAGE bool \
pgy_channel_recv_timeout_##SuffixName(PgyChannel_##SuffixName *ch, \
                                      CType *out, uint64_t timeout_ns) \
{ \
    pgy_channel_require_operable(ch == NULL, \
        ch != NULL && (ch->buf == NULL || ch->cap == 0), out == NULL, \
        "recv_timeout_" #SuffixName); \
    struct timespec deadline = pgy_timespec_after_ns(timeout_ns); \
    pthread_mutex_lock(&ch->mutex); \
    while (ch->count == 0 && !ch->closed) { \
        if (pgy_async_in_coroutine()) { \
            pthread_mutex_unlock(&ch->mutex); \
            pgy_async_yield(); \
            pthread_mutex_lock(&ch->mutex); \
        } else { \
            pthread_mutex_unlock(&ch->mutex); \
            if (!pgy_async_progress_one()) { \
                pthread_mutex_lock(&ch->mutex); \
                int wait_status = pthread_cond_timedwait( \
                    &ch->cond_not_empty, &ch->mutex, &deadline); \
                if (wait_status == ETIMEDOUT && ch->count == 0 && !ch->closed) { \
                    pgy_runtime_warn_invalid_channel("recv_timeout_" #SuffixName, \
                        "deadline reached while channel remained empty"); \
                    pthread_mutex_unlock(&ch->mutex); \
                    return false; \
                } \
                if (wait_status != 0) { \
                    pgy_runtime_warn_invalid_channel("recv_timeout_" #SuffixName, \
                        "not-empty condition timed wait failed"); \
                    pthread_mutex_unlock(&ch->mutex); \
                    return false; \
                } \
            } else { \
                pthread_mutex_lock(&ch->mutex); \
            } \
        } \
    } \
    if (ch->count == 0 && ch->closed) { \
        pgy_runtime_warn_invalid_channel("recv_timeout_" #SuffixName, "channel is closed and empty"); \
        pthread_mutex_unlock(&ch->mutex); \
        return false; \
    } \
    *out = ch->buf[ch->head]; \
    ch->head = (ch->head + 1) % ch->cap; \
    ch->count--; \
    pthread_cond_signal(&ch->cond_not_full); \
    pthread_mutex_unlock(&ch->mutex); \
    return true; \
} \
\
/* Non-blocking try_recv. Returns false if empty. */ \
PGY_CH_STORAGE bool \
pgy_channel_try_recv_##SuffixName(PgyChannel_##SuffixName *ch, CType *out) \
{ \
    pgy_channel_require_operable(ch == NULL, \
        ch != NULL && (ch->buf == NULL || ch->cap == 0), out == NULL, \
        "try_recv_" #SuffixName); \
    if (!pgy_async_in_coroutine()) \
        (void)pgy_async_progress_one(); \
    pthread_mutex_lock(&ch->mutex); \
    if (ch->count == 0) { \
        /* Empty is the CONTRACT outcome of a non-blocking try_recv (the
         * select default path polls exactly this), not misuse -- warning
         * here floods stderr nondeterministically under polling. Only
         * null/uninitialized stay warned above. */ \
        pthread_mutex_unlock(&ch->mutex); \
        return false; \
    } \
    *out = ch->buf[ch->head]; \
    ch->head = (ch->head + 1) % ch->cap; \
    ch->count--; \
    pthread_cond_signal(&ch->cond_not_full); \
    pthread_mutex_unlock(&ch->mutex); \
    return true; \
} \
\
/* Non-blocking readiness check */ \
PGY_CH_STORAGE bool \
pgy_channel_ready_##SuffixName(PgyChannel_##SuffixName *ch) \
{ \
    if (ch == NULL || ch->buf == NULL || ch->cap == 0) \
        return false; \
    if (!pgy_async_in_coroutine()) \
        (void)pgy_async_progress_one(); \
    pthread_mutex_lock(&ch->mutex); \
    bool ready = ch->count > 0; \
    pthread_mutex_unlock(&ch->mutex); \
    return ready; \
} \
\
/* Backpressure observation helpers. */ \
PGY_CH_STORAGE int32_t \
pgy_channel_length_##SuffixName(PgyChannel_##SuffixName *ch) \
{ \
    if (ch == NULL || ch->buf == NULL || ch->cap == 0) \
        return 0; \
    pthread_mutex_lock(&ch->mutex); \
    int32_t len = (ch->count > (size_t)INT32_MAX) ? INT32_MAX : (int32_t)ch->count; \
    pthread_mutex_unlock(&ch->mutex); \
    return len; \
} \
\
PGY_CH_STORAGE int32_t \
pgy_channel_capacity_##SuffixName(PgyChannel_##SuffixName *ch) \
{ \
    if (ch == NULL || ch->buf == NULL || ch->cap == 0) \
        return 0; \
    pthread_mutex_lock(&ch->mutex); \
    int32_t cap = (ch->cap > (size_t)INT32_MAX) ? INT32_MAX : (int32_t)ch->cap; \
    pthread_mutex_unlock(&ch->mutex); \
    return cap; \
} \
\
PGY_CH_STORAGE bool \
pgy_channel_full_##SuffixName(PgyChannel_##SuffixName *ch) \
{ \
    if (ch == NULL || ch->buf == NULL || ch->cap == 0) \
        return false; \
    pthread_mutex_lock(&ch->mutex); \
    bool full = ch->count >= ch->cap; \
    pthread_mutex_unlock(&ch->mutex); \
    return full; \
} \
\
PGY_CH_STORAGE int32_t \
pgy_channel_space_##SuffixName(PgyChannel_##SuffixName *ch) \
{ \
    if (ch == NULL || ch->buf == NULL || ch->cap == 0) \
        return 0; \
    pthread_mutex_lock(&ch->mutex); \
    size_t used = (ch->count <= ch->cap) ? ch->count : ch->cap; \
    size_t raw_space = ch->cap - used; \
    int32_t space = (raw_space > (size_t)INT32_MAX) ? INT32_MAX : (int32_t)raw_space; \
    pthread_mutex_unlock(&ch->mutex); \
    return space; \
} \
\
PGY_CH_STORAGE bool \
pgy_channel_closed_##SuffixName(PgyChannel_##SuffixName *ch) \
{ \
    if (ch == NULL || ch->buf == NULL || ch->cap == 0) \
        return true; \
    pthread_mutex_lock(&ch->mutex); \
    bool closed = ch->closed; \
    pthread_mutex_unlock(&ch->mutex); \
    return closed; \
}

#define PGY_CHANNEL_DEFINE(SuffixName, CType, PGY_CH_STORAGE) \
    PGY_CHANNEL_CORE_DEFINE(SuffixName, CType, PGY_CH_STORAGE) \
    PGY_CHANNEL_RESULT_DEFINE(SuffixName, CType, PGY_CH_STORAGE)

/* The LLVM-export twin (pgy_runtime_lib_channel_int_exports.h) includes this
 * header with PGY_CHANNEL_MACRO_ONLY defined to reuse the PGY_CHANNEL_DEFINE
 * body above WITHOUT this file's own static-inline Int instantiation, then
 * re-instantiates the same macro with `extern` storage. One macro body, two
 * storage classes -- the C-inline twin and the linked-runtime twin can no
 * longer drift (target #4, dual-backend unified consumption). */
#ifndef PGY_CHANNEL_MACRO_ONLY
PGY_CHANNEL_DEFINE(Int, int32_t, static inline)
PGY_CHANNEL_LANE_DEFINE(Int, int32_t, static inline)

#include "pgy_runtime_channel_string_inline.h"
#include "pgy_runtime_channel_spsc_inline.h"
#endif
