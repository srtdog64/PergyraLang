
#include <stdint.h>

/* =================================================================
 * List<Int> — dynamic array (grows automatically)
 * ================================================================= */

#ifndef PGY_RUNTIME_ELEM_CAPACITY_FITS
#define PGY_RUNTIME_ELEM_CAPACITY_FITS(capacity, CType) \
    ((capacity) != 0 \
        && (capacity) <= (size_t)INT32_MAX \
        && (capacity) <= SIZE_MAX / sizeof(CType))
#endif

#ifndef PGY_RUNTIME_LIST_IS_INITIALIZED
#define PGY_RUNTIME_LIST_IS_INITIALIZED(list, CType) \
    ((list) != NULL \
        && PGY_RUNTIME_ELEM_CAPACITY_FITS((list)->capacity, CType) \
        && (list)->data != NULL)
#endif

#define PGY_SET_INLINE_EMPTY 0u
#define PGY_SET_INLINE_LIVE 1u
#define PGY_SET_INLINE_DELETED 2u

#ifndef PGY_RUNTIME_HASH_CAPACITY_FITS
#define PGY_RUNTIME_HASH_CAPACITY_FITS(capacity) \
    ((capacity) != 0 && (capacity) <= (size_t)INT32_MAX)
#endif

#ifndef PGY_RUNTIME_SET_IS_INITIALIZED
#define PGY_RUNTIME_SET_IS_INITIALIZED(set, CType) \
    ((set) != NULL \
        && PGY_RUNTIME_HASH_CAPACITY_FITS((set)->capacity) \
        && PGY_RUNTIME_ELEM_CAPACITY_FITS((set)->capacity, CType) \
        && (set)->capacity <= SIZE_MAX / sizeof(uint8_t) \
        && (set)->data != NULL \
        && (set)->occupied != NULL)
#endif

#ifndef PGY_RUNTIME_STRING_SET_IS_INITIALIZED
#define PGY_RUNTIME_STRING_SET_IS_INITIALIZED(set) \
    ((set) != NULL \
        && PGY_RUNTIME_HASH_CAPACITY_FITS((set)->capacity) \
        && PGY_RUNTIME_ELEM_CAPACITY_FITS((set)->capacity, char *) \
        && (set)->capacity <= SIZE_MAX / sizeof(uint8_t) \
        && (set)->keys != NULL \
        && (set)->occupied != NULL)
#endif

#include "pgy_runtime_list_generic_inline.h"

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
    if (!PGY_RUNTIME_ELEM_CAPACITY_FITS(l.capacity, int32_t)) {
        l.data = NULL;
        l.capacity = 0;
        pgy_runtime_warn_invalid_collection("list_new_int", "allocation size overflow");
        return l;
    }
    l.data = (int32_t *)calloc(l.capacity, sizeof(int32_t));
    if (l.data == NULL) {
        l.capacity = 0;
        pgy_runtime_warn_invalid_collection("list_new_int", "allocation failed");
    }
    return l;
}

static inline void pgy_list_push_int(PgyList_Int *l, int32_t val)
{
    if (!PGY_RUNTIME_LIST_IS_INITIALIZED(l, int32_t)) {
        pgy_runtime_warn_invalid_collection("list_push_int", "list is not initialized");
        return;
    }
    if (l->count >= l->capacity) {
        size_t new_capacity;
        int32_t *grown;
        if (l->capacity == 0) {
            new_capacity = 16;
        } else {
            if (l->capacity > SIZE_MAX / 2) {
                pgy_runtime_warn_invalid_collection("list_push_int", "capacity overflow");
                return;
            }
            new_capacity = l->capacity * 2;
        }
        if (!PGY_RUNTIME_ELEM_CAPACITY_FITS(new_capacity, int32_t)) {
            pgy_runtime_warn_invalid_collection("list_push_int", "allocation size overflow");
            return;
        }
        grown = (int32_t *)realloc(l->data, new_capacity * sizeof(int32_t));
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
    if (!PGY_RUNTIME_LIST_IS_INITIALIZED(l, int32_t))
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_INTERNAL_INVARIANT, "list get on invalid list");
    if (index < 0 || (size_t)index >= l->count)
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_OUT_OF_BOUNDS, "list index out of bounds");
    return l->data[index];
}

