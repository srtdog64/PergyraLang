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
    if (m == NULL || m->capacity == 0 || m->keys == NULL || m->values == NULL || m->occupied == NULL)
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_INTERNAL_INVARIANT, "map get on invalid map");
    if (key == NULL)
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_INTERNAL_INVARIANT, "map get with null key");
    if (m->count == 0)
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_OUT_OF_BOUNDS, "map key not found");
    uint32_t h = pgy_hash_string(key) % (uint32_t)m->capacity;
    size_t p = 0;
    while (m->occupied[h] && p < m->capacity) {
        if (m->keys[h] && strcmp(m->keys[h], key) == 0)
            return m->values[h] ? m->values[h] : "";
        h = (h + 1) % (uint32_t)m->capacity; p++;
    }
    PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_OUT_OF_BOUNDS, "map key not found");
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
    if (m == NULL || m->capacity == 0 || m->keys == NULL || m->values == NULL || m->occupied == NULL)
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_INTERNAL_INVARIANT, "map remove on invalid map");
    if (key == NULL)
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_INTERNAL_INVARIANT, "map remove with null key");
    if (m->count == 0)
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_OUT_OF_BOUNDS, "map remove key not found");
    uint32_t cap = (uint32_t)m->capacity;

    uint32_t h = pgy_hash_string(key) % cap;
    size_t probes = 0;
    while (m->occupied[h] && probes < m->capacity) {
        if (m->keys[h] != NULL && strcmp(m->keys[h], key) == 0) {
            free(m->keys[h]);
            m->keys[h] = NULL;
            free(m->values[h]);
            m->values[h] = NULL;
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
                    m->values[j]    = NULL;
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
    PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_OUT_OF_BOUNDS,
                      "map remove key not found");
}

static inline int32_t pgy_map_size_string(PgyHashMap_String *m)
{
    return m != NULL ? (int32_t)m->count : 0;
}

static inline void pgy_map_set_i32_string(PgyHashMap_String *m, int32_t key, const char *val)
{
    char *key_str = pgy_map_format_i32_key(key);
    if (key_str == NULL) {
        pgy_runtime_warn_invalid_collection("map_set_i32_string", "key formatting failed");
        return;
    }
    pgy_map_set_string(m, key_str, val);
    free(key_str);
}

static inline char *pgy_map_get_i32_string(PgyHashMap_String *m, int32_t key)
{
    char *value = "";
    char *key_str = pgy_map_format_i32_key(key);
    if (key_str == NULL) {
        pgy_runtime_warn_invalid_collection("map_get_i32_string", "key formatting failed");
        return value;
    }
    value = pgy_map_get_string(m, key_str);
    free(key_str);
    return value;
}

static inline bool pgy_map_has_i32_string(PgyHashMap_String *m, int32_t key)
{
    bool result = false;
    char *key_str = pgy_map_format_i32_key(key);
    if (key_str == NULL) {
        pgy_runtime_warn_invalid_collection("map_has_i32_string", "key formatting failed");
        return false;
    }
    result = pgy_map_has_string(m, key_str);
    free(key_str);
    return result;
}

static inline void pgy_map_remove_i32_string(PgyHashMap_String *m, int32_t key)
{
    char *key_str = pgy_map_format_i32_key(key);
    if (key_str == NULL) {
        pgy_runtime_warn_invalid_collection("map_remove_i32_string", "key formatting failed");
        return;
    }
    pgy_map_remove_string(m, key_str);
    free(key_str);
}

static inline void pgy_map_set_i64_string(PgyHashMap_String *m, int64_t key, const char *val)
{
    char *key_str = pgy_map_format_i64_key(key);
    if (key_str == NULL) {
        pgy_runtime_warn_invalid_collection("map_set_i64_string", "key formatting failed");
        return;
    }
    pgy_map_set_string(m, key_str, val);
    free(key_str);
}

