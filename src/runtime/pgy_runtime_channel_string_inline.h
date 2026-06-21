/* =================================================================
 * Channel<String> inline runtime
 *
 * String channels own messages. Send copies the payload into channel-owned
 * storage, recv transfers that owned payload, and destroy frees pending
 * messages. Keep this owner separate from the generic channel macro so
 * pointer-storage channels cannot silently apply to String.
 *
 * Destroy is quiescent-only: close/drain/join producers and consumers before
 * freeing the backing buffer and destroying mutex/condvar state.
 * ================================================================= */

#ifndef PGY_RUNTIME_CHANNEL_STRING_INLINE_H
#define PGY_RUNTIME_CHANNEL_STRING_INLINE_H

#include "pgy_runtime_channel_status.h"

typedef struct
{
    char           **buf;
    size_t           cap;
    size_t           head;
    size_t           tail;
    size_t           count;
    bool             closed;
    pthread_mutex_t  mutex;
    pthread_cond_t   cond_not_full;
    pthread_cond_t   cond_not_empty;
} PgyChannel_String;

static inline bool
pgy_channel_string_inline_is_initialized(PgyChannel_String *ch)
{
    return ch != NULL && ch->buf != NULL && ch->cap > 0;
}

static inline char *
pgy_channel_string_inline_dup(const char *value)
{
    return pgy_runtime_strdup(value != NULL ? value : "");
}

static inline void
pgy_channel_string_inline_free_pending(PgyChannel_String *ch)
{
    if (ch == NULL || ch->buf == NULL || ch->cap == 0)
        return;
    for (size_t i = 0; i < ch->cap; i++) {
        free(ch->buf[i]);
        ch->buf[i] = NULL;
    }
    ch->head = 0;
    ch->tail = 0;
    ch->count = 0;
}

static inline void
pgy_channel_init_String(PgyChannel_String *ch, size_t capacity)
{
    if (ch == NULL) {
        pgy_runtime_warn_invalid_channel("init_String", "null channel");
        return;
    }
    /* CHANNEL_COUNT budget charge (R6) -- C path String channel (see
     * pgy_runtime_channel_inline.h for the rationale; LLVM charges the export
     * twin in pgy_runtime_lib_channel_string_exports.h). */
    if (pgy_budget_is_imposed_export())
        pgy_budget_charge_export(PGY_BUDGET_CHANNEL_COUNT, 1, "channel");
    capacity = pgy_runtime_channel_capacity_or_default("init_String", capacity);
    if (capacity > SIZE_MAX / sizeof(char *)) {
        pgy_runtime_warn_invalid_channel("init_String", "capacity overflows buffer size");
        ch->buf = NULL;
        ch->cap = 0;
        return;
    }
    ch->buf = (char **)calloc(capacity, sizeof(char *));
    if (ch->buf == NULL) {
        pgy_runtime_warn_invalid_channel("init_String", "buffer allocation failed");
        ch->cap = 0;
        return;
    }
    ch->cap = capacity;
    ch->head = 0;
    ch->tail = 0;
    ch->count = 0;
    ch->closed = false;
    if (pthread_mutex_init(&ch->mutex, NULL) != 0) {
        pgy_runtime_warn_invalid_channel("init_String", "mutex initialization failed");
        free(ch->buf);
        ch->buf = NULL;
        ch->cap = 0;
        return;
    }
    if (pthread_cond_init(&ch->cond_not_full, NULL) != 0) {
        pgy_runtime_warn_invalid_channel("init_String", "not-full condition initialization failed");
        pthread_mutex_destroy(&ch->mutex);
        free(ch->buf);
        ch->buf = NULL;
        ch->cap = 0;
        return;
    }
    if (pthread_cond_init(&ch->cond_not_empty, NULL) != 0) {
        pgy_runtime_warn_invalid_channel("init_String", "not-empty condition initialization failed");
        pthread_cond_destroy(&ch->cond_not_full);
        pthread_mutex_destroy(&ch->mutex);
        free(ch->buf);
        ch->buf = NULL;
        ch->cap = 0;
        return;
    }
}

