#ifndef PGY_RUNTIME_LIB_RAW_MAP_KEY_EXPORTS_H
#define PGY_RUNTIME_LIB_RAW_MAP_KEY_EXPORTS_H

void
pgy_map_set_raw_i32_export(void *map_ptr, int32_t key, void *value_ptr,
                           int64_t value_size)
{
    PgyHashMapRaw *map = (PgyHashMapRaw *)map_ptr;
    uint32_t h;
    uint32_t first_deleted = UINT32_MAX;
    size_t probes = 0;
    if (map == NULL || value_ptr == NULL || value_size <= 0) {
        pgy_runtime_warn_invalid_collection("map_set_i32", "invalid argument");
        return;
    }
    if (!pgy_map_raw_is_initialized(map)) {
        pgy_runtime_warn_invalid_collection("map_set_i32", "map is not initialized");
        return;
    }
    pgy_map_raw_require_storage_kind(map, PGY_HASHMAP_KEY_STORAGE_I32);
    h = pgy_hashmap_hash_i32(key) % (uint32_t)map->capacity;
    while (map->occupied[h] != PGY_MAP_RAW_EMPTY && probes < map->capacity) {
        if (map->occupied[h] == PGY_MAP_RAW_LIVE
            && PGY_MAP_RAW_I32_KEYS(map)[h] == key) {
            memcpy((char *)map->values + (h * (size_t)value_size),
                   value_ptr, (size_t)value_size);
            return;
        }
        if (map->occupied[h] == PGY_MAP_RAW_DELETED
            && first_deleted == UINT32_MAX)
            first_deleted = h;
        h = (h + 1) % (uint32_t)map->capacity;
        probes++;
    }
    if ((map->count + map->deleted_count + 1) * 4 > map->capacity * 3) {
        if (!pgy_map_grow_raw_export(map, value_size)) return;
        h = pgy_hashmap_hash_i32(key) % (uint32_t)map->capacity;
        first_deleted = UINT32_MAX; probes = 0;
        while (map->occupied[h] == PGY_MAP_RAW_LIVE && probes < map->capacity) {
            h = (h + 1) % (uint32_t)map->capacity; probes++;
        }
    }
    if (first_deleted != UINT32_MAX)
        h = first_deleted;
    else if (probes >= map->capacity) {
        pgy_runtime_warn_invalid_collection("map_set_i32", "map is full");
        return;
    }
    PGY_MAP_RAW_I32_KEYS(map)[h] = key;
    memcpy((char *)map->values + (h * (size_t)value_size),
           value_ptr, (size_t)value_size);
    map->occupied[h] = PGY_MAP_RAW_LIVE;
    map->count++;
    if (first_deleted != UINT32_MAX)
        map->deleted_count--;
}

void
pgy_map_get_raw_i32_export(void *map_ptr, int32_t key, void *out_ptr,
                           int64_t value_size)
{
    PgyHashMapRaw *map = (PgyHashMapRaw *)map_ptr;
    uint32_t h;
    size_t probes = 0;
    if (out_ptr == NULL || value_size <= 0)
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_INTERNAL_INVARIANT,
                          "map i32 get with invalid output");
    memset(out_ptr, 0, (size_t)value_size);
    if (!pgy_map_raw_is_initialized(map))
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_INTERNAL_INVARIANT,
                          "map i32 get on invalid map");
    pgy_map_raw_require_storage_kind(map, PGY_HASHMAP_KEY_STORAGE_I32);
    h = pgy_hashmap_hash_i32(key) % (uint32_t)map->capacity;
    while (map->occupied[h] != PGY_MAP_RAW_EMPTY && probes < map->capacity) {
        if (map->occupied[h] == PGY_MAP_RAW_LIVE
            && PGY_MAP_RAW_I32_KEYS(map)[h] == key) {
            memcpy(out_ptr,
                   (char *)map->values + (h * (size_t)value_size),
                   (size_t)value_size);
            return;
        }
        h = (h + 1) % (uint32_t)map->capacity;
        probes++;
    }
    PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_OUT_OF_BOUNDS,
                      "map key not found");
}