static inline char *pgy_map_get_i64_string(PgyHashMap_String *m, int64_t key)
{
    char *value = "";
    char *key_str = pgy_map_format_i64_key(key);
    if (key_str == NULL) {
        pgy_runtime_warn_invalid_collection("map_get_i64_string", "key formatting failed");
        return value;
    }
    value = pgy_map_get_string(m, key_str);
    free(key_str);
    return value;
}

static inline bool pgy_map_has_i64_string(PgyHashMap_String *m, int64_t key)
{
    bool result = false;
    char *key_str = pgy_map_format_i64_key(key);
    if (key_str == NULL) {
        pgy_runtime_warn_invalid_collection("map_has_i64_string", "key formatting failed");
        return false;
    }
    result = pgy_map_has_string(m, key_str);
    free(key_str);
    return result;
}

static inline void pgy_map_remove_i64_string(PgyHashMap_String *m, int64_t key)
{
    char *key_str = pgy_map_format_i64_key(key);
    if (key_str == NULL) {
        pgy_runtime_warn_invalid_collection("map_remove_i64_string", "key formatting failed");
        return;
    }
    pgy_map_remove_string(m, key_str);
    free(key_str);
}

static inline void pgy_map_set_bool_string(PgyHashMap_String *m, bool key, const char *val)
{
    char *key_str = pgy_map_format_bool_key(key);
    if (key_str == NULL) {
        pgy_runtime_warn_invalid_collection("map_set_bool_string", "key formatting failed");
        return;
    }
    pgy_map_set_string(m, key_str, val);
    free(key_str);
}

static inline char *pgy_map_get_bool_string(PgyHashMap_String *m, bool key)
{
    char *value = "";
    char *key_str = pgy_map_format_bool_key(key);
    if (key_str == NULL) {
        pgy_runtime_warn_invalid_collection("map_get_bool_string", "key formatting failed");
        return value;
    }
    value = pgy_map_get_string(m, key_str);
    free(key_str);
    return value;
}

static inline bool pgy_map_has_bool_string(PgyHashMap_String *m, bool key)
{
    bool result = false;
    char *key_str = pgy_map_format_bool_key(key);
    if (key_str == NULL) {
        pgy_runtime_warn_invalid_collection("map_has_bool_string", "key formatting failed");
        return false;
    }
    result = pgy_map_has_string(m, key_str);
    free(key_str);
    return result;
}

static inline void pgy_map_remove_bool_string(PgyHashMap_String *m, bool key)
{
    char *key_str = pgy_map_format_bool_key(key);
    if (key_str == NULL) {
        pgy_runtime_warn_invalid_collection("map_remove_bool_string", "key formatting failed");
        return;
    }
    pgy_map_remove_string(m, key_str);
    free(key_str);
}

