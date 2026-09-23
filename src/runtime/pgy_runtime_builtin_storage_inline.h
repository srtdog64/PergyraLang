/* =================================================================
 * Instantiate Built-in Slot Types
 * ================================================================= */

PGY_SLOT_DEFINE(Int,    int32_t)
PGY_SLOT_DEFINE(Long,   int64_t)
PGY_SLOT_DEFINE(Float,  float)
PGY_SLOT_DEFINE(Double, double)
PGY_SLOT_DEFINE(Bool,   bool)
PGY_SLOT_DEFINE(String, char*)

PGY_DEVICE_SLOT_DEFINE(Int,    int32_t)
PGY_DEVICE_SLOT_DEFINE(Long,   int64_t)
PGY_DEVICE_SLOT_DEFINE(Float,  float)
PGY_DEVICE_SLOT_DEFINE(Double, double)
PGY_DEVICE_SLOT_DEFINE(Bool,   bool)
PGY_DEVICE_SLOT_DEFINE(String, char*)

PGY_SECURE_SLOT_DEFINE(Int,    int32_t)
PGY_SECURE_SLOT_DEFINE(Long,   int64_t)
PGY_SECURE_SLOT_DEFINE(Float,  float)
PGY_SECURE_SLOT_DEFINE(Double, double)
PGY_SECURE_SLOT_DEFINE(Bool,   bool)
PGY_SECURE_SLOT_DEFINE(String, char*)

/* =================================================================
 * Instantiate Box Types for Built-ins
 * ================================================================= */

PGY_BOX_DEFINE(Int,    int32_t)
PGY_BOX_DEFINE(Long,   int64_t)
PGY_BOX_DEFINE(Float,  float)
PGY_BOX_DEFINE(Double, double)
PGY_BOX_DEFINE(Bool,   bool)
PGY_BOX_DEFINE(String, char*)

/* =================================================================
 * Instantiate Array / Slice / Rc / Weak / BoxArray for Built-ins
 * ================================================================= */

PGY_ARRAY_DEFINE(Int,    int32_t)
PGY_ARRAY_DEFINE(Long,   int64_t)
PGY_ARRAY_DEFINE(Float,  float)
PGY_ARRAY_DEFINE(Double, double)
PGY_ARRAY_DEFINE(Bool,   bool)
PGY_ARRAY_DEFINE(String, char*)

/* Explicit owner pair for compiler semantic scratch arrays. The ordinary
 * Array<String> beta surface remains no-free; these helpers are valid only
 * for arrays whose elements were inserted through the matching push helper. */
static inline void
pgy_array_push_owned_String(PgyArray_String *arr, char *value)
{
    char *owned = pgy_runtime_strdup(value != NULL ? value : "");
    if (owned == NULL)
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_OOM,
                          PGY_RUNTIME_PANIC_REASON_ALLOCATION_FAILED);
    pgy_array_push_String(arr, owned);
}

static inline void
pgy_array_drop_owned_String(PgyArray_String *arr)
{
    if (arr == NULL)
        return;
    for (size_t i = 0; i < arr->length; i++) {
        free(arr->data[i]);
        arr->data[i] = NULL;
    }
    pgy_array_drop_String(arr);
}

PGY_RC_DEFINE(Int,    int32_t)
PGY_RC_DEFINE(Long,   int64_t)
PGY_RC_DEFINE(Float,  float)
PGY_RC_DEFINE(Double, double)
PGY_RC_DEFINE(Bool,   bool)
PGY_RC_DEFINE(String, char*)

PGY_BOX_ARRAY_DEFINE(Int,    int32_t)
PGY_BOX_ARRAY_DEFINE(Long,   int64_t)
PGY_BOX_ARRAY_DEFINE(Float,  float)
PGY_BOX_ARRAY_DEFINE(Double, double)
PGY_BOX_ARRAY_DEFINE(Bool,   bool)
PGY_BOX_ARRAY_DEFINE(String, char*)

#include "pgy_runtime_builtin_hashmap_inline.h"