static inline void pgy_list_set_int(PgyList_Int *l, int32_t index, int32_t val)
{
    if (!PGY_RUNTIME_LIST_IS_INITIALIZED(l, int32_t))
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_INTERNAL_INVARIANT,
                          "list set on invalid list");
    if (index < 0 || (size_t)index >= l->count)
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_OUT_OF_BOUNDS,
                          "list set index out of bounds");
    l->data[index] = val;
}

static inline int32_t pgy_list_size_int(PgyList_Int *l)
{
    return PGY_RUNTIME_LIST_IS_INITIALIZED(l, int32_t) ? (int32_t)l->count : 0;
}

static inline void pgy_list_remove_int(PgyList_Int *l, int32_t index)
{
    if (!PGY_RUNTIME_LIST_IS_INITIALIZED(l, int32_t))
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_INTERNAL_INVARIANT,
                          "list remove on invalid list");
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
    if (!PGY_RUNTIME_ELEM_CAPACITY_FITS(l.capacity, char *)) {
        l.data = NULL;
        l.capacity = 0;
        pgy_runtime_warn_invalid_collection("list_new_string", "allocation size overflow");
        return l;
    }
    l.data = (char **)calloc(l.capacity, sizeof(char *));
    if (l.data == NULL) {
        l.capacity = 0;
        pgy_runtime_warn_invalid_collection("list_new_string", "allocation failed");
    }
    return l;
}

static inline void pgy_list_push_string(PgyList_String *l, const char *val)
{
    if (!PGY_RUNTIME_LIST_IS_INITIALIZED(l, char *)) {
        pgy_runtime_warn_invalid_collection("list_push_string", "list is not initialized");
        return;
    }
    if (l->count >= l->capacity) {
        size_t new_capacity;
        char **grown;
        if (l->capacity == 0) {
            new_capacity = 16;
        } else {
            if (l->capacity > SIZE_MAX / 2) {
                pgy_runtime_warn_invalid_collection("list_push_string", "capacity overflow");
                return;
            }
            new_capacity = l->capacity * 2;
        }
        if (!PGY_RUNTIME_ELEM_CAPACITY_FITS(new_capacity, char *)) {
            pgy_runtime_warn_invalid_collection("list_push_string", "allocation size overflow");
            return;
        }
        grown = (char **)realloc(l->data, new_capacity * sizeof(char *));
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
    if (!PGY_RUNTIME_LIST_IS_INITIALIZED(l, char *))
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_INTERNAL_INVARIANT, "list get on invalid list");
    if (index < 0 || (size_t)index >= l->count)
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_OUT_OF_BOUNDS, "list index out of bounds");
    return l->data[index] ? l->data[index] : "";
}

static inline void pgy_list_set_string(PgyList_String *l, int32_t index, const char *val)
{
    char *owned;

    if (!PGY_RUNTIME_LIST_IS_INITIALIZED(l, char *))
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_INTERNAL_INVARIANT, "list set on invalid list");
    if (index < 0 || (size_t)index >= l->count)
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_OUT_OF_BOUNDS, "list set index out of bounds");
    owned = pgy_runtime_strdup(val ? val : "");
    if (owned == NULL)
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_OOM, "list set string duplication failed");
    free(l->data[index]);
    l->data[index] = owned;
}

static inline void pgy_list_remove_string(PgyList_String *l, int32_t index)
{
    size_t tail_count;

    if (!PGY_RUNTIME_LIST_IS_INITIALIZED(l, char *))
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_INTERNAL_INVARIANT, "list remove on invalid list");
    if (index < 0 || (size_t)index >= l->count)
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_OUT_OF_BOUNDS, "list remove index out of bounds");
    free(l->data[index]);
    l->data[index] = NULL;
    tail_count = l->count - (size_t)index - 1;
    if (tail_count > 0) {
        memmove(&l->data[index], &l->data[index + 1], tail_count * sizeof(char *));
        l->data[l->count - 1] = NULL;
    }
    l->count--;
}