#define PGY_DEFINE_MAP_KEYS_EXPORTS(TypeSuffix, FuncSuffix) \
static inline PgyArray_String pgy_map_keys_##FuncSuffix(PgyHashMap_##TypeSuffix *m) \
{ \
    PgyArray_String out = pgy_array_new_String(m != NULL ? m->count : 0); \
    if (m == NULL) { \
        pgy_runtime_warn_invalid_collection("map_keys_" #FuncSuffix, "null map"); \
        return out; \
    } \
    if (m->count == 0 || m->keys == NULL || m->occupied == NULL) \
        return out; \
    for (size_t i = 0; i < m->capacity; i++) { \
        char *dup_key; \
        if (!m->occupied[i] || m->keys[i] == NULL) \
            continue; \
        dup_key = pgy_runtime_strdup(m->keys[i]); \
        if (dup_key == NULL) { \
            pgy_runtime_warn_invalid_collection("map_keys_" #FuncSuffix, "key duplication failed"); \
            continue; \
        } \
        pgy_array_push_String(&out, dup_key); \
    } \
    return out; \
} \
\
static inline PgyArray_Int pgy_map_keys_i32_##FuncSuffix(PgyHashMap_##TypeSuffix *m) \
{ \
    PgyArray_Int out = pgy_array_new_Int(m != NULL ? m->count : 0); \
    if (m == NULL) { \
        pgy_runtime_warn_invalid_collection("map_keys_i32_" #FuncSuffix, "null map"); \
        return out; \
    } \
    if (m->count == 0 || m->keys == NULL || m->occupied == NULL) \
        return out; \
    for (size_t i = 0; i < m->capacity; i++) { \
        char *end = NULL; \
        long parsed; \
        if (!m->occupied[i] || m->keys[i] == NULL) \
            continue; \
        parsed = strtol(m->keys[i], &end, 10); \
        if (end == m->keys[i] || (end != NULL && *end != '\0')) { \
            pgy_runtime_warn_invalid_collection("map_keys_i32_" #FuncSuffix, "invalid stored int key"); \
            continue; \
        } \
        pgy_array_push_Int(&out, (int32_t)parsed); \
    } \
    return out; \
} \
\
static inline PgyArray_Long pgy_map_keys_i64_##FuncSuffix(PgyHashMap_##TypeSuffix *m) \
{ \
    PgyArray_Long out = pgy_array_new_Long(m != NULL ? m->count : 0); \
    if (m == NULL) { \
        pgy_runtime_warn_invalid_collection("map_keys_i64_" #FuncSuffix, "null map"); \
        return out; \
    } \
    if (m->count == 0 || m->keys == NULL || m->occupied == NULL) \
        return out; \
    for (size_t i = 0; i < m->capacity; i++) { \
        char *end = NULL; \
        long long parsed; \
        if (!m->occupied[i] || m->keys[i] == NULL) \
            continue; \
        parsed = strtoll(m->keys[i], &end, 10); \
        if (end == m->keys[i] || (end != NULL && *end != '\0')) { \
            pgy_runtime_warn_invalid_collection("map_keys_i64_" #FuncSuffix, "invalid stored long key"); \
            continue; \
        } \
        pgy_array_push_Long(&out, (int64_t)parsed); \
    } \
    return out; \
} \
\
static inline PgyArray_Bool pgy_map_keys_bool_##FuncSuffix(PgyHashMap_##TypeSuffix *m) \
{ \
    PgyArray_Bool out = pgy_array_new_Bool(m != NULL ? m->count : 0); \
    if (m == NULL) { \
        pgy_runtime_warn_invalid_collection("map_keys_bool_" #FuncSuffix, "null map"); \
        return out; \
    } \
    if (m->count == 0 || m->keys == NULL || m->occupied == NULL) \
        return out; \
    for (size_t i = 0; i < m->capacity; i++) { \
        bool parsed; \
        if (!m->occupied[i] || m->keys[i] == NULL) \
            continue; \
        if (strcmp(m->keys[i], "true") == 0) \
            parsed = true; \
        else if (strcmp(m->keys[i], "false") == 0) \
            parsed = false; \
        else { \
            pgy_runtime_warn_invalid_collection("map_keys_bool_" #FuncSuffix, "invalid stored bool key"); \
            continue; \
        } \
        pgy_array_push_Bool(&out, parsed); \
    } \
    return out; \
}

PGY_DEFINE_MAP_KEYS_EXPORTS(Int, int)
PGY_DEFINE_MAP_KEYS_EXPORTS(String, string)

void pgy_map_keys_raw_export(void *map_ptr, void *out_array_ptr);
void pgy_map_keys_raw_i32_export(void *map_ptr, void *out_array_ptr);
void pgy_map_keys_raw_i64_export(void *map_ptr, void *out_array_ptr);
void pgy_map_keys_raw_bool_export(void *map_ptr, void *out_array_ptr);

/* =================================================================
 * List<Int> — dynamic array (grows automatically)
 * ================================================================= */

#define PGY_LIST_DEFINE(SuffixName, CType) \
typedef struct \
{ \
    CType   *data; \
    size_t   count; \
    size_t   capacity; \
} PgyList_##SuffixName; \
\
static inline PgyList_##SuffixName pgy_list_new_##SuffixName(void) \
{ \
    PgyList_##SuffixName l; \
    l.capacity = 16; \
    l.count = 0; \
    l.data = (CType *)calloc(l.capacity, sizeof(CType)); \
    if (l.data == NULL) { \
        l.capacity = 0; \
        pgy_runtime_warn_invalid_collection("list_new_" #SuffixName, "allocation failed"); \
    } \
    return l; \
} \
\
static inline void pgy_list_push_##SuffixName(PgyList_##SuffixName *l, CType val) \
{ \
    if (l == NULL || (l->data == NULL && l->capacity == 0)) { \
        pgy_runtime_warn_invalid_collection("list_push_" #SuffixName, "list is not initialized"); \
        return; \
    } \
    if (l->count >= l->capacity) { \
        size_t new_capacity = l->capacity == 0 ? 16 : l->capacity * 2; \
        CType *grown = (CType *)realloc(l->data, new_capacity * sizeof(CType)); \
        if (grown == NULL) { \
            pgy_runtime_warn_invalid_collection("list_push_" #SuffixName, "realloc failed"); \
            return; \
        } \
        l->data = grown; \
        l->capacity = new_capacity; \
    } \
    l->data[l->count++] = val; \
} \
\
static inline CType pgy_list_get_##SuffixName(PgyList_##SuffixName *l, int32_t index) \
{ \
    if (l == NULL) \
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_INTERNAL_INVARIANT, "list get on null list"); \
    if (index < 0 || (size_t)index >= l->count) \
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_OUT_OF_BOUNDS, "list index out of bounds"); \
    return l->data[index]; \
} \
\
static inline void pgy_list_set_##SuffixName(PgyList_##SuffixName *l, int32_t index, CType val) \
{ \
    if (l == NULL) \
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_INTERNAL_INVARIANT, "list set on null list"); \
    if (index < 0 || (size_t)index >= l->count) \
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_OUT_OF_BOUNDS, "list set index out of bounds"); \
    l->data[index] = val; \
} \
\
static inline int32_t pgy_list_size_##SuffixName(PgyList_##SuffixName *l) { return (int32_t)l->count; } \
\
static inline void pgy_list_remove_##SuffixName(PgyList_##SuffixName *l, int32_t index) \
{ \
    if (l == NULL) \
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_INTERNAL_INVARIANT, "list remove on null list"); \
    if (index < 0 || (size_t)index >= l->count) \
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_OUT_OF_BOUNDS, "list remove index out of bounds"); \
    for (size_t i = (size_t)index; i < l->count - 1; i++) \
        l->data[i] = l->data[i + 1]; \
    l->count--; \
}

typedef struct
{
    int32_t *data;
    size_t   count;
    size_t   capacity;
} PgyList_Int;

static inline PgyList_Int pgy_list_new_int(void)
{
    PgyList_Int l;
    l.capacity = 16;
    l.count = 0;
    l.data = (int32_t *)calloc(l.capacity, sizeof(int32_t));
    if (l.data == NULL) {
        l.capacity = 0;
        pgy_runtime_warn_invalid_collection("list_new_int", "allocation failed");
    }
    return l;
}

static inline void pgy_list_push_int(PgyList_Int *l, int32_t val)
{
    if (l == NULL || (l->data == NULL && l->capacity == 0)) {
        pgy_runtime_warn_invalid_collection("list_push_int", "list is not initialized");
        return;
    }
    if (l->count >= l->capacity) {
        size_t new_capacity = l->capacity == 0 ? 16 : l->capacity * 2;
        int32_t *grown = (int32_t *)realloc(l->data, new_capacity * sizeof(int32_t));
        if (grown == NULL) {
            pgy_runtime_warn_invalid_collection("list_push_int", "realloc failed");
            return;
        }
        l->data = grown;
        l->capacity = new_capacity;
    }
    l->data[l->count++] = val;
}

static inline int32_t pgy_list_get_int(PgyList_Int *l, int32_t index)
{
    if (l == NULL)
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_INTERNAL_INVARIANT, "list get on null list");
    if (index < 0 || (size_t)index >= l->count)
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_OUT_OF_BOUNDS, "list index out of bounds");
    return l->data[index];
}

static inline void pgy_list_set_int(PgyList_Int *l, int32_t index, int32_t val)
{
    if (l == NULL)
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_INTERNAL_INVARIANT,
                          "list set on null list");
    if (index < 0 || (size_t)index >= l->count)
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_OUT_OF_BOUNDS,
                          "list set index out of bounds");
    l->data[index] = val;
}

static inline int32_t pgy_list_size_int(PgyList_Int *l) { return (int32_t)l->count; }

static inline void pgy_list_remove_int(PgyList_Int *l, int32_t index)
{
    if (l == NULL)
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_INTERNAL_INVARIANT,
                          "list remove on null list");
    if (index < 0 || (size_t)index >= l->count)
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_OUT_OF_BOUNDS,
                          "list remove index out of bounds");
    for (size_t i = (size_t)index; i < l->count - 1; i++)
        l->data[i] = l->data[i + 1];
    l->count--;
}