static inline PgyHashMap_Int pgy_map_new_int(void)
{
    PgyHashMap_Int m = {0};
    m.capacity = PGY_HASHMAP_INIT_CAP;
    m.key_storage_kind = PGY_HASHMAP_KEY_STORAGE_STRING;
    if (!PGY_RUNTIME_HASHMAP_CAPACITY_FITS(m.capacity, int32_t)) {
        m.capacity = 0;
        pgy_runtime_panic_collection_oom("map_new_int", "allocation size overflow");
        return m;
    }
    m.keys = PGY_HASHMAP_CALLOC(m.capacity, sizeof(char *));
    m.values = (int32_t *)PGY_HASHMAP_CALLOC(m.capacity, sizeof(int32_t));
    m.occupied = (uint8_t *)PGY_HASHMAP_CALLOC(m.capacity, sizeof(uint8_t));
    if (m.keys == NULL || m.values == NULL || m.occupied == NULL) {
        free(m.keys); free(m.values); free(m.occupied);
        memset(&m, 0, sizeof(m));
        pgy_runtime_panic_collection_oom("map_new_int", "allocation failed");
    }
    return m;
}

static inline PgyHashMap_Int pgy_map_new_i32_int(void)
{
    PgyHashMap_Int m = {0};
    m.capacity = PGY_HASHMAP_INIT_CAP;
    m.key_storage_kind = PGY_HASHMAP_KEY_STORAGE_I32;
    if (!PGY_RUNTIME_HASHMAP_CAPACITY_FITS(m.capacity, int32_t)) {
        m.capacity = 0;
        pgy_runtime_panic_collection_oom("map_new_i32_int", "allocation size overflow");
        return m;
    }
    m.keys = PGY_HASHMAP_CALLOC(m.capacity, sizeof(int32_t));
    m.values = (int32_t *)PGY_HASHMAP_CALLOC(m.capacity, sizeof(int32_t));
    m.occupied = (uint8_t *)PGY_HASHMAP_CALLOC(m.capacity, sizeof(uint8_t));
    if (m.keys == NULL || m.values == NULL || m.occupied == NULL) {
        free(m.keys); free(m.values); free(m.occupied);
        memset(&m, 0, sizeof(m));
        pgy_runtime_panic_collection_oom("map_new_i32_int", "allocation failed");
    }
    return m;
}

static inline PgyHashMap_Int pgy_map_new_i64_int(void)
{
    PgyHashMap_Int m = {0};
    m.capacity = PGY_HASHMAP_INIT_CAP;
    m.key_storage_kind = PGY_HASHMAP_KEY_STORAGE_I64;
    if (!PGY_RUNTIME_HASHMAP_CAPACITY_FITS(m.capacity, int32_t)) {
        m.capacity = 0;
        pgy_runtime_panic_collection_oom("map_new_i64_int", "allocation size overflow");
        return m;
    }
    m.keys = PGY_HASHMAP_CALLOC(m.capacity, sizeof(int64_t));
    m.values = (int32_t *)PGY_HASHMAP_CALLOC(m.capacity, sizeof(int32_t));
    m.occupied = (uint8_t *)PGY_HASHMAP_CALLOC(m.capacity, sizeof(uint8_t));
    if (m.keys == NULL || m.values == NULL || m.occupied == NULL) {
        free(m.keys); free(m.values); free(m.occupied);
        memset(&m, 0, sizeof(m));
        pgy_runtime_panic_collection_oom("map_new_i64_int", "allocation failed");
    }
    return m;
}

