#include <pthread.h>

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

void pgy_channel_init_String(PgyChannel_String_RT *ch, size_t cap)
{
    if (ch == NULL) {
        pgy_runtime_warn_invalid_channel("init_String", "null channel");
        return;
    }
    cap = pgy_runtime_channel_capacity_or_default("init_String", cap);
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
    pthread_mutex_init(&ch->mutex, NULL);
    pthread_cond_init(&ch->cond_not_full, NULL);
    pthread_cond_init(&ch->cond_not_empty, NULL);
}

void pgy_channel_destroy_String(PgyChannel_String_RT *ch)
{
    if (ch == NULL) return;
    pthread_mutex_destroy(&ch->mutex);
    pthread_cond_destroy(&ch->cond_not_full);
    pthread_cond_destroy(&ch->cond_not_empty);
    free(ch->buffer);
    ch->buffer = NULL;
}

void pgy_channel_close_String(PgyChannel_String_RT *ch)
{
    if (ch == NULL) return;
    pthread_mutex_lock(&ch->mutex);
    ch->closed = true;
    pthread_cond_broadcast(&ch->cond_not_full);
    pthread_cond_broadcast(&ch->cond_not_empty);
    pthread_mutex_unlock(&ch->mutex);
}

bool pgy_channel_send_String(PgyChannel_String_RT *ch, char *v)
{
    if (ch == NULL) {
        pgy_runtime_warn_invalid_channel("send_String", "null channel");
        return false;
    }
    if (ch->buffer == NULL || ch->capacity == 0) {
        pgy_runtime_warn_invalid_channel("send_String", "channel is not initialized");
        return false;
    }
    pthread_mutex_lock(&ch->mutex);
    while (ch->count >= ch->capacity && !ch->closed)
        pthread_cond_wait(&ch->cond_not_full, &ch->mutex);
    if (ch->closed) {
        pgy_runtime_warn_invalid_channel("send_String", "channel is closed");
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

bool pgy_channel_try_send_String(PgyChannel_String_RT *ch, char *v)
{
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
    ch->buffer[ch->tail] = v;
    ch->tail = (ch->tail + 1) % ch->capacity;
    ch->count++;
    pthread_cond_signal(&ch->cond_not_empty);
    pthread_mutex_unlock(&ch->mutex);
    return true;
}

bool pgy_channel_send_timeout_String(PgyChannel_String_RT *ch, char *v,
                                     uint64_t timeout_ns)
{
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
        if (pthread_cond_timedwait(&ch->cond_not_full, &ch->mutex, &deadline)
            == ETIMEDOUT && ch->count >= ch->capacity && !ch->closed) {
            pgy_runtime_warn_invalid_channel("send_timeout_String", "deadline reached while channel remained full");
            pthread_mutex_unlock(&ch->mutex);
            return false;
        }
    }
    if (ch->closed) {
        pgy_runtime_warn_invalid_channel("send_timeout_String", "channel is closed");
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

bool pgy_channel_recv_String(PgyChannel_String_RT *ch, char **out)
{
    if (ch == NULL || out == NULL) {
        pgy_runtime_warn_invalid_channel("recv_String",
            ch == NULL ? "null channel" : "null output pointer");
        return false;
    }
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
                pthread_cond_wait(&ch->cond_not_empty, &ch->mutex);
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
                if (pthread_cond_timedwait(&ch->cond_not_empty, &ch->mutex, &deadline)
                    == ETIMEDOUT && ch->count == 0 && !ch->closed) {
                    pgy_runtime_warn_invalid_channel("recv_timeout_String", "deadline reached while channel remained empty");
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
    ch->head = (ch->head + 1) % ch->capacity;
    ch->count--;
    pthread_cond_signal(&ch->cond_not_full);
    pthread_mutex_unlock(&ch->mutex);
    return true;
}

bool pgy_channel_ready_String(PgyChannel_String_RT *ch)
{
    if (ch == NULL) {
        pgy_runtime_warn_invalid_channel("ready_String", "null channel");
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
    if (ch == NULL) {
        pgy_runtime_warn_invalid_channel("length_String", "null channel");
        return 0;
    }
    pthread_mutex_lock(&ch->mutex);
    int32_t len = (int32_t)ch->count;
    pthread_mutex_unlock(&ch->mutex);
    return len;
}

int32_t pgy_channel_capacity_String(PgyChannel_String_RT *ch)
{
    if (ch == NULL) {
        pgy_runtime_warn_invalid_channel("capacity_String", "null channel");
        return 0;
    }
    pthread_mutex_lock(&ch->mutex);
    int32_t cap = (int32_t)ch->capacity;
    pthread_mutex_unlock(&ch->mutex);
    return cap;
}

bool pgy_channel_full_String(PgyChannel_String_RT *ch)
{
    if (ch == NULL) {
        pgy_runtime_warn_invalid_channel("full_String", "null channel");
        return false;
    }
    pthread_mutex_lock(&ch->mutex);
    bool full = ch->count >= ch->capacity;
    pthread_mutex_unlock(&ch->mutex);
    return full;
}

int32_t pgy_channel_space_String(PgyChannel_String_RT *ch)
{
    if (ch == NULL) {
        pgy_runtime_warn_invalid_channel("space_String", "null channel");
        return 0;
    }
    pthread_mutex_lock(&ch->mutex);
    int32_t space = (int32_t)(ch->capacity - ch->count);
    pthread_mutex_unlock(&ch->mutex);
    return space;
}

bool pgy_channel_closed_String(PgyChannel_String_RT *ch)
{
    if (ch == NULL) {
        pgy_runtime_warn_invalid_channel("closed_String", "null channel");
        return true;
    }
    pthread_mutex_lock(&ch->mutex);
    bool closed = ch->closed;
    pthread_mutex_unlock(&ch->mutex);
    return closed;
}

char *pgy_channel_recv_val_String(PgyChannel_String_RT *ch)
{
    char *out = NULL;
    pgy_channel_recv_String(ch, &out);
    return out;
}
