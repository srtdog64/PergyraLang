#include <pthread.h>
#include "pgy_runtime_channel_status.h"

/* Destroy is quiescent-only: close/drain/join producers and consumers before
 * freeing the backing buffer and destroying mutex/condvar state. */

typedef struct {
    char **buffer;
    size_t capacity;
    size_t head;
    size_t tail;
    size_t count;
    bool closed;
    pthread_mutex_t mutex;
    pthread_cond_t cond_not_full;
    pthread_cond_t cond_not_empty;
} PgyChannel_String_RT;

static bool
pgy_channel_string_is_initialized(PgyChannel_String_RT *ch)
{
    return ch != NULL && ch->buffer != NULL && ch->capacity > 0;
}

static int32_t
pgy_channel_string_size_to_i32(size_t value)
{
    return value > (size_t)INT32_MAX ? INT32_MAX : (int32_t)value;
}

static char *
pgy_channel_dup_String(const char *value)
{
    return pgy_runtime_lib_strdup(value != NULL ? value : "");
}

static void
pgy_channel_free_pending_String(PgyChannel_String_RT *ch)
{
    if (ch == NULL || ch->buffer == NULL || ch->capacity == 0)
        return;
    for (size_t i = 0; i < ch->capacity; i++) {
        free(ch->buffer[i]);
        ch->buffer[i] = NULL;
    }
    ch->head = 0;
    ch->tail = 0;
    ch->count = 0;
}

void pgy_channel_init_String(PgyChannel_String_RT *ch, size_t cap)
{
    if (ch == NULL) {
        pgy_runtime_warn_invalid_channel("init_String", "null channel");
        return;
    }
    cap = pgy_runtime_channel_capacity_or_default("init_String", cap);
    if (cap > SIZE_MAX / sizeof(char *)) {
        pgy_runtime_warn_invalid_channel("init_String", "capacity overflows buffer size");
        ch->buffer = NULL;
        ch->capacity = 0;
        return;
    }
    ch->buffer = (char **)calloc(cap, sizeof(char *));
    if (ch->buffer == NULL) {
        pgy_runtime_warn_invalid_channel("init_String", "buffer allocation failed");
        ch->capacity = 0;
        return;
    }
    ch->capacity = cap;
    ch->head = 0;
    ch->tail = 0;
    ch->count = 0;
    ch->closed = false;
    if (pthread_mutex_init(&ch->mutex, NULL) != 0) {
        pgy_runtime_warn_invalid_channel("init_String", "mutex initialization failed");
        free(ch->buffer);
        ch->buffer = NULL;
        ch->capacity = 0;
        return;
    }
    if (pthread_cond_init(&ch->cond_not_full, NULL) != 0) {
        pgy_runtime_warn_invalid_channel("init_String", "not-full condition initialization failed");
        pthread_mutex_destroy(&ch->mutex);
        free(ch->buffer);
        ch->buffer = NULL;
        ch->capacity = 0;
        return;
    }
    if (pthread_cond_init(&ch->cond_not_empty, NULL) != 0) {
        pgy_runtime_warn_invalid_channel("init_String", "not-empty condition initialization failed");
        pthread_cond_destroy(&ch->cond_not_full);
        pthread_mutex_destroy(&ch->mutex);
        free(ch->buffer);
        ch->buffer = NULL;
        ch->capacity = 0;
        return;
    }
}

void pgy_channel_destroy_String(PgyChannel_String_RT *ch)
{
    if (!pgy_channel_string_is_initialized(ch)) return;
    pthread_mutex_lock(&ch->mutex);
    pgy_channel_free_pending_String(ch);
    pthread_mutex_unlock(&ch->mutex);
    pthread_mutex_destroy(&ch->mutex);
    pthread_cond_destroy(&ch->cond_not_full);
    pthread_cond_destroy(&ch->cond_not_empty);
    free(ch->buffer);
    ch->buffer = NULL;
}