static inline PgyHashMap_Int pgy_map_new_bool_int(void)
{
    PgyHashMap_Int m = {0};
    m.capacity = PGY_HASHMAP_INIT_CAP;
    m.key_storage_kind = PGY_HASHMAP_KEY_STORAGE_BOOL;
    if (!PGY_RUNTIME_HASHMAP_CAPACITY_FITS(m.capacity, int32_t)) {
        m.capacity = 0;
        pgy_runtime_panic_collection_oom("map_new_bool_int", "allocation size overflow");
        return m;
    }
    m.keys = PGY_HASHMAP_CALLOC(m.capacity, sizeof(bool));
    m.values = (int32_t *)PGY_HASHMAP_CALLOC(m.capacity, sizeof(int32_t));
    m.occupied = (uint8_t *)PGY_HASHMAP_CALLOC(m.capacity, sizeof(uint8_t));
    if (m.keys == NULL || m.values == NULL || m.occupied == NULL) {
        free(m.keys); free(m.values); free(m.occupied);
        memset(&m, 0, sizeof(m));
        pgy_runtime_panic_collection_oom("map_new_bool_int", "allocation failed");
    }
    return m;
}

static inline bool pgy_map_int_is_initialized(const PgyHashMap_Int *m)
{
    return m != NULL
        && PGY_RUNTIME_HASHMAP_CAPACITY_FITS(m->capacity, int32_t)
        && m->keys != NULL && m->values != NULL && m->occupied != NULL
        && (m->key_storage_kind == PGY_HASHMAP_KEY_STORAGE_STRING
            || m->key_storage_kind == PGY_HASHMAP_KEY_STORAGE_I32
            || m->key_storage_kind == PGY_HASHMAP_KEY_STORAGE_I64
            || m->key_storage_kind == PGY_HASHMAP_KEY_STORAGE_BOOL);
}

static inline void pgy_map_drop_int(PgyHashMap_Int *m)
{
    if (m == NULL)
        return;
    if (m->capacity == 0 && m->keys == NULL
        && m->values == NULL && m->occupied == NULL) {
        memset(m, 0, sizeof(*m));
        return;
    }
    if (!pgy_map_int_is_initialized(m))
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_INTERNAL_INVARIANT,
                          "map drop on invalid map");
    if (m->key_storage_kind == PGY_HASHMAP_KEY_STORAGE_STRING) {
        for (size_t i = 0; i < m->capacity; i++) {
            if (m->occupied[i] == PGY_HASHMAP_LIVE) {
                free(PGY_HASHMAP_STRING_KEYS(m)[i]);
                PGY_HASHMAP_STRING_KEYS(m)[i] = NULL;
            }
        }
    }
    free(m->keys);
    free(m->values);
    free(m->occupied);
    memset(m, 0, sizeof(*m));
}

static inline void pgy_map_int_require_storage(
    const PgyHashMap_Int *m, PgyHashMapKeyStorageKind expected)
{
    if (m == NULL || m->key_storage_kind != (int32_t)expected)
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_INTERNAL_INVARIANT,
                          "map key storage kind mismatch");
}

