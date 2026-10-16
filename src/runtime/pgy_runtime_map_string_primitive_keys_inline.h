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
    new_capacity = pgy_hashmap_rebuild_capacity(m->capacity, m->count, PGY_HASHMAP_INIT_CAP);
    if (new_capacity == 0) {
        pgy_runtime_panic_collection_oom("map_grow_i32_string", "capacity overflow");
        return false;
    }
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
    new_capacity = pgy_hashmap_rebuild_capacity(m->capacity, m->count, PGY_HASHMAP_INIT_CAP);
    if (new_capacity == 0) {
        pgy_runtime_panic_collection_oom("map_grow_i64_string", "capacity overflow");
        return false;
    }
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
        size_t new_capacity = pgy_hashmap_rebuild_capacity(m->capacity, m->count, PGY_HASHMAP_INIT_CAP);
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
