/* =================================================================
 * Log functions
 * ================================================================= */

void pgy_log_int(int32_t v)    { printf("%d\n", v); }
void pgy_log_long(int64_t v)   { printf("%lld\n", (long long)v); }
void pgy_log_float(float v)    { printf("%f\n", v); }
void pgy_log_double(double v)  { printf("%lf\n", v); }
void pgy_log_bool(bool v)      { printf("%s\n", v ? "true" : "false"); }
void
pgy_log_string(const char *v)
{
    size_t len;

    if (v == NULL)
        v = "(null)";

    fputs(v, stdout);
    len = strlen(v);
    if (len == 0 || v[len - 1] != '\n')
        fputc('\n', stdout);
    fflush(stdout);
}

void
pgy_log_banner(const char *v)
{
    pgy_log_string(v);
}

int32_t
pgy_now_ms(void)
{
#ifdef _WIN32
    return (int32_t)GetTickCount64();
#else
    struct timespec ts;
    if (clock_gettime(CLOCK_REALTIME, &ts) != 0)
        return 0;
    return (int32_t)((ts.tv_sec * 1000LL) + (ts.tv_nsec / 1000000LL));
#endif
}

void
pgy_sleep_ms(int32_t ms)
{
    if (ms <= 0)
        return;
#ifdef _WIN32
    Sleep((DWORD)ms);
#else
    struct timespec req;
    req.tv_sec = ms / 1000;
    req.tv_nsec = (long)((ms % 1000) * 1000000L);
    while (nanosleep(&req, &req) != 0 && errno == EINTR) {
    }
#endif
}

static char *pgy_runtime_strdup_export(const char *src);

char *pgy_int_to_string(int32_t v)
{
    char stack_buf[32];
    int len = snprintf(stack_buf, sizeof(stack_buf), "%d", v);
    if (len < 0) {
        char *fallback = (char *)malloc(2);
        if (fallback != NULL) {
            fallback[0] = '0';
            fallback[1] = '\0';
        }
        return fallback;
    }
    char *buf = (char *)malloc((size_t)len + 1);
    if (buf == NULL) return NULL;
    memcpy(buf, stack_buf, (size_t)len + 1);
    return buf;
}

typedef struct {
    void   *data;
    size_t  count;
    size_t  capacity;
} PgyListRaw;

typedef struct {
    void   *data;
    size_t  head;
    size_t  tail;
    size_t  count;
    size_t  capacity;
} PgyQueueRaw;

typedef struct {
    char    **keys;
    void     *values;
    uint8_t  *occupied;
    size_t    count;
    size_t    capacity;
} PgyHashMapRaw;

static uint32_t
pgy_hash_string_export(const char *s)
{
    uint32_t h = 2166136261u;
    if (s == NULL)
        return 0;
    while (*s != '\0') {
        h ^= (uint8_t)(*s++);
        h *= 16777619u;
    }
    return h;
}

void
pgy_list_new_raw_export(void *list_ptr, int64_t elem_size)
{
    PgyListRaw *list = (PgyListRaw *)list_ptr;
    if (list == NULL) {
        pgy_runtime_warn_invalid_collection("list_new", "null list");
        return;
    }
    if (elem_size <= 0) {
        pgy_runtime_warn_invalid_collection("list_new", "non-positive element size");
        return;
    }
    list->capacity = 16;
    list->count = 0;
    list->data = calloc((size_t)list->capacity, (size_t)elem_size);
    if (list->data == NULL) {
        list->capacity = 0;
        pgy_runtime_warn_invalid_collection("list_new", "allocation failed");
    }
}

