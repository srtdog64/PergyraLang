#include "pgy_runtime_linkage.h"
/* String-value variant */
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

PGY_RT_DECL void pgy_map_keys_sort_bool_array(PgyArray_Bool *out)

#ifndef PGY_RUNTIME_DECLS_ONLY
{
    size_t false_count = 0;

    if (out == NULL || out->data == NULL || out->length <= 1)
        return;
    for (size_t i = 0; i < out->length; i++) {
        if (!out->data[i])
            false_count++;
    }
    for (size_t i = 0; i < out->length; i++)
        out->data[i] = i >= false_count;
}
#else
;
#endif


PGY_RT_DECL bool pgy_map_string_capacity_fits(size_t capacity)

#ifndef PGY_RUNTIME_DECLS_ONLY
{
    return capacity != 0
        && capacity <= (size_t)INT32_MAX
        && capacity <= SIZE_MAX / sizeof(char *)
        && capacity <= SIZE_MAX / sizeof(uint8_t);
}
#else
;
#endif


typedef struct
{
    void    *keys;
    char   **values;
    uint8_t *occupied;
    size_t   count;
    size_t   capacity;
    size_t   deleted_count;
    int32_t  key_storage_kind;
} PgyHashMap_String;

PGY_RT_DECL bool pgy_map_string_is_initialized(const PgyHashMap_String *m)

#ifndef PGY_RUNTIME_DECLS_ONLY
{
    return m != NULL
        && pgy_map_string_capacity_fits(m->capacity)
        && m->keys != NULL
        && m->values != NULL
        && m->occupied != NULL
        && (m->key_storage_kind == PGY_HASHMAP_KEY_STORAGE_STRING
            || m->key_storage_kind == PGY_HASHMAP_KEY_STORAGE_I32
            || m->key_storage_kind == PGY_HASHMAP_KEY_STORAGE_I64
            || m->key_storage_kind == PGY_HASHMAP_KEY_STORAGE_BOOL);
}
#else
;
#endif


PGY_RT_DECL PgyHashMap_String pgy_map_new_string(void)

#ifndef PGY_RUNTIME_DECLS_ONLY
{
    PgyHashMap_String m = {0};
    m.capacity = PGY_HASHMAP_INIT_CAP;
    m.key_storage_kind = PGY_HASHMAP_KEY_STORAGE_STRING;
    if (!pgy_map_string_capacity_fits(m.capacity)) {
        m.keys = NULL; m.values = NULL; m.occupied = NULL; m.capacity = 0;
        pgy_runtime_panic_collection_oom("map_new_string", "allocation size overflow");
        return m;
    }
    m.keys     = PGY_HASHMAP_CALLOC(m.capacity, sizeof(char *));
    m.values   = (char **)PGY_HASHMAP_CALLOC(m.capacity, sizeof(char *));
    m.occupied = (uint8_t *)PGY_HASHMAP_CALLOC(m.capacity, sizeof(uint8_t));
    if (m.keys == NULL || m.values == NULL || m.occupied == NULL) {
        free(m.keys); free(m.values); free(m.occupied);
        memset(&m, 0, sizeof(m));
        pgy_runtime_panic_collection_oom("map_new_string", "allocation failed");
    }
    return m;
}
#else
;
#endif


PGY_RT_DECL PgyHashMap_String pgy_map_new_i32_string(void)

#ifndef PGY_RUNTIME_DECLS_ONLY
{
    PgyHashMap_String m = {0};
    m.capacity = PGY_HASHMAP_INIT_CAP;
    m.key_storage_kind = PGY_HASHMAP_KEY_STORAGE_I32;
    if (!pgy_map_string_capacity_fits(m.capacity)) {
        m.capacity = 0;
        pgy_runtime_panic_collection_oom("map_new_i32_string", "allocation size overflow");
        return m;
    }
    m.keys = PGY_HASHMAP_CALLOC(m.capacity, sizeof(int32_t));
    m.values = (char **)PGY_HASHMAP_CALLOC(m.capacity, sizeof(char *));
    m.occupied = (uint8_t *)PGY_HASHMAP_CALLOC(m.capacity, sizeof(uint8_t));
    if (m.keys == NULL || m.values == NULL || m.occupied == NULL) {
        free(m.keys); free(m.values); free(m.occupied);
        memset(&m, 0, sizeof(m));
        pgy_runtime_panic_collection_oom("map_new_i32_string", "allocation failed");
    }
    return m;
}
#else
;
#endif


PGY_RT_DECL PgyHashMap_String pgy_map_new_i64_string(void)

#ifndef PGY_RUNTIME_DECLS_ONLY
{
    PgyHashMap_String m = {0};
    m.capacity = PGY_HASHMAP_INIT_CAP;
    m.key_storage_kind = PGY_HASHMAP_KEY_STORAGE_I64;
    if (!pgy_map_string_capacity_fits(m.capacity)) {
        m.capacity = 0;
        pgy_runtime_panic_collection_oom("map_new_i64_string", "allocation size overflow");
        return m;
    }
    m.keys = PGY_HASHMAP_CALLOC(m.capacity, sizeof(int64_t));
    m.values = (char **)PGY_HASHMAP_CALLOC(m.capacity, sizeof(char *));
    m.occupied = (uint8_t *)PGY_HASHMAP_CALLOC(m.capacity, sizeof(uint8_t));
    if (m.keys == NULL || m.values == NULL || m.occupied == NULL) {
        free(m.keys); free(m.values); free(m.occupied);
        memset(&m, 0, sizeof(m));
        pgy_runtime_panic_collection_oom("map_new_i64_string", "allocation failed");
    }
    return m;
}
#else
;
#endif

