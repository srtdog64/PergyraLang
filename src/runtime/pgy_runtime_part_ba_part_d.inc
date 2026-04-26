
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