void
pgy_list_push_raw_export(void *list_ptr, void *value_ptr, int64_t elem_size)
{
    PgyListRaw *list = (PgyListRaw *)list_ptr;
    char *dst;
    if (list == NULL) {
        pgy_runtime_warn_invalid_collection("list_push", "null list");
        return;
    }
    if (value_ptr == NULL) {
        pgy_runtime_warn_invalid_collection("list_push", "null value");
        return;
    }
    if (elem_size <= 0) {
        pgy_runtime_warn_invalid_collection("list_push", "non-positive element size");
        return;
    }
    if (list->data == NULL && list->capacity == 0) {
        pgy_runtime_warn_invalid_collection("list_push", "list is not initialized");
        return;
    }
    if (list->count >= list->capacity) {
        size_t new_capacity = list->capacity == 0 ? 16 : list->capacity * 2;
        void *grown = realloc(list->data, new_capacity * (size_t)elem_size);
        if (grown == NULL) {
            pgy_runtime_warn_invalid_collection("list_push", "realloc failed");
            return;
        }
        list->data = grown;
        list->capacity = new_capacity;
    }
    dst = (char *)list->data + (list->count * (size_t)elem_size);
    memcpy(dst, value_ptr, (size_t)elem_size);
    list->count++;
}

void
pgy_list_get_raw_export(void *list_ptr, int32_t index, void *out_ptr, int64_t elem_size)
{
    PgyListRaw *list = (PgyListRaw *)list_ptr;
    if (out_ptr == NULL) {
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_INTERNAL_INVARIANT,
                          "list get on null output");
    }
    if (elem_size <= 0) {
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_INTERNAL_INVARIANT,
                          "list get with invalid element size");
    }
    memset(out_ptr, 0, (size_t)elem_size);
    if (list == NULL) {
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_INTERNAL_INVARIANT,
                          "list get on null list");
    }
    if (index < 0 || (size_t)index >= list->count) {
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_OUT_OF_BOUNDS,
                          "list index out of bounds");
    }
    memcpy(out_ptr,
           (char *)list->data + ((size_t)index * (size_t)elem_size),
           (size_t)elem_size);
}

void
pgy_list_set_raw_export(void *list_ptr, int32_t index, void *value_ptr, int64_t elem_size)
{
    PgyListRaw *list = (PgyListRaw *)list_ptr;
    if (list == NULL) {
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_INTERNAL_INVARIANT,
                          "list set on null list");
    }
    if (value_ptr == NULL) {
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_INTERNAL_INVARIANT,
                          "list set with null value");
    }
    if (elem_size <= 0) {
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_INTERNAL_INVARIANT,
                          "list set with invalid element size");
    }
    if (index < 0 || (size_t)index >= list->count) {
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_OUT_OF_BOUNDS,
                          "list set index out of bounds");
    }
    memcpy((char *)list->data + ((size_t)index * (size_t)elem_size),
           value_ptr, (size_t)elem_size);
}

int32_t
pgy_list_size_raw_export(void *list_ptr)
{
    PgyListRaw *list = (PgyListRaw *)list_ptr;
    if (list == NULL) {
        pgy_runtime_warn_invalid_collection("list_size", "null list");
        return 0;
    }
    return (int32_t)list->count;
}

void
pgy_list_remove_raw_export(void *list_ptr, int32_t index, int64_t elem_size)
{
    PgyListRaw *list = (PgyListRaw *)list_ptr;
    size_t tail_count;
    if (list == NULL) {
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_INTERNAL_INVARIANT,
                          "list remove on null list");
    }
    if (elem_size <= 0) {
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_INTERNAL_INVARIANT,
                          "list remove with invalid element size");
    }
    if (index < 0 || (size_t)index >= list->count) {
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_OUT_OF_BOUNDS,
                          "list remove index out of bounds");
    }
    tail_count = list->count - (size_t)index - 1;
    if (tail_count > 0) {
        memmove((char *)list->data + ((size_t)index * (size_t)elem_size),
                (char *)list->data + (((size_t)index + 1) * (size_t)elem_size),
                tail_count * (size_t)elem_size);
    }
    list->count--;
}

void
pgy_queue_new_raw_export(void *queue_ptr, int64_t elem_size)
{
    PgyQueueRaw *queue = (PgyQueueRaw *)queue_ptr;
    if (queue == NULL) {
        pgy_runtime_warn_invalid_collection("queue_new", "null queue");
        return;
    }
    if (elem_size <= 0) {
        pgy_runtime_warn_invalid_collection("queue_new", "non-positive element size");
        return;
    }
    queue->capacity = 16;
    queue->head = 0;
    queue->tail = 0;
    queue->count = 0;
    queue->data = calloc(queue->capacity, (size_t)elem_size);
    if (queue->data == NULL) {
        queue->capacity = 0;
        pgy_runtime_warn_invalid_collection("queue_new", "allocation failed");
    }
}