/* String variant */
typedef struct
{
    char   **data;
    size_t   count;
    size_t   capacity;
} PgyList_String;

static inline PgyList_String pgy_list_new_string(void)
{
    PgyList_String l;
    l.capacity = 16;
    l.count = 0;
    l.data = (char **)calloc(l.capacity, sizeof(char *));
    if (l.data == NULL) {
        l.capacity = 0;
        pgy_runtime_warn_invalid_collection("list_new_string", "allocation failed");
    }
    return l;
}

static inline void pgy_list_push_string(PgyList_String *l, const char *val)
{
    if (l == NULL || (l->data == NULL && l->capacity == 0)) {
        pgy_runtime_warn_invalid_collection("list_push_string", "list is not initialized");
        return;
    }
    if (l->count >= l->capacity) {
        size_t new_capacity = l->capacity == 0 ? 16 : l->capacity * 2;
        char **grown = (char **)realloc(l->data, new_capacity * sizeof(char *));
        if (grown == NULL) {
            pgy_runtime_warn_invalid_collection("list_push_string", "realloc failed");
            return;
        }
        l->data = grown;
        l->capacity = new_capacity;
    }
    l->data[l->count] = pgy_runtime_strdup(val ? val : "");
    if (l->data[l->count] == NULL) {
        pgy_runtime_warn_invalid_collection("list_push_string", "string duplication failed");
        return;
    }
    l->count++;
}