void pgy_channel_close_String(PgyChannel_String_RT *ch)
{
    if (!pgy_channel_string_is_initialized(ch)) return;
    pthread_mutex_lock(&ch->mutex);
    ch->closed = true;
    pthread_cond_broadcast(&ch->cond_not_full);
    pthread_cond_broadcast(&ch->cond_not_empty);
    pthread_mutex_unlock(&ch->mutex);
}

bool pgy_channel_send_String(PgyChannel_String_RT *ch, char *v)
{
    char *owned = NULL;
    if (ch == NULL) {
        pgy_runtime_warn_invalid_channel("send_String", "null channel");
        return false;
    }
    if (ch->buffer == NULL || ch->capacity == 0) {
        pgy_runtime_warn_invalid_channel("send_String", "channel is not initialized");
        return false;
    }
    pthread_mutex_lock(&ch->mutex);
    while (ch->count >= ch->capacity && !ch->closed) {
        if (pthread_cond_wait(&ch->cond_not_full, &ch->mutex) != 0) {
            pgy_runtime_warn_invalid_channel("send_String",
                "not-full condition wait failed");
            pthread_mutex_unlock(&ch->mutex);
            return false;
        }
    }
    if (ch->closed) {
        pgy_runtime_warn_invalid_channel("send_String", "channel is closed");
        pthread_mutex_unlock(&ch->mutex);
        return false;
    }
    owned = pgy_channel_dup_String(v);
    if (owned == NULL) {
        pgy_runtime_warn_invalid_channel("send_String", "payload duplication failed");
        pthread_mutex_unlock(&ch->mutex);
        return false;
    }
    ch->buffer[ch->tail] = owned;
    ch->tail = (ch->tail + 1) % ch->capacity;
    ch->count++;
    pthread_cond_signal(&ch->cond_not_empty);
    pthread_mutex_unlock(&ch->mutex);
    return true;
}

bool pgy_channel_try_send_String(PgyChannel_String_RT *ch, char *v)
{
    char *owned = NULL;
    if (ch == NULL) {
        pgy_runtime_warn_invalid_channel("try_send_String", "null channel");
        return false;
    }
    if (ch->buffer == NULL || ch->capacity == 0) {
        pgy_runtime_warn_invalid_channel("try_send_String", "channel is not initialized");
        return false;
    }
    pthread_mutex_lock(&ch->mutex);
    if (ch->closed || ch->count >= ch->capacity) {
        pgy_runtime_warn_invalid_channel("try_send_String",
            ch->closed ? "channel is closed" : "channel is full");
        pthread_mutex_unlock(&ch->mutex);
        return false;
    }
    owned = pgy_channel_dup_String(v);
    if (owned == NULL) {
        pgy_runtime_warn_invalid_channel("try_send_String", "payload duplication failed");
        pthread_mutex_unlock(&ch->mutex);
        return false;
    }
    ch->buffer[ch->tail] = owned;
    ch->tail = (ch->tail + 1) % ch->capacity;
    ch->count++;
    pthread_cond_signal(&ch->cond_not_empty);
    pthread_mutex_unlock(&ch->mutex);
    return true;
}

PgyOption_Bool pgy_channel_try_send_status_String(PgyChannel_String_RT *ch,
                                                  char *v)
{
    char *owned = NULL;

    if (!pgy_channel_string_is_initialized(ch))
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
    owned = pgy_channel_dup_String(v);
    if (owned == NULL) {
        pthread_mutex_unlock(&ch->mutex);
        return Some_Bool(false);
    }
    ch->buffer[ch->tail] = owned;
    ch->tail = (ch->tail + 1) % ch->capacity;
    ch->count++;
    pthread_cond_signal(&ch->cond_not_empty);
    pthread_mutex_unlock(&ch->mutex);
    return Some_Bool(true);
}

int32_t pgy_channel_try_send_status_code_String(PgyChannel_String_RT *ch,
                                                char *v)
{
    PgyOption_Bool status = pgy_channel_try_send_status_String(ch, v);
    if (status.tag != 0)
        return -1;
    return status.value ? 1 : 0;
}