static inline void
pgy_channel_close_String(PgyChannel_String *ch)
{
    if (!pgy_channel_string_inline_is_initialized(ch))
        return;
    pthread_mutex_lock(&ch->mutex);
    ch->closed = true;
    pthread_cond_broadcast(&ch->cond_not_full);
    pthread_cond_broadcast(&ch->cond_not_empty);
    pthread_mutex_unlock(&ch->mutex);
}

static inline void
pgy_channel_destroy_String(PgyChannel_String *ch)
{
    if (!pgy_channel_string_inline_is_initialized(ch))
        return;
    pthread_mutex_lock(&ch->mutex);
    pgy_channel_string_inline_free_pending(ch);
    pthread_mutex_unlock(&ch->mutex);
    pthread_mutex_destroy(&ch->mutex);
    pthread_cond_destroy(&ch->cond_not_full);
    pthread_cond_destroy(&ch->cond_not_empty);
    free(ch->buf);
    ch->buf = NULL;
    ch->cap = 0;
    ch->head = 0;
    ch->tail = 0;
    ch->count = 0;
    ch->closed = true;
}

static inline bool
pgy_channel_send_String(PgyChannel_String *ch, char *value)
{
    if (!pgy_channel_string_inline_is_initialized(ch)) {
        pgy_runtime_warn_invalid_channel("send_String",
            ch == NULL ? "null channel" : "channel is not initialized");
        return false;
    }
    pthread_mutex_lock(&ch->mutex);
    while (ch->count >= ch->cap && !ch->closed) {
        if (pgy_async_in_coroutine()) {
            pthread_mutex_unlock(&ch->mutex);
            pgy_async_yield();
            pthread_mutex_lock(&ch->mutex);
        } else {
            if (pthread_cond_wait(&ch->cond_not_full, &ch->mutex) != 0) {
                pgy_runtime_warn_invalid_channel("send_String",
                    "not-full condition wait failed");
                pthread_mutex_unlock(&ch->mutex);
                return false;
            }
        }
    }
    if (ch->closed) {
        pgy_runtime_warn_invalid_channel("send_String", "channel is closed");
        pthread_mutex_unlock(&ch->mutex);
        return false;
    }
    char *owned = pgy_channel_string_inline_dup(value);
    if (owned == NULL) {
        pgy_runtime_warn_invalid_channel("send_String", "payload duplication failed");
        pthread_mutex_unlock(&ch->mutex);
        return false;
    }
    ch->buf[ch->tail] = owned;
    ch->tail = (ch->tail + 1) % ch->cap;
    ch->count++;
    pthread_cond_signal(&ch->cond_not_empty);
    pthread_mutex_unlock(&ch->mutex);
    return true;
}

static inline bool
pgy_channel_try_send_String(PgyChannel_String *ch, char *value)
{
    if (!pgy_channel_string_inline_is_initialized(ch)) {
        pgy_runtime_warn_invalid_channel("try_send_String",
            ch == NULL ? "null channel" : "channel is not initialized");
        return false;
    }
    pthread_mutex_lock(&ch->mutex);
    if (ch->closed || ch->count >= ch->cap) {
        pgy_runtime_warn_invalid_channel("try_send_String",
            ch->closed ? "channel is closed" : "channel is full");
        pthread_mutex_unlock(&ch->mutex);
        return false;
    }
    char *owned = pgy_channel_string_inline_dup(value);
    if (owned == NULL) {
        pgy_runtime_warn_invalid_channel("try_send_String", "payload duplication failed");
        pthread_mutex_unlock(&ch->mutex);
        return false;
    }
    ch->buf[ch->tail] = owned;
    ch->tail = (ch->tail + 1) % ch->cap;
    ch->count++;
    pthread_cond_signal(&ch->cond_not_empty);
    pthread_mutex_unlock(&ch->mutex);
    return true;
}

