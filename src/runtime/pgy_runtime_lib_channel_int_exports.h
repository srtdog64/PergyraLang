/* =================================================================
 * Channel - Int (thread-safe with mutex + condvar)
 *
 * Destroy is quiescent-only: close/drain/join producers and consumers before
 * freeing the backing buffer and destroying mutex/condvar state.
 * ================================================================= */

#include <pthread.h>
#include "pgy_runtime_channel_status.h"

/* PgyOption_Bool is needed for try_send_status / send_timeout_status.
 * Must match PGY_OPTION_DEFINE(Bool, bool) in pgy_runtime.h. */
typedef struct { int32_t tag; bool value; } PgyOption_Bool;
static inline PgyOption_Bool Some_Bool(bool v) { return (PgyOption_Bool){ 0, v }; }
static inline PgyOption_Bool None_Bool(void)   { return (PgyOption_Bool){ 1, false }; }

typedef struct {
    int32_t        *buffer;
    size_t          capacity;
    size_t          head;
    size_t          tail;
    size_t          count;
    bool            closed;
    pthread_mutex_t mutex;
    pthread_cond_t  cond_not_full;
    pthread_cond_t  cond_not_empty;
} PgyChannel_Int_RT;

static bool
pgy_channel_int_is_initialized(PgyChannel_Int_RT *ch)
{
    return ch != NULL && ch->buffer != NULL && ch->capacity > 0;
}

static int32_t
pgy_channel_int_size_to_i32(size_t value)
{
    return value > (size_t)INT32_MAX ? INT32_MAX : (int32_t)value;
}

void pgy_channel_init_Int(PgyChannel_Int_RT *ch, size_t cap)
{
    if (ch == NULL) {
        pgy_runtime_warn_invalid_channel("init_Int", "null channel");
        return;
    }
    /* CHANNEL_COUNT budget charge (R6) -- this is the LLVM/.bc path's channel
     * init (the C path charges in the pgy_channel_init_<T> inline macro). Keep
     * the two in lockstep so a CHANNEL_COUNT ceiling fail-closes identically on
     * both backends. Deny-before-allocate, behind the imposed fast-path. */
    if (pgy_budget_is_imposed_export())
        pgy_budget_charge_export(PGY_BUDGET_CHANNEL_COUNT, 1, "channel");
    cap = pgy_runtime_channel_capacity_or_default("init_Int", cap);
    if (cap > SIZE_MAX / sizeof(int32_t)) {
        pgy_runtime_warn_invalid_channel("init_Int", "capacity overflows buffer size");
        ch->buffer = NULL;
        ch->capacity = 0;
        return;
    }
    ch->buffer   = (int32_t *)calloc(cap, sizeof(int32_t));
    if (ch->buffer == NULL) {
        pgy_runtime_warn_invalid_channel("init_Int", "buffer allocation failed");
        ch->capacity = 0;
        return;
    }
    ch->capacity = cap;
    ch->head     = 0;
    ch->tail     = 0;
    ch->count    = 0;
    ch->closed   = false;
    if (pthread_mutex_init(&ch->mutex, NULL) != 0) {
        pgy_runtime_warn_invalid_channel("init_Int", "mutex initialization failed");
        free(ch->buffer);
        ch->buffer = NULL;
        ch->capacity = 0;
        return;
    }
    if (pthread_cond_init(&ch->cond_not_full, NULL) != 0) {
        pgy_runtime_warn_invalid_channel("init_Int", "not-full condition initialization failed");
        pthread_mutex_destroy(&ch->mutex);
        free(ch->buffer);
        ch->buffer = NULL;
        ch->capacity = 0;
        return;
    }
    if (pthread_cond_init(&ch->cond_not_empty, NULL) != 0) {
        pgy_runtime_warn_invalid_channel("init_Int", "not-empty condition initialization failed");
        pthread_cond_destroy(&ch->cond_not_full);
        pthread_mutex_destroy(&ch->mutex);
        free(ch->buffer);
        ch->buffer = NULL;
        ch->capacity = 0;
        return;
    }
}

void pgy_channel_destroy_Int(PgyChannel_Int_RT *ch)
{
    if (!pgy_channel_int_is_initialized(ch)) return;
    pthread_mutex_destroy(&ch->mutex);
    pthread_cond_destroy(&ch->cond_not_full);
    pthread_cond_destroy(&ch->cond_not_empty);
    free(ch->buffer);
    ch->buffer = NULL;
}