PGY_RT_DECL PgyHashMap_String pgy_map_new_bool_string(void)

#ifndef PGY_RUNTIME_DECLS_ONLY
{
    PgyHashMap_String m = {0};
    m.capacity = PGY_HASHMAP_INIT_CAP;
    m.key_storage_kind = PGY_HASHMAP_KEY_STORAGE_BOOL;
    if (!pgy_map_string_capacity_fits(m.capacity)) {
        m.capacity = 0;
        pgy_runtime_panic_collection_oom("map_new_bool_string", "allocation size overflow");
        return m;
    }
    m.keys = PGY_HASHMAP_CALLOC(m.capacity, sizeof(bool));
    m.values = (char **)PGY_HASHMAP_CALLOC(m.capacity, sizeof(char *));
    m.occupied = (uint8_t *)PGY_HASHMAP_CALLOC(m.capacity, sizeof(uint8_t));
    if (m.keys == NULL || m.values == NULL || m.occupied == NULL) {
        free(m.keys); free(m.values); free(m.occupied);
        memset(&m, 0, sizeof(m));
        pgy_runtime_panic_collection_oom("map_new_bool_string", "allocation failed");
    }
    return m;
}
#else
;
#endif


PGY_RT_DECL void pgy_map_drop_string(PgyHashMap_String *m)

#ifndef PGY_RUNTIME_DECLS_ONLY
{
    if (m == NULL)
        return;
    if (m->capacity == 0 && m->keys == NULL
        && m->values == NULL && m->occupied == NULL) {
        memset(m, 0, sizeof(*m));
        return;
    }
    if (!pgy_map_string_is_initialized(m))
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_INTERNAL_INVARIANT,
                          "map drop on invalid map");
    for (size_t i = 0; i < m->capacity; i++) {
        if (m->occupied[i] != PGY_HASHMAP_LIVE)
            continue;
        if (m->key_storage_kind == PGY_HASHMAP_KEY_STORAGE_STRING) {
            free(PGY_HASHMAP_STRING_KEYS(m)[i]);
            PGY_HASHMAP_STRING_KEYS(m)[i] = NULL;
        }
        free(m->values[i]);
        m->values[i] = NULL;
    }
    free(m->keys);
    free(m->values);
    free(m->occupied);
    memset(m, 0, sizeof(*m));
}
#else
;
#endif


PGY_RT_DECL void pgy_map_set_string(PgyHashMap_String *m, const char *key, const char *val)

#ifndef PGY_RUNTIME_DECLS_ONLY
{
    char *owned_key;
    char *owned_value;
    if (!pgy_map_string_is_initialized(m)) {
        pgy_runtime_panic_invalid_collection("map_set_string", "map is not initialized");
        return;
    }
    if (key == NULL) {
        pgy_runtime_panic_invalid_collection("map_set_string", "null key");
        return;
    }
    if (m->key_storage_kind != PGY_HASHMAP_KEY_STORAGE_STRING)
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_INTERNAL_INVARIANT,
                          "map key storage kind mismatch");
    uint32_t h = pgy_hash_string(key) % (uint32_t)m->capacity;
    size_t probes = 0;
    while (m->occupied[h] && probes < m->capacity) {
        if (PGY_HASHMAP_STRING_KEYS(m)[h] != NULL
            && strcmp(PGY_HASHMAP_STRING_KEYS(m)[h], key) == 0) {
            char *owned = pgy_runtime_strdup(val != NULL ? val : "");
            if (owned == NULL) {
                pgy_runtime_panic_collection_oom("map_set_string", "value duplication failed");
                return;
            }
            free(m->values[h]);
            m->values[h] = owned;
            return;
        }
        h = (h + 1) % (uint32_t)m->capacity;
        probes++;
    }
    owned_key = pgy_runtime_strdup(key);
    owned_value = pgy_runtime_strdup(val != NULL ? val : "");
    if (owned_key == NULL || owned_value == NULL) {
        free(owned_key); free(owned_value);
        pgy_runtime_panic_collection_oom("map_set_string", "key/value duplication failed");
        return;
    }
    if ((m->count + m->deleted_count + 1) * 4 > m->capacity * 3) {
        size_t old_cap = m->capacity;
        char **ok = PGY_HASHMAP_STRING_KEYS(m); char **ov = m->values; uint8_t *oo = m->occupied;
        size_t new_capacity;
        char **new_keys;
        char **new_values;
        uint8_t *new_occupied;
        if (m->capacity == 0) {
            new_capacity = PGY_HASHMAP_INIT_CAP;
        } else {
            if (m->capacity > SIZE_MAX / 2) {
                free(owned_key); free(owned_value);
                pgy_runtime_panic_collection_oom("map_set_string", "capacity overflow");
                return;
            }
            new_capacity = m->capacity * 2;
        }
        if (!pgy_map_string_capacity_fits(new_capacity)) {
            free(owned_key); free(owned_value);
            pgy_runtime_panic_collection_oom("map_set_string", "allocation size overflow");
            return;
        }
        new_keys = (char **)PGY_HASHMAP_CALLOC(new_capacity, sizeof(char *));
        new_values = (char **)PGY_HASHMAP_CALLOC(new_capacity, sizeof(char *));
        new_occupied = (uint8_t *)PGY_HASHMAP_CALLOC(new_capacity, sizeof(uint8_t));
        if (new_keys == NULL || new_values == NULL || new_occupied == NULL) {
            free(new_keys); free(new_values); free(new_occupied);
            free(owned_key); free(owned_value);
            pgy_runtime_panic_collection_oom("map_set_string", "map growth allocation failed");
            return;
        }
        m->capacity = new_capacity;
        m->keys = new_keys;
        m->values = new_values;
        m->occupied = new_occupied;
        m->count = 0;
        m->deleted_count = 0;
        for (size_t i = 0; i < old_cap; i++) {
            if (oo[i]) {
                uint32_t h2 = pgy_hash_string(ok[i]) % (uint32_t)m->capacity;
                while (m->occupied[h2]) h2 = (h2 + 1) % (uint32_t)m->capacity;
                PGY_HASHMAP_STRING_KEYS(m)[h2] = ok[i]; m->values[h2] = ov[i]; m->occupied[h2] = PGY_HASHMAP_LIVE; m->count++;
            }
        }
        free(ok); free(ov); free(oo);
    }
    h = pgy_hash_string(key) % (uint32_t)m->capacity;
    probes = 0;
    while (m->occupied[h] && probes < m->capacity) {
        h = (h + 1) % (uint32_t)m->capacity;
        probes++;
    }
    if (probes >= m->capacity) {
        free(owned_key); free(owned_value);
        pgy_runtime_panic_invalid_collection("map_set_string", "map is full");
        return;
    }
    PGY_HASHMAP_STRING_KEYS(m)[h] = owned_key;
    m->values[h] = owned_value;
    m->occupied[h] = 1; m->count++;
}
#else
;
#endif


