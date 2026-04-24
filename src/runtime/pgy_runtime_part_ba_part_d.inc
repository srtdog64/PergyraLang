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

static inline char* pgy_long_to_string(int64_t val) {
    char stack_buf[32];
    int len = snprintf(stack_buf, sizeof(stack_buf), "%lld", (long long)val);
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

static inline char* pgy_float_to_string(float val) {
    char stack_buf[32];
    int len = snprintf(stack_buf, sizeof(stack_buf), "%g", (double)val);
    if (len < 0) {
        char *fallback = (char *)malloc(4);
        if (fallback != NULL) { fallback[0] = '0'; fallback[1] = '.'; fallback[2] = '0'; fallback[3] = '\0'; }
        return fallback;
    }
    char *buf = (char *)malloc((size_t)len + 1);
    if (buf == NULL) return NULL;
    memcpy(buf, stack_buf, (size_t)len + 1);
    return buf;
}

static inline char* pgy_bool_to_string(bool val) {
    const char *s = val ? "true" : "false";
    size_t len = strlen(s);
    char *buf = (char *)malloc(len + 1);
    if (buf != NULL) memcpy(buf, s, len + 1);
    return buf;
}

/* Console I/O: Input(prompt), Print(msg) are already defined below as
 * pgy_input() and pgy_print(). See type_checker_builtins.c for semantic. */

/* =================================================================
 * HashMap<String, T> / HashMap<Int, T> / HashMap<Long, T> /
 * HashMap<Bool, T> — stable key subset over open-addressing
 * ================================================================= */

#define PGY_HASHMAP_INIT_CAP 16
#define PGY_HASHMAP_LOAD_FACTOR 0.75

typedef struct
{
    char   **keys;
    int32_t *values;
    uint8_t *occupied;
    size_t   count;
    size_t   capacity;
} PgyHashMap_Int;

static inline uint32_t pgy_hash_string(const char *s)
{
    uint32_t h = 5381;
    if (s == NULL) return h;
    while (*s) { h = ((h << 5) + h) ^ (uint32_t)*s++; }
    return h;
}

static inline char *pgy_map_format_i32_key(int32_t key)
{
    return pgy_int_to_string(key);
}

static inline char *pgy_map_format_i64_key(int64_t key)
{
    return pgy_long_to_string(key);
}

static inline char *pgy_map_format_bool_key(bool key)
{
    return pgy_bool_to_string(key);
}

#define PGY_HASHMAP_DEFINE(SuffixName, CType) \
typedef struct \
{ \
    char    **keys; \
    CType    *values; \
    uint8_t  *occupied; \
    size_t    count; \
    size_t    capacity; \
} PgyHashMap_##SuffixName; \
\
static inline PgyHashMap_##SuffixName pgy_map_new_##SuffixName(void) \
{ \
    PgyHashMap_##SuffixName m; \
    m.capacity = PGY_HASHMAP_INIT_CAP; \
    m.count = 0; \
    m.keys = (char **)calloc(m.capacity, sizeof(char *)); \
    m.values = (CType *)calloc(m.capacity, sizeof(CType)); \
    m.occupied = (uint8_t *)calloc(m.capacity, sizeof(uint8_t)); \
    if (m.keys == NULL || m.values == NULL || m.occupied == NULL) { \
        free(m.keys); free(m.values); free(m.occupied); \
        m.keys = NULL; m.values = NULL; m.occupied = NULL; \
        m.capacity = 0; \
        pgy_runtime_warn_invalid_collection("map_new_" #SuffixName, "allocation failed"); \
    } \
    return m; \
} \
\
static inline void pgy_map_grow_##SuffixName(PgyHashMap_##SuffixName *m) \
{ \
    size_t old_cap = m->capacity; \
    char **old_keys = m->keys; \
    CType *old_vals = m->values; \
    uint8_t *old_occ = m->occupied; \
    size_t new_capacity = m->capacity == 0 ? PGY_HASHMAP_INIT_CAP : m->capacity * 2; \
    char **new_keys = (char **)calloc(new_capacity, sizeof(char *)); \
    CType *new_values = (CType *)calloc(new_capacity, sizeof(CType)); \
    uint8_t *new_occupied = (uint8_t *)calloc(new_capacity, sizeof(uint8_t)); \
    if (new_keys == NULL || new_values == NULL || new_occupied == NULL) { \
        free(new_keys); free(new_values); free(new_occupied); \
        pgy_runtime_warn_invalid_collection("map_grow_" #SuffixName, "allocation failed"); \
        return; \
    } \
    m->capacity = new_capacity; \
    m->keys = new_keys; \
    m->values = new_values; \
    m->occupied = new_occupied; \
    m->count = 0; \
    for (size_t i = 0; i < old_cap; i++) { \
        if (old_occ[i]) { \
            uint32_t h = pgy_hash_string(old_keys[i]) % (uint32_t)m->capacity; \
            while (m->occupied[h]) h = (h + 1) % (uint32_t)m->capacity; \
            m->keys[h] = old_keys[i]; \
            m->values[h] = old_vals[i]; \
            m->occupied[h] = 1; \
            m->count++; \
        } \
    } \
    free(old_keys); free(old_vals); free(old_occ); \
} \
\
static inline void pgy_map_set_##SuffixName(PgyHashMap_##SuffixName *m, const char *key, CType val) \
{ \
    if (m == NULL || m->capacity == 0 || m->keys == NULL || m->values == NULL || m->occupied == NULL) { \
        pgy_runtime_warn_invalid_collection("map_set_" #SuffixName, "map is not initialized"); \
        return; \
    } \
    if ((double)m->count / (double)m->capacity > PGY_HASHMAP_LOAD_FACTOR) \
        pgy_map_grow_##SuffixName(m); \
    if (m->capacity == 0 || m->keys == NULL || m->values == NULL || m->occupied == NULL) { \
        pgy_runtime_warn_invalid_collection("map_set_" #SuffixName, "map growth failed"); \
        return; \
    } \
    uint32_t h = pgy_hash_string(key) % (uint32_t)m->capacity; \
    while (m->occupied[h]) { \
        if (m->keys[h] != NULL && strcmp(m->keys[h], key) == 0) { \
            m->values[h] = val; \
            return; \
        } \
        h = (h + 1) % (uint32_t)m->capacity; \
    } \
    m->keys[h] = pgy_runtime_strdup(key); \
    if (m->keys[h] == NULL) { \
        pgy_runtime_warn_invalid_collection("map_set_" #SuffixName, "key duplication failed"); \
        return; \
    } \
    m->values[h] = val; \
    m->occupied[h] = 1; \
    m->count++; \
} \
\
static inline CType pgy_map_get_##SuffixName(PgyHashMap_##SuffixName *m, const char *key) \
{ \
    CType zero_value; \
    memset(&zero_value, 0, sizeof(CType)); \
    if (m == NULL || m->capacity == 0 || m->keys == NULL || m->values == NULL || m->occupied == NULL) return zero_value; \
    if (m->count == 0) return zero_value; \
    uint32_t h = pgy_hash_string(key) % (uint32_t)m->capacity; \
    size_t probes = 0; \
    while (m->occupied[h] && probes < m->capacity) { \
        if (m->keys[h] != NULL && strcmp(m->keys[h], key) == 0) \
            return m->values[h]; \
        h = (h + 1) % (uint32_t)m->capacity; \
        probes++; \
    } \
    return zero_value; \
} \
\
static inline bool pgy_map_has_##SuffixName(PgyHashMap_##SuffixName *m, const char *key) \
{ \
    if (m == NULL || m->capacity == 0 || m->keys == NULL || m->values == NULL || m->occupied == NULL) return false; \
    if (m->count == 0) return false; \
    uint32_t h = pgy_hash_string(key) % (uint32_t)m->capacity; \
    size_t probes = 0; \
    while (m->occupied[h] && probes < m->capacity) { \
        if (m->keys[h] != NULL && strcmp(m->keys[h], key) == 0) \
            return true; \
        h = (h + 1) % (uint32_t)m->capacity; \
        probes++; \
    } \
    return false; \
} \
\
static inline void pgy_map_remove_##SuffixName(PgyHashMap_##SuffixName *m, const char *key) \
{ \
    if (m == NULL || m->capacity == 0 || m->keys == NULL || m->values == NULL || m->occupied == NULL) return; \
    if (m->count == 0) return; \
    uint32_t h = pgy_hash_string(key) % (uint32_t)m->capacity; \
    size_t probes = 0; \
    while (m->occupied[h] && probes < m->capacity) { \
        if (m->keys[h] != NULL && strcmp(m->keys[h], key) == 0) { \
            free(m->keys[h]); \
            m->keys[h] = NULL; \
            memset(&m->values[h], 0, sizeof(CType)); \
            m->occupied[h] = 0; \
            m->count--; \
            return; \
        } \
        h = (h + 1) % (uint32_t)m->capacity; \
        probes++; \
    } \
} \
\
static inline int32_t pgy_map_size_##SuffixName(PgyHashMap_##SuffixName *m) \
{ \
    return (int32_t)m->count; \
} \
\
static inline void pgy_map_set_i32_##SuffixName(PgyHashMap_##SuffixName *m, int32_t key, CType val) \
{ \
    char *key_str = pgy_map_format_i32_key(key); \
    if (key_str == NULL) { \
        pgy_runtime_warn_invalid_collection("map_set_i32_" #SuffixName, "key formatting failed"); \
        return; \
    } \
    pgy_map_set_##SuffixName(m, key_str, val); \
    free(key_str); \
} \
\
static inline CType pgy_map_get_i32_##SuffixName(PgyHashMap_##SuffixName *m, int32_t key) \
{ \
    CType value; \
    char *key_str = pgy_map_format_i32_key(key); \
    memset(&value, 0, sizeof(CType)); \
    if (key_str == NULL) { \
        pgy_runtime_warn_invalid_collection("map_get_i32_" #SuffixName, "key formatting failed"); \
        return value; \
    } \
    value = pgy_map_get_##SuffixName(m, key_str); \
    free(key_str); \
    return value; \
} \
\
static inline bool pgy_map_has_i32_##SuffixName(PgyHashMap_##SuffixName *m, int32_t key) \
{ \
    bool result; \
    char *key_str = pgy_map_format_i32_key(key); \
    if (key_str == NULL) { \
        pgy_runtime_warn_invalid_collection("map_has_i32_" #SuffixName, "key formatting failed"); \
        return false; \
    } \
    result = pgy_map_has_##SuffixName(m, key_str); \
    free(key_str); \
    return result; \
} \
\
static inline void pgy_map_remove_i32_##SuffixName(PgyHashMap_##SuffixName *m, int32_t key) \
{ \
    char *key_str = pgy_map_format_i32_key(key); \
    if (key_str == NULL) { \
        pgy_runtime_warn_invalid_collection("map_remove_i32_" #SuffixName, "key formatting failed"); \
        return; \
    } \
    pgy_map_remove_##SuffixName(m, key_str); \
    free(key_str); \
} \
\
static inline void pgy_map_set_i64_##SuffixName(PgyHashMap_##SuffixName *m, int64_t key, CType val) \
{ \
    char *key_str = pgy_map_format_i64_key(key); \
    if (key_str == NULL) { \
        pgy_runtime_warn_invalid_collection("map_set_i64_" #SuffixName, "key formatting failed"); \
        return; \
    } \
    pgy_map_set_##SuffixName(m, key_str, val); \
    free(key_str); \
} \
\
static inline CType pgy_map_get_i64_##SuffixName(PgyHashMap_##SuffixName *m, int64_t key) \
{ \
    CType value; \
    char *key_str = pgy_map_format_i64_key(key); \
    memset(&value, 0, sizeof(CType)); \
    if (key_str == NULL) { \
        pgy_runtime_warn_invalid_collection("map_get_i64_" #SuffixName, "key formatting failed"); \
        return value; \
    } \
    value = pgy_map_get_##SuffixName(m, key_str); \
    free(key_str); \
    return value; \
} \
\
static inline bool pgy_map_has_i64_##SuffixName(PgyHashMap_##SuffixName *m, int64_t key) \
{ \
    bool result = false; \
    char *key_str = pgy_map_format_i64_key(key); \
    if (key_str == NULL) { \
        pgy_runtime_warn_invalid_collection("map_has_i64_" #SuffixName, "key formatting failed"); \
        return false; \
    } \
    result = pgy_map_has_##SuffixName(m, key_str); \
    free(key_str); \
    return result; \
} \
\
static inline void pgy_map_remove_i64_##SuffixName(PgyHashMap_##SuffixName *m, int64_t key) \
{ \
    char *key_str = pgy_map_format_i64_key(key); \
    if (key_str == NULL) { \
        pgy_runtime_warn_invalid_collection("map_remove_i64_" #SuffixName, "key formatting failed"); \
        return; \
    } \
    pgy_map_remove_##SuffixName(m, key_str); \
    free(key_str); \
} \
\
static inline void pgy_map_set_bool_##SuffixName(PgyHashMap_##SuffixName *m, bool key, CType val) \
{ \
    char *key_str = pgy_map_format_bool_key(key); \
    if (key_str == NULL) { \
        pgy_runtime_warn_invalid_collection("map_set_bool_" #SuffixName, "key formatting failed"); \
        return; \
    } \
    pgy_map_set_##SuffixName(m, key_str, val); \
    free(key_str); \
} \
\
static inline CType pgy_map_get_bool_##SuffixName(PgyHashMap_##SuffixName *m, bool key) \
{ \
    CType value; \
    char *key_str = pgy_map_format_bool_key(key); \
    memset(&value, 0, sizeof(CType)); \
    if (key_str == NULL) { \
        pgy_runtime_warn_invalid_collection("map_get_bool_" #SuffixName, "key formatting failed"); \
        return value; \
    } \
    value = pgy_map_get_##SuffixName(m, key_str); \
    free(key_str); \
    return value; \
} \
\
static inline bool pgy_map_has_bool_##SuffixName(PgyHashMap_##SuffixName *m, bool key) \
{ \
    bool result = false; \
    char *key_str = pgy_map_format_bool_key(key); \
    if (key_str == NULL) { \
        pgy_runtime_warn_invalid_collection("map_has_bool_" #SuffixName, "key formatting failed"); \
        return false; \
    } \
    result = pgy_map_has_##SuffixName(m, key_str); \
    free(key_str); \
    return result; \
} \
\
static inline void pgy_map_remove_bool_##SuffixName(PgyHashMap_##SuffixName *m, bool key) \
{ \
    char *key_str = pgy_map_format_bool_key(key); \
    if (key_str == NULL) { \
        pgy_runtime_warn_invalid_collection("map_remove_bool_" #SuffixName, "key formatting failed"); \
        return; \
    } \
    pgy_map_remove_##SuffixName(m, key_str); \
    free(key_str); \
}

static inline PgyHashMap_Int pgy_map_new_int(void)
{
    PgyHashMap_Int m;
    m.capacity = PGY_HASHMAP_INIT_CAP;
    m.count = 0;
    m.keys     = (char **)calloc(m.capacity, sizeof(char *));
    m.values   = (int32_t *)calloc(m.capacity, sizeof(int32_t));
    m.occupied = (uint8_t *)calloc(m.capacity, sizeof(uint8_t));
    if (m.keys == NULL || m.values == NULL || m.occupied == NULL) {
        free(m.keys); free(m.values); free(m.occupied);
        m.keys = NULL; m.values = NULL; m.occupied = NULL; m.capacity = 0;
        pgy_runtime_warn_invalid_collection("map_new_int", "allocation failed");
    }
    return m;
}

static inline void pgy_map_grow_int(PgyHashMap_Int *m)
{
    size_t old_cap = m->capacity;
    char **old_keys = m->keys;
    int32_t *old_vals = m->values;
    uint8_t *old_occ = m->occupied;
    size_t new_capacity = m->capacity == 0 ? PGY_HASHMAP_INIT_CAP : m->capacity * 2;
    char **new_keys = (char **)calloc(new_capacity, sizeof(char *));
    int32_t *new_values = (int32_t *)calloc(new_capacity, sizeof(int32_t));
    uint8_t *new_occupied = (uint8_t *)calloc(new_capacity, sizeof(uint8_t));
    if (new_keys == NULL || new_values == NULL || new_occupied == NULL) {
        free(new_keys); free(new_values); free(new_occupied);
        pgy_runtime_warn_invalid_collection("map_grow_int", "allocation failed");
        return;
    }
    m->capacity = new_capacity;
    m->keys     = new_keys;
    m->values   = new_values;
    m->occupied = new_occupied;
    m->count = 0;

    for (size_t i = 0; i < old_cap; i++) {
        if (old_occ[i]) {
            uint32_t h = pgy_hash_string(old_keys[i]) % (uint32_t)m->capacity;
            while (m->occupied[h]) h = (h + 1) % (uint32_t)m->capacity;
            m->keys[h] = old_keys[i];
            m->values[h] = old_vals[i];
            m->occupied[h] = 1;
            m->count++;
        }
    }
    free(old_keys); free(old_vals); free(old_occ);
}

static inline void pgy_map_set_int(PgyHashMap_Int *m, const char *key, int32_t val)
{
    if (m == NULL || m->capacity == 0 || m->keys == NULL || m->values == NULL || m->occupied == NULL) {
        pgy_runtime_warn_invalid_collection("map_set_int", "map is not initialized");
        return;
    }
    if ((double)m->count / (double)m->capacity > PGY_HASHMAP_LOAD_FACTOR)
        pgy_map_grow_int(m);
    if (m->capacity == 0 || m->keys == NULL || m->values == NULL || m->occupied == NULL) {
        pgy_runtime_warn_invalid_collection("map_set_int", "map growth failed");
        return;
    }
    uint32_t h = pgy_hash_string(key) % (uint32_t)m->capacity;
    while (m->occupied[h]) {
        if (m->keys[h] != NULL && strcmp(m->keys[h], key) == 0) {
            m->values[h] = val;
            return;
        }
        h = (h + 1) % (uint32_t)m->capacity;
    }
    m->keys[h] = pgy_runtime_strdup(key);
    if (m->keys[h] == NULL) {
        pgy_runtime_warn_invalid_collection("map_set_int", "key duplication failed");
        return;
    }
    m->values[h] = val;
    m->occupied[h] = 1;
    m->count++;
}

static inline int32_t pgy_map_get_int(PgyHashMap_Int *m, const char *key)
{
    if (m == NULL || m->capacity == 0 || m->keys == NULL || m->values == NULL || m->occupied == NULL) return 0;
    if (m->count == 0) return 0;
    uint32_t h = pgy_hash_string(key) % (uint32_t)m->capacity;
    size_t probes = 0;
    while (m->occupied[h] && probes < m->capacity) {
        if (m->keys[h] != NULL && strcmp(m->keys[h], key) == 0)
            return m->values[h];
        h = (h + 1) % (uint32_t)m->capacity;
        probes++;
    }
    return 0;
}

static inline bool pgy_map_has_int(PgyHashMap_Int *m, const char *key)
{
    if (m == NULL || m->capacity == 0 || m->keys == NULL || m->values == NULL || m->occupied == NULL) return false;
    if (m->count == 0) return false;
    uint32_t h = pgy_hash_string(key) % (uint32_t)m->capacity;
    size_t probes = 0;
    while (m->occupied[h] && probes < m->capacity) {
        if (m->keys[h] != NULL && strcmp(m->keys[h], key) == 0)
            return true;
        h = (h + 1) % (uint32_t)m->capacity;
        probes++;
    }
    return false;
}

static inline void pgy_map_remove_int(PgyHashMap_Int *m, const char *key)
{
    if (m == NULL || m->capacity == 0 || m->keys == NULL || m->values == NULL || m->occupied == NULL) return;
    if (m->count == 0) return;
    uint32_t cap = (uint32_t)m->capacity;
    uint32_t h = pgy_hash_string(key) % cap;
    size_t probes = 0;
    while (m->occupied[h] && probes < m->capacity) {
        if (m->keys[h] != NULL && strcmp(m->keys[h], key) == 0) {
            free(m->keys[h]);
            m->keys[h] = NULL;
            m->values[h] = 0;
            m->occupied[h] = 0;
            m->count--;
            /* Backward-shift: rehash subsequent entries to fill the gap */
            uint32_t gap = h;
            uint32_t j = (gap + 1) % cap;
            while (m->occupied[j]) {
                uint32_t ideal = pgy_hash_string(m->keys[j]) % cap;
                uint32_t dist_to_j   = (j - ideal + cap) % cap;
                uint32_t dist_to_gap = (gap - ideal + cap) % cap;
                if (dist_to_gap < dist_to_j) {
                    m->keys[gap]     = m->keys[j];
                    m->values[gap]   = m->values[j];
                    m->occupied[gap] = 1;
                    m->keys[j]      = NULL;
                    m->values[j]    = 0;
                    m->occupied[j]  = 0;
                    gap = j;
                }
                j = (j + 1) % cap;
            }
            return;
        }
        h = (h + 1) % cap;
        probes++;
    }
}

static inline int32_t pgy_map_size_int(PgyHashMap_Int *m)
{
    return (int32_t)m->count;
}

static inline void pgy_map_set_i32_int(PgyHashMap_Int *m, int32_t key, int32_t val)
{
    char *key_str = pgy_map_format_i32_key(key);
    if (key_str == NULL) {
        pgy_runtime_warn_invalid_collection("map_set_i32_int", "key formatting failed");
        return;
    }
    pgy_map_set_int(m, key_str, val);
    free(key_str);
}

static inline int32_t pgy_map_get_i32_int(PgyHashMap_Int *m, int32_t key)
{
    int32_t value = 0;
    char *key_str = pgy_map_format_i32_key(key);
    if (key_str == NULL) {
        pgy_runtime_warn_invalid_collection("map_get_i32_int", "key formatting failed");
        return value;
    }
    value = pgy_map_get_int(m, key_str);
    free(key_str);
    return value;
}

static inline bool pgy_map_has_i32_int(PgyHashMap_Int *m, int32_t key)
{
    bool result = false;
    char *key_str = pgy_map_format_i32_key(key);
    if (key_str == NULL) {
        pgy_runtime_warn_invalid_collection("map_has_i32_int", "key formatting failed");
        return false;
    }
    result = pgy_map_has_int(m, key_str);
    free(key_str);
    return result;
}

static inline void pgy_map_remove_i32_int(PgyHashMap_Int *m, int32_t key)
{
    char *key_str = pgy_map_format_i32_key(key);
    if (key_str == NULL) {
        pgy_runtime_warn_invalid_collection("map_remove_i32_int", "key formatting failed");
        return;
    }
    pgy_map_remove_int(m, key_str);
    free(key_str);
}

static inline void pgy_map_set_i64_int(PgyHashMap_Int *m, int64_t key, int32_t val)
{
    char *key_str = pgy_map_format_i64_key(key);
    if (key_str == NULL) {
        pgy_runtime_warn_invalid_collection("map_set_i64_int", "key formatting failed");
        return;
    }
    pgy_map_set_int(m, key_str, val);
    free(key_str);
}

static inline int32_t pgy_map_get_i64_int(PgyHashMap_Int *m, int64_t key)
{
    int32_t value = 0;
    char *key_str = pgy_map_format_i64_key(key);
    if (key_str == NULL) {
        pgy_runtime_warn_invalid_collection("map_get_i64_int", "key formatting failed");
        return value;
    }
    value = pgy_map_get_int(m, key_str);
    free(key_str);
    return value;
}

static inline bool pgy_map_has_i64_int(PgyHashMap_Int *m, int64_t key)
{
    bool result = false;
    char *key_str = pgy_map_format_i64_key(key);
    if (key_str == NULL) {
        pgy_runtime_warn_invalid_collection("map_has_i64_int", "key formatting failed");
        return false;
    }
    result = pgy_map_has_int(m, key_str);
    free(key_str);
    return result;
}

static inline void pgy_map_remove_i64_int(PgyHashMap_Int *m, int64_t key)
{
    char *key_str = pgy_map_format_i64_key(key);
    if (key_str == NULL) {
        pgy_runtime_warn_invalid_collection("map_remove_i64_int", "key formatting failed");
        return;
    }
    pgy_map_remove_int(m, key_str);
    free(key_str);
}

static inline void pgy_map_set_bool_int(PgyHashMap_Int *m, bool key, int32_t val)
{
    char *key_str = pgy_map_format_bool_key(key);
    if (key_str == NULL) {
        pgy_runtime_warn_invalid_collection("map_set_bool_int", "key formatting failed");
        return;
    }
    pgy_map_set_int(m, key_str, val);
    free(key_str);
}

static inline int32_t pgy_map_get_bool_int(PgyHashMap_Int *m, bool key)
{
    int32_t value = 0;
    char *key_str = pgy_map_format_bool_key(key);
    if (key_str == NULL) {
        pgy_runtime_warn_invalid_collection("map_get_bool_int", "key formatting failed");
        return value;
    }
    value = pgy_map_get_int(m, key_str);
    free(key_str);
    return value;
}

static inline bool pgy_map_has_bool_int(PgyHashMap_Int *m, bool key)
{
    bool result = false;
    char *key_str = pgy_map_format_bool_key(key);
    if (key_str == NULL) {
        pgy_runtime_warn_invalid_collection("map_has_bool_int", "key formatting failed");
        return false;
    }
    result = pgy_map_has_int(m, key_str);
    free(key_str);
    return result;
}

static inline void pgy_map_remove_bool_int(PgyHashMap_Int *m, bool key)
{
    char *key_str = pgy_map_format_bool_key(key);
    if (key_str == NULL) {
        pgy_runtime_warn_invalid_collection("map_remove_bool_int", "key formatting failed");
        return;
    }
    pgy_map_remove_int(m, key_str);
    free(key_str);
}

/* String-value variant */
typedef struct
{
    char   **keys;
    char   **values;
    uint8_t *occupied;
    size_t   count;
    size_t   capacity;
} PgyHashMap_String;

static inline PgyHashMap_String pgy_map_new_string(void)
{
    PgyHashMap_String m;
    m.capacity = PGY_HASHMAP_INIT_CAP;
    m.count = 0;
    m.keys     = (char **)calloc(m.capacity, sizeof(char *));
    m.values   = (char **)calloc(m.capacity, sizeof(char *));
    m.occupied = (uint8_t *)calloc(m.capacity, sizeof(uint8_t));
    if (m.keys == NULL || m.values == NULL || m.occupied == NULL) {
        free(m.keys); free(m.values); free(m.occupied);
        m.keys = NULL; m.values = NULL; m.occupied = NULL; m.capacity = 0;
        pgy_runtime_warn_invalid_collection("map_new_string", "allocation failed");
    }
    return m;
}

static inline void pgy_map_set_string(PgyHashMap_String *m, const char *key, const char *val)
{
    if (m == NULL || m->capacity == 0 || m->keys == NULL || m->values == NULL || m->occupied == NULL) {
        pgy_runtime_warn_invalid_collection("map_set_string", "map is not initialized");
        return;
    }
    if ((double)m->count / (double)m->capacity > PGY_HASHMAP_LOAD_FACTOR) {
        size_t old_cap = m->capacity;
        char **ok = m->keys; char **ov = m->values; uint8_t *oo = m->occupied;
        size_t new_capacity = m->capacity == 0 ? PGY_HASHMAP_INIT_CAP : m->capacity * 2;
        char **new_keys = (char **)calloc(new_capacity, sizeof(char *));
        char **new_values = (char **)calloc(new_capacity, sizeof(char *));
        uint8_t *new_occupied = (uint8_t *)calloc(new_capacity, sizeof(uint8_t));
        if (new_keys == NULL || new_values == NULL || new_occupied == NULL) {
            free(new_keys); free(new_values); free(new_occupied);
            pgy_runtime_warn_invalid_collection("map_set_string", "map growth allocation failed");
            return;
        }
        m->capacity = new_capacity;
        m->keys = new_keys;
        m->values = new_values;
        m->occupied = new_occupied;
        m->count = 0;
        for (size_t i = 0; i < old_cap; i++) {
            if (oo[i]) {
                uint32_t h2 = pgy_hash_string(ok[i]) % (uint32_t)m->capacity;
                while (m->occupied[h2]) h2 = (h2 + 1) % (uint32_t)m->capacity;
                m->keys[h2] = ok[i]; m->values[h2] = ov[i]; m->occupied[h2] = 1; m->count++;
            }
        }
        free(ok); free(ov); free(oo);
    }
    uint32_t h = pgy_hash_string(key) % (uint32_t)m->capacity;
    while (m->occupied[h]) {
        if (m->keys[h] && strcmp(m->keys[h], key) == 0) {
            free(m->values[h]);
            m->values[h] = pgy_runtime_strdup(val);
            if (m->values[h] == NULL) {
                pgy_runtime_warn_invalid_collection("map_set_string", "value duplication failed");
            }
            return;
        }
        h = (h + 1) % (uint32_t)m->capacity;
    }
    m->keys[h] = pgy_runtime_strdup(key); m->values[h] = pgy_runtime_strdup(val);
    if (m->keys[h] == NULL || m->values[h] == NULL) {
        free(m->keys[h]); free(m->values[h]);
        m->keys[h] = NULL; m->values[h] = NULL;
        pgy_runtime_warn_invalid_collection("map_set_string", "key/value duplication failed");
        return;
    }
    m->occupied[h] = 1; m->count++;
}

static inline char *pgy_map_get_string(PgyHashMap_String *m, const char *key)
{
    if (m == NULL || m->capacity == 0 || m->keys == NULL || m->values == NULL || m->occupied == NULL) return "";
    if (m->count == 0) return "";
    uint32_t h = pgy_hash_string(key) % (uint32_t)m->capacity;
    size_t p = 0;
    while (m->occupied[h] && p < m->capacity) {
        if (m->keys[h] && strcmp(m->keys[h], key) == 0)
            return m->values[h] ? m->values[h] : "";
        h = (h + 1) % (uint32_t)m->capacity; p++;
    }
    return "";
}

static inline bool pgy_map_has_string(PgyHashMap_String *m, const char *key)
{
    if (m == NULL || m->capacity == 0 || m->keys == NULL || m->values == NULL || m->occupied == NULL) return false;
    if (m->count == 0) return false;
    uint32_t h = pgy_hash_string(key) % (uint32_t)m->capacity;
    size_t p = 0;
    while (m->occupied[h] && p < m->capacity) {
        if (m->keys[h] && strcmp(m->keys[h], key) == 0) return true;
        h = (h + 1) % (uint32_t)m->capacity; p++;
    }
    return false;
}

static inline void pgy_map_remove_string(PgyHashMap_String *m, const char *key)
{
    if (m == NULL || m->capacity == 0 || m->keys == NULL || m->values == NULL || m->occupied == NULL) return;
    if (m->count == 0) return;
    uint32_t cap = (uint32_t)m->capacity;