void pgy_channel_close_Int(PgyChannel_Int_RT *ch)
{
    if (!pgy_channel_int_is_initialized(ch)) return;
    pthread_mutex_lock(&ch->mutex);
    ch->closed = true;
    pthread_cond_broadcast(&ch->cond_not_full);
    pthread_cond_broadcast(&ch->cond_not_empty);
    pthread_mutex_unlock(&ch->mutex);
}

bool pgy_channel_send_Int(PgyChannel_Int_RT *ch, int32_t v)
{
    if (ch == NULL) {
        pgy_runtime_warn_invalid_channel("send_Int", "null channel");
        return false;
    }
    if (ch->buffer == NULL || ch->capacity == 0) {
        pgy_runtime_warn_invalid_channel("send_Int", "channel is not initialized");
        return false;
    }
    pthread_mutex_lock(&ch->mutex);
    while (ch->count >= ch->capacity && !ch->closed) {
        if (pthread_cond_wait(&ch->cond_not_full, &ch->mutex) != 0) {
            pgy_runtime_warn_invalid_channel("send_Int",
                "not-full condition wait failed");
            pthread_mutex_unlock(&ch->mutex);
            return false;
        }
    }
    if (ch->closed) {
        pgy_runtime_warn_invalid_channel("send_Int", "channel is closed");
        pthread_mutex_unlock(&ch->mutex);
        return false;
    }
    ch->buffer[ch->tail] = v;
    ch->tail = (ch->tail + 1) % ch->capacity;
    ch->count++;
    pthread_cond_signal(&ch->cond_not_empty);
    pthread_mutex_unlock(&ch->mutex);
    return true;
}

bool pgy_channel_try_send_Int(PgyChannel_Int_RT *ch, int32_t v)
{
    if (ch == NULL) {
        pgy_runtime_warn_invalid_channel("try_send_Int", "null channel");
        return false;
    }
    if (ch->buffer == NULL || ch->capacity == 0) {
        pgy_runtime_warn_invalid_channel("try_send_Int", "channel is not initialized");
        return false;
    }
    pthread_mutex_lock(&ch->mutex);
    if (ch->closed || ch->count >= ch->capacity) {
        pgy_runtime_warn_invalid_channel("try_send_Int",
            ch->closed ? "channel is closed" : "channel is full");
        pthread_mutex_unlock(&ch->mutex);
        return false;
    }
    ch->buffer[ch->tail] = v;
    ch->tail = (ch->tail + 1) % ch->capacity;
    ch->count++;
    pthread_cond_signal(&ch->cond_not_empty);
    pthread_mutex_unlock(&ch->mutex);
    return true;
}

PgyOption_Bool pgy_channel_try_send_status_Int(PgyChannel_Int_RT *ch,
                                               int32_t v)
{
    if (!pgy_channel_int_is_initialized(ch))
        return Some_Bool(false);
    pthread_mutex_lock(&ch->mutex);
    if (ch->closed) {
        pthread_mutex_unlock(&ch->mutex);
        return Some_Bool(false);
    }
    if (ch->count >= ch->capacity) {
        pthread_mutex_unlock(&ch->mutex);
        return None_Bool();
    }
    ch->buffer[ch->tail] = v;
    ch->tail = (ch->tail + 1) % ch->capacity;
    ch->count++;
    pthread_cond_signal(&ch->cond_not_empty);
    pthread_mutex_unlock(&ch->mutex);
    return Some_Bool(true);
}

int32_t pgy_channel_try_send_status_code_Int(PgyChannel_Int_RT *ch,
                                             int32_t v)
{
    PgyOption_Bool status = pgy_channel_try_send_status_Int(ch, v);
    if (status.tag != 0)
        return -1;
    return status.value ? 1 : 0;
}

bool pgy_channel_send_timeout_Int(PgyChannel_Int_RT *ch, int32_t v,
                                  uint64_t timeout_ns)
{
    if (ch == NULL) {
        pgy_runtime_warn_invalid_channel("send_timeout_Int", "null channel");
        return false;
    }
    if (ch->buffer == NULL || ch->capacity == 0) {
        pgy_runtime_warn_invalid_channel("send_timeout_Int", "channel is not initialized");
        return false;
    }
    struct timespec deadline = pgy_runtime_deadline_after_ns(timeout_ns);
    pthread_mutex_lock(&ch->mutex);
    while (ch->count >= ch->capacity && !ch->closed) {
        int wait_status =
            pthread_cond_timedwait(&ch->cond_not_full, &ch->mutex, &deadline);
        if (wait_status == ETIMEDOUT && ch->count >= ch->capacity && !ch->closed) {
            pgy_runtime_warn_invalid_channel("send_timeout_Int", "deadline reached while channel remained full");
            pthread_mutex_unlock(&ch->mutex);
            return false;
        }
        if (wait_status != 0) {
            pgy_runtime_warn_invalid_channel("send_timeout_Int",
                "not-full condition timed wait failed");
            pthread_mutex_unlock(&ch->mutex);
            return false;
        }
    }
    if (ch->closed) {
        pgy_runtime_warn_invalid_channel("send_timeout_Int", "channel is closed");
        pthread_mutex_unlock(&ch->mutex);
        return false;
    }
    ch->buffer[ch->tail] = v;
    ch->tail = (ch->tail + 1) % ch->capacity;
    ch->count++;
    pthread_cond_signal(&ch->cond_not_empty);
    pthread_mutex_unlock(&ch->mutex);
    return true;
}

