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
    pgy_array_push_String(arr, value);
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
    PgyHashMap_Int m;
    m.capacity = PGY_HASHMAP_INIT_CAP;
    m.count = 0;
    if (!PGY_RUNTIME_HASHMAP_CAPACITY_FITS(m.capacity, int32_t)) {
        m.keys = NULL; m.values = NULL; m.occupied = NULL; m.capacity = 0;
        pgy_runtime_warn_invalid_collection("map_new_int", "allocation size overflow");
        return m;
    }
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

static inline bool pgy_map_int_is_initialized(const PgyHashMap_Int *m)
{
    return m != NULL
        && PGY_RUNTIME_HASHMAP_CAPACITY_FITS(m->capacity, int32_t)
        && m->keys != NULL
        && m->values != NULL
        && m->occupied != NULL;
}

static inline void pgy_map_grow_int(PgyHashMap_Int *m)
{
    size_t old_cap = m->capacity;
    char **old_keys = m->keys;
    int32_t *old_vals = m->values;
    uint8_t *old_occ = m->occupied;
    size_t new_capacity;
    char **new_keys;
    int32_t *new_values;
    uint8_t *new_occupied;
    if (m->capacity == 0) {
        new_capacity = PGY_HASHMAP_INIT_CAP;
    } else {
        if (m->capacity > SIZE_MAX / 2) {
            pgy_runtime_warn_invalid_collection("map_grow_int", "capacity overflow");
            return;
        }
        new_capacity = m->capacity * 2;
    }
    if (!PGY_RUNTIME_HASHMAP_CAPACITY_FITS(new_capacity, int32_t)) {
        pgy_runtime_warn_invalid_collection("map_grow_int", "allocation size overflow");
        return;
    }
    new_keys = (char **)calloc(new_capacity, sizeof(char *));
    new_values = (int32_t *)calloc(new_capacity, sizeof(int32_t));
    new_occupied = (uint8_t *)calloc(new_capacity, sizeof(uint8_t));
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
    if (!pgy_map_int_is_initialized(m)) {
        pgy_runtime_warn_invalid_collection("map_set_int", "map is not initialized");
        return;
    }
    if (key == NULL) {
        pgy_runtime_warn_invalid_collection("map_set_int", "null key");
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
    if (!pgy_map_int_is_initialized(m))
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_INTERNAL_INVARIANT, "map get on invalid map");
    if (key == NULL)
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_INTERNAL_INVARIANT, "map get with null key");
    if (m->count == 0)
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_OUT_OF_BOUNDS, "map key not found");
    uint32_t h = pgy_hash_string(key) % (uint32_t)m->capacity;
    size_t probes = 0;
    while (m->occupied[h] && probes < m->capacity) {
        if (m->keys[h] != NULL && strcmp(m->keys[h], key) == 0)
            return m->values[h];
        h = (h + 1) % (uint32_t)m->capacity;
        probes++;
    }
    PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_OUT_OF_BOUNDS, "map key not found");
    return 0;
}

static inline bool pgy_map_has_int(PgyHashMap_Int *m, const char *key)
{
    if (!pgy_map_int_is_initialized(m)) return false;
    if (key == NULL) return false;
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
    if (!pgy_map_int_is_initialized(m))
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
    PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_OUT_OF_BOUNDS, "map remove key not found");
}