void
pgy_queue_push_raw_export(void *queue_ptr, void *value_ptr, int64_t elem_size)
{
    PgyQueueRaw *queue = (PgyQueueRaw *)queue_ptr;
    if (queue == NULL) {
        pgy_runtime_warn_invalid_collection("queue_push", "null queue");
        return;
    }
    if (value_ptr == NULL) {
        pgy_runtime_warn_invalid_collection("queue_push", "null value");
        return;
    }
    if (elem_size <= 0) {
        pgy_runtime_warn_invalid_collection("queue_push", "non-positive element size");
        return;
    }
    if (queue->data == NULL && queue->capacity == 0) {
        pgy_runtime_warn_invalid_collection("queue_push", "queue is not initialized");
        return;
    }
    if (queue->count >= queue->capacity) {
        size_t new_capacity = queue->capacity == 0 ? 16 : queue->capacity * 2;
        void *new_data = calloc(new_capacity, (size_t)elem_size);
        if (new_data == NULL) {
            pgy_runtime_warn_invalid_collection("queue_push", "allocation failed");
            return;
        }
        for (size_t i = 0; i < queue->count; i++) {
            memcpy((char *)new_data + (i * (size_t)elem_size),
                   (char *)queue->data + (((queue->head + i) % queue->capacity) * (size_t)elem_size),
                   (size_t)elem_size);
        }
        free(queue->data);
        queue->data = new_data;
        queue->head = 0;
        queue->tail = queue->count;
        queue->capacity = new_capacity;
    }
    memcpy((char *)queue->data + (queue->tail * (size_t)elem_size),
           value_ptr, (size_t)elem_size);
    queue->tail = (queue->tail + 1) % queue->capacity;
    queue->count++;
}

void
pgy_queue_pop_raw_export(void *queue_ptr, void *out_ptr, int64_t elem_size)
{
    PgyQueueRaw *queue = (PgyQueueRaw *)queue_ptr;
    if (out_ptr == NULL) {
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_INTERNAL_INVARIANT,
                          "queue pop on null output");
    }
    if (elem_size <= 0) {
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_INTERNAL_INVARIANT,
                          "queue pop with invalid element size");
    }
    memset(out_ptr, 0, (size_t)elem_size);
    if (queue == NULL) {
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_INTERNAL_INVARIANT,
                          "queue pop on null queue");
    }
    if (queue->count == 0) {
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_OUT_OF_BOUNDS,
                          "queue pop from empty queue");
    }
    memcpy(out_ptr,
           (char *)queue->data + (queue->head * (size_t)elem_size),
           (size_t)elem_size);
    queue->head = (queue->head + 1) % queue->capacity;
    queue->count--;
}

int32_t
pgy_queue_size_raw_export(void *queue_ptr)
{
    PgyQueueRaw *queue = (PgyQueueRaw *)queue_ptr;
    if (queue == NULL) {
        pgy_runtime_warn_invalid_collection("queue_size", "null queue");
        return 0;
    }
    return (int32_t)queue->count;
}

bool
pgy_queue_empty_raw_export(void *queue_ptr)
{
    PgyQueueRaw *queue = (PgyQueueRaw *)queue_ptr;
    if (queue == NULL)
        pgy_runtime_warn_invalid_collection("queue_empty", "null queue");
    return queue == NULL || queue->count == 0;
}

void
pgy_map_new_raw_export(void *map_ptr, int64_t value_size)
{
    PgyHashMapRaw *map = (PgyHashMapRaw *)map_ptr;
    if (map == NULL) {
        pgy_runtime_warn_invalid_collection("map_new", "null map");
        return;
    }
    if (value_size <= 0) {
        pgy_runtime_warn_invalid_collection("map_new", "non-positive value size");
        return;
    }
    map->capacity = 16;
    map->count = 0;
    map->keys = (char **)calloc(map->capacity, sizeof(char *));
    map->values = calloc(map->capacity, (size_t)value_size);
    map->occupied = (uint8_t *)calloc(map->capacity, sizeof(uint8_t));
    if (map->keys == NULL || map->values == NULL || map->occupied == NULL) {
        free(map->keys);
        free(map->values);
        free(map->occupied);
        map->keys = NULL;
        map->values = NULL;
        map->occupied = NULL;
        map->capacity = 0;
        pgy_runtime_warn_invalid_collection("map_new", "allocation failed");
    }
}