void
pgy_map_set_raw_i64_export(void *map_ptr, int64_t key, void *value_ptr,
                           int64_t value_size)
{
    PgyHashMapRaw *map = (PgyHashMapRaw *)map_ptr;
    uint32_t h;
    uint32_t first_deleted = UINT32_MAX;
    size_t probes = 0;
    if (map == NULL || value_ptr == NULL || value_size <= 0) {
        pgy_runtime_warn_invalid_collection("map_set_i64", "invalid argument");
        return;
    }
    if (!pgy_map_raw_is_initialized(map)) {
        pgy_runtime_warn_invalid_collection("map_set_i64", "map is not initialized");
        return;
    }
    pgy_map_raw_require_storage_kind(map, PGY_HASHMAP_KEY_STORAGE_I64);
    h = pgy_hashmap_hash_i64(key) % (uint32_t)map->capacity;
    while (map->occupied[h] != PGY_MAP_RAW_EMPTY && probes < map->capacity) {
        if (map->occupied[h] == PGY_MAP_RAW_LIVE
            && PGY_MAP_RAW_I64_KEYS(map)[h] == key) {
            memcpy((char *)map->values + (h * (size_t)value_size),
                   value_ptr, (size_t)value_size);
            return;
        }
        if (map->occupied[h] == PGY_MAP_RAW_DELETED
            && first_deleted == UINT32_MAX)
            first_deleted = h;
        h = (h + 1) % (uint32_t)map->capacity;
        probes++;
    }
    if ((map->count + map->deleted_count + 1) * 4 > map->capacity * 3) {
        if (!pgy_map_grow_raw_export(map, value_size)) return;
        h = pgy_hashmap_hash_i64(key) % (uint32_t)map->capacity;
        first_deleted = UINT32_MAX; probes = 0;
        while (map->occupied[h] == PGY_MAP_RAW_LIVE && probes < map->capacity) {
            h = (h + 1) % (uint32_t)map->capacity; probes++;
        }
    }
    if (first_deleted != UINT32_MAX)
        h = first_deleted;
    else if (probes >= map->capacity) {
        pgy_runtime_warn_invalid_collection("map_set_i64", "map is full");
        return;
    }
    PGY_MAP_RAW_I64_KEYS(map)[h] = key;
    memcpy((char *)map->values + (h * (size_t)value_size),
           value_ptr, (size_t)value_size);
    map->occupied[h] = PGY_MAP_RAW_LIVE;
    map->count++;
    if (first_deleted != UINT32_MAX)
        map->deleted_count--;
}

void
pgy_map_get_raw_i64_export(void *map_ptr, int64_t key, void *out_ptr,
                           int64_t value_size)
{
    PgyHashMapRaw *map = (PgyHashMapRaw *)map_ptr;
    uint32_t h;
    size_t probes = 0;
    if (out_ptr == NULL || value_size <= 0)
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_INTERNAL_INVARIANT,
                          "map i64 get with invalid output");
    memset(out_ptr, 0, (size_t)value_size);
    if (!pgy_map_raw_is_initialized(map))
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_INTERNAL_INVARIANT,
                          "map i64 get on invalid map");
    pgy_map_raw_require_storage_kind(map, PGY_HASHMAP_KEY_STORAGE_I64);
    h = pgy_hashmap_hash_i64(key) % (uint32_t)map->capacity;
    while (map->occupied[h] != PGY_MAP_RAW_EMPTY && probes < map->capacity) {
        if (map->occupied[h] == PGY_MAP_RAW_LIVE
            && PGY_MAP_RAW_I64_KEYS(map)[h] == key) {
            memcpy(out_ptr,
                   (char *)map->values + (h * (size_t)value_size),
                   (size_t)value_size);
            return;
        }
        h = (h + 1) % (uint32_t)map->capacity;
        probes++;
    }
    PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_OUT_OF_BOUNDS,
                      "map key not found");
}

bool
pgy_map_has_raw_i64_export(void *map_ptr, int64_t key)
{
    PgyHashMapRaw *map = (PgyHashMapRaw *)map_ptr;
    uint32_t h;
    size_t probes = 0;
    if (!pgy_map_raw_is_initialized(map)) {
        pgy_runtime_warn_invalid_collection("map_has_i64", "map is not initialized");
        return false;
    }
    pgy_map_raw_require_storage_kind(map, PGY_HASHMAP_KEY_STORAGE_I64);
    h = pgy_hashmap_hash_i64(key) % (uint32_t)map->capacity;
    while (map->occupied[h] != PGY_MAP_RAW_EMPTY && probes < map->capacity) {
        if (map->occupied[h] == PGY_MAP_RAW_LIVE
            && PGY_MAP_RAW_I64_KEYS(map)[h] == key)
            return true;
        h = (h + 1) % (uint32_t)map->capacity;
        probes++;
    }
    return false;
}