PGY_RT_DECL char *pgy_map_get_string(PgyHashMap_String *m, const char *key)

#ifndef PGY_RUNTIME_DECLS_ONLY
{
    if (!pgy_map_string_is_initialized(m))
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_INTERNAL_INVARIANT, "map get on invalid map");
    if (key == NULL)
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_INTERNAL_INVARIANT, "map get with null key");
    if (m->key_storage_kind != PGY_HASHMAP_KEY_STORAGE_STRING)
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_INTERNAL_INVARIANT, "map key storage kind mismatch");
    if (m->count == 0)
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_OUT_OF_BOUNDS, "map key not found");
    uint32_t h = pgy_hash_string(key) % (uint32_t)m->capacity;
    size_t p = 0;
    while (m->occupied[h] && p < m->capacity) {
        if (PGY_HASHMAP_STRING_KEYS(m)[h] && strcmp(PGY_HASHMAP_STRING_KEYS(m)[h], key) == 0)
            return m->values[h] ? m->values[h] : "";
        h = (h + 1) % (uint32_t)m->capacity; p++;
    }
    PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_OUT_OF_BOUNDS, "map key not found");
    return "";
}
#else
;
#endif


PGY_RT_DECL bool pgy_map_has_string(PgyHashMap_String *m, const char *key)

#ifndef PGY_RUNTIME_DECLS_ONLY
{
    if (!pgy_map_string_is_initialized(m)) return false;
    if (key == NULL) return false;
    if (m->key_storage_kind != PGY_HASHMAP_KEY_STORAGE_STRING)
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_INTERNAL_INVARIANT, "map key storage kind mismatch");
    if (m->count == 0) return false;
    uint32_t h = pgy_hash_string(key) % (uint32_t)m->capacity;
    size_t p = 0;
    while (m->occupied[h] && p < m->capacity) {
        if (PGY_HASHMAP_STRING_KEYS(m)[h] && strcmp(PGY_HASHMAP_STRING_KEYS(m)[h], key) == 0) return true;
        h = (h + 1) % (uint32_t)m->capacity; p++;
    }
    return false;
}
#else
;
#endif


PGY_RT_DECL void pgy_map_remove_string(PgyHashMap_String *m, const char *key)