static void
pgy_map_grow_raw_export(PgyHashMapRaw *map, int64_t value_size)
{
    size_t old_capacity = map->capacity;
    char **old_keys = map->keys;
    void *old_values = map->values;
    uint8_t *old_occupied = map->occupied;

    size_t new_capacity = map->capacity == 0 ? 16 : map->capacity * 2;
    char **new_keys = (char **)calloc(new_capacity, sizeof(char *));
    void *new_values = calloc(new_capacity, (size_t)value_size);
    uint8_t *new_occupied = (uint8_t *)calloc(new_capacity, sizeof(uint8_t));
    if (new_keys == NULL || new_values == NULL || new_occupied == NULL) {
        free(new_keys);
        free(new_values);
        free(new_occupied);
        pgy_runtime_warn_invalid_collection("map_grow", "allocation failed");
        return;
    }
    map->capacity = new_capacity;
    map->keys = new_keys;
    map->values = new_values;
    map->occupied = new_occupied;
    map->count = 0;

    for (size_t i = 0; i < old_capacity; i++) {
        if (!old_occupied[i] || old_keys[i] == NULL)
            continue;
        {
            uint32_t h = pgy_hash_string_export(old_keys[i]) % (uint32_t)map->capacity;
            while (map->occupied[h])
                h = (h + 1) % (uint32_t)map->capacity;
            map->keys[h] = old_keys[i];
            memcpy((char *)map->values + (h * (size_t)value_size),
                   (char *)old_values + (i * (size_t)value_size),
                   (size_t)value_size);
            map->occupied[h] = 1;
            map->count++;
        }
    }

    free(old_keys);
    free(old_values);
    free(old_occupied);
}

void
pgy_map_set_raw_export(void *map_ptr, const char *key, void *value_ptr, int64_t value_size)
{
    PgyHashMapRaw *map = (PgyHashMapRaw *)map_ptr;
    uint32_t h;
    if (map == NULL) {
        pgy_runtime_warn_invalid_collection("map_set", "null map");
        return;
    }
    if (key == NULL) {
        pgy_runtime_warn_invalid_collection("map_set", "null key");
        return;
    }
    if (value_ptr == NULL) {
        pgy_runtime_warn_invalid_collection("map_set", "null value");
        return;
    }
    if (value_size <= 0) {
        pgy_runtime_warn_invalid_collection("map_set", "non-positive value size");
        return;
    }
    if (map->capacity == 0 || map->keys == NULL || map->values == NULL
        || map->occupied == NULL) {
        pgy_runtime_warn_invalid_collection("map_set", "map is not initialized");
        return;
    }
    if ((double)map->count / (double)map->capacity > 0.75)
        pgy_map_grow_raw_export(map, value_size);
    if (map->capacity == 0 || map->keys == NULL || map->values == NULL
        || map->occupied == NULL) {
        pgy_runtime_warn_invalid_collection("map_set", "map growth failed");
        return;
    }
    h = pgy_hash_string_export(key) % (uint32_t)map->capacity;
    while (map->occupied[h]) {
        if (map->keys[h] != NULL && strcmp(map->keys[h], key) == 0) {
            memcpy((char *)map->values + (h * (size_t)value_size),
                   value_ptr, (size_t)value_size);
            return;
        }
        h = (h + 1) % (uint32_t)map->capacity;
    }
    map->keys[h] = pgy_runtime_strdup_export(key);
    if (map->keys[h] == NULL) {
        pgy_runtime_warn_invalid_collection("map_set", "key duplication failed");
        return;
    }
    memcpy((char *)map->values + (h * (size_t)value_size),
           value_ptr, (size_t)value_size);
    map->occupied[h] = 1;
    map->count++;
}

