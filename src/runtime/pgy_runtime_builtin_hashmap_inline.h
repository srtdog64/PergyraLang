/* =================================================================
 * HashMap<String, T> / HashMap<Int, T> / HashMap<Long, T> /
 * HashMap<Bool, T> -- stable key subset over open-addressing
 * ================================================================= */

#define PGY_HASHMAP_INIT_CAP 16
#define PGY_HASHMAP_LOAD_FACTOR 0.75

#ifndef PGY_RUNTIME_HASHMAP_CAPACITY_FITS
#define PGY_RUNTIME_HASHMAP_CAPACITY_FITS(capacity, CType) \
    ((capacity) != 0 \
        && (capacity) <= UINT32_MAX \
        && (capacity) <= SIZE_MAX / sizeof(char *) \
        && (capacity) <= SIZE_MAX / sizeof(CType) \
        && (capacity) <= SIZE_MAX / sizeof(uint8_t))
#endif

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
    if (!PGY_RUNTIME_HASHMAP_CAPACITY_FITS(m.capacity, CType)) { \
        m.keys = NULL; m.values = NULL; m.occupied = NULL; \
        m.capacity = 0; \
        pgy_runtime_warn_invalid_collection("map_new_" #SuffixName, "allocation size overflow"); \
        return m; \
    } \
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
    size_t new_capacity; \
    char **new_keys; \
    CType *new_values; \
    uint8_t *new_occupied; \
    if (m->capacity == 0) { \
        new_capacity = PGY_HASHMAP_INIT_CAP; \
    } else { \
        if (m->capacity > SIZE_MAX / 2) { \
            pgy_runtime_warn_invalid_collection("map_grow_" #SuffixName, "capacity overflow"); \
            return; \
        } \
        new_capacity = m->capacity * 2; \
    } \
    if (!PGY_RUNTIME_HASHMAP_CAPACITY_FITS(new_capacity, CType)) { \
        pgy_runtime_warn_invalid_collection("map_grow_" #SuffixName, "allocation size overflow"); \
        return; \
    } \
    new_keys = (char **)calloc(new_capacity, sizeof(char *)); \
    new_values = (CType *)calloc(new_capacity, sizeof(CType)); \
    new_occupied = (uint8_t *)calloc(new_capacity, sizeof(uint8_t)); \
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
    if (m == NULL || m->capacity == 0 || m->keys == NULL || m->values == NULL || m->occupied == NULL) \
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_INTERNAL_INVARIANT, "map get on invalid map"); \
    if (key == NULL) \
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_INTERNAL_INVARIANT, "map get with null key"); \
    if (m->count == 0) \
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_OUT_OF_BOUNDS, "map key not found"); \
    uint32_t h = pgy_hash_string(key) % (uint32_t)m->capacity; \
    size_t probes = 0; \
    while (m->occupied[h] && probes < m->capacity) { \
        if (m->keys[h] != NULL && strcmp(m->keys[h], key) == 0) \
            return m->values[h]; \
        h = (h + 1) % (uint32_t)m->capacity; \
        probes++; \
    } \
    PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_OUT_OF_BOUNDS, "map key not found"); \
    { CType zero_value; memset(&zero_value, 0, sizeof(zero_value)); return zero_value; } \
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
    if (m == NULL || m->capacity == 0 || m->keys == NULL || m->values == NULL || m->occupied == NULL) \
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_INTERNAL_INVARIANT, "map remove on invalid map"); \
    if (key == NULL) \
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_INTERNAL_INVARIANT, "map remove with null key"); \
    if (m->count == 0) \
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_OUT_OF_BOUNDS, "map remove key not found"); \
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
    PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_OUT_OF_BOUNDS, "map remove key not found"); \
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