PgyOption_Bool pgy_channel_send_timeout_status_Int(PgyChannel_Int_RT *ch,
                                                   int32_t v,
                                                   uint64_t timeout_ns)
{
    if (!pgy_channel_int_is_initialized(ch))
        return Some_Bool(false);
    struct timespec deadline = pgy_runtime_deadline_after_ns(timeout_ns);
    pthread_mutex_lock(&ch->mutex);
    while (ch->count >= ch->capacity && !ch->closed) {
        int wait_status =
            pthread_cond_timedwait(&ch->cond_not_full, &ch->mutex, &deadline);
        if (wait_status == ETIMEDOUT && ch->count >= ch->capacity && !ch->closed) {
            pthread_mutex_unlock(&ch->mutex);
            return None_Bool();
        }
        if (wait_status != 0) {
            pgy_runtime_warn_invalid_channel("send_timeout_status_Int",
                "not-full condition timed wait failed");
            pthread_mutex_unlock(&ch->mutex);
            return Some_Bool(false);
        }
    }
    if (ch->closed) {
        pthread_mutex_unlock(&ch->mutex);
        return Some_Bool(false);
    }
    ch->buffer[ch->tail] = v;
    ch->tail = (ch->tail + 1) % ch->capacity;
    ch->count++;
    pthread_cond_signal(&ch->cond_not_empty);
    pthread_mutex_unlock(&ch->mutex);
    return Some_Bool(true);
}

int32_t pgy_channel_send_timeout_status_code_Int(PgyChannel_Int_RT *ch,
                                                 int32_t v,
                                                 uint64_t timeout_ns)
{
    PgyOption_Bool status =
        pgy_channel_send_timeout_status_Int(ch, v, timeout_ns);
    if (status.tag != 0)
        return -1;
    return status.value ? 1 : 0;
}

bool pgy_channel_recv_Int(PgyChannel_Int_RT *ch, int32_t *out)
{
    if (ch == NULL || out == NULL) {
        pgy_runtime_warn_invalid_channel("recv_Int",
            ch == NULL ? "null channel" : "null output pointer");
        return false;
    }
    if (ch->buffer == NULL || ch->capacity == 0) {
        pgy_runtime_warn_invalid_channel("recv_Int", "channel is not initialized");
        return false;
    }
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
                    pgy_runtime_warn_invalid_channel("recv_Int",
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
        pgy_runtime_warn_invalid_channel("recv_Int", "channel is closed and empty");
        pthread_mutex_unlock(&ch->mutex);
        return false;
    }
    *out = ch->buffer[ch->head];
    ch->head = (ch->head + 1) % ch->capacity;
    ch->count--;
    pthread_cond_signal(&ch->cond_not_full);
    pthread_mutex_unlock(&ch->mutex);
    return true;
}

bool pgy_channel_ready_Int(PgyChannel_Int_RT *ch)
{
    if (!pgy_channel_int_is_initialized(ch)) {
        pgy_runtime_warn_invalid_channel("ready_Int",
            ch == NULL ? "null channel" : "channel is not initialized");
        return false;
    }
    if (!pgy_async_in_coroutine())
        (void)pgy_async_progress_one();
    pthread_mutex_lock(&ch->mutex);
    bool ready = ch->count > 0;
    pthread_mutex_unlock(&ch->mutex);
    return ready;
}

int32_t pgy_channel_length_Int(PgyChannel_Int_RT *ch)
{
    if (!pgy_channel_int_is_initialized(ch)) {
        pgy_runtime_warn_invalid_channel("length_Int",
            ch == NULL ? "null channel" : "channel is not initialized");
        return 0;
    }
    pthread_mutex_lock(&ch->mutex);
    int32_t len = pgy_channel_int_size_to_i32(ch->count);
    pthread_mutex_unlock(&ch->mutex);
    return len;
}