#ifndef PGY_RUNTIME_DECLS_ONLY
{
    if (!pgy_map_string_is_initialized(m))
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_INTERNAL_INVARIANT, "map remove on invalid map");
    if (key == NULL)
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_INTERNAL_INVARIANT, "map remove with null key");
    if (m->key_storage_kind != PGY_HASHMAP_KEY_STORAGE_STRING)
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_INTERNAL_INVARIANT, "map key storage kind mismatch");
    if (m->count == 0)
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_OUT_OF_BOUNDS, "map remove key not found");
    uint32_t cap = (uint32_t)m->capacity;

    uint32_t h = pgy_hash_string(key) % cap;
    size_t probes = 0;
    while (m->occupied[h] && probes < m->capacity) {
        if (PGY_HASHMAP_STRING_KEYS(m)[h] != NULL && strcmp(PGY_HASHMAP_STRING_KEYS(m)[h], key) == 0) {
            free(PGY_HASHMAP_STRING_KEYS(m)[h]);
            PGY_HASHMAP_STRING_KEYS(m)[h] = NULL;
            free(m->values[h]);
            m->values[h] = NULL;
            m->occupied[h] = 0;
            m->count--;
            /* Backward-shift: rehash subsequent entries to fill the gap */
            uint32_t gap = h;
            uint32_t j = (gap + 1) % cap;
            while (m->occupied[j]) {
                uint32_t ideal = pgy_hash_string(PGY_HASHMAP_STRING_KEYS(m)[j]) % cap;
                uint32_t dist_to_j   = (j - ideal + cap) % cap;
                uint32_t dist_to_gap = (gap - ideal + cap) % cap;
                if (dist_to_gap < dist_to_j) {
                    PGY_HASHMAP_STRING_KEYS(m)[gap] = PGY_HASHMAP_STRING_KEYS(m)[j];
                    m->values[gap]   = m->values[j];
                    m->occupied[gap] = 1;
                    PGY_HASHMAP_STRING_KEYS(m)[j] = NULL;
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
#else
;
#endif


PGY_RT_DECL int32_t pgy_map_size_string(PgyHashMap_String *m)

#ifndef PGY_RUNTIME_DECLS_ONLY
{
    return pgy_map_string_is_initialized(m) ? (int32_t)m->count : 0;
}
#else
;
#endif


PGY_RT_DECL bool pgy_map_grow_i32_string(PgyHashMap_String *m)

#ifndef PGY_RUNTIME_DECLS_ONLY
{
    size_t old_capacity = m->capacity;
    int32_t *old_keys = PGY_HASHMAP_I32_KEYS(m);
    char **old_values = m->values;
    uint8_t *old_occupied = m->occupied;
    size_t new_capacity;
    int32_t *new_keys;
    char **new_values;
    uint8_t *new_occupied;
    if (m->key_storage_kind != PGY_HASHMAP_KEY_STORAGE_I32)
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_INTERNAL_INVARIANT,
                          "map key storage kind mismatch");
    if (m->capacity > SIZE_MAX / 2) {
        pgy_runtime_panic_collection_oom("map_grow_i32_string", "capacity overflow");
        return false;
    }
    new_capacity = m->capacity == 0
        ? PGY_HASHMAP_INIT_CAP : m->capacity * 2;
    if (!pgy_map_string_capacity_fits(new_capacity)) {
        pgy_runtime_panic_collection_oom("map_grow_i32_string", "capacity overflow");
        return false;
    }
    new_keys = (int32_t *)PGY_HASHMAP_CALLOC(new_capacity, sizeof(int32_t));
    new_values = (char **)PGY_HASHMAP_CALLOC(new_capacity, sizeof(char *));
    new_occupied = (uint8_t *)PGY_HASHMAP_CALLOC(new_capacity, sizeof(uint8_t));
    if (new_keys == NULL || new_values == NULL || new_occupied == NULL) {
        free(new_keys); free(new_values); free(new_occupied);
        pgy_runtime_panic_collection_oom("map_grow_i32_string", "allocation failed");
        return false;
    }
    m->capacity = new_capacity; m->keys = new_keys; m->values = new_values;
    m->occupied = new_occupied; m->count = 0; m->deleted_count = 0;
    for (size_t i = 0; i < old_capacity; i++) {
        uint32_t h;
        if (old_occupied[i] != PGY_HASHMAP_LIVE) continue;
        h = pgy_hashmap_hash_i32(old_keys[i]) % (uint32_t)new_capacity;
        while (m->occupied[h] == PGY_HASHMAP_LIVE)
            h = (h + 1) % (uint32_t)new_capacity;
        PGY_HASHMAP_I32_KEYS(m)[h] = old_keys[i];
        m->values[h] = old_values[i]; m->occupied[h] = PGY_HASHMAP_LIVE; m->count++;
    }
    free(old_keys); free(old_values); free(old_occupied);
    return true;
}
#else
;
#endif


PGY_RT_DECL void pgy_map_set_i32_string(PgyHashMap_String *m, int32_t key, const char *val)

#ifndef PGY_RUNTIME_DECLS_ONLY
{
    uint32_t h, first_deleted = UINT32_MAX;
    size_t probes = 0;
    char *owned;
    if (!pgy_map_string_is_initialized(m)) {
        pgy_runtime_panic_invalid_collection("map_set_i32_string", "map is not initialized");
        return;
    }
    if (m->key_storage_kind != PGY_HASHMAP_KEY_STORAGE_I32)
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_INTERNAL_INVARIANT, "map key storage kind mismatch");
    h = pgy_hashmap_hash_i32(key) % (uint32_t)m->capacity;
    while (m->occupied[h] != PGY_HASHMAP_EMPTY && probes < m->capacity) {
        if (m->occupied[h] == PGY_HASHMAP_LIVE && PGY_HASHMAP_I32_KEYS(m)[h] == key) {
            owned = pgy_runtime_strdup(val != NULL ? val : "");
            if (owned == NULL) { pgy_runtime_panic_collection_oom("map_set_i32_string", "value duplication failed"); return; }
            free(m->values[h]); m->values[h] = owned; return;
        }
        if (m->occupied[h] == PGY_HASHMAP_DELETED && first_deleted == UINT32_MAX) first_deleted = h;
        h = (h + 1) % (uint32_t)m->capacity; probes++;
    }
    if ((m->count + m->deleted_count + 1) * 4 > m->capacity * 3) {
        if (!pgy_map_grow_i32_string(m)) return;
        h = pgy_hashmap_hash_i32(key) % (uint32_t)m->capacity;
        first_deleted = UINT32_MAX; probes = 0;
        while (m->occupied[h] == PGY_HASHMAP_LIVE && probes < m->capacity) {
            h = (h + 1) % (uint32_t)m->capacity; probes++;
        }
    }
    if (first_deleted != UINT32_MAX) h = first_deleted;
    else if (probes >= m->capacity) { pgy_runtime_panic_invalid_collection("map_set_i32_string", "map is full"); return; }
    owned = pgy_runtime_strdup(val != NULL ? val : "");
    if (owned == NULL) { pgy_runtime_panic_collection_oom("map_set_i32_string", "value duplication failed"); return; }
    PGY_HASHMAP_I32_KEYS(m)[h] = key; m->values[h] = owned;
    m->occupied[h] = PGY_HASHMAP_LIVE; m->count++;
    if (first_deleted != UINT32_MAX) m->deleted_count--;
}
#else
;
#endif


PGY_RT_DECL char *pgy_map_get_i32_string(PgyHashMap_String *m, int32_t key)

#ifndef PGY_RUNTIME_DECLS_ONLY
{
    if (!pgy_map_string_is_initialized(m))
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_INTERNAL_INVARIANT, "map i32 get on invalid map");
    if (m->key_storage_kind != PGY_HASHMAP_KEY_STORAGE_I32)
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_INTERNAL_INVARIANT, "map key storage kind mismatch");
    uint32_t h = pgy_hashmap_hash_i32(key) % (uint32_t)m->capacity; size_t probes = 0;
    while (m->occupied[h] != PGY_HASHMAP_EMPTY && probes < m->capacity) {
        if (m->occupied[h] == PGY_HASHMAP_LIVE && PGY_HASHMAP_I32_KEYS(m)[h] == key)
            return m->values[h] != NULL ? m->values[h] : "";
        h = (h + 1) % (uint32_t)m->capacity; probes++;
    }
    PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_OUT_OF_BOUNDS, "map key not found");
    return "";
}
#else
;
#endif