static inline char *pgy_list_get_string(PgyList_String *l, int32_t index)
{
    if (l == NULL)
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_INTERNAL_INVARIANT, "list get on null list");
    if (index < 0 || (size_t)index >= l->count)
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_OUT_OF_BOUNDS, "list index out of bounds");
    return l->data[index] ? l->data[index] : "";
}

static inline int32_t pgy_list_size_string(PgyList_String *l) { return (int32_t)l->count; }

/* =================================================================
 * Set<String> — hash set (string keys)
 * ================================================================= */

typedef struct
{
    char   **keys;
    uint8_t *occupied;
    size_t   count;
    size_t   capacity;
} PgySet_String;

static inline PgySet_String pgy_set_new_string(void)
{
    PgySet_String s;
    s.capacity = 16;
    s.count = 0;
    s.keys = (char **)calloc(s.capacity, sizeof(char *));
    s.occupied = (uint8_t *)calloc(s.capacity, sizeof(uint8_t));
    if (s.keys == NULL || s.occupied == NULL) {
        free(s.keys); free(s.occupied);
        s.keys = NULL; s.occupied = NULL; s.capacity = 0;
        pgy_runtime_warn_invalid_collection("set_new_string", "allocation failed");
    }
    return s;
}

static inline bool pgy_set_has_string(PgySet_String *s, const char *key)
{
    if (s == NULL || s->capacity == 0 || s->keys == NULL || s->occupied == NULL) return false;
    if (s->count == 0 || key == NULL) return false;
    uint32_t h = pgy_hash_string(key) % (uint32_t)s->capacity;
    size_t p = 0;
    while (s->occupied[h] && p < s->capacity) {
        if (s->keys[h] && strcmp(s->keys[h], key) == 0) return true;
        h = (h + 1) % (uint32_t)s->capacity; p++;
    }
    return false;
}

