/* StringJoin(arr, sep) -> String */
char *StringJoin(PgyArray_String *arr, const char *sep)
{
    if (arr == NULL || arr->length == 0)
        return pgy_runtime_lib_strdup("");
    size_t slen = (sep != NULL) ? strlen(sep) : 0;
    size_t total = 0;
    for (size_t i = 0; i < arr->length; i++) {
        if (arr->data[i] != NULL) total += strlen(arr->data[i]);
        if (i > 0) total += slen;
    }
    char *buf = (char *)malloc(total + 1);
    if (buf == NULL) return pgy_runtime_lib_strdup("");
    char *wp = buf;
    for (size_t i = 0; i < arr->length; i++) {
        if (i > 0 && slen > 0) { memcpy(wp, sep, slen); wp += slen; }
        if (arr->data[i] != NULL) {
            size_t l = strlen(arr->data[i]);
            memcpy(wp, arr->data[i], l);
            wp += l;
        }
    }
    *wp = '\0';
    return buf;
}

int32_t ToInt(const char *s)
{
    if (s == NULL) return 0;
    return (int32_t)strtol(s, NULL, 10);
}

float ToFloat(const char *s)
{
    if (s == NULL) return 0.0f;
    return strtof(s, NULL);
}

#include <math.h>

float Sqrt(float x)  { return sqrtf(x); }
float Pow(float x, float y) { return powf(x, y); }
float Floor(float x) { return floorf(x); }
float Ceil(float x)  { return ceilf(x); }

int32_t Random(int32_t max)
{
    if (max <= 0) return 0;
    return (int32_t)(rand() % max);
}

void SeedRandom(int32_t seed)
{
    srand((unsigned int)seed);
}

/* =================================================================
 * Channel — Int (thread-safe with mutex + condvar)
 * ================================================================= */

#include <pthread.h>

/* PgyOption_Bool — needed for try_send_status / send_timeout_status.
 * Must match PGY_OPTION_DEFINE(Bool, bool) in pgy_runtime.h. */
typedef struct { int tag; bool value; } PgyOption_Bool;
static inline PgyOption_Bool Some_Bool(bool v) { return (PgyOption_Bool){ 1, v }; }
static inline PgyOption_Bool None_Bool(void)   { return (PgyOption_Bool){ 0, false }; }

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

void pgy_channel_init_Int(PgyChannel_Int_RT *ch, size_t cap)
{
    if (ch == NULL) {
        pgy_runtime_warn_invalid_channel("init_Int", "null channel");
        return;
    }
    cap = pgy_runtime_channel_capacity_or_default("init_Int", cap);
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
    pthread_mutex_init(&ch->mutex, NULL);
    pthread_cond_init(&ch->cond_not_full, NULL);
    pthread_cond_init(&ch->cond_not_empty, NULL);
}

void pgy_channel_destroy_Int(PgyChannel_Int_RT *ch)
{
    if (ch == NULL) return;
    pthread_mutex_destroy(&ch->mutex);
    pthread_cond_destroy(&ch->cond_not_full);
    pthread_cond_destroy(&ch->cond_not_empty);
    free(ch->buffer);
    ch->buffer = NULL;
}