void
pgy_map_remove_raw_i64_export(void *map_ptr, int64_t key, int64_t value_size)
{
    PgyHashMapRaw *map = (PgyHashMapRaw *)map_ptr;
    uint32_t h;
    size_t probes = 0;
    if (value_size <= 0 || !pgy_map_raw_is_initialized(map))
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_INTERNAL_INVARIANT,
                          "map i64 remove on invalid map");
    pgy_map_raw_require_storage_kind(map, PGY_HASHMAP_KEY_STORAGE_I64);
    h = pgy_hashmap_hash_i64(key) % (uint32_t)map->capacity;
    while (map->occupied[h] != PGY_MAP_RAW_EMPTY && probes < map->capacity) {
        if (map->occupied[h] == PGY_MAP_RAW_LIVE
            && PGY_MAP_RAW_I64_KEYS(map)[h] == key) {
            PGY_MAP_RAW_I64_KEYS(map)[h] = 0;
            memset((char *)map->values + (h * (size_t)value_size), 0,
                   (size_t)value_size);
            map->occupied[h] = PGY_MAP_RAW_DELETED;
            map->count--;
            map->deleted_count++;
            return;
        }
        h = (h + 1) % (uint32_t)map->capacity;
        probes++;
    }
    PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_OUT_OF_BOUNDS,
                      "map remove key not found");
}

void
pgy_map_set_raw_bool_export(void *map_ptr, bool key, void *value_ptr,
                            int64_t value_size)
{
    PgyHashMapRaw *map = (PgyHashMapRaw *)map_ptr;
    uint32_t h;
    uint32_t first_deleted = UINT32_MAX;
    size_t probes = 0;
    if (map == NULL || value_ptr == NULL || value_size <= 0) {
        pgy_runtime_warn_invalid_collection("map_set_bool", "invalid argument");
        return;
    }
    if (!pgy_map_raw_is_initialized(map)) {
        pgy_runtime_warn_invalid_collection("map_set_bool", "map is not initialized");
        return;
    }
    pgy_map_raw_require_storage_kind(map, PGY_HASHMAP_KEY_STORAGE_BOOL);
    h = pgy_hashmap_hash_bool(key) % (uint32_t)map->capacity;
    while (map->occupied[h] != PGY_MAP_RAW_EMPTY && probes < map->capacity) {
        if (map->occupied[h] == PGY_MAP_RAW_LIVE
            && PGY_MAP_RAW_BOOL_KEYS(map)[h] == key) {
            memcpy((char *)map->values + (h * (size_t)value_size),
                   value_ptr, (size_t)value_size);
            return;
        }
        if (map->occupied[h] == PGY_MAP_RAW_DELETED
            && first_deleted == UINT32_MAX)
            first_deleted = h;
        h = (h + 1) % (uint32_t)map->capacity;
        probes++;
    }
    if ((map->count + map->deleted_count + 1) * 4 > map->capacity * 3) {
        if (!pgy_map_grow_raw_export(map, value_size)) return;
        h = pgy_hashmap_hash_bool(key) % (uint32_t)map->capacity;
        first_deleted = UINT32_MAX; probes = 0;
        while (map->occupied[h] == PGY_MAP_RAW_LIVE && probes < map->capacity) {
            h = (h + 1) % (uint32_t)map->capacity; probes++;
        }
    }
    if (first_deleted != UINT32_MAX)
        h = first_deleted;
    else if (probes >= map->capacity) {
        pgy_runtime_warn_invalid_collection("map_set_bool", "map is full");
        return;
    }
    PGY_MAP_RAW_BOOL_KEYS(map)[h] = key;
    memcpy((char *)map->values + (h * (size_t)value_size),
           value_ptr, (size_t)value_size);
    map->occupied[h] = PGY_MAP_RAW_LIVE;
    map->count++;
    if (first_deleted != UINT32_MAX)
        map->deleted_count--;
}

