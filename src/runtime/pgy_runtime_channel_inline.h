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

#define PGY_CHANNEL_DEFINE(SuffixName, CType) \
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
static inline void \
pgy_channel_init_##SuffixName(PgyChannel_##SuffixName *ch, size_t capacity) \
{ \
    if (ch == NULL) { \
        pgy_runtime_warn_invalid_channel("init_" #SuffixName, "null channel"); \
        return; \
    } \
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
static inline void \
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
static inline void \
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
static inline bool \
pgy_channel_send_##SuffixName(PgyChannel_##SuffixName *ch, CType value) \
{ \
    if (ch == NULL || ch->buf == NULL || ch->cap == 0) { \
        pgy_runtime_warn_invalid_channel("send_" #SuffixName, \
            ch == NULL ? "null channel" : "channel is not initialized"); \
        return false; \
    } \
    pthread_mutex_lock(&ch->mutex); \
    while (ch->count >= ch->cap && !ch->closed) { \
        if (pgy_async_in_coroutine()) { \
            pthread_mutex_unlock(&ch->mutex); \
            pgy_async_yield(); \
            pthread_mutex_lock(&ch->mutex); \
        } else { \
            if (pthread_cond_wait(&ch->cond_not_full, &ch->mutex) != 0) { \
                pgy_runtime_warn_invalid_channel("send_" #SuffixName, \
                    "not-full condition wait failed"); \
                pthread_mutex_unlock(&ch->mutex); \
                return false; \
            } \
        } \
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
static inline bool \
pgy_channel_try_send_##SuffixName(PgyChannel_##SuffixName *ch, CType value) \
{ \
    if (ch == NULL || ch->buf == NULL || ch->cap == 0) { \
        pgy_runtime_warn_invalid_channel("try_send_" #SuffixName, \
            ch == NULL ? "null channel" : "channel is not initialized"); \
        return false; \
    } \
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
static inline PgyOption_Bool \
pgy_channel_try_send_status_##SuffixName(PgyChannel_##SuffixName *ch, CType value) \
{ \
    if (ch == NULL || ch->buf == NULL || ch->cap == 0) \
        return Some_Bool(false); \
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
static inline bool \
pgy_channel_send_timeout_##SuffixName(PgyChannel_##SuffixName *ch, \
                                      CType value, uint64_t timeout_ns) \
{ \
    if (ch == NULL || ch->buf == NULL || ch->cap == 0) { \
        pgy_runtime_warn_invalid_channel("send_timeout_" #SuffixName, \
            ch == NULL ? "null channel" : "channel is not initialized"); \
        return false; \
    } \
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
static inline PgyOption_Bool \
pgy_channel_send_timeout_status_##SuffixName(PgyChannel_##SuffixName *ch, \
                                             CType value, uint64_t timeout_ns) \
{ \
    if (ch == NULL || ch->buf == NULL || ch->cap == 0) \
        return Some_Bool(false); \
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
static inline bool \
pgy_channel_recv_##SuffixName(PgyChannel_##SuffixName *ch, CType *out) \
{ \
    if (ch == NULL || out == NULL || ch->buf == NULL || ch->cap == 0) { \
        pgy_runtime_warn_invalid_channel("recv_" #SuffixName, \
            ch == NULL ? "null channel" : (out == NULL ? "null output pointer" : "channel is not initialized")); \
        return false; \
    } \
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
                if (pthread_cond_wait(&ch->cond_not_empty, &ch->mutex) != 0) { \
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
    if (ch->count == 0 && ch->closed) { \
        pgy_runtime_warn_invalid_channel("recv_" #SuffixName, "channel is closed and empty"); \
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
static inline bool \
pgy_channel_recv_timeout_##SuffixName(PgyChannel_##SuffixName *ch, \
                                      CType *out, uint64_t timeout_ns) \
{ \
    if (ch == NULL || out == NULL || ch->buf == NULL || ch->cap == 0) { \
        pgy_runtime_warn_invalid_channel("recv_timeout_" #SuffixName, \
            ch == NULL ? "null channel" : (out == NULL ? "null output pointer" : "channel is not initialized")); \
        return false; \
    } \
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
static inline bool \
pgy_channel_try_recv_##SuffixName(PgyChannel_##SuffixName *ch, CType *out) \
{ \
    if (ch == NULL || out == NULL || ch->buf == NULL || ch->cap == 0) { \
        pgy_runtime_warn_invalid_channel("try_recv_" #SuffixName, \
            ch == NULL ? "null channel" : (out == NULL ? "null output pointer" : "channel is not initialized")); \
        return false; \
    } \
    if (!pgy_async_in_coroutine()) \
        (void)pgy_async_progress_one(); \
    pthread_mutex_lock(&ch->mutex); \
    if (ch->count == 0) { \
        pgy_runtime_warn_invalid_channel("try_recv_" #SuffixName, "channel is empty"); \
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
static inline bool \
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
static inline int32_t \
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
static inline int32_t \
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
static inline bool \
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
static inline int32_t \
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
static inline bool \
pgy_channel_closed_##SuffixName(PgyChannel_##SuffixName *ch) \
{ \
    if (ch == NULL || ch->buf == NULL || ch->cap == 0) \
        return true; \
    pthread_mutex_lock(&ch->mutex); \
    bool closed = ch->closed; \
    pthread_mutex_unlock(&ch->mutex); \
    return closed; \
} \
\
static inline PgyRuntimeChannel##SuffixName##Result \
pgy_channel_recv_result_##SuffixName(PgyChannel_##SuffixName *ch) \
{ \
    PgyRuntimeChannel##SuffixName##Result result; \
    CType out; \
    memset(&out, 0, sizeof(CType)); \
    if (ch == NULL) { \
        result.tag = PGY_RUNTIME_CHANNEL_RESULT_ERR; \
        result.err = pgy_runtime_channel_failure_from_status( \
            PGY_RUNTIME_CHANNEL_STATUS_NULL_CHANNEL, "recv_" #SuffixName); \
        return result; \
    } \
    if (ch->buf == NULL || ch->cap == 0) { \
        result.tag = PGY_RUNTIME_CHANNEL_RESULT_ERR; \
        result.err = pgy_runtime_channel_failure_from_status( \
            PGY_RUNTIME_CHANNEL_STATUS_UNINITIALIZED, "recv_" #SuffixName); \
        return result; \
    } \
    if (!pgy_channel_recv_##SuffixName(ch, &out)) { \
        result.tag = PGY_RUNTIME_CHANNEL_RESULT_ERR; \
        result.err = pgy_runtime_channel_failure_from_status( \
            PGY_RUNTIME_CHANNEL_STATUS_CLOSED_EMPTY, "recv_" #SuffixName); \
        return result; \
    } \
    result.tag = PGY_RUNTIME_CHANNEL_RESULT_OK; \
    result.ok = out; \
    return result; \
} \
\
static inline PgyRuntimeChannel##SuffixName##Result \
pgy_channel_try_recv_result_##SuffixName(PgyChannel_##SuffixName *ch) \
{ \
    PgyRuntimeChannel##SuffixName##Result result; \
    CType out; \
    memset(&out, 0, sizeof(CType)); \
    if (ch == NULL) { \
        result.tag = PGY_RUNTIME_CHANNEL_RESULT_ERR; \
        result.err = pgy_runtime_channel_failure_from_status( \
            PGY_RUNTIME_CHANNEL_STATUS_NULL_CHANNEL, "try_recv_" #SuffixName); \
        return result; \
    } \
    if (ch->buf == NULL || ch->cap == 0) { \
        result.tag = PGY_RUNTIME_CHANNEL_RESULT_ERR; \
        result.err = pgy_runtime_channel_failure_from_status( \
            PGY_RUNTIME_CHANNEL_STATUS_UNINITIALIZED, "try_recv_" #SuffixName); \
        return result; \
    } \
    if (!pgy_channel_try_recv_##SuffixName(ch, &out)) { \
        result.tag = PGY_RUNTIME_CHANNEL_RESULT_ERR; \
        result.err = pgy_runtime_channel_failure_from_status( \
            pgy_channel_closed_##SuffixName(ch) \
                ? PGY_RUNTIME_CHANNEL_STATUS_CLOSED_EMPTY \
                : PGY_RUNTIME_CHANNEL_STATUS_EMPTY, \
            "try_recv_" #SuffixName); \
        return result; \
    } \
    result.tag = PGY_RUNTIME_CHANNEL_RESULT_OK; \
    result.ok = out; \
    return result; \
} \
\
static inline PgyRuntimeChannel##SuffixName##Result \
pgy_channel_recv_timeout_result_##SuffixName(PgyChannel_##SuffixName *ch, \
                                             uint64_t timeout_ns) \
{ \
    PgyRuntimeChannel##SuffixName##Result result; \
    CType out; \
    memset(&out, 0, sizeof(CType)); \
    if (ch == NULL) { \
        result.tag = PGY_RUNTIME_CHANNEL_RESULT_ERR; \
        result.err = pgy_runtime_channel_failure_from_status( \
            PGY_RUNTIME_CHANNEL_STATUS_NULL_CHANNEL, \
            "recv_timeout_" #SuffixName); \
        return result; \
    } \
    if (ch->buf == NULL || ch->cap == 0) { \
        result.tag = PGY_RUNTIME_CHANNEL_RESULT_ERR; \
        result.err = pgy_runtime_channel_failure_from_status( \
            PGY_RUNTIME_CHANNEL_STATUS_UNINITIALIZED, \
            "recv_timeout_" #SuffixName); \
        return result; \
    } \
    if (!pgy_channel_recv_timeout_##SuffixName(ch, &out, timeout_ns)) { \
        result.tag = PGY_RUNTIME_CHANNEL_RESULT_ERR; \
        result.err = pgy_runtime_channel_failure_from_status( \
            pgy_channel_closed_##SuffixName(ch) \
                ? PGY_RUNTIME_CHANNEL_STATUS_CLOSED_EMPTY \
                : PGY_RUNTIME_CHANNEL_STATUS_TIMEOUT, \
            "recv_timeout_" #SuffixName); \
        return result; \
    } \
    result.tag = PGY_RUNTIME_CHANNEL_RESULT_OK; \
    result.ok = out; \
    return result; \
} \
\
/* Blocking recv that returns the value (convenience wrapper). \
 * Returns zero-initialized value if channel closed and empty. */ \
static inline CType \
pgy_channel_recv_val_##SuffixName(PgyChannel_##SuffixName *ch) \
{ \
    PgyRuntimeChannel##SuffixName##Result result = \
        pgy_channel_recv_result_##SuffixName(ch); \
    return result.tag == PGY_RUNTIME_CHANNEL_RESULT_OK \
        ? result.ok : (CType)0; \
}

PGY_CHANNEL_DEFINE(Int, int32_t)

#include "pgy_runtime_channel_string_inline.h"
#include "pgy_runtime_channel_spsc_inline.h"
