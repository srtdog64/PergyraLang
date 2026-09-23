#ifndef PGY_RUNTIME_MAP_INT_KEY_INLINE_H
#define PGY_RUNTIME_MAP_INT_KEY_INLINE_H

#include "pgy_runtime_linkage.h"

PGY_RT_DECL int32_t pgy_map_size_int(PgyHashMap_Int *m)

#ifndef PGY_RUNTIME_DECLS_ONLY
{
    return pgy_map_int_is_initialized(m) ? (int32_t)m->count : 0;
}
#else
;
#endif


PGY_RT_DECL void pgy_map_set_i32_int(PgyHashMap_Int *m, int32_t key, int32_t val)

#ifndef PGY_RUNTIME_DECLS_ONLY
{
    uint32_t h, first_deleted = UINT32_MAX;
    size_t probes = 0;
    if (!pgy_map_int_is_initialized(m)) {
        pgy_runtime_panic_invalid_collection("map_set_i32_int", "map is not initialized");
        return;
    }
    pgy_map_int_require_storage(m, PGY_HASHMAP_KEY_STORAGE_I32);
    h = pgy_hashmap_hash_i32(key) % (uint32_t)m->capacity;
    while (m->occupied[h] != PGY_HASHMAP_EMPTY && probes < m->capacity) {
        if (m->occupied[h] == PGY_HASHMAP_LIVE
            && PGY_HASHMAP_I32_KEYS(m)[h] == key) {
            m->values[h] = val; return;
        }
        if (m->occupied[h] == PGY_HASHMAP_DELETED && first_deleted == UINT32_MAX)
            first_deleted = h;
        h = (h + 1) % (uint32_t)m->capacity; probes++;
    }
    if ((m->count + m->deleted_count + 1) * 4 > m->capacity * 3) {
        if (!pgy_map_grow_int(m)) return;
        h = pgy_hashmap_hash_i32(key) % (uint32_t)m->capacity;
        first_deleted = UINT32_MAX; probes = 0;
        while (m->occupied[h] == PGY_HASHMAP_LIVE && probes < m->capacity) {
            h = (h + 1) % (uint32_t)m->capacity; probes++;
        }
    }
    if (first_deleted != UINT32_MAX) h = first_deleted;
    else if (probes >= m->capacity) {
        pgy_runtime_panic_invalid_collection("map_set_i32_int", "map is full"); return;
    }
    PGY_HASHMAP_I32_KEYS(m)[h] = key; m->values[h] = val;
    m->occupied[h] = PGY_HASHMAP_LIVE; m->count++;
    if (first_deleted != UINT32_MAX) m->deleted_count--;
}
#else
;
#endif


PGY_RT_DECL int32_t pgy_map_get_i32_int(PgyHashMap_Int *m, int32_t key)

#ifndef PGY_RUNTIME_DECLS_ONLY
{
    if (!pgy_map_int_is_initialized(m))
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_INTERNAL_INVARIANT, "map i32 get on invalid map");
    pgy_map_int_require_storage(m, PGY_HASHMAP_KEY_STORAGE_I32);
    uint32_t h = pgy_hashmap_hash_i32(key) % (uint32_t)m->capacity; size_t probes = 0;
    while (m->occupied[h] != PGY_HASHMAP_EMPTY && probes < m->capacity) {
        if (m->occupied[h] == PGY_HASHMAP_LIVE
            && PGY_HASHMAP_I32_KEYS(m)[h] == key) return m->values[h];
        h = (h + 1) % (uint32_t)m->capacity; probes++;
    }
    PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_OUT_OF_BOUNDS, "map key not found");
    return 0;
}
#else
;
#endif


PGY_RT_DECL bool pgy_map_has_i32_int(PgyHashMap_Int *m, int32_t key)