static inline bool pgy_map_grow_int(PgyHashMap_Int *m)
{
    size_t old_cap = m->capacity;
    void *old_keys = m->keys;
    int32_t *old_vals = m->values;
    uint8_t *old_occ = m->occupied;
    size_t key_size = pgy_hashmap_key_storage_size(
        (PgyHashMapKeyStorageKind)m->key_storage_kind);
    size_t new_capacity;
    void *new_keys;
    int32_t *new_values;
    uint8_t *new_occupied;
    new_capacity = pgy_hashmap_rebuild_capacity(m->capacity, m->count, PGY_HASHMAP_INIT_CAP);
    if (key_size == 0 || new_capacity == 0) {
        pgy_runtime_panic_invalid_collection("map_grow_int", "invalid storage or capacity overflow");
        return false;
    }
    if (!PGY_RUNTIME_HASHMAP_CAPACITY_FITS(new_capacity, int32_t)
        || new_capacity > SIZE_MAX / key_size) {
        pgy_runtime_panic_collection_oom("map_grow_int", "allocation size overflow");
        return false;
    }
    new_keys = PGY_HASHMAP_CALLOC(new_capacity, key_size);
    new_values = (int32_t *)PGY_HASHMAP_CALLOC(new_capacity, sizeof(int32_t));
    new_occupied = (uint8_t *)PGY_HASHMAP_CALLOC(new_capacity, sizeof(uint8_t));
    if (new_keys == NULL || new_values == NULL || new_occupied == NULL) {
        free(new_keys); free(new_values); free(new_occupied);
        pgy_runtime_panic_collection_oom("map_grow_int", "allocation failed");
        return false;
    }
    m->capacity = new_capacity;
    m->keys = new_keys;
    m->values = new_values;
    m->occupied = new_occupied;
    m->count = 0;
    m->deleted_count = 0;
    for (size_t i = 0; i < old_cap; i++) {
        uint32_t h;
        if (old_occ[i] != PGY_HASHMAP_LIVE)
            continue;
        if (m->key_storage_kind == PGY_HASHMAP_KEY_STORAGE_STRING) {
            char *key = ((char **)old_keys)[i];
            if (key == NULL) continue;
            h = pgy_hash_string(key) % (uint32_t)m->capacity;
            while (m->occupied[h] == PGY_HASHMAP_LIVE)
                h = (h + 1) % (uint32_t)m->capacity;
            PGY_HASHMAP_STRING_KEYS(m)[h] = key;
        } else if (m->key_storage_kind == PGY_HASHMAP_KEY_STORAGE_I32) {
            int32_t key = ((int32_t *)old_keys)[i];
            h = pgy_hashmap_hash_i32(key) % (uint32_t)m->capacity;
            while (m->occupied[h] == PGY_HASHMAP_LIVE)
                h = (h + 1) % (uint32_t)m->capacity;
            PGY_HASHMAP_I32_KEYS(m)[h] = key;
        } else if (m->key_storage_kind == PGY_HASHMAP_KEY_STORAGE_I64) {
            int64_t key = ((int64_t *)old_keys)[i];
            h = pgy_hashmap_hash_i64(key) % (uint32_t)m->capacity;
            while (m->occupied[h] == PGY_HASHMAP_LIVE)
                h = (h + 1) % (uint32_t)m->capacity;
            PGY_HASHMAP_I64_KEYS(m)[h] = key;
        } else {
            bool key = ((bool *)old_keys)[i];
            h = pgy_hashmap_hash_bool(key) % (uint32_t)m->capacity;
            while (m->occupied[h] == PGY_HASHMAP_LIVE)
                h = (h + 1) % (uint32_t)m->capacity;
            PGY_HASHMAP_BOOL_KEYS(m)[h] = key;
        }
        m->values[h] = old_vals[i];
        m->occupied[h] = PGY_HASHMAP_LIVE;
        m->count++;
    }
    free(old_keys); free(old_vals); free(old_occ);
    return true;
}

static inline void pgy_map_set_int(PgyHashMap_Int *m, const char *key, int32_t val)
{
    uint32_t h, first_deleted = UINT32_MAX;
    size_t probes = 0;
    char *owned_key;
    if (!pgy_map_int_is_initialized(m) || key == NULL) {
        pgy_runtime_panic_invalid_collection("map_set_int", "invalid map or key");
        return;
    }
    pgy_map_int_require_storage(m, PGY_HASHMAP_KEY_STORAGE_STRING);
    h = pgy_hash_string(key) % (uint32_t)m->capacity;
    while (m->occupied[h] != PGY_HASHMAP_EMPTY && probes < m->capacity) {
        if (m->occupied[h] == PGY_HASHMAP_LIVE
            && PGY_HASHMAP_STRING_KEYS(m)[h] != NULL
            && strcmp(PGY_HASHMAP_STRING_KEYS(m)[h], key) == 0) {
            m->values[h] = val; return;
        }
        if (m->occupied[h] == PGY_HASHMAP_DELETED && first_deleted == UINT32_MAX)
            first_deleted = h;
        h = (h + 1) % (uint32_t)m->capacity; probes++;
    }
    owned_key = pgy_runtime_strdup(key);
    if (owned_key == NULL) {
        pgy_runtime_panic_collection_oom("map_set_int", "key duplication failed"); return;
    }
    if ((m->count + m->deleted_count + 1) * 4 > m->capacity * 3) {
        if (!pgy_map_grow_int(m)) { free(owned_key); return; }
        h = pgy_hash_string(key) % (uint32_t)m->capacity;
        first_deleted = UINT32_MAX; probes = 0;
        while (m->occupied[h] == PGY_HASHMAP_LIVE && probes < m->capacity) {
            h = (h + 1) % (uint32_t)m->capacity; probes++;
        }
    }
    if (first_deleted != UINT32_MAX) h = first_deleted;
    else if (probes >= m->capacity) {
        free(owned_key);
        pgy_runtime_panic_invalid_collection("map_set_int", "map is full"); return;
    }
    PGY_HASHMAP_STRING_KEYS(m)[h] = owned_key;
    m->values[h] = val; m->occupied[h] = PGY_HASHMAP_LIVE; m->count++;
    if (first_deleted != UINT32_MAX) m->deleted_count--;
}