void
pgy_map_get_raw_bool_export(void *map_ptr, bool key, void *out_ptr,
                            int64_t value_size)
{
    PgyHashMapRaw *map = (PgyHashMapRaw *)map_ptr;
    uint32_t h;
    size_t probes = 0;
    if (out_ptr == NULL || value_size <= 0)
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_INTERNAL_INVARIANT,
                          "map bool get with invalid output");
    memset(out_ptr, 0, (size_t)value_size);
    if (!pgy_map_raw_is_initialized(map))
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_INTERNAL_INVARIANT,
                          "map bool get on invalid map");
    pgy_map_raw_require_storage_kind(map, PGY_HASHMAP_KEY_STORAGE_BOOL);
    h = pgy_hashmap_hash_bool(key) % (uint32_t)map->capacity;
    while (map->occupied[h] != PGY_MAP_RAW_EMPTY && probes < map->capacity) {
        if (map->occupied[h] == PGY_MAP_RAW_LIVE
            && PGY_MAP_RAW_BOOL_KEYS(map)[h] == key) {
            memcpy(out_ptr,
                   (char *)map->values + (h * (size_t)value_size),
                   (size_t)value_size);
            return;
        }
        h = (h + 1) % (uint32_t)map->capacity;
        probes++;
    }
    PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_OUT_OF_BOUNDS,
                      "map key not found");
}

bool
pgy_map_has_raw_bool_export(void *map_ptr, bool key)
{
    PgyHashMapRaw *map = (PgyHashMapRaw *)map_ptr;
    uint32_t h;
    size_t probes = 0;
    if (!pgy_map_raw_is_initialized(map)) {
        pgy_runtime_warn_invalid_collection("map_has_bool", "map is not initialized");
        return false;
    }
    pgy_map_raw_require_storage_kind(map, PGY_HASHMAP_KEY_STORAGE_BOOL);
    h = pgy_hashmap_hash_bool(key) % (uint32_t)map->capacity;
    while (map->occupied[h] != PGY_MAP_RAW_EMPTY && probes < map->capacity) {
        if (map->occupied[h] == PGY_MAP_RAW_LIVE
            && PGY_MAP_RAW_BOOL_KEYS(map)[h] == key)
            return true;
        h = (h + 1) % (uint32_t)map->capacity;
        probes++;
    }
    return false;
}

void
pgy_map_remove_raw_bool_export(void *map_ptr, bool key, int64_t value_size)
{
    PgyHashMapRaw *map = (PgyHashMapRaw *)map_ptr;
    uint32_t h;
    size_t probes = 0;
    if (value_size <= 0 || !pgy_map_raw_is_initialized(map))
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_INTERNAL_INVARIANT,
                          "map bool remove on invalid map");
    pgy_map_raw_require_storage_kind(map, PGY_HASHMAP_KEY_STORAGE_BOOL);
    h = pgy_hashmap_hash_bool(key) % (uint32_t)map->capacity;
    while (map->occupied[h] != PGY_MAP_RAW_EMPTY && probes < map->capacity) {
        if (map->occupied[h] == PGY_MAP_RAW_LIVE
            && PGY_MAP_RAW_BOOL_KEYS(map)[h] == key) {
            PGY_MAP_RAW_BOOL_KEYS(map)[h] = false;
            memset((char *)map->values + (h * (size_t)value_size), 0,
                   (size_t)value_size);
            map->occupied[h] = PGY_MAP_RAW_DELETED;
            map->count--;
            map->deleted_count++;
            return;
        }
        h = (h + 1) % (uint32_t)map->capacity;
        probes++;
    }
    PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_OUT_OF_BOUNDS,
                      "map remove key not found");
}

bool
pgy_map_has_raw_i32_export(void *map_ptr, int32_t key)
{
    PgyHashMapRaw *map = (PgyHashMapRaw *)map_ptr;
    uint32_t h;
    size_t probes = 0;
    if (!pgy_map_raw_is_initialized(map)) {
        pgy_runtime_warn_invalid_collection("map_has_i32", "map is not initialized");
        return false;
    }
    pgy_map_raw_require_storage_kind(map, PGY_HASHMAP_KEY_STORAGE_I32);
    h = pgy_hashmap_hash_i32(key) % (uint32_t)map->capacity;
    while (map->occupied[h] != PGY_MAP_RAW_EMPTY && probes < map->capacity) {
        if (map->occupied[h] == PGY_MAP_RAW_LIVE
            && PGY_MAP_RAW_I32_KEYS(map)[h] == key)
            return true;
        h = (h + 1) % (uint32_t)map->capacity;
        probes++;
    }
    return false;
}