void
pgy_map_get_raw_export(void *map_ptr, const char *key, void *out_ptr, int64_t value_size)
{
    PgyHashMapRaw *map = (PgyHashMapRaw *)map_ptr;
    uint32_t h;
    size_t probes = 0;
    if (out_ptr == NULL) {
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_INTERNAL_INVARIANT,
                          "map get on null output");
    }
    if (value_size <= 0) {
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_INTERNAL_INVARIANT,
                          "map get with invalid value size");
    }
    memset(out_ptr, 0, (size_t)value_size);
    if (map == NULL) {
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_INTERNAL_INVARIANT,
                          "map get on null map");
    }
    if (key == NULL) {
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_INTERNAL_INVARIANT,
                          "map get with null key");
    }
    if (map->capacity == 0 || map->keys == NULL || map->values == NULL
        || map->occupied == NULL) {
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_INTERNAL_INVARIANT,
                          "map get on uninitialized map");
    }
    if (map->count == 0) {
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_OUT_OF_BOUNDS,
                          "map key not found");
    }
    h = pgy_hash_string_export(key) % (uint32_t)map->capacity;
    while (map->occupied[h] && probes < map->capacity) {
        if (map->keys[h] != NULL && strcmp(map->keys[h], key) == 0) {
            memcpy(out_ptr,
                   (char *)map->values + (h * (size_t)value_size),
                   (size_t)value_size);
            return;
        }
        h = (h + 1) % (uint32_t)map->capacity;
        probes++;
    }
    PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_OUT_OF_BOUNDS,
                      "map key not found");
}

bool
pgy_map_has_raw_export(void *map_ptr, const char *key)
{
    PgyHashMapRaw *map = (PgyHashMapRaw *)map_ptr;
    uint32_t h;
    size_t probes = 0;
    if (map == NULL) {
        pgy_runtime_warn_invalid_collection("map_has", "null map");
        return false;
    }
    if (key == NULL) {
        pgy_runtime_warn_invalid_collection("map_has", "null key");
        return false;
    }
    if (map->count == 0)
        return false;
    h = pgy_hash_string_export(key) % (uint32_t)map->capacity;
    while (map->occupied[h] && probes < map->capacity) {
        if (map->keys[h] != NULL && strcmp(map->keys[h], key) == 0)
            return true;
        h = (h + 1) % (uint32_t)map->capacity;
        probes++;
    }
    return false;
}

void
pgy_map_remove_raw_export(void *map_ptr, const char *key, int64_t value_size)
{
    PgyHashMapRaw *map = (PgyHashMapRaw *)map_ptr;
    uint32_t h;
    size_t probes = 0;
    if (map == NULL) {
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_INTERNAL_INVARIANT,
                          "map remove on null map");
    }
    if (key == NULL) {
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_INTERNAL_INVARIANT,
                          "map remove with null key");
    }
    if (value_size <= 0) {
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_INTERNAL_INVARIANT,
                          "map remove with invalid value size");
    }
    if (map->count == 0) {
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_OUT_OF_BOUNDS,
                          "map remove key not found");
    }
    h = pgy_hash_string_export(key) % (uint32_t)map->capacity;
    while (map->occupied[h] && probes < map->capacity) {
        if (map->keys[h] != NULL && strcmp(map->keys[h], key) == 0) {
            free(map->keys[h]);
            map->keys[h] = NULL;
            memset((char *)map->values + (h * (size_t)value_size), 0, (size_t)value_size);
            map->occupied[h] = 0;
            map->count--;
            return;
        }
        h = (h + 1) % (uint32_t)map->capacity;
        probes++;
    }
    PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_OUT_OF_BOUNDS,
                      "map remove key not found");
}

int32_t
pgy_map_size_raw_export(void *map_ptr)
{
    PgyHashMapRaw *map = (PgyHashMapRaw *)map_ptr;
    if (map == NULL) {
        pgy_runtime_warn_invalid_collection("map_size", "null map");
        return 0;
    }
    return (int32_t)map->count;
}

static char *
pgy_map_i32_key_string_export(int32_t key)
{
    char stack_buf[32];
    int len = snprintf(stack_buf, sizeof(stack_buf), "%d", key);
    char *buf;

    if (len < 0)
        return NULL;
    buf = (char *)malloc((size_t)len + 1);
    if (buf == NULL)
        return NULL;
    memcpy(buf, stack_buf, (size_t)len + 1);
    return buf;
}

