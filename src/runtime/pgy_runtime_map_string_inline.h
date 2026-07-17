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
    char   **keys;
    char   **values;
    uint8_t *occupied;
    size_t   count;
    size_t   capacity;
} PgyHashMap_String;

PGY_RT_DECL bool pgy_map_string_is_initialized(const PgyHashMap_String *m)

#ifndef PGY_RUNTIME_DECLS_ONLY
{
    return m != NULL
        && pgy_map_string_capacity_fits(m->capacity)
        && m->keys != NULL
        && m->values != NULL
        && m->occupied != NULL;
}
#else
;
#endif


PGY_RT_DECL PgyHashMap_String pgy_map_new_string(void)

#ifndef PGY_RUNTIME_DECLS_ONLY
{
    PgyHashMap_String m;
    m.capacity = PGY_HASHMAP_INIT_CAP;
    m.count = 0;
    if (!pgy_map_string_capacity_fits(m.capacity)) {
        m.keys = NULL; m.values = NULL; m.occupied = NULL; m.capacity = 0;
        pgy_runtime_warn_invalid_collection("map_new_string", "allocation size overflow");
        return m;
    }
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
#else
;
#endif


PGY_RT_DECL void pgy_map_set_string(PgyHashMap_String *m, const char *key, const char *val)

#ifndef PGY_RUNTIME_DECLS_ONLY
{
    if (!pgy_map_string_is_initialized(m)) {
        pgy_runtime_warn_invalid_collection("map_set_string", "map is not initialized");
        return;
    }
    if (key == NULL) {
        pgy_runtime_warn_invalid_collection("map_set_string", "null key");
        return;
    }
    if ((double)m->count / (double)m->capacity > PGY_HASHMAP_LOAD_FACTOR) {
        size_t old_cap = m->capacity;
        char **ok = m->keys; char **ov = m->values; uint8_t *oo = m->occupied;
        size_t new_capacity;
        char **new_keys;
        char **new_values;
        uint8_t *new_occupied;
        if (m->capacity == 0) {
            new_capacity = PGY_HASHMAP_INIT_CAP;
        } else {
            if (m->capacity > SIZE_MAX / 2) {
                pgy_runtime_warn_invalid_collection("map_set_string", "capacity overflow");
                return;
            }
            new_capacity = m->capacity * 2;
        }
        if (!pgy_map_string_capacity_fits(new_capacity)) {
            pgy_runtime_warn_invalid_collection("map_set_string", "allocation size overflow");
            return;
        }
        new_keys = (char **)calloc(new_capacity, sizeof(char *));
        new_values = (char **)calloc(new_capacity, sizeof(char *));
        new_occupied = (uint8_t *)calloc(new_capacity, sizeof(uint8_t));
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
    if (m->count == 0) return false;
    uint32_t h = pgy_hash_string(key) % (uint32_t)m->capacity;
    size_t p = 0;
    while (m->occupied[h] && p < m->capacity) {
        if (m->keys[h] && strcmp(m->keys[h], key) == 0) return true;
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


PGY_RT_DECL void pgy_map_set_i32_string(PgyHashMap_String *m, int32_t key, const char *val)

#ifndef PGY_RUNTIME_DECLS_ONLY
{
    char *key_str = pgy_map_format_i32_key(key);
    if (key_str == NULL) {
        pgy_runtime_warn_invalid_collection("map_set_i32_string", "key formatting failed");
        return;
    }
    pgy_map_set_string(m, key_str, val);
    free(key_str);
}
#else
;
#endif


PGY_RT_DECL char *pgy_map_get_i32_string(PgyHashMap_String *m, int32_t key)

#ifndef PGY_RUNTIME_DECLS_ONLY
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
#else
;
#endif


PGY_RT_DECL bool pgy_map_has_i32_string(PgyHashMap_String *m, int32_t key)

#ifndef PGY_RUNTIME_DECLS_ONLY
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
#else
;
#endif


PGY_RT_DECL void pgy_map_remove_i32_string(PgyHashMap_String *m, int32_t key)

#ifndef PGY_RUNTIME_DECLS_ONLY
{
    char *key_str = pgy_map_format_i32_key(key);
    if (key_str == NULL) {
        pgy_runtime_warn_invalid_collection("map_remove_i32_string", "key formatting failed");
        return;
    }
    pgy_map_remove_string(m, key_str);
    free(key_str);
}
#else
;
#endif


PGY_RT_DECL void pgy_map_set_i64_string(PgyHashMap_String *m, int64_t key, const char *val)

#ifndef PGY_RUNTIME_DECLS_ONLY
{
    char *key_str = pgy_map_format_i64_key(key);
    if (key_str == NULL) {
        pgy_runtime_warn_invalid_collection("map_set_i64_string", "key formatting failed");
        return;
    }
    pgy_map_set_string(m, key_str, val);
    free(key_str);
}
#else
;
#endif


PGY_RT_DECL char *pgy_map_get_i64_string(PgyHashMap_String *m, int64_t key)

#ifndef PGY_RUNTIME_DECLS_ONLY
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
#else
;
#endif


PGY_RT_DECL bool pgy_map_has_i64_string(PgyHashMap_String *m, int64_t key)

#ifndef PGY_RUNTIME_DECLS_ONLY
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
#else
;
#endif


PGY_RT_DECL void pgy_map_remove_i64_string(PgyHashMap_String *m, int64_t key)

#ifndef PGY_RUNTIME_DECLS_ONLY
{
    char *key_str = pgy_map_format_i64_key(key);
    if (key_str == NULL) {
        pgy_runtime_warn_invalid_collection("map_remove_i64_string", "key formatting failed");
        return;
    }
    pgy_map_remove_string(m, key_str);
    free(key_str);
}
#else
;
#endif


PGY_RT_DECL void pgy_map_set_bool_string(PgyHashMap_String *m, bool key, const char *val)

#ifndef PGY_RUNTIME_DECLS_ONLY
{
    char *key_str = pgy_map_format_bool_key(key);
    if (key_str == NULL) {
        pgy_runtime_warn_invalid_collection("map_set_bool_string", "key formatting failed");
        return;
    }
    pgy_map_set_string(m, key_str, val);
    free(key_str);
}
#else
;
#endif


PGY_RT_DECL char *pgy_map_get_bool_string(PgyHashMap_String *m, bool key)

#ifndef PGY_RUNTIME_DECLS_ONLY
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
#else
;
#endif


PGY_RT_DECL bool pgy_map_has_bool_string(PgyHashMap_String *m, bool key)

#ifndef PGY_RUNTIME_DECLS_ONLY
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
#else
;
#endif


PGY_RT_DECL void pgy_map_remove_bool_string(PgyHashMap_String *m, bool key)

#ifndef PGY_RUNTIME_DECLS_ONLY
{
    char *key_str = pgy_map_format_bool_key(key);
    if (key_str == NULL) {
        pgy_runtime_warn_invalid_collection("map_remove_bool_string", "key formatting failed");
        return;
    }
    pgy_map_remove_string(m, key_str);
    free(key_str);
}
#else
;
#endif


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
    pgy_array_sort_String(out.data, out.length); \
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
    pgy_array_sort_Int(out.data, out.length); \
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
    pgy_array_sort_Long(out.data, out.length); \
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
    pgy_map_keys_sort_bool_array(&out); \
    return out; \
}

PGY_DEFINE_MAP_KEYS_EXPORTS(Int, int)
PGY_DEFINE_MAP_KEYS_EXPORTS(String, string)

void pgy_map_keys_raw_export(void *map_ptr, void *out_array_ptr);
void pgy_map_keys_raw_i32_export(void *map_ptr, void *out_array_ptr);
void pgy_map_keys_raw_i64_export(void *map_ptr, void *out_array_ptr);
void pgy_map_keys_raw_bool_export(void *map_ptr, void *out_array_ptr);