void pgy_channel_close_Int(PgyChannel_Int_RT *ch)
{
    if (ch == NULL) return;
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
    while (ch->count >= ch->capacity && !ch->closed)
        pthread_cond_wait(&ch->cond_not_full, &ch->mutex);
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
        if (pthread_cond_timedwait(&ch->cond_not_full, &ch->mutex, &deadline)
            == ETIMEDOUT && ch->count >= ch->capacity && !ch->closed) {
            pgy_runtime_warn_invalid_channel("send_timeout_Int", "deadline reached while channel remained full");
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
                pthread_cond_wait(&ch->cond_not_empty, &ch->mutex);
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
    if (ch == NULL) {
        pgy_runtime_warn_invalid_channel("ready_Int", "null channel");
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
    if (ch == NULL) {
        pgy_runtime_warn_invalid_channel("length_Int", "null channel");
        return 0;
    }
    pthread_mutex_lock(&ch->mutex);
    int32_t len = (int32_t)ch->count;
    pthread_mutex_unlock(&ch->mutex);
    return len;
}

int32_t pgy_channel_capacity_Int(PgyChannel_Int_RT *ch)
{
    if (ch == NULL) {
        pgy_runtime_warn_invalid_channel("capacity_Int", "null channel");
        return 0;
    }
    pthread_mutex_lock(&ch->mutex);
    int32_t cap = (int32_t)ch->capacity;
    pthread_mutex_unlock(&ch->mutex);
    return cap;
}

bool pgy_channel_full_Int(PgyChannel_Int_RT *ch)
{
    if (ch == NULL) {
        pgy_runtime_warn_invalid_channel("full_Int", "null channel");
        return false;
    }
    pthread_mutex_lock(&ch->mutex);
    bool full = ch->count >= ch->capacity;
    pthread_mutex_unlock(&ch->mutex);
    return full;
}

int32_t pgy_channel_space_Int(PgyChannel_Int_RT *ch)
{
    if (ch == NULL) {
        pgy_runtime_warn_invalid_channel("space_Int", "null channel");
        return 0;
    }
    pthread_mutex_lock(&ch->mutex);
    int32_t space = (int32_t)(ch->capacity - ch->count);
    pthread_mutex_unlock(&ch->mutex);
    return space;
}

bool pgy_channel_closed_Int(PgyChannel_Int_RT *ch)
{
    if (ch == NULL) {
        pgy_runtime_warn_invalid_channel("closed_Int", "null channel");
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
                if (pthread_cond_timedwait(&ch->cond_not_empty, &ch->mutex, &deadline)
                    == ETIMEDOUT && ch->count == 0 && !ch->closed) {
                    pgy_runtime_warn_invalid_channel("recv_timeout_Int", "deadline reached while channel remained empty");
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

int32_t pgy_channel_recv_val_Int(PgyChannel_Int_RT *ch)
{
    int32_t out = 0;
    pgy_channel_recv_Int(ch, &out);
    return out;
}

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

/* =================================================================
 * QubitSlot runtime — N-qubit entanglement pool model
 *
 * Matches pgy_runtime.h's pool_id / PgyEntanglementPool design.
 * Supports GHZ states via pool merge on Entangle().
 * ================================================================= */

#define PGY_QUBIT_RT_MAX 64

typedef struct {
    int32_t state;       /* 0=|0>, 1=|1>, 2=superposition, -1=released */
    int32_t pool_id;     /* entanglement pool id, -1 if none */
    bool    measured;
} PgyQubit_RT;

typedef struct {
    int32_t members[PGY_QUBIT_RT_MAX];
    int32_t count;
    bool    active;
} PgyEntanglementPool_RT;

static PgyQubit_RT              pgy_qubits_rt[PGY_QUBIT_RT_MAX];
static int32_t                  pgy_qubit_next_rt = 0;
static bool                     pgy_qubit_rng_init_rt = false;

static PgyEntanglementPool_RT   pgy_qubit_pools_rt[PGY_QUBIT_RT_MAX];
static int32_t                  pgy_qubit_pool_next_rt = 0;

/* --- Pool helpers --- */

static int32_t
rt_alloc_pool(void)
{
    if (pgy_qubit_pool_next_rt >= PGY_QUBIT_RT_MAX) return -1;
    int32_t id = pgy_qubit_pool_next_rt++;
    pgy_qubit_pools_rt[id].count  = 0;
    pgy_qubit_pools_rt[id].active = true;
    return id;
}

static void
rt_pool_add(int32_t pool_id, int32_t qubit_id)
{
    if (pool_id < 0 || pool_id >= PGY_QUBIT_RT_MAX) return;
    PgyEntanglementPool_RT *pool = &pgy_qubit_pools_rt[pool_id];
    if (pool->count >= PGY_QUBIT_RT_MAX) return;
    for (int32_t i = 0; i < pool->count; i++)
        if (pool->members[i] == qubit_id) return;
    pool->members[pool->count++] = qubit_id;
    pgy_qubits_rt[qubit_id].pool_id = pool_id;
}

static void
rt_pool_remove(int32_t pool_id, int32_t qubit_id)
{
    if (pool_id < 0 || pool_id >= PGY_QUBIT_RT_MAX) return;
    PgyEntanglementPool_RT *pool = &pgy_qubit_pools_rt[pool_id];
    for (int32_t i = 0; i < pool->count; i++) {
        if (pool->members[i] == qubit_id) {
            pool->members[i] = pool->members[pool->count - 1];
            pool->count--;
            return;
        }
    }
}

static void
rt_pool_merge(int32_t dst_pool, int32_t src_pool)
{
    if (dst_pool == src_pool) return;
    if (dst_pool < 0 || src_pool < 0) return;
    PgyEntanglementPool_RT *src = &pgy_qubit_pools_rt[src_pool];
    for (int32_t i = 0; i < src->count; i++) {
        int32_t qid = src->members[i];
        rt_pool_add(dst_pool, qid);
    }
    src->count  = 0;
    src->active = false;
}
