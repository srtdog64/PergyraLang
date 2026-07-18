#ifndef PGY_RUNTIME_MAP_KEYS_INLINE_H
#define PGY_RUNTIME_MAP_KEYS_INLINE_H

/* Typed deterministic key projections for the string-key map storage owner. */
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

#endif /* PGY_RUNTIME_MAP_KEYS_INLINE_H */