bool pgy_channel_send_timeout_String(PgyChannel_String_RT *ch, char *v,
                                     uint64_t timeout_ns)
{
    char *owned = NULL;
    if (ch == NULL) {
        pgy_runtime_warn_invalid_channel("send_timeout_String", "null channel");
        return false;
    }
    if (ch->buffer == NULL || ch->capacity == 0) {
        pgy_runtime_warn_invalid_channel("send_timeout_String", "channel is not initialized");
        return false;
    }
    struct timespec deadline = pgy_runtime_deadline_after_ns(timeout_ns);
    pthread_mutex_lock(&ch->mutex);
    while (ch->count >= ch->capacity && !ch->closed) {
        int wait_status =
            pthread_cond_timedwait(&ch->cond_not_full, &ch->mutex, &deadline);
        if (wait_status == ETIMEDOUT && ch->count >= ch->capacity && !ch->closed) {
            pgy_runtime_warn_invalid_channel("send_timeout_String", "deadline reached while channel remained full");
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
    owned = pgy_channel_dup_String(v);
    if (owned == NULL) {
        pgy_runtime_warn_invalid_channel("send_timeout_String", "payload duplication failed");
        pthread_mutex_unlock(&ch->mutex);
        return false;
    }
    ch->buffer[ch->tail] = owned;
    ch->tail = (ch->tail + 1) % ch->capacity;
    ch->count++;
    pthread_cond_signal(&ch->cond_not_empty);
    pthread_mutex_unlock(&ch->mutex);
    return true;
}

PgyOption_Bool pgy_channel_send_timeout_status_String(
    PgyChannel_String_RT *ch,
    char *v,
    uint64_t timeout_ns)
{
    char *owned = NULL;

    if (!pgy_channel_string_is_initialized(ch))
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
    owned = pgy_channel_dup_String(v);
    if (owned == NULL) {
        pthread_mutex_unlock(&ch->mutex);
        return Some_Bool(false);
    }
    ch->buffer[ch->tail] = owned;
    ch->tail = (ch->tail + 1) % ch->capacity;
    ch->count++;
    pthread_cond_signal(&ch->cond_not_empty);
    pthread_mutex_unlock(&ch->mutex);
    return Some_Bool(true);
}

int32_t pgy_channel_send_timeout_status_code_String(
    PgyChannel_String_RT *ch,
    char *v,
    uint64_t timeout_ns)
{
    PgyOption_Bool status =
        pgy_channel_send_timeout_status_String(ch, v, timeout_ns);
    if (status.tag != 0)
        return -1;
    return status.value ? 1 : 0;
}

bool pgy_channel_recv_String(PgyChannel_String_RT *ch, char **out)
{
    if (ch == NULL || out == NULL) {
        pgy_runtime_warn_invalid_channel("recv_String",
            ch == NULL ? "null channel" : "null output pointer");
        return false;
    }
    *out = NULL;
    if (ch->buffer == NULL || ch->capacity == 0) {
        pgy_runtime_warn_invalid_channel("recv_String", "channel is not initialized");
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
    *out = ch->buffer[ch->head];
    ch->buffer[ch->head] = NULL;
    ch->head = (ch->head + 1) % ch->capacity;
    ch->count--;
    pthread_cond_signal(&ch->cond_not_full);
    pthread_mutex_unlock(&ch->mutex);
    return true;
}

bool pgy_channel_recv_timeout_String(PgyChannel_String_RT *ch, char **out,
                                     uint64_t timeout_ns)
{
    if (ch == NULL || out == NULL) {
        pgy_runtime_warn_invalid_channel("recv_timeout_String",
            ch == NULL ? "null channel" : "null output pointer");
        return false;
    }
    *out = NULL;
    if (ch->buffer == NULL || ch->capacity == 0) {
        pgy_runtime_warn_invalid_channel("recv_timeout_String", "channel is not initialized");
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
                    pgy_runtime_warn_invalid_channel("recv_timeout_String", "deadline reached while channel remained empty");
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
    *out = ch->buffer[ch->head];
    ch->buffer[ch->head] = NULL;
    ch->head = (ch->head + 1) % ch->capacity;
    ch->count--;
    pthread_cond_signal(&ch->cond_not_full);
    pthread_mutex_unlock(&ch->mutex);
    return true;
}

bool pgy_channel_try_recv_String(PgyChannel_String_RT *ch, char **out)
{
    if (ch == NULL || out == NULL) {
        pgy_runtime_warn_invalid_channel("try_recv_String",
            ch == NULL ? "null channel" : "null output pointer");
        return false;
    }
    *out = NULL;
    if (ch->buffer == NULL || ch->capacity == 0) {
        pgy_runtime_warn_invalid_channel("try_recv_String", "channel is not initialized");
        return false;
    }
    if (!pgy_async_in_coroutine())
        (void)pgy_async_progress_one();
    pthread_mutex_lock(&ch->mutex);
    if (ch->count == 0) {
        pgy_runtime_warn_invalid_channel("try_recv_String", "channel is empty");
        pthread_mutex_unlock(&ch->mutex);
        return false;
    }
    *out = ch->buffer[ch->head];
    ch->buffer[ch->head] = NULL;
    ch->head = (ch->head + 1) % ch->capacity;
    ch->count--;
    pthread_cond_signal(&ch->cond_not_full);
    pthread_mutex_unlock(&ch->mutex);
    return true;
}

bool pgy_channel_ready_String(PgyChannel_String_RT *ch)
{
    if (!pgy_channel_string_is_initialized(ch)) {
        pgy_runtime_warn_invalid_channel("ready_String",
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

int32_t pgy_channel_length_String(PgyChannel_String_RT *ch)
{
    if (!pgy_channel_string_is_initialized(ch)) {
        pgy_runtime_warn_invalid_channel("length_String",
            ch == NULL ? "null channel" : "channel is not initialized");
        return 0;
    }
    pthread_mutex_lock(&ch->mutex);
    int32_t len = pgy_channel_string_size_to_i32(ch->count);
    pthread_mutex_unlock(&ch->mutex);
    return len;
}

int32_t pgy_channel_capacity_String(PgyChannel_String_RT *ch)
{
    if (!pgy_channel_string_is_initialized(ch)) {
        pgy_runtime_warn_invalid_channel("capacity_String",
            ch == NULL ? "null channel" : "channel is not initialized");
        return 0;
    }
    pthread_mutex_lock(&ch->mutex);
    int32_t cap = pgy_channel_string_size_to_i32(ch->capacity);
    pthread_mutex_unlock(&ch->mutex);
    return cap;
}

bool pgy_channel_full_String(PgyChannel_String_RT *ch)
{
    if (!pgy_channel_string_is_initialized(ch)) {
        pgy_runtime_warn_invalid_channel("full_String",
            ch == NULL ? "null channel" : "channel is not initialized");
        return false;
    }
    pthread_mutex_lock(&ch->mutex);
    bool full = ch->count >= ch->capacity;
    pthread_mutex_unlock(&ch->mutex);
    return full;
}

int32_t pgy_channel_space_String(PgyChannel_String_RT *ch)
{
    if (!pgy_channel_string_is_initialized(ch)) {
        pgy_runtime_warn_invalid_channel("space_String",
            ch == NULL ? "null channel" : "channel is not initialized");
        return 0;
    }
    pthread_mutex_lock(&ch->mutex);
    size_t used = ch->count <= ch->capacity ? ch->count : ch->capacity;
    int32_t space = pgy_channel_string_size_to_i32(ch->capacity - used);
    pthread_mutex_unlock(&ch->mutex);
    return space;
}

bool pgy_channel_closed_String(PgyChannel_String_RT *ch)
{
    if (!pgy_channel_string_is_initialized(ch)) {
        pgy_runtime_warn_invalid_channel("closed_String",
            ch == NULL ? "null channel" : "channel is not initialized");
        return true;
    }
    pthread_mutex_lock(&ch->mutex);
    bool closed = ch->closed;
    pthread_mutex_unlock(&ch->mutex);
    return closed;
}

#include "pgy_runtime_lib_channel_string_result_exports.h"
