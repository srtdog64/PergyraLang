#ifndef PGY_RUNTIME_QUEUE_INLINE_H
#define PGY_RUNTIME_QUEUE_INLINE_H

#define PGY_QUEUE_DEFINE(SuffixName, CType) \
typedef struct \
{ \
    CType   *data; \
    size_t   head; \
    size_t   tail; \
    size_t   count; \
    size_t   capacity; \
} PgyQueue_##SuffixName; \
\
static inline PgyQueue_##SuffixName pgy_queue_new_##SuffixName(void) \
{ \
    PgyQueue_##SuffixName q; \
    q.capacity = 16; \
    q.count = q.head = q.tail = 0; \
    q.data = (CType *)calloc(q.capacity, sizeof(CType)); \
    if (q.data == NULL) { \
        q.capacity = 0; \
        pgy_runtime_warn_invalid_collection("queue_new_" #SuffixName, "allocation failed"); \
    } \
    return q; \
} \
\
static inline void pgy_queue_push_##SuffixName(PgyQueue_##SuffixName *q, CType val) \
{ \
    if (q == NULL || (q->data == NULL && q->capacity == 0)) { \
        pgy_runtime_warn_invalid_collection("queue_push_" #SuffixName, "queue is not initialized"); \
        return; \
    } \
    if (q->count >= q->capacity) { \
        size_t nc = q->capacity == 0 ? 16 : q->capacity * 2; \
        CType *nd = NULL; \
        if (q->head == 0) { \
            nd = (CType *)realloc(q->data, nc * sizeof(CType)); \
            if (nd == NULL) { \
                pgy_runtime_warn_invalid_collection("queue_push_" #SuffixName, "growth allocation failed"); \
                return; \
            } \
        } else { \
            nd = (CType *)calloc(nc, sizeof(CType)); \
            if (nd == NULL) { \
                pgy_runtime_warn_invalid_collection("queue_push_" #SuffixName, "growth allocation failed"); \
                return; \
            } \
            for (size_t i = 0; i < q->count; i++) \
                nd[i] = q->data[(q->head + i) % q->capacity]; \
            free(q->data); \
        } \
        q->data = nd; \
        q->head = 0; q->tail = q->count; q->capacity = nc; \
    } \
    q->data[q->tail] = val; \
    q->tail = (q->tail + 1) % q->capacity; \
    q->count++; \
} \
\
static inline CType pgy_queue_pop_##SuffixName(PgyQueue_##SuffixName *q) \
{ \
    if (q == NULL || q->data == NULL || q->capacity == 0) \
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_INTERNAL_INVARIANT, "queue pop on invalid queue"); \
    if (q->count == 0) \
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_OUT_OF_BOUNDS, "queue pop from empty queue"); \
    CType val = q->data[q->head]; \
    q->head = (q->head + 1) % q->capacity; \
    q->count--; \
    return val; \
} \
\
static inline int32_t pgy_queue_size_##SuffixName(PgyQueue_##SuffixName *q) { return (int32_t)q->count; } \
static inline bool pgy_queue_empty_##SuffixName(PgyQueue_##SuffixName *q) { return q->count == 0; }

typedef struct
{
    int32_t *data;
    size_t   head;
    size_t   tail;
    size_t   count;
    size_t   capacity;
} PgyQueue_Int;

static inline PgyQueue_Int pgy_queue_new_int(void)
{
    PgyQueue_Int q;
    q.capacity = 16;
    q.count = q.head = q.tail = 0;
    q.data = (int32_t *)calloc(q.capacity, sizeof(int32_t));
    if (q.data == NULL) {
        q.capacity = 0;
        pgy_runtime_warn_invalid_collection("queue_new_int", "allocation failed");
    }
    return q;
}

static inline void pgy_queue_push_int(PgyQueue_Int *q, int32_t val)
{
    if (q == NULL || (q->data == NULL && q->capacity == 0)) {
        pgy_runtime_warn_invalid_collection("queue_push_int", "queue is not initialized");
        return;
    }
    if (q->count >= q->capacity) {
        size_t nc = q->capacity == 0 ? 16 : q->capacity * 2;
        int32_t *nd = NULL;
        if (q->head == 0) {
            nd = (int32_t *)realloc(q->data, nc * sizeof(int32_t));
            if (nd == NULL) {
                pgy_runtime_warn_invalid_collection("queue_push_int", "growth allocation failed");
                return;
            }
        } else {
            nd = (int32_t *)calloc(nc, sizeof(int32_t));
            if (nd == NULL) {
                pgy_runtime_warn_invalid_collection("queue_push_int", "growth allocation failed");
                return;
            }
            for (size_t i = 0; i < q->count; i++)
                nd[i] = q->data[(q->head + i) % q->capacity];
            free(q->data);
        }
        q->data = nd;
        q->head = 0;
        q->tail = q->count;
        q->capacity = nc;
    }
    q->data[q->tail] = val;
    q->tail = (q->tail + 1) % q->capacity;
    q->count++;
}

static inline int32_t pgy_queue_pop_int(PgyQueue_Int *q)
{
    int32_t val;
    if (q == NULL || q->data == NULL || q->capacity == 0)
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_INTERNAL_INVARIANT,
                          "queue pop on invalid queue");
    if (q->count == 0)
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_OUT_OF_BOUNDS,
                          "queue pop from empty queue");
    val = q->data[q->head];
    q->head = (q->head + 1) % q->capacity;
    q->count--;
    return val;
}

static inline int32_t pgy_queue_size_int(PgyQueue_Int *q) { return (int32_t)q->count; }
static inline bool pgy_queue_empty_int(PgyQueue_Int *q) { return q->count == 0; }

typedef struct
{
    char   **data;
    size_t   head;
    size_t   tail;
    size_t   count;
    size_t   capacity;
} PgyQueue_String;

static inline PgyQueue_String pgy_queue_new_string(void)
{
    PgyQueue_String q;
    q.capacity = 16;
    q.count = q.head = q.tail = 0;
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
        char **nd = NULL;
        if (q->head == 0) {
            nd = (char **)realloc(q->data, nc * sizeof(char *));
            if (nd == NULL) {
                pgy_runtime_warn_invalid_collection("queue_push_string", "growth allocation failed");
                return;
            }
        } else {
            nd = (char **)calloc(nc, sizeof(char *));
            if (nd == NULL) {
                pgy_runtime_warn_invalid_collection("queue_push_string", "growth allocation failed");
                return;
            }
            for (size_t i = 0; i < q->count; i++)
                nd[i] = q->data[(q->head + i) % q->capacity];
            free(q->data);
        }
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

#endif /* PGY_RUNTIME_QUEUE_INLINE_H */