static char *
pgy_map_i64_key_string_export(int64_t key)
{
    char stack_buf[48];
    int len = snprintf(stack_buf, sizeof(stack_buf), "%lld", (long long)key);
    char *buf;

    if (len < 0)
        return NULL;
    buf = (char *)malloc((size_t)len + 1);
    if (buf == NULL)
        return NULL;
    memcpy(buf, stack_buf, (size_t)len + 1);
    return buf;
}

static char *
pgy_map_bool_key_string_export(bool key)
{
    const char *src = key ? "true" : "false";
    size_t len = strlen(src);
    char *buf = (char *)malloc(len + 1);
    if (buf == NULL)
        return NULL;
    memcpy(buf, src, len + 1);
    return buf;
}

void
pgy_map_set_raw_i32_export(void *map_ptr, int32_t key, void *value_ptr, int64_t value_size)
{
    char *key_str = pgy_map_i32_key_string_export(key);
    if (key_str == NULL) {
        pgy_runtime_warn_invalid_collection("map_set_i32", "key formatting failed");
        return;
    }
    pgy_map_set_raw_export(map_ptr, key_str, value_ptr, value_size);
    free(key_str);
}

void
pgy_map_get_raw_i32_export(void *map_ptr, int32_t key, void *out_ptr, int64_t value_size)
{
    char *key_str = pgy_map_i32_key_string_export(key);
    if (key_str == NULL) {
        pgy_runtime_warn_invalid_collection("map_get_i32", "key formatting failed");
        if (out_ptr != NULL && value_size > 0)
            memset(out_ptr, 0, (size_t)value_size);
        return;
    }
    pgy_map_get_raw_export(map_ptr, key_str, out_ptr, value_size);
    free(key_str);
}

void
pgy_map_set_raw_i64_export(void *map_ptr, int64_t key, void *value_ptr, int64_t value_size)
{
    char *key_str = pgy_map_i64_key_string_export(key);
    if (key_str == NULL) {
        pgy_runtime_warn_invalid_collection("map_set_i64", "key formatting failed");
        return;
    }
    pgy_map_set_raw_export(map_ptr, key_str, value_ptr, value_size);
    free(key_str);
}

void
pgy_map_get_raw_i64_export(void *map_ptr, int64_t key, void *out_ptr, int64_t value_size)
{
    char *key_str = pgy_map_i64_key_string_export(key);
    if (key_str == NULL) {
        pgy_runtime_warn_invalid_collection("map_get_i64", "key formatting failed");
        if (out_ptr != NULL && value_size > 0)
            memset(out_ptr, 0, (size_t)value_size);
        return;
    }
    pgy_map_get_raw_export(map_ptr, key_str, out_ptr, value_size);
    free(key_str);
}

bool
pgy_map_has_raw_i64_export(void *map_ptr, int64_t key)
{
    bool result;
    char *key_str = pgy_map_i64_key_string_export(key);
    if (key_str == NULL) {
        pgy_runtime_warn_invalid_collection("map_has_i64", "key formatting failed");
        return false;
    }
    result = pgy_map_has_raw_export(map_ptr, key_str);
    free(key_str);
    return result;
}

void
pgy_map_remove_raw_i64_export(void *map_ptr, int64_t key, int64_t value_size)
{
    char *key_str = pgy_map_i64_key_string_export(key);
    if (key_str == NULL) {
        pgy_runtime_warn_invalid_collection("map_remove_i64", "key formatting failed");
        return;
    }
    pgy_map_remove_raw_export(map_ptr, key_str, value_size);
    free(key_str);
}

void
pgy_map_set_raw_bool_export(void *map_ptr, bool key, void *value_ptr, int64_t value_size)
{
    char *key_str = pgy_map_bool_key_string_export(key);
    if (key_str == NULL) {
        pgy_runtime_warn_invalid_collection("map_set_bool", "key formatting failed");
        return;
    }
    pgy_map_set_raw_export(map_ptr, key_str, value_ptr, value_size);
    free(key_str);
}