static inline void pgy_set_add_string(PgySet_String *s, const char *key)
{
    if (s == NULL || s->capacity == 0 || s->keys == NULL || s->occupied == NULL) {
        pgy_runtime_warn_invalid_collection("set_add_string", "set is not initialized");
        return;
    }
    if (pgy_set_has_string(s, key)) return;
    if ((double)s->count / (double)s->capacity > 0.75) {
        size_t oc = s->capacity; char **ok = s->keys; uint8_t *oo = s->occupied;
        size_t new_capacity = s->capacity == 0 ? 16 : s->capacity * 2;
        char **new_keys = (char **)calloc(new_capacity, sizeof(char *));
        uint8_t *new_occupied = (uint8_t *)calloc(new_capacity, sizeof(uint8_t));
        if (new_keys == NULL || new_occupied == NULL) {
            free(new_keys); free(new_occupied);
            pgy_runtime_warn_invalid_collection("set_add_string", "growth allocation failed");
            return;
        }
        s->capacity = new_capacity;
        s->keys = new_keys;
        s->occupied = new_occupied;
        s->count = 0;
        for (size_t i = 0; i < oc; i++) {
            if (oo[i]) { pgy_set_add_string(s, ok[i]); free(ok[i]); }
        }
        free(ok); free(oo);
    }
    uint32_t h = pgy_hash_string(key) % (uint32_t)s->capacity;
    while (s->occupied[h]) h = (h + 1) % (uint32_t)s->capacity;
    s->keys[h] = pgy_runtime_strdup(key);
    if (s->keys[h] == NULL) {
        pgy_runtime_warn_invalid_collection("set_add_string", "string duplication failed");
        return;
    }
    s->occupied[h] = 1; s->count++;
}

static inline void pgy_set_remove_string(PgySet_String *s, const char *key)
{
    if (s == NULL || s->capacity == 0 || s->keys == NULL || s->occupied == NULL) return;
    if (s->count == 0 || key == NULL) return;
    uint32_t h = pgy_hash_string(key) % (uint32_t)s->capacity;
    size_t p = 0;
    while (s->occupied[h] && p < s->capacity) {
        if (s->keys[h] && strcmp(s->keys[h], key) == 0) {
            free(s->keys[h]); s->keys[h] = NULL; s->occupied[h] = 0; s->count--;
            return;
        }
        h = (h + 1) % (uint32_t)s->capacity; p++;
    }
}

static inline int32_t pgy_set_size_string(PgySet_String *s) { return (int32_t)s->count; }

/* =================================================================
 * Set<T> — Generic hash set macro (value-based, no string conversion)
 *
 * Uses FNV-1a hash on raw bytes for non-string types.
 * For String type, use PgySet_String above (strcmp-based).
 * ================================================================= */