static inline int32_t pgy_map_get_int(PgyHashMap_Int *m, const char *key)
{
    if (!pgy_map_int_is_initialized(m) || key == NULL)
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_INTERNAL_INVARIANT, "map get on invalid map");
    pgy_map_int_require_storage(m, PGY_HASHMAP_KEY_STORAGE_STRING);
    uint32_t h = pgy_hash_string(key) % (uint32_t)m->capacity; size_t probes = 0;
    while (m->occupied[h] != PGY_HASHMAP_EMPTY && probes < m->capacity) {
        if (m->occupied[h] == PGY_HASHMAP_LIVE
            && PGY_HASHMAP_STRING_KEYS(m)[h] != NULL
            && strcmp(PGY_HASHMAP_STRING_KEYS(m)[h], key) == 0) return m->values[h];
        h = (h + 1) % (uint32_t)m->capacity; probes++;
    }
    PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_OUT_OF_BOUNDS, "map key not found");
    return 0;
}

static inline bool pgy_map_has_int(PgyHashMap_Int *m, const char *key)
{
    if (!pgy_map_int_is_initialized(m) || key == NULL) return false;
    pgy_map_int_require_storage(m, PGY_HASHMAP_KEY_STORAGE_STRING);
    uint32_t h = pgy_hash_string(key) % (uint32_t)m->capacity; size_t probes = 0;
    while (m->occupied[h] != PGY_HASHMAP_EMPTY && probes < m->capacity) {
        if (m->occupied[h] == PGY_HASHMAP_LIVE
            && PGY_HASHMAP_STRING_KEYS(m)[h] != NULL
            && strcmp(PGY_HASHMAP_STRING_KEYS(m)[h], key) == 0) return true;
        h = (h + 1) % (uint32_t)m->capacity; probes++;
    }
    return false;
}

static inline void pgy_map_remove_int(PgyHashMap_Int *m, const char *key)
{
    if (!pgy_map_int_is_initialized(m) || key == NULL)
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_INTERNAL_INVARIANT, "map remove on invalid map");
    pgy_map_int_require_storage(m, PGY_HASHMAP_KEY_STORAGE_STRING);
    uint32_t h = pgy_hash_string(key) % (uint32_t)m->capacity; size_t probes = 0;
    while (m->occupied[h] != PGY_HASHMAP_EMPTY && probes < m->capacity) {
        if (m->occupied[h] == PGY_HASHMAP_LIVE
            && PGY_HASHMAP_STRING_KEYS(m)[h] != NULL
            && strcmp(PGY_HASHMAP_STRING_KEYS(m)[h], key) == 0) {
            free(PGY_HASHMAP_STRING_KEYS(m)[h]); PGY_HASHMAP_STRING_KEYS(m)[h] = NULL;
            m->values[h] = 0; m->occupied[h] = PGY_HASHMAP_DELETED;
            m->count--; m->deleted_count++; return;
        }
        h = (h + 1) % (uint32_t)m->capacity; probes++;
    }
    PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_OUT_OF_BOUNDS, "map remove key not found");
}