void
pgy_map_get_raw_bool_export(void *map_ptr, bool key, void *out_ptr, int64_t value_size)
{
    char *key_str = pgy_map_bool_key_string_export(key);
    if (key_str == NULL) {
        pgy_runtime_warn_invalid_collection("map_get_bool", "key formatting failed");
        if (out_ptr != NULL && value_size > 0)
            memset(out_ptr, 0, (size_t)value_size);
        return;
    }
    pgy_map_get_raw_export(map_ptr, key_str, out_ptr, value_size);
    free(key_str);
}

bool
pgy_map_has_raw_bool_export(void *map_ptr, bool key)
{
    bool result;
    char *key_str = pgy_map_bool_key_string_export(key);
    if (key_str == NULL) {
        pgy_runtime_warn_invalid_collection("map_has_bool", "key formatting failed");
        return false;
    }
    result = pgy_map_has_raw_export(map_ptr, key_str);
    free(key_str);
    return result;
}

void
pgy_map_remove_raw_bool_export(void *map_ptr, bool key, int64_t value_size)
{
    char *key_str = pgy_map_bool_key_string_export(key);
    if (key_str == NULL) {
        pgy_runtime_warn_invalid_collection("map_remove_bool", "key formatting failed");
        return;
    }
    pgy_map_remove_raw_export(map_ptr, key_str, value_size);
    free(key_str);
}

bool
pgy_map_has_raw_i32_export(void *map_ptr, int32_t key)
{
    bool result;
    char *key_str = pgy_map_i32_key_string_export(key);
    if (key_str == NULL) {
        pgy_runtime_warn_invalid_collection("map_has_i32", "key formatting failed");
        return false;
    }
    result = pgy_map_has_raw_export(map_ptr, key_str);

    free(key_str);
    return result;
}

void
pgy_map_remove_raw_i32_export(void *map_ptr, int32_t key, int64_t value_size)
{
    char *key_str = pgy_map_i32_key_string_export(key);
    if (key_str == NULL) {
        pgy_runtime_warn_invalid_collection("map_remove_i32", "key formatting failed");
        return;
    }
    pgy_map_remove_raw_export(map_ptr, key_str, value_size);
    free(key_str);
}

/* =================================================================
 * Set — raw (type-erased) export functions for LLVM linking
 *
 * Generic hash set using open-addressing with FNV-1a hash on raw bytes.
 * Works for ANY element type (Int, String, Bool, Float, structs)
 * without requiring string conversion.
 *
 * Layout matches PgySet_Generic:
 *   void    *data       — element storage (elem_size * capacity)
 *   uint8_t *occupied   — slot occupancy flags
 *   size_t   count
 *   size_t   capacity
 * ================================================================= */

typedef struct {
    void    *data;
    uint8_t *occupied;
    size_t   count;
    size_t   capacity;
} PgySetRaw;

static uint32_t
pgy_hash_bytes(const void *ptr, size_t len)
{
    const uint8_t *p = (const uint8_t *)ptr;
    uint32_t h = 2166136261u;
    for (size_t i = 0; i < len; i++) {
        h ^= p[i];
        h *= 16777619u;
    }
    return h;
}

static bool
pgy_set_raw_elem_eq(const void *a, const void *b, int64_t elem_size)
{
    /* String comparison for pointer-sized string elements */
    if (elem_size == (int64_t)sizeof(char *)) {
        const char *sa = *(const char *const *)a;
        const char *sb = *(const char *const *)b;
        if (sa == sb) return true;
        if (sa == NULL || sb == NULL) return false;
        return strcmp(sa, sb) == 0;
    }
    return memcmp(a, b, (size_t)elem_size) == 0;
}

static uint32_t
pgy_set_raw_hash(const void *elem, int64_t elem_size)
{
    /* String hashing for pointer-sized string elements */
    if (elem_size == (int64_t)sizeof(char *)) {
        const char *s = *(const char *const *)elem;
        return s != NULL ? pgy_hash_string_export(s) : 0;
    }
    return pgy_hash_bytes(elem, (size_t)elem_size);
}

#define SET_RAW_ELEM(set, idx, esz) ((char *)(set)->data + (idx) * (size_t)(esz))