#ifndef PGY_RUNTIME_DECLS_ONLY
{
    if (!pgy_map_int_is_initialized(m)) return false;
    pgy_map_int_require_storage(m, PGY_HASHMAP_KEY_STORAGE_I32);
    uint32_t h = pgy_hashmap_hash_i32(key) % (uint32_t)m->capacity; size_t probes = 0;
    while (m->occupied[h] != PGY_HASHMAP_EMPTY && probes < m->capacity) {
        if (m->occupied[h] == PGY_HASHMAP_LIVE
            && PGY_HASHMAP_I32_KEYS(m)[h] == key) return true;
        h = (h + 1) % (uint32_t)m->capacity; probes++;
    }
    return false;
}
#else
;
#endif


PGY_RT_DECL void pgy_map_remove_i32_int(PgyHashMap_Int *m, int32_t key)

#ifndef PGY_RUNTIME_DECLS_ONLY
{
    if (!pgy_map_int_is_initialized(m))
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_INTERNAL_INVARIANT, "map i32 remove on invalid map");
    pgy_map_int_require_storage(m, PGY_HASHMAP_KEY_STORAGE_I32);
    uint32_t h = pgy_hashmap_hash_i32(key) % (uint32_t)m->capacity; size_t probes = 0;
    while (m->occupied[h] != PGY_HASHMAP_EMPTY && probes < m->capacity) {
        if (m->occupied[h] == PGY_HASHMAP_LIVE
            && PGY_HASHMAP_I32_KEYS(m)[h] == key) {
            PGY_HASHMAP_I32_KEYS(m)[h] = 0; m->values[h] = 0;
            m->occupied[h] = PGY_HASHMAP_DELETED; m->count--; m->deleted_count++; return;
        }
        h = (h + 1) % (uint32_t)m->capacity; probes++;
    }
    PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_OUT_OF_BOUNDS, "map remove key not found");
}
#else
;
#endif


PGY_RT_DECL void pgy_map_set_i64_int(PgyHashMap_Int *m, int64_t key, int32_t val)

#ifndef PGY_RUNTIME_DECLS_ONLY
{
    uint32_t h, first_deleted = UINT32_MAX;
    size_t probes = 0;
    if (!pgy_map_int_is_initialized(m)) {
        pgy_runtime_panic_invalid_collection("map_set_i64_int", "map is not initialized");
        return;
    }
    pgy_map_int_require_storage(m, PGY_HASHMAP_KEY_STORAGE_I64);
    h = pgy_hashmap_hash_i64(key) % (uint32_t)m->capacity;
    while (m->occupied[h] != PGY_HASHMAP_EMPTY && probes < m->capacity) {
        if (m->occupied[h] == PGY_HASHMAP_LIVE
            && PGY_HASHMAP_I64_KEYS(m)[h] == key) {
            m->values[h] = val; return;
        }
        if (m->occupied[h] == PGY_HASHMAP_DELETED && first_deleted == UINT32_MAX)
            first_deleted = h;
        h = (h + 1) % (uint32_t)m->capacity; probes++;
    }
    if ((m->count + m->deleted_count + 1) * 4 > m->capacity * 3) {
        if (!pgy_map_grow_int(m)) return;
        h = pgy_hashmap_hash_i64(key) % (uint32_t)m->capacity;
        first_deleted = UINT32_MAX; probes = 0;
        while (m->occupied[h] == PGY_HASHMAP_LIVE && probes < m->capacity) {
            h = (h + 1) % (uint32_t)m->capacity; probes++;
        }
    }
    if (first_deleted != UINT32_MAX) h = first_deleted;
    else if (probes >= m->capacity) {
        pgy_runtime_panic_invalid_collection("map_set_i64_int", "map is full"); return;
    }
    PGY_HASHMAP_I64_KEYS(m)[h] = key; m->values[h] = val;
    m->occupied[h] = PGY_HASHMAP_LIVE; m->count++;
    if (first_deleted != UINT32_MAX) m->deleted_count--;
}
#else
;
#endif


PGY_RT_DECL int32_t pgy_map_get_i64_int(PgyHashMap_Int *m, int64_t key)