PGY_RT_DECL bool pgy_map_has_i32_string(PgyHashMap_String *m, int32_t key)

#ifndef PGY_RUNTIME_DECLS_ONLY
{
    if (!pgy_map_string_is_initialized(m)) return false;
    if (m->key_storage_kind != PGY_HASHMAP_KEY_STORAGE_I32)
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_INTERNAL_INVARIANT, "map key storage kind mismatch");
    uint32_t h = pgy_hashmap_hash_i32(key) % (uint32_t)m->capacity; size_t probes = 0;
    while (m->occupied[h] != PGY_HASHMAP_EMPTY && probes < m->capacity) {
        if (m->occupied[h] == PGY_HASHMAP_LIVE && PGY_HASHMAP_I32_KEYS(m)[h] == key) return true;
        h = (h + 1) % (uint32_t)m->capacity; probes++;
    }
    return false;
}
#else
;
#endif


PGY_RT_DECL void pgy_map_remove_i32_string(PgyHashMap_String *m, int32_t key)

#ifndef PGY_RUNTIME_DECLS_ONLY
{
    if (!pgy_map_string_is_initialized(m))
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_INTERNAL_INVARIANT, "map i32 remove on invalid map");
    if (m->key_storage_kind != PGY_HASHMAP_KEY_STORAGE_I32)
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_INTERNAL_INVARIANT, "map key storage kind mismatch");
    uint32_t h = pgy_hashmap_hash_i32(key) % (uint32_t)m->capacity; size_t probes = 0;
    while (m->occupied[h] != PGY_HASHMAP_EMPTY && probes < m->capacity) {
        if (m->occupied[h] == PGY_HASHMAP_LIVE && PGY_HASHMAP_I32_KEYS(m)[h] == key) {
            PGY_HASHMAP_I32_KEYS(m)[h] = 0; free(m->values[h]); m->values[h] = NULL;
            m->occupied[h] = PGY_HASHMAP_DELETED; m->count--; m->deleted_count++; return;
        }
        h = (h + 1) % (uint32_t)m->capacity; probes++;
    }
    PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_OUT_OF_BOUNDS, "map remove key not found");
}
#else
;
#endif


PGY_RT_DECL bool pgy_map_grow_i64_string(PgyHashMap_String *m)

