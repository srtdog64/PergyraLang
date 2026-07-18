#ifndef PGY_RUNTIME_SET_GENERIC_INLINE_H
#define PGY_RUNTIME_SET_GENERIC_INLINE_H

#include "pgy_runtime_linkage.h"

#ifndef PGY_RUNTIME_SET_IS_INITIALIZED
#error "generic Set<T> runtime requires the concrete set substrate owner"
#endif

/* Generic value-hash Set<T>; String remains on the concrete set path. */
#define PGY_SET_DEFINE(SuffixName, CType) \
typedef struct \
{ \
    CType   *data; \
    uint8_t *occupied; \
    size_t   count; \
    size_t   capacity; \
} PgySet_##SuffixName; \
\
PGY_RT_PROGRAM_DECL uint32_t pgy_set_hash_##SuffixName(CType val) \
PGY_RT_PROGRAM_BODY({ \
    const uint8_t *p = (const uint8_t *)&val; \
    uint32_t h = 2166136261u; \
    for (size_t i = 0; i < sizeof(CType); i++) { h ^= p[i]; h *= 16777619u; } \
    return h; \
}) \
\
PGY_RT_PROGRAM_DECL PgySet_##SuffixName pgy_set_new_##SuffixName(void) \
PGY_RT_PROGRAM_BODY({ \
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
}) \
\
PGY_RT_PROGRAM_DECL bool pgy_set_has_##SuffixName(PgySet_##SuffixName *s, CType val) \
PGY_RT_PROGRAM_BODY({ \
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
}) \
\
PGY_RT_PROGRAM_DECL void pgy_set_add_##SuffixName(PgySet_##SuffixName *s, CType val) \
PGY_RT_PROGRAM_BODY({ \
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
}) \
\
PGY_RT_PROGRAM_DECL void pgy_set_remove_##SuffixName(PgySet_##SuffixName *s, CType val) \
PGY_RT_PROGRAM_BODY({ \
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
}) \
\
PGY_RT_PROGRAM_DECL int32_t pgy_set_size_##SuffixName(PgySet_##SuffixName *s) \
PGY_RT_PROGRAM_BODY({ return PGY_RUNTIME_SET_IS_INITIALIZED(s, CType) ? (int32_t)s->count : 0; })

#define PGY_SET_VALUES_DEFINE(SetSuffixName, CType, ArraySuffixName) \
PGY_RT_PROGRAM_DECL PgyArray_##ArraySuffixName pgy_set_values_##SetSuffixName(PgySet_##SetSuffixName *s) \
PGY_RT_PROGRAM_BODY({ \
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
})

/* Pre-instantiate Set<Int> (lowercase suffix to match collection_runtime_suffix) */
PGY_SET_DEFINE(int, int32_t)
PGY_SET_VALUES_DEFINE(int, int32_t, Int)

/* =================================================================
 * Queue<Int> — ring buffer FIFO
 * ================================================================= */


#endif /* PGY_RUNTIME_SET_GENERIC_INLINE_H */