static inline PgyOption_Bool
pgy_channel_try_send_status_String(PgyChannel_String *ch, char *value)
{
    if (!pgy_channel_string_inline_is_initialized(ch))
        return Some_Bool(false);
    pthread_mutex_lock(&ch->mutex);
    if (ch->closed) {
        pthread_mutex_unlock(&ch->mutex);
        return Some_Bool(false);
    }
    if (ch->count >= ch->cap) {
        pthread_mutex_unlock(&ch->mutex);
        return None_Bool();
    }
    char *owned = pgy_channel_string_inline_dup(value);
    if (owned == NULL) {
        pthread_mutex_unlock(&ch->mutex);
        return Some_Bool(false);
    }
    ch->buf[ch->tail] = owned;
    ch->tail = (ch->tail + 1) % ch->cap;
    ch->count++;
    pthread_cond_signal(&ch->cond_not_empty);
    pthread_mutex_unlock(&ch->mutex);
    return Some_Bool(true);
}

static inline bool
pgy_channel_send_timeout_String(PgyChannel_String *ch,
                                char *value, uint64_t timeout_ns)
{
    if (!pgy_channel_string_inline_is_initialized(ch)) {
        pgy_runtime_warn_invalid_channel("send_timeout_String",
            ch == NULL ? "null channel" : "channel is not initialized");
        return false;
    }
    struct timespec deadline = pgy_timespec_after_ns(timeout_ns);
    pthread_mutex_lock(&ch->mutex);
    while (ch->count >= ch->cap && !ch->closed) {
        int wait_status =
            pthread_cond_timedwait(&ch->cond_not_full, &ch->mutex, &deadline);
        if (wait_status == ETIMEDOUT && ch->count >= ch->cap && !ch->closed) {
            pgy_runtime_warn_invalid_channel("send_timeout_String",
                "deadline reached while channel remained full");
            pthread_mutex_unlock(&ch->mutex);
            return false;
        }
        if (wait_status != 0) {
            pgy_runtime_warn_invalid_channel("send_timeout_String",
                "not-full condition timed wait failed");
            pthread_mutex_unlock(&ch->mutex);
            return false;
        }
    }
    if (ch->closed) {
        pgy_runtime_warn_invalid_channel("send_timeout_String", "channel is closed");
        pthread_mutex_unlock(&ch->mutex);
        return false;
    }
    char *owned = pgy_channel_string_inline_dup(value);
    if (owned == NULL) {
        pgy_runtime_warn_invalid_channel("send_timeout_String", "payload duplication failed");
        pthread_mutex_unlock(&ch->mutex);
        return false;
    }
    ch->buf[ch->tail] = owned;
    ch->tail = (ch->tail + 1) % ch->cap;
    ch->count++;
    pthread_cond_signal(&ch->cond_not_empty);
    pthread_mutex_unlock(&ch->mutex);
    return true;
}

static inline PgyOption_Bool
pgy_channel_send_timeout_status_String(PgyChannel_String *ch,
                                       char *value, uint64_t timeout_ns)
{
    if (!pgy_channel_string_inline_is_initialized(ch))
        return Some_Bool(false);
    struct timespec deadline = pgy_timespec_after_ns(timeout_ns);
    pthread_mutex_lock(&ch->mutex);
    while (ch->count >= ch->cap && !ch->closed) {
        int wait_status =
            pthread_cond_timedwait(&ch->cond_not_full, &ch->mutex, &deadline);
        if (wait_status == ETIMEDOUT && ch->count >= ch->cap && !ch->closed) {
            pthread_mutex_unlock(&ch->mutex);
            return None_Bool();
        }
        if (wait_status != 0) {
            pgy_runtime_warn_invalid_channel("send_timeout_status_String",
                "not-full condition timed wait failed");
            pthread_mutex_unlock(&ch->mutex);
            return Some_Bool(false);
        }
    }
    if (ch->closed) {
        pthread_mutex_unlock(&ch->mutex);
        return Some_Bool(false);
    }
    char *owned = pgy_channel_string_inline_dup(value);
    if (owned == NULL) {
        pthread_mutex_unlock(&ch->mutex);
        return Some_Bool(false);
    }
    ch->buf[ch->tail] = owned;
    ch->tail = (ch->tail + 1) % ch->cap;
    ch->count++;
    pthread_cond_signal(&ch->cond_not_empty);
    pthread_mutex_unlock(&ch->mutex);
    return Some_Bool(true);
}