#ifndef PGY_RUNTIME_DECLS_ONLY
{
    size_t old_capacity = m->capacity;
    int64_t *old_keys = PGY_HASHMAP_I64_KEYS(m);
    char **old_values = m->values;
    uint8_t *old_occupied = m->occupied;
    size_t new_capacity;
    int64_t *new_keys;
    char **new_values;
    uint8_t *new_occupied;
    if (m->key_storage_kind != PGY_HASHMAP_KEY_STORAGE_I64)
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_INTERNAL_INVARIANT,
                          "map key storage kind mismatch");
    if (m->capacity > SIZE_MAX / 2) {
        pgy_runtime_panic_collection_oom("map_grow_i64_string", "capacity overflow");
        return false;
    }
    new_capacity = m->capacity == 0
        ? PGY_HASHMAP_INIT_CAP : m->capacity * 2;
    if (!pgy_map_string_capacity_fits(new_capacity)) {
        pgy_runtime_panic_collection_oom("map_grow_i64_string", "capacity overflow");
        return false;
    }
    new_keys = (int64_t *)PGY_HASHMAP_CALLOC(new_capacity, sizeof(int64_t));
    new_values = (char **)PGY_HASHMAP_CALLOC(new_capacity, sizeof(char *));
    new_occupied = (uint8_t *)PGY_HASHMAP_CALLOC(new_capacity, sizeof(uint8_t));
    if (new_keys == NULL || new_values == NULL || new_occupied == NULL) {
        free(new_keys); free(new_values); free(new_occupied);
        pgy_runtime_panic_collection_oom("map_grow_i64_string", "allocation failed");
        return false;
    }
    m->capacity = new_capacity; m->keys = new_keys; m->values = new_values;
    m->occupied = new_occupied; m->count = 0; m->deleted_count = 0;
    for (size_t i = 0; i < old_capacity; i++) {
        uint32_t h;
        if (old_occupied[i] != PGY_HASHMAP_LIVE) continue;
        h = pgy_hashmap_hash_i64(old_keys[i]) % (uint32_t)new_capacity;
        while (m->occupied[h] == PGY_HASHMAP_LIVE)
            h = (h + 1) % (uint32_t)new_capacity;
        PGY_HASHMAP_I64_KEYS(m)[h] = old_keys[i];
        m->values[h] = old_values[i]; m->occupied[h] = PGY_HASHMAP_LIVE; m->count++;
    }
    free(old_keys); free(old_values); free(old_occupied);
    return true;
}
#else
;
#endif


PGY_RT_DECL void pgy_map_set_i64_string(PgyHashMap_String *m, int64_t key, const char *val)

#ifndef PGY_RUNTIME_DECLS_ONLY
{
    uint32_t h, first_deleted = UINT32_MAX;
    size_t probes = 0;
    char *owned;
    if (!pgy_map_string_is_initialized(m)) {
        pgy_runtime_panic_invalid_collection("map_set_i64_string", "map is not initialized");
        return;
    }
    if (m->key_storage_kind != PGY_HASHMAP_KEY_STORAGE_I64)
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_INTERNAL_INVARIANT, "map key storage kind mismatch");
    h = pgy_hashmap_hash_i64(key) % (uint32_t)m->capacity;
    while (m->occupied[h] != PGY_HASHMAP_EMPTY && probes < m->capacity) {
        if (m->occupied[h] == PGY_HASHMAP_LIVE && PGY_HASHMAP_I64_KEYS(m)[h] == key) {
            owned = pgy_runtime_strdup(val != NULL ? val : "");
            if (owned == NULL) { pgy_runtime_panic_collection_oom("map_set_i64_string", "value duplication failed"); return; }
            free(m->values[h]); m->values[h] = owned; return;
        }
        if (m->occupied[h] == PGY_HASHMAP_DELETED && first_deleted == UINT32_MAX) first_deleted = h;
        h = (h + 1) % (uint32_t)m->capacity; probes++;
    }
    if ((m->count + m->deleted_count + 1) * 4 > m->capacity * 3) {
        if (!pgy_map_grow_i64_string(m)) return;
        h = pgy_hashmap_hash_i64(key) % (uint32_t)m->capacity;
        first_deleted = UINT32_MAX; probes = 0;
        while (m->occupied[h] == PGY_HASHMAP_LIVE && probes < m->capacity) {
            h = (h + 1) % (uint32_t)m->capacity; probes++;
        }
    }
    if (first_deleted != UINT32_MAX) h = first_deleted;
    else if (probes >= m->capacity) { pgy_runtime_panic_invalid_collection("map_set_i64_string", "map is full"); return; }
    owned = pgy_runtime_strdup(val != NULL ? val : "");
    if (owned == NULL) { pgy_runtime_panic_collection_oom("map_set_i64_string", "value duplication failed"); return; }
    PGY_HASHMAP_I64_KEYS(m)[h] = key; m->values[h] = owned;
    m->occupied[h] = PGY_HASHMAP_LIVE; m->count++;
    if (first_deleted != UINT32_MAX) m->deleted_count--;
}
#else
;
#endif


PGY_RT_DECL char *pgy_map_get_i64_string(PgyHashMap_String *m, int64_t key)

#ifndef PGY_RUNTIME_DECLS_ONLY
{
    if (!pgy_map_string_is_initialized(m))
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_INTERNAL_INVARIANT, "map i64 get on invalid map");
    if (m->key_storage_kind != PGY_HASHMAP_KEY_STORAGE_I64)
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_INTERNAL_INVARIANT, "map key storage kind mismatch");
    uint32_t h = pgy_hashmap_hash_i64(key) % (uint32_t)m->capacity; size_t probes = 0;
    while (m->occupied[h] != PGY_HASHMAP_EMPTY && probes < m->capacity) {
        if (m->occupied[h] == PGY_HASHMAP_LIVE && PGY_HASHMAP_I64_KEYS(m)[h] == key)
            return m->values[h] != NULL ? m->values[h] : "";
        h = (h + 1) % (uint32_t)m->capacity; probes++;
    }
    PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_OUT_OF_BOUNDS, "map key not found");
    return "";
}
#else
;
#endif