void
pgy_map_remove_raw_i32_export(void *map_ptr, int32_t key, int64_t value_size)
{
    PgyHashMapRaw *map = (PgyHashMapRaw *)map_ptr;
    uint32_t h;
    size_t probes = 0;
    if (value_size <= 0 || !pgy_map_raw_is_initialized(map))
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_INTERNAL_INVARIANT,
                          "map i32 remove on invalid map");
    pgy_map_raw_require_storage_kind(map, PGY_HASHMAP_KEY_STORAGE_I32);
    h = pgy_hashmap_hash_i32(key) % (uint32_t)map->capacity;
    while (map->occupied[h] != PGY_MAP_RAW_EMPTY && probes < map->capacity) {
        if (map->occupied[h] == PGY_MAP_RAW_LIVE
            && PGY_MAP_RAW_I32_KEYS(map)[h] == key) {
            PGY_MAP_RAW_I32_KEYS(map)[h] = 0;
            memset((char *)map->values + (h * (size_t)value_size), 0,
                   (size_t)value_size);
            map->occupied[h] = PGY_MAP_RAW_DELETED;
            map->count--;
            map->deleted_count++;
            return;
        }
        h = (h + 1) % (uint32_t)map->capacity;
        probes++;
    }
    PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_OUT_OF_BOUNDS,
                      "map remove key not found");
}

void
pgy_map_set_string_value_raw_i32_export(void *map_ptr, int32_t key,
                                        const char *value)
{
    PgyHashMapRaw *map = (PgyHashMapRaw *)map_ptr;
    uint32_t h;
    uint32_t first_deleted = UINT32_MAX;
    size_t probes = 0;
    char *owned;
    if (!pgy_map_raw_is_initialized(map)) {
        pgy_runtime_warn_invalid_collection("map_set_string_value_i32", "map is not initialized");
        return;
    }
    pgy_map_raw_require_storage_kind(map, PGY_HASHMAP_KEY_STORAGE_I32);
    h = pgy_hashmap_hash_i32(key) % (uint32_t)map->capacity;
    while (map->occupied[h] != PGY_MAP_RAW_EMPTY && probes < map->capacity) {
        if (map->occupied[h] == PGY_MAP_RAW_LIVE
            && PGY_MAP_RAW_I32_KEYS(map)[h] == key) {
            char **slot = (char **)((char *)map->values + h * sizeof(char *));
            owned = pgy_runtime_strdup_export(value != NULL ? value : "");
            if (owned == NULL) {
                pgy_runtime_warn_invalid_collection("map_set_string_value_i32", "value duplication failed");
                return;
            }
            free(*slot);
            *slot = owned;
            return;
        }
        if (map->occupied[h] == PGY_MAP_RAW_DELETED
            && first_deleted == UINT32_MAX)
            first_deleted = h;
        h = (h + 1) % (uint32_t)map->capacity;
        probes++;
    }
    if ((map->count + map->deleted_count + 1) * 4 > map->capacity * 3) {
        if (!pgy_map_grow_raw_export(map, (int64_t)sizeof(char *))) return;
        h = pgy_hashmap_hash_i32(key) % (uint32_t)map->capacity;
        first_deleted = UINT32_MAX; probes = 0;
        while (map->occupied[h] == PGY_MAP_RAW_LIVE && probes < map->capacity) {
            h = (h + 1) % (uint32_t)map->capacity; probes++;
        }
    }
    if (first_deleted != UINT32_MAX)
        h = first_deleted;
    else if (probes >= map->capacity) {
        pgy_runtime_warn_invalid_collection("map_set_string_value_i32", "map is full");
        return;
    }
    owned = pgy_runtime_strdup_export(value != NULL ? value : "");
    if (owned == NULL) {
        pgy_runtime_warn_invalid_collection("map_set_string_value_i32", "value duplication failed");
        return;
    }
    PGY_MAP_RAW_I32_KEYS(map)[h] = key;
    *(char **)((char *)map->values + h * sizeof(char *)) = owned;
    map->occupied[h] = PGY_MAP_RAW_LIVE;
    map->count++;
    if (first_deleted != UINT32_MAX)
        map->deleted_count--;
}