static inline bool
pgy_channel_recv_String(PgyChannel_String *ch, char **out)
{
    if (ch == NULL || out == NULL || ch->buf == NULL || ch->cap == 0) {
        pgy_runtime_warn_invalid_channel("recv_String",
            ch == NULL ? "null channel" : (out == NULL ? "null output pointer" : "channel is not initialized"));
        return false;
    }
    *out = NULL;
    pthread_mutex_lock(&ch->mutex);
    while (ch->count == 0 && !ch->closed) {
        if (pgy_async_in_coroutine()) {
            pthread_mutex_unlock(&ch->mutex);
            pgy_async_yield();
            pthread_mutex_lock(&ch->mutex);
        } else {
            pthread_mutex_unlock(&ch->mutex);
            if (!pgy_async_progress_one()) {
                pthread_mutex_lock(&ch->mutex);
                if (pthread_cond_wait(&ch->cond_not_empty, &ch->mutex) != 0) {
                    pgy_runtime_warn_invalid_channel("recv_String",
                        "not-empty condition wait failed");
                    pthread_mutex_unlock(&ch->mutex);
                    return false;
                }
            } else {
                pthread_mutex_lock(&ch->mutex);
            }
        }
    }
    if (ch->count == 0 && ch->closed) {
        pgy_runtime_warn_invalid_channel("recv_String", "channel is closed and empty");
        pthread_mutex_unlock(&ch->mutex);
        return false;
    }
    *out = ch->buf[ch->head];
    ch->buf[ch->head] = NULL;
    ch->head = (ch->head + 1) % ch->cap;
    ch->count--;
    pthread_cond_signal(&ch->cond_not_full);
    pthread_mutex_unlock(&ch->mutex);
    return true;
}

static inline bool
pgy_channel_recv_timeout_String(PgyChannel_String *ch,
                                char **out, uint64_t timeout_ns)
{
    if (ch == NULL || out == NULL || ch->buf == NULL || ch->cap == 0) {
        pgy_runtime_warn_invalid_channel("recv_timeout_String",
            ch == NULL ? "null channel" : (out == NULL ? "null output pointer" : "channel is not initialized"));
        return false;
    }
    *out = NULL;
    struct timespec deadline = pgy_timespec_after_ns(timeout_ns);
    pthread_mutex_lock(&ch->mutex);
    while (ch->count == 0 && !ch->closed) {
        if (pgy_async_in_coroutine()) {
            pthread_mutex_unlock(&ch->mutex);
            pgy_async_yield();
            pthread_mutex_lock(&ch->mutex);
        } else {
            pthread_mutex_unlock(&ch->mutex);
            if (!pgy_async_progress_one()) {
                pthread_mutex_lock(&ch->mutex);
                int wait_status = pthread_cond_timedwait(
                    &ch->cond_not_empty, &ch->mutex, &deadline);
                if (wait_status == ETIMEDOUT && ch->count == 0 && !ch->closed) {
                    pgy_runtime_warn_invalid_channel("recv_timeout_String",
                        "deadline reached while channel remained empty");
                    pthread_mutex_unlock(&ch->mutex);
                    return false;
                }
                if (wait_status != 0) {
                    pgy_runtime_warn_invalid_channel("recv_timeout_String",
                        "not-empty condition timed wait failed");
                    pthread_mutex_unlock(&ch->mutex);
                    return false;
                }
            } else {
                pthread_mutex_lock(&ch->mutex);
            }
        }
    }
    if (ch->count == 0 && ch->closed) {
        pgy_runtime_warn_invalid_channel("recv_timeout_String", "channel is closed and empty");
        pthread_mutex_unlock(&ch->mutex);
        return false;
    }
    *out = ch->buf[ch->head];
    ch->buf[ch->head] = NULL;
    ch->head = (ch->head + 1) % ch->cap;
    ch->count--;
    pthread_cond_signal(&ch->cond_not_full);
    pthread_mutex_unlock(&ch->mutex);
    return true;
}