PGY_RT_DECL bool pgy_map_has_i64_string(PgyHashMap_String *m, int64_t key)

#ifndef PGY_RUNTIME_DECLS_ONLY
{
    if (!pgy_map_string_is_initialized(m)) return false;
    if (m->key_storage_kind != PGY_HASHMAP_KEY_STORAGE_I64)
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_INTERNAL_INVARIANT, "map key storage kind mismatch");
    uint32_t h = pgy_hashmap_hash_i64(key) % (uint32_t)m->capacity; size_t probes = 0;
    while (m->occupied[h] != PGY_HASHMAP_EMPTY && probes < m->capacity) {
        if (m->occupied[h] == PGY_HASHMAP_LIVE && PGY_HASHMAP_I64_KEYS(m)[h] == key) return true;
        h = (h + 1) % (uint32_t)m->capacity; probes++;
    }
    return false;
}
#else
;
#endif


PGY_RT_DECL void pgy_map_remove_i64_string(PgyHashMap_String *m, int64_t key)

#ifndef PGY_RUNTIME_DECLS_ONLY
{
    if (!pgy_map_string_is_initialized(m))
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_INTERNAL_INVARIANT, "map i64 remove on invalid map");
    if (m->key_storage_kind != PGY_HASHMAP_KEY_STORAGE_I64)
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_INTERNAL_INVARIANT, "map key storage kind mismatch");
    uint32_t h = pgy_hashmap_hash_i64(key) % (uint32_t)m->capacity; size_t probes = 0;
    while (m->occupied[h] != PGY_HASHMAP_EMPTY && probes < m->capacity) {
        if (m->occupied[h] == PGY_HASHMAP_LIVE && PGY_HASHMAP_I64_KEYS(m)[h] == key) {
            PGY_HASHMAP_I64_KEYS(m)[h] = 0; free(m->values[h]); m->values[h] = NULL;
            m->occupied[h] = PGY_HASHMAP_DELETED; m->count--; m->deleted_count++; return;
        }
        h = (h + 1) % (uint32_t)m->capacity; probes++;
    }
    PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_OUT_OF_BOUNDS, "map remove key not found");
}
#else
;
#endif


PGY_RT_DECL void pgy_map_set_bool_string(PgyHashMap_String *m, bool key, const char *val)

#ifndef PGY_RUNTIME_DECLS_ONLY
{
    uint32_t h, first_deleted = UINT32_MAX;
    size_t probes = 0;
    char *owned;
    if (!pgy_map_string_is_initialized(m)) {
        pgy_runtime_panic_invalid_collection("map_set_bool_string", "map is not initialized");
        return;
    }
    if (m->key_storage_kind != PGY_HASHMAP_KEY_STORAGE_BOOL)
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_INTERNAL_INVARIANT, "map key storage kind mismatch");
    h = pgy_hashmap_hash_bool(key) % (uint32_t)m->capacity;
    while (m->occupied[h] != PGY_HASHMAP_EMPTY && probes < m->capacity) {
        if (m->occupied[h] == PGY_HASHMAP_LIVE && PGY_HASHMAP_BOOL_KEYS(m)[h] == key) {
            owned = pgy_runtime_strdup(val != NULL ? val : "");
            if (owned == NULL) { pgy_runtime_panic_collection_oom("map_set_bool_string", "value duplication failed"); return; }
            free(m->values[h]); m->values[h] = owned; return;
        }
        if (m->occupied[h] == PGY_HASHMAP_DELETED && first_deleted == UINT32_MAX) first_deleted = h;
        h = (h + 1) % (uint32_t)m->capacity; probes++;
    }
    if ((m->count + m->deleted_count + 1) * 4 > m->capacity * 3) {
        size_t old_capacity = m->capacity;
        bool *old_keys = PGY_HASHMAP_BOOL_KEYS(m);
        char **old_values = m->values;
        uint8_t *old_occupied = m->occupied;
        size_t new_capacity = m->capacity > SIZE_MAX / 2 ? 0 : m->capacity * 2;
        bool *new_keys;
        char **new_values;
        uint8_t *new_occupied;
        if (!pgy_map_string_capacity_fits(new_capacity)) {
            pgy_runtime_panic_collection_oom("map_grow_bool_string", "capacity overflow"); return;
        }
        new_keys = (bool *)PGY_HASHMAP_CALLOC(new_capacity, sizeof(bool));
        new_values = (char **)PGY_HASHMAP_CALLOC(new_capacity, sizeof(char *));
        new_occupied = (uint8_t *)PGY_HASHMAP_CALLOC(new_capacity, sizeof(uint8_t));
        if (new_keys == NULL || new_values == NULL || new_occupied == NULL) {
            free(new_keys); free(new_values); free(new_occupied);
            pgy_runtime_panic_collection_oom("map_grow_bool_string", "allocation failed"); return;
        }
        m->capacity = new_capacity; m->keys = new_keys; m->values = new_values;
        m->occupied = new_occupied; m->count = 0; m->deleted_count = 0;
        for (size_t i = 0; i < old_capacity; i++) {
            uint32_t slot;
            if (old_occupied[i] != PGY_HASHMAP_LIVE) continue;
            slot = pgy_hashmap_hash_bool(old_keys[i]) % (uint32_t)new_capacity;
            while (m->occupied[slot] == PGY_HASHMAP_LIVE) slot = (slot + 1) % (uint32_t)new_capacity;
            PGY_HASHMAP_BOOL_KEYS(m)[slot] = old_keys[i]; m->values[slot] = old_values[i];
            m->occupied[slot] = PGY_HASHMAP_LIVE; m->count++;
        }
        free(old_keys); free(old_values); free(old_occupied);
        h = pgy_hashmap_hash_bool(key) % (uint32_t)m->capacity;
        first_deleted = UINT32_MAX; probes = 0;
        while (m->occupied[h] == PGY_HASHMAP_LIVE && probes < m->capacity) { h = (h + 1) % (uint32_t)m->capacity; probes++; }
    }
    if (first_deleted != UINT32_MAX) h = first_deleted;
    else if (probes >= m->capacity) { pgy_runtime_panic_invalid_collection("map_set_bool_string", "map is full"); return; }
    owned = pgy_runtime_strdup(val != NULL ? val : "");
    if (owned == NULL) { pgy_runtime_panic_collection_oom("map_set_bool_string", "value duplication failed"); return; }
    PGY_HASHMAP_BOOL_KEYS(m)[h] = key; m->values[h] = owned;
    m->occupied[h] = PGY_HASHMAP_LIVE; m->count++;
    if (first_deleted != UINT32_MAX) m->deleted_count--;
}
#else
;
#endif