int32_t pgy_channel_capacity_Int(PgyChannel_Int_RT *ch)
{
    if (!pgy_channel_int_is_initialized(ch)) {
        pgy_runtime_warn_invalid_channel("capacity_Int",
            ch == NULL ? "null channel" : "channel is not initialized");
        return 0;
    }
    pthread_mutex_lock(&ch->mutex);
    int32_t cap = pgy_channel_int_size_to_i32(ch->capacity);
    pthread_mutex_unlock(&ch->mutex);
    return cap;
}

bool pgy_channel_full_Int(PgyChannel_Int_RT *ch)
{
    if (!pgy_channel_int_is_initialized(ch)) {
        pgy_runtime_warn_invalid_channel("full_Int",
            ch == NULL ? "null channel" : "channel is not initialized");
        return false;
    }
    pthread_mutex_lock(&ch->mutex);
    bool full = ch->count >= ch->capacity;
    pthread_mutex_unlock(&ch->mutex);
    return full;
}

int32_t pgy_channel_space_Int(PgyChannel_Int_RT *ch)
{
    if (!pgy_channel_int_is_initialized(ch)) {
        pgy_runtime_warn_invalid_channel("space_Int",
            ch == NULL ? "null channel" : "channel is not initialized");
        return 0;
    }
    pthread_mutex_lock(&ch->mutex);
    size_t used = ch->count <= ch->capacity ? ch->count : ch->capacity;
    int32_t space = pgy_channel_int_size_to_i32(ch->capacity - used);
    pthread_mutex_unlock(&ch->mutex);
    return space;
}

bool pgy_channel_closed_Int(PgyChannel_Int_RT *ch)
{
    if (!pgy_channel_int_is_initialized(ch)) {
        pgy_runtime_warn_invalid_channel("closed_Int",
            ch == NULL ? "null channel" : "channel is not initialized");
        return true;
    }
    pthread_mutex_lock(&ch->mutex);
    bool closed = ch->closed;
    pthread_mutex_unlock(&ch->mutex);
    return closed;
}

bool pgy_channel_try_recv_Int(PgyChannel_Int_RT *ch, int32_t *out)
{
    if (ch == NULL || out == NULL) {
        pgy_runtime_warn_invalid_channel("try_recv_Int",
            ch == NULL ? "null channel" : "null output pointer");
        return false;
    }
    if (ch->buffer == NULL || ch->capacity == 0) {
        pgy_runtime_warn_invalid_channel("try_recv_Int", "channel is not initialized");
        return false;
    }
    if (!pgy_async_in_coroutine())
        (void)pgy_async_progress_one();
    pthread_mutex_lock(&ch->mutex);
    if (ch->count == 0) {
        pgy_runtime_warn_invalid_channel("try_recv_Int", "channel is empty");
        pthread_mutex_unlock(&ch->mutex);
        return false;
    }
    *out = ch->buffer[ch->head];
    ch->head = (ch->head + 1) % ch->capacity;
    ch->count--;
    pthread_cond_signal(&ch->cond_not_full);
    pthread_mutex_unlock(&ch->mutex);
    return true;
}

bool pgy_channel_recv_timeout_Int(PgyChannel_Int_RT *ch, int32_t *out,
                                  uint64_t timeout_ns)
{
    if (ch == NULL || out == NULL) {
        pgy_runtime_warn_invalid_channel("recv_timeout_Int",
            ch == NULL ? "null channel" : "null output pointer");
        return false;
    }
    if (ch->buffer == NULL || ch->capacity == 0) {
        pgy_runtime_warn_invalid_channel("recv_timeout_Int", "channel is not initialized");
        return false;
    }
    struct timespec deadline = pgy_runtime_deadline_after_ns(timeout_ns);
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
                    pgy_runtime_warn_invalid_channel("recv_timeout_Int", "deadline reached while channel remained empty");
                    pthread_mutex_unlock(&ch->mutex);
                    return false;
                }
                if (wait_status != 0) {
                    pgy_runtime_warn_invalid_channel("recv_timeout_Int",
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
        pgy_runtime_warn_invalid_channel("recv_timeout_Int", "channel is closed and empty");
        pthread_mutex_unlock(&ch->mutex);
        return false;
    }
    *out = ch->buffer[ch->head];
    ch->head = (ch->head + 1) % ch->capacity;
    ch->count--;
    pthread_cond_signal(&ch->cond_not_full);
    pthread_mutex_unlock(&ch->mutex);
    return true;
}

