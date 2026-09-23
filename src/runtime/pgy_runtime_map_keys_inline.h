#ifndef PGY_RUNTIME_MAP_KEYS_INLINE_H
#define PGY_RUNTIME_MAP_KEYS_INLINE_H

/* Typed deterministic key projections for the string-key map storage owner. */
#define PGY_DEFINE_MAP_KEYS_EXPORTS(TypeSuffix, FuncSuffix) \
static inline PgyArray_String pgy_map_keys_##FuncSuffix(PgyHashMap_##TypeSuffix *m) \
{ \
    PgyArray_String out = {0}; \
    if (m == NULL) { \
        pgy_runtime_panic_invalid_collection("map_keys_" #FuncSuffix, "null map"); \
        return out; \
    } \
    if (m->capacity == 0 || m->capacity > (size_t)INT32_MAX \
        || m->keys == NULL || m->values == NULL || m->occupied == NULL) \
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_INTERNAL_INVARIANT, "map keys on invalid map"); \
    if (m->key_storage_kind != PGY_HASHMAP_KEY_STORAGE_STRING) \
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_INTERNAL_INVARIANT, "map key storage kind mismatch"); \
    out = pgy_array_new_String(m->count); \
    if (m->count == 0 || m->keys == NULL || m->occupied == NULL) \
        return out; \
    for (size_t i = 0; i < m->capacity; i++) { \
        char *dup_key; \
        if (m->occupied[i] != PGY_HASHMAP_LIVE || PGY_HASHMAP_STRING_KEYS(m)[i] == NULL) \
            continue; \
        dup_key = pgy_runtime_strdup(PGY_HASHMAP_STRING_KEYS(m)[i]); \
        if (dup_key == NULL) { \
            PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_OOM, PGY_RUNTIME_PANIC_REASON_ALLOCATION_FAILED); \
        } \
        pgy_array_push_String(&out, dup_key); \
    } \
    pgy_array_sort_String(out.data, out.length); \
    return out; \
} \
\
static inline PgyArray_Int pgy_map_keys_i32_##FuncSuffix(PgyHashMap_##TypeSuffix *m) \
{ \
    PgyArray_Int out = {0}; \
    if (m == NULL) { \
        pgy_runtime_panic_invalid_collection("map_keys_i32_" #FuncSuffix, "null map"); \
        return out; \
    } \
    if (m->capacity == 0 || m->capacity > (size_t)INT32_MAX \
        || m->keys == NULL || m->values == NULL || m->occupied == NULL) \
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_INTERNAL_INVARIANT, "map keys on invalid map"); \
    if (m->key_storage_kind != PGY_HASHMAP_KEY_STORAGE_I32) \
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_INTERNAL_INVARIANT, "map key storage kind mismatch"); \
    out = pgy_array_new_Int(m->count); \
    if (m->count == 0 || m->keys == NULL || m->occupied == NULL) \
        return out; \
    for (size_t i = 0; i < m->capacity; i++) { \
        if (m->occupied[i] != PGY_HASHMAP_LIVE) \
            continue; \
        pgy_array_push_Int(&out, PGY_HASHMAP_I32_KEYS(m)[i]); \
    } \
    pgy_array_sort_Int(out.data, out.length); \
    return out; \
} \
\
static inline PgyArray_Long pgy_map_keys_i64_##FuncSuffix(PgyHashMap_##TypeSuffix *m) \
{ \
    PgyArray_Long out = {0}; \
    if (m == NULL) { \
        pgy_runtime_panic_invalid_collection("map_keys_i64_" #FuncSuffix, "null map"); \
        return out; \
    } \
    if (m->capacity == 0 || m->capacity > (size_t)INT32_MAX \
        || m->keys == NULL || m->values == NULL || m->occupied == NULL) \
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_INTERNAL_INVARIANT, "map keys on invalid map"); \
    if (m->key_storage_kind != PGY_HASHMAP_KEY_STORAGE_I64) \
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_INTERNAL_INVARIANT, "map key storage kind mismatch"); \
    out = pgy_array_new_Long(m->count); \
    if (m->count == 0 || m->keys == NULL || m->occupied == NULL) \
        return out; \
    for (size_t i = 0; i < m->capacity; i++) { \
        if (m->occupied[i] != PGY_HASHMAP_LIVE) \
            continue; \
        pgy_array_push_Long(&out, PGY_HASHMAP_I64_KEYS(m)[i]); \
    } \
    pgy_array_sort_Long(out.data, out.length); \
    return out; \
} \
\
static inline PgyArray_Bool pgy_map_keys_bool_##FuncSuffix(PgyHashMap_##TypeSuffix *m) \
{ \
    PgyArray_Bool out = {0}; \
    if (m == NULL) { \
        pgy_runtime_panic_invalid_collection("map_keys_bool_" #FuncSuffix, "null map"); \
        return out; \
    } \
    if (m->capacity == 0 || m->capacity > (size_t)INT32_MAX \
        || m->keys == NULL || m->values == NULL || m->occupied == NULL) \
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_INTERNAL_INVARIANT, "map keys on invalid map"); \
    if (m->key_storage_kind != PGY_HASHMAP_KEY_STORAGE_BOOL) \
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_INTERNAL_INVARIANT, "map key storage kind mismatch"); \
    out = pgy_array_new_Bool(m->count); \
    if (m->count == 0 || m->keys == NULL || m->occupied == NULL) \
        return out; \
    for (size_t i = 0; i < m->capacity; i++) { \
        if (m->occupied[i] != PGY_HASHMAP_LIVE) \
            continue; \
        pgy_array_push_Bool(&out, PGY_HASHMAP_BOOL_KEYS(m)[i]); \
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

#endif /* PGY_RUNTIME_MAP_KEYS_INLINE_H */