PGY_RT_DECL char *pgy_map_get_bool_string(PgyHashMap_String *m, bool key)

#ifndef PGY_RUNTIME_DECLS_ONLY
{
    if (!pgy_map_string_is_initialized(m))
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_INTERNAL_INVARIANT, "map bool get on invalid map");
    if (m->key_storage_kind != PGY_HASHMAP_KEY_STORAGE_BOOL)
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_INTERNAL_INVARIANT, "map key storage kind mismatch");
    uint32_t h = pgy_hashmap_hash_bool(key) % (uint32_t)m->capacity; size_t probes = 0;
    while (m->occupied[h] != PGY_HASHMAP_EMPTY && probes < m->capacity) {
        if (m->occupied[h] == PGY_HASHMAP_LIVE && PGY_HASHMAP_BOOL_KEYS(m)[h] == key)
            return m->values[h] != NULL ? m->values[h] : "";
        h = (h + 1) % (uint32_t)m->capacity; probes++;
    }
    PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_OUT_OF_BOUNDS, "map key not found");
    return "";
}
#else
;
#endif


PGY_RT_DECL bool pgy_map_has_bool_string(PgyHashMap_String *m, bool key)

#ifndef PGY_RUNTIME_DECLS_ONLY
{
    if (!pgy_map_string_is_initialized(m)) return false;
    if (m->key_storage_kind != PGY_HASHMAP_KEY_STORAGE_BOOL)
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_INTERNAL_INVARIANT, "map key storage kind mismatch");
    uint32_t h = pgy_hashmap_hash_bool(key) % (uint32_t)m->capacity; size_t probes = 0;
    while (m->occupied[h] != PGY_HASHMAP_EMPTY && probes < m->capacity) {
        if (m->occupied[h] == PGY_HASHMAP_LIVE && PGY_HASHMAP_BOOL_KEYS(m)[h] == key) return true;
        h = (h + 1) % (uint32_t)m->capacity; probes++;
    }
    return false;
}
#else
;
#endif


PGY_RT_DECL void pgy_map_remove_bool_string(PgyHashMap_String *m, bool key)

#ifndef PGY_RUNTIME_DECLS_ONLY
{
    if (!pgy_map_string_is_initialized(m))
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_INTERNAL_INVARIANT, "map bool remove on invalid map");
    if (m->key_storage_kind != PGY_HASHMAP_KEY_STORAGE_BOOL)
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_INTERNAL_INVARIANT, "map key storage kind mismatch");
    uint32_t h = pgy_hashmap_hash_bool(key) % (uint32_t)m->capacity; size_t probes = 0;
    while (m->occupied[h] != PGY_HASHMAP_EMPTY && probes < m->capacity) {
        if (m->occupied[h] == PGY_HASHMAP_LIVE && PGY_HASHMAP_BOOL_KEYS(m)[h] == key) {
            PGY_HASHMAP_BOOL_KEYS(m)[h] = false; free(m->values[h]); m->values[h] = NULL;
            m->occupied[h] = PGY_HASHMAP_DELETED; m->count--; m->deleted_count++; return;
        }
        h = (h + 1) % (uint32_t)m->capacity; probes++;
    }
    PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_OUT_OF_BOUNDS, "map remove key not found");
}
#else
;
#endif


#include "pgy_runtime_map_keys_inline.h"