void
pgy_map_get_string_value_raw_i32_export(void *map_ptr, int32_t key,
                                        char **out_ptr)
{
    PgyHashMapRaw *map = (PgyHashMapRaw *)map_ptr;
    uint32_t h;
    size_t probes = 0;
    if (out_ptr == NULL)
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_INTERNAL_INVARIANT,
                          "map i32 string get with null output");
    *out_ptr = NULL;
    if (!pgy_map_raw_is_initialized(map))
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_INTERNAL_INVARIANT,
                          "map i32 string get on invalid map");
    pgy_map_raw_require_storage_kind(map, PGY_HASHMAP_KEY_STORAGE_I32);
    h = pgy_hashmap_hash_i32(key) % (uint32_t)map->capacity;
    while (map->occupied[h] != PGY_MAP_RAW_EMPTY && probes < map->capacity) {
        if (map->occupied[h] == PGY_MAP_RAW_LIVE
            && PGY_MAP_RAW_I32_KEYS(map)[h] == key) {
            *out_ptr = *(char **)((char *)map->values + h * sizeof(char *));
            return;
        }
        h = (h + 1) % (uint32_t)map->capacity;
        probes++;
    }
    PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_OUT_OF_BOUNDS,
                      "map key not found");
}

void
pgy_map_remove_string_value_raw_i32_export(void *map_ptr, int32_t key)
{
    PgyHashMapRaw *map = (PgyHashMapRaw *)map_ptr;
    uint32_t h;
    size_t probes = 0;
    if (!pgy_map_raw_is_initialized(map))
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_INTERNAL_INVARIANT,
                          "map i32 string remove on invalid map");
    pgy_map_raw_require_storage_kind(map, PGY_HASHMAP_KEY_STORAGE_I32);
    h = pgy_hashmap_hash_i32(key) % (uint32_t)map->capacity;
    while (map->occupied[h] != PGY_MAP_RAW_EMPTY && probes < map->capacity) {
        if (map->occupied[h] == PGY_MAP_RAW_LIVE
            && PGY_MAP_RAW_I32_KEYS(map)[h] == key) {
            char **slot = (char **)((char *)map->values + h * sizeof(char *));
            free(*slot);
            *slot = NULL;
            PGY_MAP_RAW_I32_KEYS(map)[h] = 0;
            map->occupied[h] = PGY_MAP_RAW_DELETED;
            map->count--;
            map->deleted_count++;
            return;
        }
        h = (h + 1) % (uint32_t)map->capacity;
        probes++;
    }
    PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_OUT_OF_BOUNDS,
                      "map remove key not found");
}

void
pgy_map_set_string_value_raw_i64_export(void *map_ptr, int64_t key,
                                        const char *value)
{
    PgyHashMapRaw *map = (PgyHashMapRaw *)map_ptr;
    uint32_t h;
    uint32_t first_deleted = UINT32_MAX;
    size_t probes = 0;
    char *owned;
    if (!pgy_map_raw_is_initialized(map)) {
        pgy_runtime_warn_invalid_collection("map_set_string_value_i64", "map is not initialized");
        return;
    }
    pgy_map_raw_require_storage_kind(map, PGY_HASHMAP_KEY_STORAGE_I64);
    h = pgy_hashmap_hash_i64(key) % (uint32_t)map->capacity;
    while (map->occupied[h] != PGY_MAP_RAW_EMPTY && probes < map->capacity) {
        if (map->occupied[h] == PGY_MAP_RAW_LIVE
            && PGY_MAP_RAW_I64_KEYS(map)[h] == key) {
            char **slot = (char **)((char *)map->values + h * sizeof(char *));
            owned = pgy_runtime_strdup_export(value != NULL ? value : "");
            if (owned == NULL) {
                pgy_runtime_warn_invalid_collection("map_set_string_value_i64", "value duplication failed");
                return;
            }
            free(*slot);
            *slot = owned;
            return;
        }
        if (map->occupied[h] == PGY_MAP_RAW_DELETED
            && first_deleted == UINT32_MAX)
            first_deleted = h;
        h = (h + 1) % (uint32_t)map->capacity;
        probes++;
    }
    if ((map->count + map->deleted_count + 1) * 4 > map->capacity * 3) {
        if (!pgy_map_grow_raw_export(map, (int64_t)sizeof(char *))) return;
        h = pgy_hashmap_hash_i64(key) % (uint32_t)map->capacity;
        first_deleted = UINT32_MAX; probes = 0;
        while (map->occupied[h] == PGY_MAP_RAW_LIVE && probes < map->capacity) {
            h = (h + 1) % (uint32_t)map->capacity; probes++;
        }
    }
    if (first_deleted != UINT32_MAX)
        h = first_deleted;
    else if (probes >= map->capacity) {
        pgy_runtime_warn_invalid_collection("map_set_string_value_i64", "map is full");
        return;
    }
    owned = pgy_runtime_strdup_export(value != NULL ? value : "");
    if (owned == NULL) {
        pgy_runtime_warn_invalid_collection("map_set_string_value_i64", "value duplication failed");
        return;
    }
    PGY_MAP_RAW_I64_KEYS(map)[h] = key;
    *(char **)((char *)map->values + h * sizeof(char *)) = owned;
    map->occupied[h] = PGY_MAP_RAW_LIVE;
    map->count++;
    if (first_deleted != UINT32_MAX)
        map->deleted_count--;
}