#ifndef PGY_RUNTIME_DECLS_ONLY
{
    if (!pgy_map_int_is_initialized(m))
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_INTERNAL_INVARIANT, "map i64 get on invalid map");
    pgy_map_int_require_storage(m, PGY_HASHMAP_KEY_STORAGE_I64);
    uint32_t h = pgy_hashmap_hash_i64(key) % (uint32_t)m->capacity; size_t probes = 0;
    while (m->occupied[h] != PGY_HASHMAP_EMPTY && probes < m->capacity) {
        if (m->occupied[h] == PGY_HASHMAP_LIVE
            && PGY_HASHMAP_I64_KEYS(m)[h] == key) return m->values[h];
        h = (h + 1) % (uint32_t)m->capacity; probes++;
    }
    PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_OUT_OF_BOUNDS, "map key not found");
    return 0;
}
#else
;
#endif


PGY_RT_DECL bool pgy_map_has_i64_int(PgyHashMap_Int *m, int64_t key)

#ifndef PGY_RUNTIME_DECLS_ONLY
{
    if (!pgy_map_int_is_initialized(m)) return false;
    pgy_map_int_require_storage(m, PGY_HASHMAP_KEY_STORAGE_I64);
    uint32_t h = pgy_hashmap_hash_i64(key) % (uint32_t)m->capacity; size_t probes = 0;
    while (m->occupied[h] != PGY_HASHMAP_EMPTY && probes < m->capacity) {
        if (m->occupied[h] == PGY_HASHMAP_LIVE
            && PGY_HASHMAP_I64_KEYS(m)[h] == key) return true;
        h = (h + 1) % (uint32_t)m->capacity; probes++;
    }
    return false;
}
#else
;
#endif


PGY_RT_DECL void pgy_map_remove_i64_int(PgyHashMap_Int *m, int64_t key)

#ifndef PGY_RUNTIME_DECLS_ONLY
{
    if (!pgy_map_int_is_initialized(m))
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_INTERNAL_INVARIANT, "map i64 remove on invalid map");
    pgy_map_int_require_storage(m, PGY_HASHMAP_KEY_STORAGE_I64);
    uint32_t h = pgy_hashmap_hash_i64(key) % (uint32_t)m->capacity; size_t probes = 0;
    while (m->occupied[h] != PGY_HASHMAP_EMPTY && probes < m->capacity) {
        if (m->occupied[h] == PGY_HASHMAP_LIVE
            && PGY_HASHMAP_I64_KEYS(m)[h] == key) {
            PGY_HASHMAP_I64_KEYS(m)[h] = 0; m->values[h] = 0;
            m->occupied[h] = PGY_HASHMAP_DELETED; m->count--; m->deleted_count++; return;
        }
        h = (h + 1) % (uint32_t)m->capacity; probes++;
    }
    PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_OUT_OF_BOUNDS, "map remove key not found");
}
#else
;
#endif


PGY_RT_DECL void pgy_map_set_bool_int(PgyHashMap_Int *m, bool key, int32_t val)

#ifndef PGY_RUNTIME_DECLS_ONLY
{
    uint32_t h, first_deleted = UINT32_MAX;
    size_t probes = 0;
    if (!pgy_map_int_is_initialized(m)) {
        pgy_runtime_panic_invalid_collection("map_set_bool_int", "map is not initialized");
        return;
    }
    pgy_map_int_require_storage(m, PGY_HASHMAP_KEY_STORAGE_BOOL);
    h = pgy_hashmap_hash_bool(key) % (uint32_t)m->capacity;
    while (m->occupied[h] != PGY_HASHMAP_EMPTY && probes < m->capacity) {
        if (m->occupied[h] == PGY_HASHMAP_LIVE
            && PGY_HASHMAP_BOOL_KEYS(m)[h] == key) {
            m->values[h] = val; return;
        }
        if (m->occupied[h] == PGY_HASHMAP_DELETED && first_deleted == UINT32_MAX)
            first_deleted = h;
        h = (h + 1) % (uint32_t)m->capacity; probes++;
    }
    if ((m->count + m->deleted_count + 1) * 4 > m->capacity * 3) {
        if (!pgy_map_grow_int(m)) return;
        h = pgy_hashmap_hash_bool(key) % (uint32_t)m->capacity;
        first_deleted = UINT32_MAX; probes = 0;
        while (m->occupied[h] == PGY_HASHMAP_LIVE && probes < m->capacity) {
            h = (h + 1) % (uint32_t)m->capacity; probes++;
        }
    }
    if (first_deleted != UINT32_MAX) h = first_deleted;
    else if (probes >= m->capacity) {
        pgy_runtime_panic_invalid_collection("map_set_bool_int", "map is full"); return;
    }
    PGY_HASHMAP_BOOL_KEYS(m)[h] = key; m->values[h] = val;
    m->occupied[h] = PGY_HASHMAP_LIVE; m->count++;
    if (first_deleted != UINT32_MAX) m->deleted_count--;
}
#else
;
#endif