static inline int32_t pgy_list_size_string(PgyList_String *l)
{
    return PGY_RUNTIME_LIST_IS_INITIALIZED(l, char *) ? (int32_t)l->count : 0;
}

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
    if (!PGY_RUNTIME_HASH_CAPACITY_FITS(s.capacity)
        || !PGY_RUNTIME_ELEM_CAPACITY_FITS(s.capacity, char *)
        || s.capacity > SIZE_MAX / sizeof(uint8_t)) {
        s.keys = NULL; s.occupied = NULL; s.capacity = 0;
        pgy_runtime_warn_invalid_collection("set_new_string", "allocation size overflow");
        return s;
    }
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
    if (!PGY_RUNTIME_STRING_SET_IS_INITIALIZED(s)) return false;
    if (s->count == 0 || key == NULL) return false;
    uint32_t h = pgy_hash_string(key) % (uint32_t)s->capacity;
    size_t p = 0;
    while (s->occupied[h] && p < s->capacity) {
        if (s->occupied[h] == PGY_SET_INLINE_LIVE
            && s->keys[h] && strcmp(s->keys[h], key) == 0) return true;
        h = (h + 1) % (uint32_t)s->capacity; p++;
    }
    return false;
}

static inline void pgy_set_add_string(PgySet_String *s, const char *key)
{
    if (!PGY_RUNTIME_STRING_SET_IS_INITIALIZED(s)) {
        pgy_runtime_warn_invalid_collection("set_add_string", "set is not initialized");
        return;
    }
    if (key == NULL) {
        pgy_runtime_warn_invalid_collection("set_add_string", "null key");
        return;
    }
    if (pgy_set_has_string(s, key)) return;
    if ((double)s->count / (double)s->capacity > 0.75) {
        size_t oc = s->capacity; char **ok = s->keys; uint8_t *oo = s->occupied;
        size_t new_capacity;
        char **new_keys;
        uint8_t *new_occupied;
        if (s->capacity == 0) {
            new_capacity = 16;
        } else {
            if (s->capacity > SIZE_MAX / 2) {
                pgy_runtime_warn_invalid_collection("set_add_string", "capacity overflow");
                return;
            }
            new_capacity = s->capacity * 2;
        }
        if (!PGY_RUNTIME_HASH_CAPACITY_FITS(new_capacity)
            || !PGY_RUNTIME_ELEM_CAPACITY_FITS(new_capacity, char *)
            || new_capacity > SIZE_MAX / sizeof(uint8_t)) {
            pgy_runtime_warn_invalid_collection("set_add_string", "allocation size overflow");
            return;
        }
        new_keys = (char **)calloc(new_capacity, sizeof(char *));
        new_occupied = (uint8_t *)calloc(new_capacity, sizeof(uint8_t));
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
            if (oo[i] == PGY_SET_INLINE_LIVE) { pgy_set_add_string(s, ok[i]); free(ok[i]); }
        }
        free(ok); free(oo);
    }
    uint32_t h = pgy_hash_string(key) % (uint32_t)s->capacity;
    uint32_t first_deleted = UINT32_MAX;
    size_t p = 0;
    while (s->occupied[h] && p < s->capacity) {
        if (s->occupied[h] == PGY_SET_INLINE_DELETED && first_deleted == UINT32_MAX) first_deleted = h;
        h = (h + 1) % (uint32_t)s->capacity; p++;
    }
    if (first_deleted != UINT32_MAX) h = first_deleted;
    s->keys[h] = pgy_runtime_strdup(key);
    if (s->keys[h] == NULL) {
        pgy_runtime_warn_invalid_collection("set_add_string", "string duplication failed");
        return;
    }
    s->occupied[h] = PGY_SET_INLINE_LIVE; s->count++;
}