PgyRuntimeChannelIntResult
pgy_channel_recv_result_Int(PgyChannel_Int_RT *ch)
{
    PgyRuntimeChannelIntResult result;
    int32_t out = 0;

    if (ch == NULL) {
        result.tag = PGY_RUNTIME_CHANNEL_RESULT_ERR;
        result.err = pgy_runtime_channel_failure_from_status(
            PGY_RUNTIME_CHANNEL_STATUS_NULL_CHANNEL, "recv_Int");
        return result;
    }
    if (ch->buffer == NULL || ch->capacity == 0) {
        result.tag = PGY_RUNTIME_CHANNEL_RESULT_ERR;
        result.err = pgy_runtime_channel_failure_from_status(
            PGY_RUNTIME_CHANNEL_STATUS_UNINITIALIZED, "recv_Int");
        return result;
    }
    if (!pgy_channel_recv_Int(ch, &out)) {
        result.tag = PGY_RUNTIME_CHANNEL_RESULT_ERR;
        result.err = pgy_runtime_channel_failure_from_status(
            PGY_RUNTIME_CHANNEL_STATUS_CLOSED_EMPTY, "recv_Int");
        return result;
    }
    result.tag = PGY_RUNTIME_CHANNEL_RESULT_OK;
    result.ok = out;
    return result;
}

PgyRuntimeChannelIntResult
pgy_channel_try_recv_result_Int(PgyChannel_Int_RT *ch)
{
    PgyRuntimeChannelIntResult result;
    int32_t out = 0;

    if (ch == NULL) {
        result.tag = PGY_RUNTIME_CHANNEL_RESULT_ERR;
        result.err = pgy_runtime_channel_failure_from_status(
            PGY_RUNTIME_CHANNEL_STATUS_NULL_CHANNEL, "try_recv_Int");
        return result;
    }
    if (ch->buffer == NULL || ch->capacity == 0) {
        result.tag = PGY_RUNTIME_CHANNEL_RESULT_ERR;
        result.err = pgy_runtime_channel_failure_from_status(
            PGY_RUNTIME_CHANNEL_STATUS_UNINITIALIZED, "try_recv_Int");
        return result;
    }
    if (!pgy_channel_try_recv_Int(ch, &out)) {
        result.tag = PGY_RUNTIME_CHANNEL_RESULT_ERR;
        result.err = pgy_runtime_channel_failure_from_status(
            pgy_channel_closed_Int(ch)
                ? PGY_RUNTIME_CHANNEL_STATUS_CLOSED_EMPTY
                : PGY_RUNTIME_CHANNEL_STATUS_EMPTY,
            "try_recv_Int");
        return result;
    }
    result.tag = PGY_RUNTIME_CHANNEL_RESULT_OK;
    result.ok = out;
    return result;
}

PgyRuntimeChannelIntResult
pgy_channel_recv_timeout_result_Int(PgyChannel_Int_RT *ch, uint64_t timeout_ns)
{
    PgyRuntimeChannelIntResult result;
    int32_t out = 0;

    if (ch == NULL) {
        result.tag = PGY_RUNTIME_CHANNEL_RESULT_ERR;
        result.err = pgy_runtime_channel_failure_from_status(
            PGY_RUNTIME_CHANNEL_STATUS_NULL_CHANNEL, "recv_timeout_Int");
        return result;
    }
    if (ch->buffer == NULL || ch->capacity == 0) {
        result.tag = PGY_RUNTIME_CHANNEL_RESULT_ERR;
        result.err = pgy_runtime_channel_failure_from_status(
            PGY_RUNTIME_CHANNEL_STATUS_UNINITIALIZED, "recv_timeout_Int");
        return result;
    }
    if (!pgy_channel_recv_timeout_Int(ch, &out, timeout_ns)) {
        result.tag = PGY_RUNTIME_CHANNEL_RESULT_ERR;
        result.err = pgy_runtime_channel_failure_from_status(
            pgy_channel_closed_Int(ch)
                ? PGY_RUNTIME_CHANNEL_STATUS_CLOSED_EMPTY
                : PGY_RUNTIME_CHANNEL_STATUS_TIMEOUT,
            "recv_timeout_Int");
        return result;
    }
    result.tag = PGY_RUNTIME_CHANNEL_RESULT_OK;
    result.ok = out;
    return result;
}

int32_t pgy_channel_recv_val_Int(PgyChannel_Int_RT *ch)
{
    PgyRuntimeChannelIntResult result = pgy_channel_recv_result_Int(ch);
    return result.tag == PGY_RUNTIME_CHANNEL_RESULT_OK ? result.ok : 0;
}