PGY_RT_DECL int32_t pgy_map_get_bool_int(PgyHashMap_Int *m, bool key)

#ifndef PGY_RUNTIME_DECLS_ONLY
{
    if (!pgy_map_int_is_initialized(m))
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_INTERNAL_INVARIANT, "map bool get on invalid map");
    pgy_map_int_require_storage(m, PGY_HASHMAP_KEY_STORAGE_BOOL);
    uint32_t h = pgy_hashmap_hash_bool(key) % (uint32_t)m->capacity; size_t probes = 0;
    while (m->occupied[h] != PGY_HASHMAP_EMPTY && probes < m->capacity) {
        if (m->occupied[h] == PGY_HASHMAP_LIVE
            && PGY_HASHMAP_BOOL_KEYS(m)[h] == key) return m->values[h];
        h = (h + 1) % (uint32_t)m->capacity; probes++;
    }
    PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_OUT_OF_BOUNDS, "map key not found");
    return 0;
}
#else
;
#endif


PGY_RT_DECL bool pgy_map_has_bool_int(PgyHashMap_Int *m, bool key)

#ifndef PGY_RUNTIME_DECLS_ONLY
{
    if (!pgy_map_int_is_initialized(m)) return false;
    pgy_map_int_require_storage(m, PGY_HASHMAP_KEY_STORAGE_BOOL);
    uint32_t h = pgy_hashmap_hash_bool(key) % (uint32_t)m->capacity; size_t probes = 0;
    while (m->occupied[h] != PGY_HASHMAP_EMPTY && probes < m->capacity) {
        if (m->occupied[h] == PGY_HASHMAP_LIVE
            && PGY_HASHMAP_BOOL_KEYS(m)[h] == key) return true;
        h = (h + 1) % (uint32_t)m->capacity; probes++;
    }
    return false;
}
#else
;
#endif


PGY_RT_DECL void pgy_map_remove_bool_int(PgyHashMap_Int *m, bool key)

#ifndef PGY_RUNTIME_DECLS_ONLY
{
    if (!pgy_map_int_is_initialized(m))
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_INTERNAL_INVARIANT, "map bool remove on invalid map");
    pgy_map_int_require_storage(m, PGY_HASHMAP_KEY_STORAGE_BOOL);
    uint32_t h = pgy_hashmap_hash_bool(key) % (uint32_t)m->capacity; size_t probes = 0;
    while (m->occupied[h] != PGY_HASHMAP_EMPTY && probes < m->capacity) {
        if (m->occupied[h] == PGY_HASHMAP_LIVE
            && PGY_HASHMAP_BOOL_KEYS(m)[h] == key) {
            PGY_HASHMAP_BOOL_KEYS(m)[h] = false; m->values[h] = 0;
            m->occupied[h] = PGY_HASHMAP_DELETED; m->count--; m->deleted_count++; return;
        }
        h = (h + 1) % (uint32_t)m->capacity; probes++;
    }
    PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_OUT_OF_BOUNDS, "map remove key not found");
}
#else
;
#endif


#endif /* PGY_RUNTIME_MAP_INT_KEY_INLINE_H */