void
pgy_set_new_raw_export(void *set_ptr, int64_t elem_size)
{
    PgySetRaw *set = (PgySetRaw *)set_ptr;
    if (set == NULL) {
        pgy_runtime_warn_invalid_collection("set_new", "null set");
        return;
    }
    if (elem_size <= 0) {
        pgy_runtime_warn_invalid_collection("set_new", "non-positive element size");
        return;
    }
    set->capacity = 16;
    set->count = 0;
    set->data = calloc(set->capacity, (size_t)elem_size);
    set->occupied = (uint8_t *)calloc(set->capacity, sizeof(uint8_t));
    if (set->data == NULL || set->occupied == NULL) {
        free(set->data);
        free(set->occupied);
        set->data = NULL;
        set->occupied = NULL;
        set->capacity = 0;
        pgy_runtime_warn_invalid_collection("set_new", "allocation failed");
    }
}

static void
pgy_set_raw_rehash(PgySetRaw *set, int64_t elem_size)
{
    size_t oc = set->capacity;
    void *od = set->data;
    uint8_t *oo = set->occupied;
    size_t new_capacity = set->capacity == 0 ? 16 : set->capacity * 2;
    void *new_data = calloc(new_capacity, (size_t)elem_size);
    uint8_t *new_occupied = (uint8_t *)calloc(new_capacity, sizeof(uint8_t));
    if (new_data == NULL || new_occupied == NULL) {
        free(new_data);
        free(new_occupied);
        pgy_runtime_warn_invalid_collection("set_rehash", "allocation failed");
        return;
    }
    set->capacity = new_capacity;
    set->data = new_data;
    set->occupied = new_occupied;
    set->count = 0;
    for (size_t i = 0; i < oc; i++) {
        if (oo[i]) {
            void *elem = (char *)od + i * (size_t)elem_size;
            uint32_t h = pgy_set_raw_hash(elem, elem_size) % (uint32_t)set->capacity;
            while (set->occupied[h]) h = (h + 1) % (uint32_t)set->capacity;
            memcpy(SET_RAW_ELEM(set, h, elem_size), elem, (size_t)elem_size);
            set->occupied[h] = 1;
            set->count++;
        }
    }
    free(od);
    free(oo);
}

void
pgy_set_add_raw_export(void *set_ptr, void *elem_ptr, int64_t elem_size)
{
    PgySetRaw *set = (PgySetRaw *)set_ptr;
    if (set == NULL) {
        pgy_runtime_warn_invalid_collection("set_add", "null set");
        return;
    }
    if (elem_ptr == NULL) {
        pgy_runtime_warn_invalid_collection("set_add", "null element");
        return;
    }
    if (elem_size <= 0) {
        pgy_runtime_warn_invalid_collection("set_add", "non-positive element size");
        return;
    }
    if (set->capacity == 0 || set->data == NULL || set->occupied == NULL) {
        pgy_runtime_warn_invalid_collection("set_add", "set is not initialized");
        return;
    }
    /* Check if already present */
    uint32_t h = pgy_set_raw_hash(elem_ptr, elem_size) % (uint32_t)set->capacity;
    size_t p = 0;
    while (set->occupied[h] && p < set->capacity) {
        if (pgy_set_raw_elem_eq(SET_RAW_ELEM(set, h, elem_size), elem_ptr, elem_size))
            return; /* already in set */
        h = (h + 1) % (uint32_t)set->capacity; p++;
    }
    /* Resize if needed */
    if ((double)set->count / (double)set->capacity > 0.75) {
        pgy_set_raw_rehash(set, elem_size);
        if (set->capacity == 0 || set->data == NULL || set->occupied == NULL) {
            pgy_runtime_warn_invalid_collection("set_add", "set rehash failed");
            return;
        }
        h = pgy_set_raw_hash(elem_ptr, elem_size) % (uint32_t)set->capacity;
        while (set->occupied[h]) h = (h + 1) % (uint32_t)set->capacity;
    }
    memcpy(SET_RAW_ELEM(set, h, elem_size), elem_ptr, (size_t)elem_size);
    set->occupied[h] = 1;
    set->count++;
}
