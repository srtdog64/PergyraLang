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
        new_capacity = pgy_hashmap_rebuild_capacity(m->capacity, m->count, PGY_HASHMAP_INIT_CAP);
        if (new_capacity == 0) {
            free(owned_key); free(owned_value);
            pgy_runtime_panic_collection_oom("map_set_string", "capacity overflow");
            return;
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


#include "pgy_runtime_map_string_primitive_keys_inline.h"

#include "pgy_runtime_map_keys_inline.h"