void
pgy_map_get_string_value_raw_i64_export(void *map_ptr, int64_t key,
                                        char **out_ptr)
{
    PgyHashMapRaw *map = (PgyHashMapRaw *)map_ptr;
    uint32_t h;
    size_t probes = 0;
    if (out_ptr == NULL)
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_INTERNAL_INVARIANT,
                          "map i64 string get with null output");
    *out_ptr = NULL;
    if (!pgy_map_raw_is_initialized(map))
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_INTERNAL_INVARIANT,
                          "map i64 string get on invalid map");
    pgy_map_raw_require_storage_kind(map, PGY_HASHMAP_KEY_STORAGE_I64);
    h = pgy_hashmap_hash_i64(key) % (uint32_t)map->capacity;
    while (map->occupied[h] != PGY_MAP_RAW_EMPTY && probes < map->capacity) {
        if (map->occupied[h] == PGY_MAP_RAW_LIVE
            && PGY_MAP_RAW_I64_KEYS(map)[h] == key) {
            *out_ptr = *(char **)((char *)map->values + h * sizeof(char *));
            return;
        }
        h = (h + 1) % (uint32_t)map->capacity;
        probes++;
    }
    PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_OUT_OF_BOUNDS,
                      "map key not found");
}

void
pgy_map_remove_string_value_raw_i64_export(void *map_ptr, int64_t key)
{
    PgyHashMapRaw *map = (PgyHashMapRaw *)map_ptr;
    uint32_t h;
    size_t probes = 0;
    if (!pgy_map_raw_is_initialized(map))
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_INTERNAL_INVARIANT,
                          "map i64 string remove on invalid map");
    pgy_map_raw_require_storage_kind(map, PGY_HASHMAP_KEY_STORAGE_I64);
    h = pgy_hashmap_hash_i64(key) % (uint32_t)map->capacity;
    while (map->occupied[h] != PGY_MAP_RAW_EMPTY && probes < map->capacity) {
        if (map->occupied[h] == PGY_MAP_RAW_LIVE
            && PGY_MAP_RAW_I64_KEYS(map)[h] == key) {
            char **slot = (char **)((char *)map->values + h * sizeof(char *));
            free(*slot);
            *slot = NULL;
            PGY_MAP_RAW_I64_KEYS(map)[h] = 0;
            map->occupied[h] = PGY_MAP_RAW_DELETED;
            map->count--;
            map->deleted_count++;
            return;
        }
        h = (h + 1) % (uint32_t)map->capacity;
        probes++;
    }
    PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_OUT_OF_BOUNDS,
                      "map remove key not found");
}

