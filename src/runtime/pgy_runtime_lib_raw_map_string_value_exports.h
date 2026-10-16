void
pgy_map_set_string_value_raw_export(void *map_ptr, const char *key, const char *value)
{
    PgyHashMapRaw *map = (PgyHashMapRaw *)map_ptr;
    uint32_t h;
    uint32_t first_deleted = UINT32_MAX;
    size_t probes = 0;
    char *owned_key = NULL;
    char *owned = NULL;
    if (map == NULL) {
        pgy_runtime_panic_invalid_collection("map_set_string_value", "null map");
        return;
    }
    if (key == NULL) {
        pgy_runtime_panic_invalid_collection("map_set_string_value", "null key");
        return;
    }
    if (!pgy_map_raw_is_initialized(map)) {
        pgy_runtime_panic_invalid_collection("map_set_string_value", "map is not initialized");
        return;
    }
    pgy_map_raw_require_storage_kind(map, PGY_HASHMAP_KEY_STORAGE_STRING);
    h = pgy_hash_string_export(key) % (uint32_t)map->capacity;
    while (map->occupied[h] && probes < map->capacity) {
        if (map->occupied[h] == PGY_MAP_RAW_LIVE
            && PGY_MAP_RAW_STRING_KEYS(map)[h] != NULL
            && strcmp(PGY_MAP_RAW_STRING_KEYS(map)[h], key) == 0) {
            char **slot = (char **)((char *)map->values + (h * sizeof(char *)));
            owned = pgy_runtime_strdup_export(value != NULL ? value : "");
            if (owned == NULL) {
                pgy_runtime_panic_collection_oom("map_set_string_value", "value duplication failed");
                return;
            }
            free(*slot);
            *slot = owned;
            return;
        }
        if (map->occupied[h] == PGY_MAP_RAW_DELETED && first_deleted == UINT32_MAX)
            first_deleted = h;
        h = (h + 1) % (uint32_t)map->capacity;
        probes++;
    }
    owned_key = pgy_runtime_strdup_export(key);
    owned = pgy_runtime_strdup_export(value != NULL ? value : "");
    if (owned_key == NULL || owned == NULL) {
        free(owned_key);
        free(owned);
        pgy_runtime_panic_collection_oom("map_set_string_value", "key/value duplication failed");
        return;
    }
    if ((map->count + map->deleted_count + 1) * 4 > map->capacity * 3) {
        if (!pgy_map_grow_raw_export(map, (int64_t)sizeof(char *))) {
            free(owned_key);
            free(owned);
            return;
        }
        h = pgy_hash_string_export(key) % (uint32_t)map->capacity;
        first_deleted = UINT32_MAX; probes = 0;
        while (map->occupied[h] == PGY_MAP_RAW_LIVE && probes < map->capacity) {
            h = (h + 1) % (uint32_t)map->capacity; probes++;
        }
    }
    if (first_deleted != UINT32_MAX)
        h = first_deleted;
    else if (probes >= map->capacity) {
        free(owned_key);
        free(owned);
        pgy_runtime_panic_invalid_collection("map_set_string_value", "map is full");
        return;
    }
    PGY_MAP_RAW_STRING_KEYS(map)[h] = owned_key;
    *(char **)((char *)map->values + (h * sizeof(char *))) = owned;
    map->occupied[h] = PGY_MAP_RAW_LIVE;
    map->count++;
    if (first_deleted != UINT32_MAX)
        map->deleted_count--;
}

void
pgy_map_get_string_value_raw_export(void *map_ptr, const char *key, char **out_ptr)
{
    PgyHashMapRaw *map = (PgyHashMapRaw *)map_ptr;
    uint32_t h;
    size_t probes = 0;
    if (out_ptr == NULL) {
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_INTERNAL_INVARIANT,
                          "map string get on null output");
    }
    *out_ptr = NULL;
    if (map == NULL) {
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_INTERNAL_INVARIANT,
                          "map string get on null map");
    }
    if (key == NULL) {
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_INTERNAL_INVARIANT,
                          "map string get with null key");
    }
    if (!pgy_map_raw_is_initialized(map)) {
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_INTERNAL_INVARIANT,
                          "map string get on uninitialized map");
    }
    pgy_map_raw_require_storage_kind(map, PGY_HASHMAP_KEY_STORAGE_STRING);
    if (map->count == 0) {
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_OUT_OF_BOUNDS,
                          "map key not found");
    }
    h = pgy_hash_string_export(key) % (uint32_t)map->capacity;
    while (map->occupied[h] && probes < map->capacity) {
        if (map->occupied[h] == PGY_MAP_RAW_LIVE
            && PGY_MAP_RAW_STRING_KEYS(map)[h] != NULL
            && strcmp(PGY_MAP_RAW_STRING_KEYS(map)[h], key) == 0) {
            *out_ptr = *(char **)((char *)map->values + (h * sizeof(char *)));
            return;
        }
        h = (h + 1) % (uint32_t)map->capacity;
        probes++;
    }
    PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_OUT_OF_BOUNDS,
                      "map key not found");
}

void
pgy_map_remove_string_value_raw_export(void *map_ptr, const char *key)
{
    PgyHashMapRaw *map = (PgyHashMapRaw *)map_ptr;
    uint32_t h;
    size_t probes = 0;
    if (map == NULL) {
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_INTERNAL_INVARIANT,
                          "map string remove on null map");
    }
    if (key == NULL) {
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_INTERNAL_INVARIANT,
                          "map string remove with null key");
    }
    if (!pgy_map_raw_is_initialized(map)) {
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_INTERNAL_INVARIANT,
                          "map string remove on uninitialized map");
    }
    pgy_map_raw_require_storage_kind(map, PGY_HASHMAP_KEY_STORAGE_STRING);
    if (map->count == 0) {
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_OUT_OF_BOUNDS,
                          "map remove key not found");
    }
    h = pgy_hash_string_export(key) % (uint32_t)map->capacity;
    while (map->occupied[h] && probes < map->capacity) {
        if (map->occupied[h] == PGY_MAP_RAW_LIVE
            && PGY_MAP_RAW_STRING_KEYS(map)[h] != NULL
            && strcmp(PGY_MAP_RAW_STRING_KEYS(map)[h], key) == 0) {
            char **slot = (char **)((char *)map->values + (h * sizeof(char *)));
            free(PGY_MAP_RAW_STRING_KEYS(map)[h]);
            free(*slot);
            PGY_MAP_RAW_STRING_KEYS(map)[h] = NULL;
            *slot = NULL;
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