static inline bool
pgy_channel_try_recv_String(PgyChannel_String *ch, char **out)
{
    if (ch == NULL || out == NULL || ch->buf == NULL || ch->cap == 0) {
        pgy_runtime_warn_invalid_channel("try_recv_String",
            ch == NULL ? "null channel" : (out == NULL ? "null output pointer" : "channel is not initialized"));
        return false;
    }
    *out = NULL;
    if (!pgy_async_in_coroutine())
        (void)pgy_async_progress_one();
    pthread_mutex_lock(&ch->mutex);
    if (ch->count == 0) {
        pgy_runtime_warn_invalid_channel("try_recv_String", "channel is empty");
        pthread_mutex_unlock(&ch->mutex);
        return false;
    }
    *out = ch->buf[ch->head];
    ch->buf[ch->head] = NULL;
    ch->head = (ch->head + 1) % ch->cap;
    ch->count--;
    pthread_cond_signal(&ch->cond_not_full);
    pthread_mutex_unlock(&ch->mutex);
    return true;
}

static inline bool
pgy_channel_ready_String(PgyChannel_String *ch)
{
    if (!pgy_channel_string_inline_is_initialized(ch))
        return false;
    if (!pgy_async_in_coroutine())
        (void)pgy_async_progress_one();
    pthread_mutex_lock(&ch->mutex);
    bool ready = ch->count > 0;
    pthread_mutex_unlock(&ch->mutex);
    return ready;
}

static inline int32_t
pgy_channel_length_String(PgyChannel_String *ch)
{
    if (!pgy_channel_string_inline_is_initialized(ch))
        return 0;
    pthread_mutex_lock(&ch->mutex);
    int32_t len = (ch->count > (size_t)INT32_MAX) ? INT32_MAX : (int32_t)ch->count;
    pthread_mutex_unlock(&ch->mutex);
    return len;
}

static inline int32_t
pgy_channel_capacity_String(PgyChannel_String *ch)
{
    if (!pgy_channel_string_inline_is_initialized(ch))
        return 0;
    pthread_mutex_lock(&ch->mutex);
    int32_t cap = (ch->cap > (size_t)INT32_MAX) ? INT32_MAX : (int32_t)ch->cap;
    pthread_mutex_unlock(&ch->mutex);
    return cap;
}

static inline bool
pgy_channel_full_String(PgyChannel_String *ch)
{
    if (!pgy_channel_string_inline_is_initialized(ch))
        return false;
    pthread_mutex_lock(&ch->mutex);
    bool full = ch->count >= ch->cap;
    pthread_mutex_unlock(&ch->mutex);
    return full;
}

static inline int32_t
pgy_channel_space_String(PgyChannel_String *ch)
{
    if (!pgy_channel_string_inline_is_initialized(ch))
        return 0;
    pthread_mutex_lock(&ch->mutex);
    size_t used = (ch->count <= ch->cap) ? ch->count : ch->cap;
    size_t raw_space = ch->cap - used;
    int32_t space = (raw_space > (size_t)INT32_MAX) ? INT32_MAX : (int32_t)raw_space;
    pthread_mutex_unlock(&ch->mutex);
    return space;
}

static inline bool
pgy_channel_closed_String(PgyChannel_String *ch)
{
    if (!pgy_channel_string_inline_is_initialized(ch))
        return true;
    pthread_mutex_lock(&ch->mutex);
    bool closed = ch->closed;
    pthread_mutex_unlock(&ch->mutex);
    return closed;
}

#include "pgy_runtime_channel_string_result_inline.h"

#endif /* PGY_RUNTIME_CHANNEL_STRING_INLINE_H */