void
pgy_map_set_string_value_raw_bool_export(void *map_ptr, bool key,
                                         const char *value)
{
    PgyHashMapRaw *map = (PgyHashMapRaw *)map_ptr;
    uint32_t h;
    uint32_t first_deleted = UINT32_MAX;
    size_t probes = 0;
    char *owned;
    if (!pgy_map_raw_is_initialized(map)) {
        pgy_runtime_warn_invalid_collection("map_set_string_value_bool", "map is not initialized");
        return;
    }
    pgy_map_raw_require_storage_kind(map, PGY_HASHMAP_KEY_STORAGE_BOOL);
    h = pgy_hashmap_hash_bool(key) % (uint32_t)map->capacity;
    while (map->occupied[h] != PGY_MAP_RAW_EMPTY && probes < map->capacity) {
        if (map->occupied[h] == PGY_MAP_RAW_LIVE
            && PGY_MAP_RAW_BOOL_KEYS(map)[h] == key) {
            char **slot = (char **)((char *)map->values + h * sizeof(char *));
            owned = pgy_runtime_strdup_export(value != NULL ? value : "");
            if (owned == NULL) {
                pgy_runtime_warn_invalid_collection("map_set_string_value_bool", "value duplication failed");
                return;
            }
            free(*slot);
            *slot = owned;
            return;
        }
        if (map->occupied[h] == PGY_MAP_RAW_DELETED
            && first_deleted == UINT32_MAX)
            first_deleted = h;
        h = (h + 1) % (uint32_t)map->capacity;
        probes++;
    }
    if ((map->count + map->deleted_count + 1) * 4 > map->capacity * 3) {
        if (!pgy_map_grow_raw_export(map, (int64_t)sizeof(char *))) return;
        h = pgy_hashmap_hash_bool(key) % (uint32_t)map->capacity;
        first_deleted = UINT32_MAX; probes = 0;
        while (map->occupied[h] == PGY_MAP_RAW_LIVE && probes < map->capacity) {
            h = (h + 1) % (uint32_t)map->capacity; probes++;
        }
    }
    if (first_deleted != UINT32_MAX)
        h = first_deleted;
    else if (probes >= map->capacity) {
        pgy_runtime_warn_invalid_collection("map_set_string_value_bool", "map is full");
        return;
    }
    owned = pgy_runtime_strdup_export(value != NULL ? value : "");
    if (owned == NULL) {
        pgy_runtime_warn_invalid_collection("map_set_string_value_bool", "value duplication failed");
        return;
    }
    PGY_MAP_RAW_BOOL_KEYS(map)[h] = key;
    *(char **)((char *)map->values + h * sizeof(char *)) = owned;
    map->occupied[h] = PGY_MAP_RAW_LIVE;
    map->count++;
    if (first_deleted != UINT32_MAX)
        map->deleted_count--;
}

void
pgy_map_get_string_value_raw_bool_export(void *map_ptr, bool key,
                                         char **out_ptr)
{
    PgyHashMapRaw *map = (PgyHashMapRaw *)map_ptr;
    uint32_t h;
    size_t probes = 0;
    if (out_ptr == NULL)
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_INTERNAL_INVARIANT,
                          "map bool string get with null output");
    *out_ptr = NULL;
    if (!pgy_map_raw_is_initialized(map))
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_INTERNAL_INVARIANT,
                          "map bool string get on invalid map");
    pgy_map_raw_require_storage_kind(map, PGY_HASHMAP_KEY_STORAGE_BOOL);
    h = pgy_hashmap_hash_bool(key) % (uint32_t)map->capacity;
    while (map->occupied[h] != PGY_MAP_RAW_EMPTY && probes < map->capacity) {
        if (map->occupied[h] == PGY_MAP_RAW_LIVE
            && PGY_MAP_RAW_BOOL_KEYS(map)[h] == key) {
            *out_ptr = *(char **)((char *)map->values + h * sizeof(char *));
            return;
        }
        h = (h + 1) % (uint32_t)map->capacity;
        probes++;
    }
    PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_OUT_OF_BOUNDS,
                      "map key not found");
}

void
pgy_map_remove_string_value_raw_bool_export(void *map_ptr, bool key)
{
    PgyHashMapRaw *map = (PgyHashMapRaw *)map_ptr;
    uint32_t h;
    size_t probes = 0;
    if (!pgy_map_raw_is_initialized(map))
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_INTERNAL_INVARIANT,
                          "map bool string remove on invalid map");
    pgy_map_raw_require_storage_kind(map, PGY_HASHMAP_KEY_STORAGE_BOOL);
    h = pgy_hashmap_hash_bool(key) % (uint32_t)map->capacity;
    while (map->occupied[h] != PGY_MAP_RAW_EMPTY && probes < map->capacity) {
        if (map->occupied[h] == PGY_MAP_RAW_LIVE
            && PGY_MAP_RAW_BOOL_KEYS(map)[h] == key) {
            char **slot = (char **)((char *)map->values + h * sizeof(char *));
            free(*slot);
            *slot = NULL;
            PGY_MAP_RAW_BOOL_KEYS(map)[h] = false;
            map->occupied[h] = PGY_MAP_RAW_DELETED;
            map->count--;
            map->deleted_count++;
            return;
        }
        h = (h + 1) % (uint32_t)map->capacity;
        probes++;
    }
    PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_OUT_OF_BOUNDS,
                      "map remove key not found");
}

#endif /* PGY_RUNTIME_LIB_RAW_MAP_KEY_EXPORTS_H */