static inline void pgy_set_remove_string(PgySet_String *s, const char *key)
{
    if (!PGY_RUNTIME_STRING_SET_IS_INITIALIZED(s)) return;
    if (s->count == 0 || key == NULL) return;
    uint32_t h = pgy_hash_string(key) % (uint32_t)s->capacity;
    size_t p = 0;
    while (s->occupied[h] && p < s->capacity) {
        if (s->occupied[h] == PGY_SET_INLINE_LIVE
            && s->keys[h] && strcmp(s->keys[h], key) == 0) {
            free(s->keys[h]); s->keys[h] = NULL; s->occupied[h] = PGY_SET_INLINE_DELETED; s->count--;
            return;
        }
        h = (h + 1) % (uint32_t)s->capacity; p++;
    }
}

static inline int32_t pgy_set_size_string(PgySet_String *s)
{
    return PGY_RUNTIME_STRING_SET_IS_INITIALIZED(s) ? (int32_t)s->count : 0;
}

static inline PgyArray_String pgy_set_values_string(PgySet_String *s)
{
    PgyArray_String out = pgy_array_new_String(s != NULL ? s->count : 0);

    if (s == NULL) {
        pgy_runtime_warn_invalid_collection("set_values_string", "null set");
        return out;
    }
    if (!PGY_RUNTIME_STRING_SET_IS_INITIALIZED(s) || s->count == 0)
        return out;
    for (size_t i = 0; i < s->capacity; i++) {
        char *dup_value;
        if (s->occupied[i] != PGY_SET_INLINE_LIVE || s->keys[i] == NULL)
            continue;
        dup_value = pgy_runtime_strdup(s->keys[i]);
        if (dup_value == NULL) {
            pgy_runtime_warn_invalid_collection("set_values_string",
                "value duplication failed");
            continue;
        }
        pgy_array_push_String(&out, dup_value);
    }
    pgy_array_sort_String(out.data, out.length);
    return out;
}

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
    if (!PGY_RUNTIME_HASH_CAPACITY_FITS(s.capacity) \
        || !PGY_RUNTIME_ELEM_CAPACITY_FITS(s.capacity, CType) \
        || s.capacity > SIZE_MAX / sizeof(uint8_t)) { \
        s.data = NULL; s.occupied = NULL; s.capacity = 0; \
        pgy_runtime_warn_invalid_collection("set_new_" #SuffixName, "allocation size overflow"); \
        return s; \
    } \
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
    if (!PGY_RUNTIME_SET_IS_INITIALIZED(s, CType)) return false; \
    if (s->count == 0) return false; \
    uint32_t h = pgy_set_hash_##SuffixName(val) % (uint32_t)s->capacity; \
    size_t p = 0; \
    while (s->occupied[h] && p < s->capacity) { \
        if (s->occupied[h] == PGY_SET_INLINE_LIVE \
            && memcmp(&s->data[h], &val, sizeof(CType)) == 0) return true; \
        h = (h + 1) % (uint32_t)s->capacity; p++; \
    } \
    return false; \
} \
\
static inline void pgy_set_add_##SuffixName(PgySet_##SuffixName *s, CType val) \
{ \
    if (!PGY_RUNTIME_SET_IS_INITIALIZED(s, CType)) { \
        pgy_runtime_warn_invalid_collection("set_add_" #SuffixName, "set is not initialized"); \
        return; \
    } \
    if (pgy_set_has_##SuffixName(s, val)) return; \
    if ((double)s->count / (double)s->capacity > 0.75) { \
        size_t oc = s->capacity; CType *od = s->data; uint8_t *oo = s->occupied; \
        size_t nc; \
        CType *nd; \
        uint8_t *no; \
        if (s->capacity == 0) { \
            nc = 16; \
        } else { \
            if (s->capacity > SIZE_MAX / 2) { \
                pgy_runtime_warn_invalid_collection("set_add_" #SuffixName, "capacity overflow"); \
                return; \
            } \
            nc = s->capacity * 2; \
        } \
        if (!PGY_RUNTIME_HASH_CAPACITY_FITS(nc) \
            || !PGY_RUNTIME_ELEM_CAPACITY_FITS(nc, CType) \
            || nc > SIZE_MAX / sizeof(uint8_t)) { \
            pgy_runtime_warn_invalid_collection("set_add_" #SuffixName, "allocation size overflow"); \
            return; \
        } \
        nd = (CType *)calloc(nc, sizeof(CType)); \
        no = (uint8_t *)calloc(nc, sizeof(uint8_t)); \
        if (nd == NULL || no == NULL) { \
            free(nd); free(no); \
            pgy_runtime_warn_invalid_collection("set_add_" #SuffixName, "rehash allocation failed"); \
            return; \
        } \
        s->capacity = nc; \
        s->data = nd; \
        s->occupied = no; \
        s->count = 0; \
        for (size_t i = 0; i < oc; i++) { if (oo[i] == PGY_SET_INLINE_LIVE) pgy_set_add_##SuffixName(s, od[i]); } \
        free(od); free(oo); \
    } \
    uint32_t h = pgy_set_hash_##SuffixName(val) % (uint32_t)s->capacity; \
    uint32_t first_deleted = UINT32_MAX; \
    size_t p = 0; \
    while (s->occupied[h] && p < s->capacity) { \
        if (s->occupied[h] == PGY_SET_INLINE_DELETED && first_deleted == UINT32_MAX) first_deleted = h; \
        h = (h + 1) % (uint32_t)s->capacity; p++; \
    } \
    if (first_deleted != UINT32_MAX) h = first_deleted; \
    s->data[h] = val; s->occupied[h] = PGY_SET_INLINE_LIVE; s->count++; \
} \
\
static inline void pgy_set_remove_##SuffixName(PgySet_##SuffixName *s, CType val) \
{ \
    if (!PGY_RUNTIME_SET_IS_INITIALIZED(s, CType)) return; \
    if (s->count == 0) return; \
    uint32_t h = pgy_set_hash_##SuffixName(val) % (uint32_t)s->capacity; \
    size_t p = 0; \
    while (s->occupied[h] && p < s->capacity) { \
        if (s->occupied[h] == PGY_SET_INLINE_LIVE \
            && memcmp(&s->data[h], &val, sizeof(CType)) == 0) { \
            memset(&s->data[h], 0, sizeof(CType)); \
            s->occupied[h] = PGY_SET_INLINE_DELETED; s->count--; return; \
        } \
        h = (h + 1) % (uint32_t)s->capacity; p++; \
    } \
} \
\
static inline int32_t pgy_set_size_##SuffixName(PgySet_##SuffixName *s) \
{ return PGY_RUNTIME_SET_IS_INITIALIZED(s, CType) ? (int32_t)s->count : 0; }

#define PGY_SET_VALUES_DEFINE(SetSuffixName, CType, ArraySuffixName) \
static inline PgyArray_##ArraySuffixName pgy_set_values_##SetSuffixName(PgySet_##SetSuffixName *s) \
{ \
    PgyArray_##ArraySuffixName out = pgy_array_new_##ArraySuffixName(s != NULL ? s->count : 0); \
    if (s == NULL) { \
        pgy_runtime_warn_invalid_collection("set_values_" #SetSuffixName, "null set"); \
        return out; \
    } \
    if (!PGY_RUNTIME_SET_IS_INITIALIZED(s, CType) || s->count == 0) \
        return out; \
    for (size_t i = 0; i < s->capacity; i++) { \
        if (s->occupied[i] != PGY_SET_INLINE_LIVE) \
            continue; \
        pgy_array_push_##ArraySuffixName(&out, s->data[i]); \
    } \
    pgy_array_sort_##ArraySuffixName(out.data, out.length); \
    return out; \
}

/* Pre-instantiate Set<Int> (lowercase suffix to match collection_runtime_suffix) */
PGY_SET_DEFINE(int, int32_t)
PGY_SET_VALUES_DEFINE(int, int32_t, Int)

/* =================================================================
 * Queue<Int> — ring buffer FIFO
 * ================================================================= */
