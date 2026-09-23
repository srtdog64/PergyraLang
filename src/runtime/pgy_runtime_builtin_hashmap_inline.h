#include "pgy_runtime_linkage.h"
/* =================================================================
 * HashMap<String, T> / HashMap<Int, T> / HashMap<Long, T> /
 * HashMap<Bool, T> -- stable key subset over open-addressing
 * ================================================================= */

/* Growable runtime storage is not a synchronization boundary.
 * The semantic layer must reject raw Array/Slice/List/Queue/Set/HashMap
 * transport across parallel/async/worker boundaries unless an explicit copy or
 * pinned read-only view owner is used.
 */

#define PGY_HASHMAP_INIT_CAP 16
#define PGY_HASHMAP_LOAD_FACTOR 0.75
#define PGY_HASHMAP_EMPTY 0u
#define PGY_HASHMAP_LIVE 1u
#define PGY_HASHMAP_DELETED 2u

#include "pgy_runtime_hashmap_key_storage_owner.h"

#define PGY_HASHMAP_STRING_KEYS(map) ((char **)((map)->keys))
#define PGY_HASHMAP_I32_KEYS(map) ((int32_t *)((map)->keys))
#define PGY_HASHMAP_I64_KEYS(map) ((int64_t *)((map)->keys))
#define PGY_HASHMAP_BOOL_KEYS(map) ((bool *)((map)->keys))

#ifndef PGY_RUNTIME_HASHMAP_CAPACITY_FITS
#define PGY_RUNTIME_HASHMAP_CAPACITY_FITS(capacity, CType) \
    ((capacity) != 0 \
        && (capacity) <= (size_t)INT32_MAX \
        && (capacity) <= SIZE_MAX / sizeof(char *) \
        && (capacity) <= SIZE_MAX / sizeof(CType) \
        && (capacity) <= SIZE_MAX / sizeof(uint8_t))
#endif

#ifndef PGY_RUNTIME_HASHMAP_IS_INITIALIZED
#define PGY_RUNTIME_HASHMAP_IS_INITIALIZED(map, CType) \
    ((map) != NULL \
        && PGY_RUNTIME_HASHMAP_CAPACITY_FITS((map)->capacity, CType) \
        && (map)->keys != NULL \
        && (map)->values != NULL \
        && (map)->occupied != NULL \
        && ((map)->key_storage_kind == PGY_HASHMAP_KEY_STORAGE_STRING \
            || (map)->key_storage_kind == PGY_HASHMAP_KEY_STORAGE_I32 \
            || (map)->key_storage_kind == PGY_HASHMAP_KEY_STORAGE_I64 \
            || (map)->key_storage_kind == PGY_HASHMAP_KEY_STORAGE_BOOL))
#endif

typedef struct
{
    void    *keys;
    int32_t *values;
    uint8_t *occupied;
    size_t   count;
    size_t   capacity;
    size_t   deleted_count;
    int32_t  key_storage_kind;
} PgyHashMap_Int;

PGY_RT_DECL uint32_t pgy_hash_string(const char *s)

#ifndef PGY_RUNTIME_DECLS_ONLY
{
    uint32_t h = 5381;
    if (s == NULL) return h;
    while (*s) { h = ((h << 5) + h) ^ (uint32_t)*s++; }
    return h;
}
#else
;
#endif