#define PGY_SET_DEFINE(SuffixName, CType) \
typedef struct \
{ \
    CType   *data; \
    uint8_t *occupied; \
    size_t   count; \
    size_t   capacity; \
} PgySet_##SuffixName; \
\
static inline uint32_t pgy_set_hash_##SuffixName(CType val) \
{ \
    const uint8_t *p = (const uint8_t *)&val; \
    uint32_t h = 2166136261u; \
    for (size_t i = 0; i < sizeof(CType); i++) { h ^= p[i]; h *= 16777619u; } \
    return h; \
} \
\
static inline PgySet_##SuffixName pgy_set_new_##SuffixName(void) \
{ \
    PgySet_##SuffixName s; \
    s.capacity = 16; s.count = 0; \
    s.data = (CType *)calloc(s.capacity, sizeof(CType)); \
    s.occupied = (uint8_t *)calloc(s.capacity, sizeof(uint8_t)); \
    if (s.data == NULL || s.occupied == NULL) { \
        free(s.data); free(s.occupied); \
        s.data = NULL; s.occupied = NULL; s.capacity = 0; \
        pgy_runtime_warn_invalid_collection("set_new_" #SuffixName, "allocation failed"); \
    } \
    return s; \
} \
\
static inline bool pgy_set_has_##SuffixName(PgySet_##SuffixName *s, CType val) \
{ \
    if (s == NULL || s->capacity == 0 || s->data == NULL || s->occupied == NULL) return false; \
    if (s->count == 0) return false; \
    uint32_t h = pgy_set_hash_##SuffixName(val) % (uint32_t)s->capacity; \
    size_t p = 0; \
    while (s->occupied[h] && p < s->capacity) { \
        if (memcmp(&s->data[h], &val, sizeof(CType)) == 0) return true; \
        h = (h + 1) % (uint32_t)s->capacity; p++; \
    } \
    return false; \
} \
\
static inline void pgy_set_add_##SuffixName(PgySet_##SuffixName *s, CType val) \
{ \
    if (s == NULL || s->capacity == 0 || s->data == NULL || s->occupied == NULL) { \
        pgy_runtime_warn_invalid_collection("set_add_" #SuffixName, "set is not initialized"); \
        return; \
    } \
    if (pgy_set_has_##SuffixName(s, val)) return; \
    if ((double)s->count / (double)s->capacity > 0.75) { \
        size_t oc = s->capacity; CType *od = s->data; uint8_t *oo = s->occupied; \
        size_t nc = s->capacity == 0 ? 16 : s->capacity * 2; \
        CType *nd = (CType *)calloc(nc, sizeof(CType)); \
        uint8_t *no = (uint8_t *)calloc(nc, sizeof(uint8_t)); \
        if (nd == NULL || no == NULL) { \
            free(nd); free(no); \
            pgy_runtime_warn_invalid_collection("set_add_" #SuffixName, "rehash allocation failed"); \
            return; \
        } \
        s->capacity = nc; \
        s->data = nd; \
        s->occupied = no; \
        s->count = 0; \
        for (size_t i = 0; i < oc; i++) { if (oo[i]) pgy_set_add_##SuffixName(s, od[i]); } \
        free(od); free(oo); \
    } \
    uint32_t h = pgy_set_hash_##SuffixName(val) % (uint32_t)s->capacity; \
    while (s->occupied[h]) h = (h + 1) % (uint32_t)s->capacity; \
    s->data[h] = val; s->occupied[h] = 1; s->count++; \
} \
\
static inline void pgy_set_remove_##SuffixName(PgySet_##SuffixName *s, CType val) \
{ \
    if (s->count == 0) return; \
    uint32_t h = pgy_set_hash_##SuffixName(val) % (uint32_t)s->capacity; \
    size_t p = 0; \
    while (s->occupied[h] && p < s->capacity) { \
        if (memcmp(&s->data[h], &val, sizeof(CType)) == 0) { \
            memset(&s->data[h], 0, sizeof(CType)); \
            s->occupied[h] = 0; s->count--; return; \
        } \
        h = (h + 1) % (uint32_t)s->capacity; p++; \
    } \
} \
\
static inline int32_t pgy_set_size_##SuffixName(PgySet_##SuffixName *s) \
{ return (int32_t)s->count; }

/* Pre-instantiate Set<Int> (lowercase suffix to match collection_runtime_suffix) */
PGY_SET_DEFINE(int, int32_t)

/* =================================================================
 * Queue<Int> — ring buffer FIFO
 * ================================================================= */