#define PGY_HASHMAP_DEFINE(SuffixName, CType) \
typedef struct \
{ \
    void     *keys; \
    CType    *values; \
    uint8_t  *occupied; \
    size_t    count; \
    size_t    capacity; \
    size_t    deleted_count; \
    int32_t   key_storage_kind; \
} PgyHashMap_##SuffixName; \
\
PGY_RT_PROGRAM_DECL PgyHashMap_##SuffixName pgy_map_new_##SuffixName(void) \
PGY_RT_PROGRAM_BODY({ \
    PgyHashMap_##SuffixName m; memset(&m, 0, sizeof(m)); \
    m.capacity = PGY_HASHMAP_INIT_CAP; \
    m.key_storage_kind = PGY_HASHMAP_KEY_STORAGE_STRING; \
    if (!PGY_RUNTIME_HASHMAP_CAPACITY_FITS(m.capacity, CType)) { \
        m.keys = NULL; m.values = NULL; m.occupied = NULL; \
        m.capacity = 0; \
        pgy_runtime_panic_collection_oom("map_new_" #SuffixName, "allocation size overflow"); \
        return m; \
    } \
    m.keys = PGY_HASHMAP_CALLOC(m.capacity, sizeof(char *)); \
    m.values = (CType *)PGY_HASHMAP_CALLOC(m.capacity, sizeof(CType)); \
    m.occupied = (uint8_t *)PGY_HASHMAP_CALLOC(m.capacity, sizeof(uint8_t)); \
    if (m.keys == NULL || m.values == NULL || m.occupied == NULL) { \
        free(m.keys); free(m.values); free(m.occupied); \
        memset(&m, 0, sizeof(m)); \
        pgy_runtime_panic_collection_oom("map_new_" #SuffixName, "allocation failed"); \
    } \
    return m; \
}) \
\
PGY_RT_PROGRAM_DECL PgyHashMap_##SuffixName pgy_map_new_i32_##SuffixName(void) \
PGY_RT_PROGRAM_BODY({ \
    PgyHashMap_##SuffixName m; memset(&m, 0, sizeof(m)); \
    m.capacity = PGY_HASHMAP_INIT_CAP; \
    m.key_storage_kind = PGY_HASHMAP_KEY_STORAGE_I32; \
    if (!PGY_RUNTIME_HASHMAP_CAPACITY_FITS(m.capacity, CType)) { \
        m.capacity = 0; \
        pgy_runtime_panic_collection_oom("map_new_i32_" #SuffixName, "allocation size overflow"); \
        return m; \
    } \
    m.keys = PGY_HASHMAP_CALLOC(m.capacity, sizeof(int32_t)); \
    m.values = (CType *)PGY_HASHMAP_CALLOC(m.capacity, sizeof(CType)); \
    m.occupied = (uint8_t *)PGY_HASHMAP_CALLOC(m.capacity, sizeof(uint8_t)); \
    if (m.keys == NULL || m.values == NULL || m.occupied == NULL) { \
        free(m.keys); free(m.values); free(m.occupied); \
        memset(&m, 0, sizeof(m)); \
        pgy_runtime_panic_collection_oom("map_new_i32_" #SuffixName, "allocation failed"); \
    } \
    return m; \
}) \
\
PGY_RT_PROGRAM_DECL PgyHashMap_##SuffixName pgy_map_new_i64_##SuffixName(void) \
PGY_RT_PROGRAM_BODY({ \
    PgyHashMap_##SuffixName m; memset(&m, 0, sizeof(m)); \
    m.capacity = PGY_HASHMAP_INIT_CAP; \
    m.key_storage_kind = PGY_HASHMAP_KEY_STORAGE_I64; \
    if (!PGY_RUNTIME_HASHMAP_CAPACITY_FITS(m.capacity, CType)) { \
        m.capacity = 0; \
        pgy_runtime_panic_collection_oom("map_new_i64_" #SuffixName, "allocation size overflow"); \
        return m; \
    } \
    m.keys = PGY_HASHMAP_CALLOC(m.capacity, sizeof(int64_t)); \
    m.values = (CType *)PGY_HASHMAP_CALLOC(m.capacity, sizeof(CType)); \
    m.occupied = (uint8_t *)PGY_HASHMAP_CALLOC(m.capacity, sizeof(uint8_t)); \
    if (m.keys == NULL || m.values == NULL || m.occupied == NULL) { \
        free(m.keys); free(m.values); free(m.occupied); \
        memset(&m, 0, sizeof(m)); \
        pgy_runtime_panic_collection_oom("map_new_i64_" #SuffixName, "allocation failed"); \
    } \
    return m; \
}) \
\
PGY_RT_PROGRAM_DECL PgyHashMap_##SuffixName pgy_map_new_bool_##SuffixName(void) \
PGY_RT_PROGRAM_BODY({ \
    PgyHashMap_##SuffixName m; memset(&m, 0, sizeof(m)); \
    m.capacity = PGY_HASHMAP_INIT_CAP; \
    m.key_storage_kind = PGY_HASHMAP_KEY_STORAGE_BOOL; \
    if (!PGY_RUNTIME_HASHMAP_CAPACITY_FITS(m.capacity, CType)) { \
        m.capacity = 0; \
        pgy_runtime_panic_collection_oom("map_new_bool_" #SuffixName, "allocation size overflow"); \
        return m; \
    } \
    m.keys = PGY_HASHMAP_CALLOC(m.capacity, sizeof(bool)); \
    m.values = (CType *)PGY_HASHMAP_CALLOC(m.capacity, sizeof(CType)); \
    m.occupied = (uint8_t *)PGY_HASHMAP_CALLOC(m.capacity, sizeof(uint8_t)); \
    if (m.keys == NULL || m.values == NULL || m.occupied == NULL) { \
        free(m.keys); free(m.values); free(m.occupied); \
        memset(&m, 0, sizeof(m)); \
        pgy_runtime_panic_collection_oom("map_new_bool_" #SuffixName, "allocation failed"); \
    } \
    return m; \
}) \
\
PGY_RT_PROGRAM_DECL void pgy_map_drop_##SuffixName(PgyHashMap_##SuffixName *m) \
PGY_RT_PROGRAM_BODY({ \
    if (m == NULL) return; \
    if (m->capacity == 0 && m->keys == NULL && m->values == NULL && m->occupied == NULL) { \
        memset(m, 0, sizeof(*m)); \
        return; \
    } \
    if (!PGY_RUNTIME_HASHMAP_IS_INITIALIZED(m, CType)) \
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_INTERNAL_INVARIANT, "map drop on invalid map"); \
    if (m->key_storage_kind == PGY_HASHMAP_KEY_STORAGE_STRING) { \
        for (size_t i = 0; i < m->capacity; i++) { \
            if (m->occupied[i] == PGY_HASHMAP_LIVE) { \
                free(PGY_HASHMAP_STRING_KEYS(m)[i]); \
                PGY_HASHMAP_STRING_KEYS(m)[i] = NULL; \
            } \
        } \
    } \
    free(m->keys); free(m->values); free(m->occupied); \
    memset(m, 0, sizeof(*m)); \
}) \
\
PGY_RT_PROGRAM_DECL bool pgy_map_grow_##SuffixName(PgyHashMap_##SuffixName *m) \
PGY_RT_PROGRAM_BODY({ \
    size_t old_cap = m->capacity; \
    char **old_keys = PGY_HASHMAP_STRING_KEYS(m); \
    CType *old_vals = m->values; \
    uint8_t *old_occ = m->occupied; \
    size_t new_capacity; \
    char **new_keys; \
    CType *new_values; \
    uint8_t *new_occupied; \
    if (m->key_storage_kind != PGY_HASHMAP_KEY_STORAGE_STRING) \
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_INTERNAL_INVARIANT, "map key storage kind mismatch"); \
    if (m->capacity == 0) { \
        new_capacity = PGY_HASHMAP_INIT_CAP; \
    } else { \
        if (m->capacity > SIZE_MAX / 2) { \
            pgy_runtime_panic_collection_oom("map_grow_" #SuffixName, "capacity overflow"); \
            return false; \
        } \
        new_capacity = m->capacity * 2; \
    } \
    if (!PGY_RUNTIME_HASHMAP_CAPACITY_FITS(new_capacity, CType)) { \
        pgy_runtime_panic_collection_oom("map_grow_" #SuffixName, "allocation size overflow"); \
        return false; \
    } \
    new_keys = (char **)PGY_HASHMAP_CALLOC(new_capacity, sizeof(char *)); \
    new_values = (CType *)PGY_HASHMAP_CALLOC(new_capacity, sizeof(CType)); \
    new_occupied = (uint8_t *)PGY_HASHMAP_CALLOC(new_capacity, sizeof(uint8_t)); \
    if (new_keys == NULL || new_values == NULL || new_occupied == NULL) { \
        free(new_keys); free(new_values); free(new_occupied); \
        pgy_runtime_panic_collection_oom("map_grow_" #SuffixName, "allocation failed"); \
        return false; \
    } \
    m->capacity = new_capacity; \
    m->keys = new_keys; \
    m->values = new_values; \
    m->occupied = new_occupied; \
    m->count = 0; \
    m->deleted_count = 0; \
    for (size_t i = 0; i < old_cap; i++) { \
        if (old_occ[i] == PGY_HASHMAP_LIVE) { \
            uint32_t h = pgy_hash_string(old_keys[i]) % (uint32_t)m->capacity; \
            while (m->occupied[h] == PGY_HASHMAP_LIVE) h = (h + 1) % (uint32_t)m->capacity; \
            PGY_HASHMAP_STRING_KEYS(m)[h] = old_keys[i]; \
            m->values[h] = old_vals[i]; \
            m->occupied[h] = PGY_HASHMAP_LIVE; \
            m->count++; \
        } \
    } \
    free(old_keys); free(old_vals); free(old_occ); \
    return true; \
}) \
\
PGY_RT_PROGRAM_DECL void pgy_map_set_##SuffixName(PgyHashMap_##SuffixName *m, const char *key, CType val) \
PGY_RT_PROGRAM_BODY({ \
    char *owned_key; \
    if (!PGY_RUNTIME_HASHMAP_IS_INITIALIZED(m, CType)) { \
        pgy_runtime_panic_invalid_collection("map_set_" #SuffixName, "map is not initialized"); \
        return; \
    } \
    if (key == NULL) { \
        pgy_runtime_panic_invalid_collection("map_set_" #SuffixName, "null key"); \
        return; \
    } \
    if (m->key_storage_kind != PGY_HASHMAP_KEY_STORAGE_STRING) \
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_INTERNAL_INVARIANT, "map key storage kind mismatch"); \
    uint32_t h = pgy_hash_string(key) % (uint32_t)m->capacity; \
    uint32_t first_deleted = UINT32_MAX; \
    size_t probes = 0; \
    while (m->occupied[h] && probes < m->capacity) { \
        if (m->occupied[h] == PGY_HASHMAP_LIVE && PGY_HASHMAP_STRING_KEYS(m)[h] != NULL && strcmp(PGY_HASHMAP_STRING_KEYS(m)[h], key) == 0) { \
            m->values[h] = val; \
            return; \
        } \
        if (m->occupied[h] == PGY_HASHMAP_DELETED && first_deleted == UINT32_MAX) first_deleted = h; \
        h = (h + 1) % (uint32_t)m->capacity; \
        probes++; \
    } \
    owned_key = pgy_runtime_strdup(key); \
    if (owned_key == NULL) { \
        pgy_runtime_panic_collection_oom("map_set_" #SuffixName, "key duplication failed"); \
        return; \
    } \
    if ((m->count + m->deleted_count + 1) * 4 > m->capacity * 3) { \
        if (!pgy_map_grow_##SuffixName(m)) { free(owned_key); return; } \
        h = pgy_hash_string(key) % (uint32_t)m->capacity; \
        first_deleted = UINT32_MAX; probes = 0; \
        while (m->occupied[h] == PGY_HASHMAP_LIVE && probes < m->capacity) { h = (h + 1) % (uint32_t)m->capacity; probes++; } \
    } \
    if (first_deleted != UINT32_MAX) h = first_deleted; \
    if (probes >= m->capacity && first_deleted == UINT32_MAX) { \
        free(owned_key); \
        pgy_runtime_panic_invalid_collection("map_set_" #SuffixName, "map is full"); \
        return; \
    } \
    PGY_HASHMAP_STRING_KEYS(m)[h] = owned_key; \
    m->values[h] = val; \
    m->occupied[h] = PGY_HASHMAP_LIVE; \
    m->count++; \
    if (first_deleted != UINT32_MAX) m->deleted_count--; \
}) \
\
PGY_RT_PROGRAM_DECL CType pgy_map_get_##SuffixName(PgyHashMap_##SuffixName *m, const char *key) \
PGY_RT_PROGRAM_BODY({ \
    if (!PGY_RUNTIME_HASHMAP_IS_INITIALIZED(m, CType)) \
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_INTERNAL_INVARIANT, "map get on invalid map"); \
    if (key == NULL) \
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_INTERNAL_INVARIANT, "map get with null key"); \
    if (m->key_storage_kind != PGY_HASHMAP_KEY_STORAGE_STRING) \
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_INTERNAL_INVARIANT, "map key storage kind mismatch"); \
    if (m->count == 0) \
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_OUT_OF_BOUNDS, "map key not found"); \
    uint32_t h = pgy_hash_string(key) % (uint32_t)m->capacity; \
    size_t probes = 0; \
    while (m->occupied[h] && probes < m->capacity) { \
        if (m->occupied[h] == PGY_HASHMAP_LIVE && PGY_HASHMAP_STRING_KEYS(m)[h] != NULL && strcmp(PGY_HASHMAP_STRING_KEYS(m)[h], key) == 0) \
            return m->values[h]; \
        h = (h + 1) % (uint32_t)m->capacity; \
        probes++; \
    } \
    PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_OUT_OF_BOUNDS, "map key not found"); \
    { CType zero_value; memset(&zero_value, 0, sizeof(zero_value)); return zero_value; } \
}) \
\
PGY_RT_PROGRAM_DECL bool pgy_map_has_##SuffixName(PgyHashMap_##SuffixName *m, const char *key) \
PGY_RT_PROGRAM_BODY({ \
    if (!PGY_RUNTIME_HASHMAP_IS_INITIALIZED(m, CType)) return false; \
    if (key == NULL) return false; \
    if (m->key_storage_kind != PGY_HASHMAP_KEY_STORAGE_STRING) \
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_INTERNAL_INVARIANT, "map key storage kind mismatch"); \
    if (m->count == 0) return false; \
    uint32_t h = pgy_hash_string(key) % (uint32_t)m->capacity; \
    size_t probes = 0; \
    while (m->occupied[h] && probes < m->capacity) { \
        if (m->occupied[h] == PGY_HASHMAP_LIVE && PGY_HASHMAP_STRING_KEYS(m)[h] != NULL && strcmp(PGY_HASHMAP_STRING_KEYS(m)[h], key) == 0) \
            return true; \
        h = (h + 1) % (uint32_t)m->capacity; \
        probes++; \
    } \
    return false; \
}) \
\
PGY_RT_PROGRAM_DECL void pgy_map_remove_##SuffixName(PgyHashMap_##SuffixName *m, const char *key) \
PGY_RT_PROGRAM_BODY({ \
    if (!PGY_RUNTIME_HASHMAP_IS_INITIALIZED(m, CType)) \
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_INTERNAL_INVARIANT, "map remove on invalid map"); \
    if (key == NULL) \
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_INTERNAL_INVARIANT, "map remove with null key"); \
    if (m->key_storage_kind != PGY_HASHMAP_KEY_STORAGE_STRING) \
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_INTERNAL_INVARIANT, "map key storage kind mismatch"); \
    if (m->count == 0) \
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_OUT_OF_BOUNDS, "map remove key not found"); \
    uint32_t h = pgy_hash_string(key) % (uint32_t)m->capacity; \
    size_t probes = 0; \
    while (m->occupied[h] && probes < m->capacity) { \
        if (m->occupied[h] == PGY_HASHMAP_LIVE && PGY_HASHMAP_STRING_KEYS(m)[h] != NULL && strcmp(PGY_HASHMAP_STRING_KEYS(m)[h], key) == 0) { \
            free(PGY_HASHMAP_STRING_KEYS(m)[h]); \
            PGY_HASHMAP_STRING_KEYS(m)[h] = NULL; \
            memset(&m->values[h], 0, sizeof(CType)); \
            m->occupied[h] = PGY_HASHMAP_DELETED; \
            m->count--; \
            m->deleted_count++; \
            return; \
        } \
        h = (h + 1) % (uint32_t)m->capacity; \
        probes++; \
    } \
    PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_OUT_OF_BOUNDS, "map remove key not found"); \
}) \
\
PGY_RT_PROGRAM_DECL int32_t pgy_map_size_##SuffixName(PgyHashMap_##SuffixName *m) \
PGY_RT_PROGRAM_BODY({ \
    if (!PGY_RUNTIME_HASHMAP_IS_INITIALIZED(m, CType)) \
        return 0; \
    return (int32_t)m->count; \
}) \
\
PGY_RT_PROGRAM_DECL bool pgy_map_grow_i32_##SuffixName(PgyHashMap_##SuffixName *m) \
PGY_RT_PROGRAM_BODY({ \
    size_t old_cap = m->capacity; int32_t *old_keys = PGY_HASHMAP_I32_KEYS(m); \
    CType *old_vals = m->values; uint8_t *old_occ = m->occupied; \
    size_t new_capacity; int32_t *new_keys; CType *new_values; uint8_t *new_occupied; \
    if (m->key_storage_kind != PGY_HASHMAP_KEY_STORAGE_I32) \
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_INTERNAL_INVARIANT, "map key storage kind mismatch"); \
    if (m->capacity > SIZE_MAX / 2) { \
        pgy_runtime_panic_collection_oom("map_grow_i32_" #SuffixName, "capacity overflow"); return false; \
    } \
    new_capacity = m->capacity == 0 ? PGY_HASHMAP_INIT_CAP : m->capacity * 2; \
    if (!PGY_RUNTIME_HASHMAP_CAPACITY_FITS(new_capacity, CType)) { \
        pgy_runtime_panic_collection_oom("map_grow_i32_" #SuffixName, "allocation size overflow"); return false; \
    } \
    new_keys = (int32_t *)PGY_HASHMAP_CALLOC(new_capacity, sizeof(int32_t)); \
    new_values = (CType *)PGY_HASHMAP_CALLOC(new_capacity, sizeof(CType)); \
    new_occupied = (uint8_t *)PGY_HASHMAP_CALLOC(new_capacity, sizeof(uint8_t)); \
    if (new_keys == NULL || new_values == NULL || new_occupied == NULL) { \
        free(new_keys); free(new_values); free(new_occupied); \
        pgy_runtime_panic_collection_oom("map_grow_i32_" #SuffixName, "allocation failed"); return false; \
    } \
    m->capacity = new_capacity; m->keys = new_keys; m->values = new_values; \
    m->occupied = new_occupied; m->count = 0; m->deleted_count = 0; \
    for (size_t i = 0; i < old_cap; i++) { \
        uint32_t h; if (old_occ[i] != PGY_HASHMAP_LIVE) continue; \
        h = pgy_hashmap_hash_i32(old_keys[i]) % (uint32_t)m->capacity; \
        while (m->occupied[h] == PGY_HASHMAP_LIVE) h = (h + 1) % (uint32_t)m->capacity; \
        PGY_HASHMAP_I32_KEYS(m)[h] = old_keys[i]; m->values[h] = old_vals[i]; \
        m->occupied[h] = PGY_HASHMAP_LIVE; m->count++; \
    } \
    free(old_keys); free(old_vals); free(old_occ); return true; \
}) \
\
PGY_RT_PROGRAM_DECL void pgy_map_set_i32_##SuffixName(PgyHashMap_##SuffixName *m, int32_t key, CType val) \
PGY_RT_PROGRAM_BODY({ \
    uint32_t h, first_deleted = UINT32_MAX; size_t probes = 0; \
    if (!PGY_RUNTIME_HASHMAP_IS_INITIALIZED(m, CType)) { pgy_runtime_panic_invalid_collection("map_set_i32_" #SuffixName, "map is not initialized"); return; } \
    if (m->key_storage_kind != PGY_HASHMAP_KEY_STORAGE_I32) \
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_INTERNAL_INVARIANT, "map key storage kind mismatch"); \
    h = pgy_hashmap_hash_i32(key) % (uint32_t)m->capacity; \
    while (m->occupied[h] != PGY_HASHMAP_EMPTY && probes < m->capacity) { \
        if (m->occupied[h] == PGY_HASHMAP_LIVE && PGY_HASHMAP_I32_KEYS(m)[h] == key) { m->values[h] = val; return; } \
        if (m->occupied[h] == PGY_HASHMAP_DELETED && first_deleted == UINT32_MAX) first_deleted = h; \
        h = (h + 1) % (uint32_t)m->capacity; probes++; \
    } \
    if ((m->count + m->deleted_count + 1) * 4 > m->capacity * 3) { \
        if (!pgy_map_grow_i32_##SuffixName(m)) return; \
        h = pgy_hashmap_hash_i32(key) % (uint32_t)m->capacity; \
        first_deleted = UINT32_MAX; probes = 0; \
        while (m->occupied[h] == PGY_HASHMAP_LIVE && probes < m->capacity) { h = (h + 1) % (uint32_t)m->capacity; probes++; } \
    } \
    if (first_deleted != UINT32_MAX) h = first_deleted; \
    else if (probes >= m->capacity) { pgy_runtime_panic_invalid_collection("map_set_i32_" #SuffixName, "map is full"); return; } \
    PGY_HASHMAP_I32_KEYS(m)[h] = key; m->values[h] = val; m->occupied[h] = PGY_HASHMAP_LIVE; m->count++; \
    if (first_deleted != UINT32_MAX) m->deleted_count--; \
}) \
\
PGY_RT_PROGRAM_DECL CType pgy_map_get_i32_##SuffixName(PgyHashMap_##SuffixName *m, int32_t key) \
PGY_RT_PROGRAM_BODY({ \
    if (!PGY_RUNTIME_HASHMAP_IS_INITIALIZED(m, CType)) PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_INTERNAL_INVARIANT, "map i32 get on invalid map"); \
    if (m->key_storage_kind != PGY_HASHMAP_KEY_STORAGE_I32) PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_INTERNAL_INVARIANT, "map key storage kind mismatch"); \
    uint32_t h = pgy_hashmap_hash_i32(key) % (uint32_t)m->capacity; size_t probes = 0; \
    while (m->occupied[h] != PGY_HASHMAP_EMPTY && probes < m->capacity) { \
        if (m->occupied[h] == PGY_HASHMAP_LIVE && PGY_HASHMAP_I32_KEYS(m)[h] == key) return m->values[h]; \
        h = (h + 1) % (uint32_t)m->capacity; probes++; \
    } \
    PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_OUT_OF_BOUNDS, "map key not found"); \
    { CType zero_value; memset(&zero_value, 0, sizeof(zero_value)); return zero_value; } \
}) \
\
PGY_RT_PROGRAM_DECL bool pgy_map_has_i32_##SuffixName(PgyHashMap_##SuffixName *m, int32_t key) \
PGY_RT_PROGRAM_BODY({ \
    if (!PGY_RUNTIME_HASHMAP_IS_INITIALIZED(m, CType)) return false; \
    if (m->key_storage_kind != PGY_HASHMAP_KEY_STORAGE_I32) PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_INTERNAL_INVARIANT, "map key storage kind mismatch"); \
    uint32_t h = pgy_hashmap_hash_i32(key) % (uint32_t)m->capacity; size_t probes = 0; \
    while (m->occupied[h] != PGY_HASHMAP_EMPTY && probes < m->capacity) { \
        if (m->occupied[h] == PGY_HASHMAP_LIVE && PGY_HASHMAP_I32_KEYS(m)[h] == key) return true; \
        h = (h + 1) % (uint32_t)m->capacity; probes++; \
    } \
    return false; \
}) \
\
PGY_RT_PROGRAM_DECL void pgy_map_remove_i32_##SuffixName(PgyHashMap_##SuffixName *m, int32_t key) \
PGY_RT_PROGRAM_BODY({ \
    if (!PGY_RUNTIME_HASHMAP_IS_INITIALIZED(m, CType)) PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_INTERNAL_INVARIANT, "map i32 remove on invalid map"); \
    if (m->key_storage_kind != PGY_HASHMAP_KEY_STORAGE_I32) PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_INTERNAL_INVARIANT, "map key storage kind mismatch"); \
    uint32_t h = pgy_hashmap_hash_i32(key) % (uint32_t)m->capacity; size_t probes = 0; \
    while (m->occupied[h] != PGY_HASHMAP_EMPTY && probes < m->capacity) { \
        if (m->occupied[h] == PGY_HASHMAP_LIVE && PGY_HASHMAP_I32_KEYS(m)[h] == key) { \
            PGY_HASHMAP_I32_KEYS(m)[h] = 0; memset(&m->values[h], 0, sizeof(CType)); \
            m->occupied[h] = PGY_HASHMAP_DELETED; m->count--; m->deleted_count++; return; \
        } \
        h = (h + 1) % (uint32_t)m->capacity; probes++; \
    } \
    PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_OUT_OF_BOUNDS, "map remove key not found"); \
}) \
\
PGY_RT_PROGRAM_DECL bool pgy_map_grow_i64_##SuffixName(PgyHashMap_##SuffixName *m) \
PGY_RT_PROGRAM_BODY({ \
    size_t old_cap = m->capacity; int64_t *old_keys = PGY_HASHMAP_I64_KEYS(m); \
    CType *old_vals = m->values; uint8_t *old_occ = m->occupied; \
    size_t new_capacity; int64_t *new_keys; CType *new_values; uint8_t *new_occupied; \
    if (m->key_storage_kind != PGY_HASHMAP_KEY_STORAGE_I64) \
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_INTERNAL_INVARIANT, "map key storage kind mismatch"); \
    if (m->capacity > SIZE_MAX / 2) { \
        pgy_runtime_panic_collection_oom("map_grow_i64_" #SuffixName, "capacity overflow"); return false; \
    } \
    new_capacity = m->capacity == 0 ? PGY_HASHMAP_INIT_CAP : m->capacity * 2; \
    if (!PGY_RUNTIME_HASHMAP_CAPACITY_FITS(new_capacity, CType)) { \
        pgy_runtime_panic_collection_oom("map_grow_i64_" #SuffixName, "allocation size overflow"); return false; \
    } \
    new_keys = (int64_t *)PGY_HASHMAP_CALLOC(new_capacity, sizeof(int64_t)); \
    new_values = (CType *)PGY_HASHMAP_CALLOC(new_capacity, sizeof(CType)); \
    new_occupied = (uint8_t *)PGY_HASHMAP_CALLOC(new_capacity, sizeof(uint8_t)); \
    if (new_keys == NULL || new_values == NULL || new_occupied == NULL) { \
        free(new_keys); free(new_values); free(new_occupied); \
        pgy_runtime_panic_collection_oom("map_grow_i64_" #SuffixName, "allocation failed"); return false; \
    } \
    m->capacity = new_capacity; m->keys = new_keys; m->values = new_values; \
    m->occupied = new_occupied; m->count = 0; m->deleted_count = 0; \
    for (size_t i = 0; i < old_cap; i++) { \
        uint32_t h; if (old_occ[i] != PGY_HASHMAP_LIVE) continue; \
        h = pgy_hashmap_hash_i64(old_keys[i]) % (uint32_t)m->capacity; \
        while (m->occupied[h] == PGY_HASHMAP_LIVE) h = (h + 1) % (uint32_t)m->capacity; \
        PGY_HASHMAP_I64_KEYS(m)[h] = old_keys[i]; m->values[h] = old_vals[i]; \
        m->occupied[h] = PGY_HASHMAP_LIVE; m->count++; \
    } \
    free(old_keys); free(old_vals); free(old_occ); return true; \
}) \
\
PGY_RT_PROGRAM_DECL void pgy_map_set_i64_##SuffixName(PgyHashMap_##SuffixName *m, int64_t key, CType val) \
PGY_RT_PROGRAM_BODY({ \
    uint32_t h, first_deleted = UINT32_MAX; size_t probes = 0; \
    if (!PGY_RUNTIME_HASHMAP_IS_INITIALIZED(m, CType)) { pgy_runtime_panic_invalid_collection("map_set_i64_" #SuffixName, "map is not initialized"); return; } \
    if (m->key_storage_kind != PGY_HASHMAP_KEY_STORAGE_I64) PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_INTERNAL_INVARIANT, "map key storage kind mismatch"); \
    h = pgy_hashmap_hash_i64(key) % (uint32_t)m->capacity; \
    while (m->occupied[h] != PGY_HASHMAP_EMPTY && probes < m->capacity) { \
        if (m->occupied[h] == PGY_HASHMAP_LIVE && PGY_HASHMAP_I64_KEYS(m)[h] == key) { m->values[h] = val; return; } \
        if (m->occupied[h] == PGY_HASHMAP_DELETED && first_deleted == UINT32_MAX) first_deleted = h; \
        h = (h + 1) % (uint32_t)m->capacity; probes++; \
    } \
    if ((m->count + m->deleted_count + 1) * 4 > m->capacity * 3) { \
        if (!pgy_map_grow_i64_##SuffixName(m)) return; \
        h = pgy_hashmap_hash_i64(key) % (uint32_t)m->capacity; \
        first_deleted = UINT32_MAX; probes = 0; \
        while (m->occupied[h] == PGY_HASHMAP_LIVE && probes < m->capacity) { h = (h + 1) % (uint32_t)m->capacity; probes++; } \
    } \
    if (first_deleted != UINT32_MAX) h = first_deleted; \
    else if (probes >= m->capacity) { pgy_runtime_panic_invalid_collection("map_set_i64_" #SuffixName, "map is full"); return; } \
    PGY_HASHMAP_I64_KEYS(m)[h] = key; m->values[h] = val; m->occupied[h] = PGY_HASHMAP_LIVE; m->count++; \
    if (first_deleted != UINT32_MAX) m->deleted_count--; \
}) \
\
PGY_RT_PROGRAM_DECL CType pgy_map_get_i64_##SuffixName(PgyHashMap_##SuffixName *m, int64_t key) \
PGY_RT_PROGRAM_BODY({ \
    if (!PGY_RUNTIME_HASHMAP_IS_INITIALIZED(m, CType)) PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_INTERNAL_INVARIANT, "map i64 get on invalid map"); \
    if (m->key_storage_kind != PGY_HASHMAP_KEY_STORAGE_I64) PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_INTERNAL_INVARIANT, "map key storage kind mismatch"); \
    uint32_t h = pgy_hashmap_hash_i64(key) % (uint32_t)m->capacity; size_t probes = 0; \
    while (m->occupied[h] != PGY_HASHMAP_EMPTY && probes < m->capacity) { \
        if (m->occupied[h] == PGY_HASHMAP_LIVE && PGY_HASHMAP_I64_KEYS(m)[h] == key) return m->values[h]; \
        h = (h + 1) % (uint32_t)m->capacity; probes++; \
    } \
    PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_OUT_OF_BOUNDS, "map key not found"); \
    { CType zero_value; memset(&zero_value, 0, sizeof(zero_value)); return zero_value; } \
}) \
\
PGY_RT_PROGRAM_DECL bool pgy_map_has_i64_##SuffixName(PgyHashMap_##SuffixName *m, int64_t key) \
PGY_RT_PROGRAM_BODY({ \
    if (!PGY_RUNTIME_HASHMAP_IS_INITIALIZED(m, CType)) return false; \
    if (m->key_storage_kind != PGY_HASHMAP_KEY_STORAGE_I64) PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_INTERNAL_INVARIANT, "map key storage kind mismatch"); \
    uint32_t h = pgy_hashmap_hash_i64(key) % (uint32_t)m->capacity; size_t probes = 0; \
    while (m->occupied[h] != PGY_HASHMAP_EMPTY && probes < m->capacity) { \
        if (m->occupied[h] == PGY_HASHMAP_LIVE && PGY_HASHMAP_I64_KEYS(m)[h] == key) return true; \
        h = (h + 1) % (uint32_t)m->capacity; probes++; \
    } \
    return false; \
}) \
\
PGY_RT_PROGRAM_DECL void pgy_map_remove_i64_##SuffixName(PgyHashMap_##SuffixName *m, int64_t key) \
PGY_RT_PROGRAM_BODY({ \
    if (!PGY_RUNTIME_HASHMAP_IS_INITIALIZED(m, CType)) PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_INTERNAL_INVARIANT, "map i64 remove on invalid map"); \
    if (m->key_storage_kind != PGY_HASHMAP_KEY_STORAGE_I64) PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_INTERNAL_INVARIANT, "map key storage kind mismatch"); \
    uint32_t h = pgy_hashmap_hash_i64(key) % (uint32_t)m->capacity; size_t probes = 0; \
    while (m->occupied[h] != PGY_HASHMAP_EMPTY && probes < m->capacity) { \
        if (m->occupied[h] == PGY_HASHMAP_LIVE && PGY_HASHMAP_I64_KEYS(m)[h] == key) { \
            PGY_HASHMAP_I64_KEYS(m)[h] = 0; memset(&m->values[h], 0, sizeof(CType)); \
            m->occupied[h] = PGY_HASHMAP_DELETED; m->count--; m->deleted_count++; return; \
        } \
        h = (h + 1) % (uint32_t)m->capacity; probes++; \
    } \
    PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_OUT_OF_BOUNDS, "map remove key not found"); \
}) \
\
PGY_RT_PROGRAM_DECL bool pgy_map_grow_bool_##SuffixName(PgyHashMap_##SuffixName *m) \
PGY_RT_PROGRAM_BODY({ \
    size_t old_cap = m->capacity; bool *old_keys = PGY_HASHMAP_BOOL_KEYS(m); \
    CType *old_vals = m->values; uint8_t *old_occ = m->occupied; \
    size_t new_capacity; bool *new_keys; CType *new_values; uint8_t *new_occupied; \
    if (m->key_storage_kind != PGY_HASHMAP_KEY_STORAGE_BOOL) \
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_INTERNAL_INVARIANT, "map key storage kind mismatch"); \
    if (m->capacity > SIZE_MAX / 2) { \
        pgy_runtime_panic_collection_oom("map_grow_bool_" #SuffixName, "capacity overflow"); return false; \
    } \
    new_capacity = m->capacity == 0 ? PGY_HASHMAP_INIT_CAP : m->capacity * 2; \
    if (!PGY_RUNTIME_HASHMAP_CAPACITY_FITS(new_capacity, CType)) { \
        pgy_runtime_panic_collection_oom("map_grow_bool_" #SuffixName, "allocation size overflow"); return false; \
    } \
    new_keys = (bool *)PGY_HASHMAP_CALLOC(new_capacity, sizeof(bool)); \
    new_values = (CType *)PGY_HASHMAP_CALLOC(new_capacity, sizeof(CType)); \
    new_occupied = (uint8_t *)PGY_HASHMAP_CALLOC(new_capacity, sizeof(uint8_t)); \
    if (new_keys == NULL || new_values == NULL || new_occupied == NULL) { \
        free(new_keys); free(new_values); free(new_occupied); \
        pgy_runtime_panic_collection_oom("map_grow_bool_" #SuffixName, "allocation failed"); return false; \
    } \
    m->capacity = new_capacity; m->keys = new_keys; m->values = new_values; \
    m->occupied = new_occupied; m->count = 0; m->deleted_count = 0; \
    for (size_t i = 0; i < old_cap; i++) { \
        uint32_t h; if (old_occ[i] != PGY_HASHMAP_LIVE) continue; \
        h = pgy_hashmap_hash_bool(old_keys[i]) % (uint32_t)m->capacity; \
        while (m->occupied[h] == PGY_HASHMAP_LIVE) h = (h + 1) % (uint32_t)m->capacity; \
        PGY_HASHMAP_BOOL_KEYS(m)[h] = old_keys[i]; m->values[h] = old_vals[i]; \
        m->occupied[h] = PGY_HASHMAP_LIVE; m->count++; \
    } \
    free(old_keys); free(old_vals); free(old_occ); return true; \
}) \
\
PGY_RT_PROGRAM_DECL void pgy_map_set_bool_##SuffixName(PgyHashMap_##SuffixName *m, bool key, CType val) \
PGY_RT_PROGRAM_BODY({ \
    uint32_t h, first_deleted = UINT32_MAX; size_t probes = 0; \
    if (!PGY_RUNTIME_HASHMAP_IS_INITIALIZED(m, CType)) { pgy_runtime_panic_invalid_collection("map_set_bool_" #SuffixName, "map is not initialized"); return; } \
    if (m->key_storage_kind != PGY_HASHMAP_KEY_STORAGE_BOOL) PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_INTERNAL_INVARIANT, "map key storage kind mismatch"); \
    h = pgy_hashmap_hash_bool(key) % (uint32_t)m->capacity; \
    while (m->occupied[h] != PGY_HASHMAP_EMPTY && probes < m->capacity) { \
        if (m->occupied[h] == PGY_HASHMAP_LIVE && PGY_HASHMAP_BOOL_KEYS(m)[h] == key) { m->values[h] = val; return; } \
        if (m->occupied[h] == PGY_HASHMAP_DELETED && first_deleted == UINT32_MAX) first_deleted = h; \
        h = (h + 1) % (uint32_t)m->capacity; probes++; \
    } \
    if ((m->count + m->deleted_count + 1) * 4 > m->capacity * 3) { \
        if (!pgy_map_grow_bool_##SuffixName(m)) return; \
        h = pgy_hashmap_hash_bool(key) % (uint32_t)m->capacity; \
        first_deleted = UINT32_MAX; probes = 0; \
        while (m->occupied[h] == PGY_HASHMAP_LIVE && probes < m->capacity) { h = (h + 1) % (uint32_t)m->capacity; probes++; } \
    } \
    if (first_deleted != UINT32_MAX) h = first_deleted; \
    else if (probes >= m->capacity) { pgy_runtime_panic_invalid_collection("map_set_bool_" #SuffixName, "map is full"); return; } \
    PGY_HASHMAP_BOOL_KEYS(m)[h] = key; m->values[h] = val; m->occupied[h] = PGY_HASHMAP_LIVE; m->count++; \
    if (first_deleted != UINT32_MAX) m->deleted_count--; \
}) \
\
PGY_RT_PROGRAM_DECL CType pgy_map_get_bool_##SuffixName(PgyHashMap_##SuffixName *m, bool key) \
PGY_RT_PROGRAM_BODY({ \
    if (!PGY_RUNTIME_HASHMAP_IS_INITIALIZED(m, CType)) PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_INTERNAL_INVARIANT, "map bool get on invalid map"); \
    if (m->key_storage_kind != PGY_HASHMAP_KEY_STORAGE_BOOL) PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_INTERNAL_INVARIANT, "map key storage kind mismatch"); \
    uint32_t h = pgy_hashmap_hash_bool(key) % (uint32_t)m->capacity; size_t probes = 0; \
    while (m->occupied[h] != PGY_HASHMAP_EMPTY && probes < m->capacity) { \
        if (m->occupied[h] == PGY_HASHMAP_LIVE && PGY_HASHMAP_BOOL_KEYS(m)[h] == key) return m->values[h]; \
        h = (h + 1) % (uint32_t)m->capacity; probes++; \
    } \
    PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_OUT_OF_BOUNDS, "map key not found"); \
    { CType zero_value; memset(&zero_value, 0, sizeof(zero_value)); return zero_value; } \
}) \
\
PGY_RT_PROGRAM_DECL bool pgy_map_has_bool_##SuffixName(PgyHashMap_##SuffixName *m, bool key) \
PGY_RT_PROGRAM_BODY({ \
    if (!PGY_RUNTIME_HASHMAP_IS_INITIALIZED(m, CType)) return false; \
    if (m->key_storage_kind != PGY_HASHMAP_KEY_STORAGE_BOOL) PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_INTERNAL_INVARIANT, "map key storage kind mismatch"); \
    uint32_t h = pgy_hashmap_hash_bool(key) % (uint32_t)m->capacity; size_t probes = 0; \
    while (m->occupied[h] != PGY_HASHMAP_EMPTY && probes < m->capacity) { \
        if (m->occupied[h] == PGY_HASHMAP_LIVE && PGY_HASHMAP_BOOL_KEYS(m)[h] == key) return true; \
        h = (h + 1) % (uint32_t)m->capacity; probes++; \
    } \
    return false; \
}) \
\
PGY_RT_PROGRAM_DECL void pgy_map_remove_bool_##SuffixName(PgyHashMap_##SuffixName *m, bool key) \
PGY_RT_PROGRAM_BODY({ \
    if (!PGY_RUNTIME_HASHMAP_IS_INITIALIZED(m, CType)) PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_INTERNAL_INVARIANT, "map bool remove on invalid map"); \
    if (m->key_storage_kind != PGY_HASHMAP_KEY_STORAGE_BOOL) PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_INTERNAL_INVARIANT, "map key storage kind mismatch"); \
    uint32_t h = pgy_hashmap_hash_bool(key) % (uint32_t)m->capacity; size_t probes = 0; \
    while (m->occupied[h] != PGY_HASHMAP_EMPTY && probes < m->capacity) { \
        if (m->occupied[h] == PGY_HASHMAP_LIVE && PGY_HASHMAP_BOOL_KEYS(m)[h] == key) { \
            PGY_HASHMAP_BOOL_KEYS(m)[h] = false; memset(&m->values[h], 0, sizeof(CType)); \
            m->occupied[h] = PGY_HASHMAP_DELETED; m->count--; m->deleted_count++; return; \
        } \
        h = (h + 1) % (uint32_t)m->capacity; probes++; \
    } \
    PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_OUT_OF_BOUNDS, "map remove key not found"); \
})
