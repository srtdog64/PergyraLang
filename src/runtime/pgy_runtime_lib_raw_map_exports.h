#include "../common/string_compat.h"
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "pgy_runtime_hashmap_key_storage_owner.h"

/* Growable runtime storage is not a synchronization boundary.
 * The semantic layer must reject raw Array/Slice/List/Queue/Set/HashMap
 * transport across parallel/async/worker boundaries unless an explicit copy or
 * pinned read-only view owner is used.
 */

typedef struct {
    void     *keys;
    void     *values;
    uint8_t  *occupied;
    size_t    count;
    size_t    capacity;
    size_t    deleted_count;
    int32_t   key_storage_kind;
} PgyHashMapRaw;

#define PGY_MAP_RAW_EMPTY 0u
#define PGY_MAP_RAW_LIVE 1u
#define PGY_MAP_RAW_DELETED 2u

static bool
pgy_map_raw_shape_fits(size_t capacity, size_t elem_size)
{
    return capacity != 0
        && capacity <= (size_t)INT32_MAX
        && capacity <= SIZE_MAX / sizeof(uint8_t)
        && elem_size != 0
        && elem_size <= SIZE_MAX / capacity;
}

#define PGY_MAP_RAW_STRING_KEYS(map) ((char **)((map)->keys))
#define PGY_MAP_RAW_I32_KEYS(map) ((int32_t *)((map)->keys))
#define PGY_MAP_RAW_I64_KEYS(map) ((int64_t *)((map)->keys))
#define PGY_MAP_RAW_BOOL_KEYS(map) ((bool *)((map)->keys))

static bool
pgy_map_raw_has_storage_kind(const PgyHashMapRaw *map,
                             PgyHashMapKeyStorageKind expected)
{
    return map != NULL && map->key_storage_kind == (int32_t)expected;
}

static void
pgy_map_raw_require_storage_kind(const PgyHashMapRaw *map,
                                 PgyHashMapKeyStorageKind expected)
{
    if (!pgy_map_raw_has_storage_kind(map, expected)) {
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_INTERNAL_INVARIANT,
                          "map key storage kind mismatch");
    }
}

static bool
pgy_map_raw_is_initialized(const PgyHashMapRaw *map)
{
    return map != NULL
        && map->capacity != 0
        && map->capacity <= (size_t)INT32_MAX
        && map->keys != NULL
        && map->values != NULL
        && map->occupied != NULL;
}

void
pgy_map_new_raw_export(void *map_ptr, int64_t value_size,
                       int32_t key_storage_kind)
{
    PgyHashMapRaw *map = (PgyHashMapRaw *)map_ptr;
    size_t elem_size;
    size_t key_size;
    if (map == NULL) {
        pgy_runtime_panic_invalid_collection("map_new", "null map");
        return;
    }
    if (value_size <= 0) {
        pgy_runtime_panic_invalid_collection("map_new", "non-positive value size");
        return;
    }
    elem_size = (size_t)value_size;
    key_size = pgy_hashmap_key_storage_size(
        (PgyHashMapKeyStorageKind)key_storage_kind);
    if (key_size == 0) {
        pgy_runtime_panic_invalid_collection("map_new", "invalid key storage kind");
        return;
    }
    map->capacity = 16;
    if (!pgy_map_raw_shape_fits(map->capacity, elem_size)
        || map->capacity > SIZE_MAX / key_size) {
        map->capacity = 0;
        pgy_runtime_panic_collection_oom("map_new", "value size overflow");
        return;
    }
    map->count = 0;
    map->deleted_count = 0;
    map->key_storage_kind = key_storage_kind;
    map->keys = PGY_HASHMAP_CALLOC(map->capacity, key_size);
    map->values = PGY_HASHMAP_CALLOC(map->capacity, elem_size);
    map->occupied = (uint8_t *)PGY_HASHMAP_CALLOC(map->capacity, sizeof(uint8_t));
    if (map->keys == NULL || map->values == NULL || map->occupied == NULL) {
        free(map->keys);
        free(map->values);
        free(map->occupied);
        map->keys = NULL;
        map->values = NULL;
        map->occupied = NULL;
        map->capacity = 0;
        map->deleted_count = 0;
        map->key_storage_kind = PGY_HASHMAP_KEY_STORAGE_INVALID;
        pgy_runtime_panic_collection_oom("map_new", "allocation failed");
    }
}

static void
pgy_map_drop_raw_impl(PgyHashMapRaw *map, bool values_are_owned_strings)
{
    if (map == NULL)
        return;
    if (map->capacity == 0 && map->keys == NULL
        && map->values == NULL && map->occupied == NULL) {
        memset(map, 0, sizeof(*map));
        return;
    }
    if (!pgy_map_raw_is_initialized(map)
        || pgy_hashmap_key_storage_size(
            (PgyHashMapKeyStorageKind)map->key_storage_kind) == 0) {
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_INTERNAL_INVARIANT,
                          "map drop on invalid map");
    }
    for (size_t i = 0; i < map->capacity; i++) {
        if (map->occupied[i] != PGY_MAP_RAW_LIVE)
            continue;
        if (map->key_storage_kind == PGY_HASHMAP_KEY_STORAGE_STRING) {
            free(PGY_MAP_RAW_STRING_KEYS(map)[i]);
            PGY_MAP_RAW_STRING_KEYS(map)[i] = NULL;
        }
        if (values_are_owned_strings) {
            char **slot = (char **)((char *)map->values + i * sizeof(char *));
            free(*slot);
            *slot = NULL;
        }
    }
    free(map->keys);
    free(map->values);
    free(map->occupied);
    memset(map, 0, sizeof(*map));
}

void
pgy_map_drop_raw_export(void *map_ptr)
{
    pgy_map_drop_raw_impl((PgyHashMapRaw *)map_ptr, false);
}

void
pgy_map_drop_string_value_raw_export(void *map_ptr)
{
    pgy_map_drop_raw_impl((PgyHashMapRaw *)map_ptr, true);
}

static bool
pgy_map_grow_raw_export(PgyHashMapRaw *map, int64_t value_size)
{
    size_t old_capacity = map->capacity;
    void *old_keys = map->keys;
    void *old_values = map->values;
    uint8_t *old_occupied = map->occupied;
    size_t elem_size;
    size_t key_size;

    if (value_size <= 0) {
        pgy_runtime_panic_invalid_collection("map_grow", "non-positive value size");
        return false;
    }
    elem_size = (size_t)value_size;
    key_size = pgy_hashmap_key_storage_size(
        (PgyHashMapKeyStorageKind)map->key_storage_kind);
    if (key_size == 0) {
        pgy_runtime_panic_invalid_collection("map_grow", "invalid key storage kind");
        return false;
    }
    if (map->capacity > SIZE_MAX / 2) {
        pgy_runtime_panic_collection_oom("map_grow", "capacity overflow");
        return false;
    }
    size_t new_capacity = map->capacity == 0 ? 16 : map->capacity * 2;
    if (!pgy_map_raw_shape_fits(new_capacity, elem_size)
        || new_capacity > SIZE_MAX / key_size) {
        pgy_runtime_panic_collection_oom("map_grow", "allocation size overflow");
        return false;
    }
    void *new_keys = PGY_HASHMAP_CALLOC(new_capacity, key_size);
    void *new_values = PGY_HASHMAP_CALLOC(new_capacity, elem_size);
    uint8_t *new_occupied = (uint8_t *)PGY_HASHMAP_CALLOC(new_capacity, sizeof(uint8_t));
    if (new_keys == NULL || new_values == NULL || new_occupied == NULL) {
        free(new_keys);
        free(new_values);
        free(new_occupied);
        pgy_runtime_panic_collection_oom("map_grow", "allocation failed");
        return false;
    }
    map->capacity = new_capacity;
    map->keys = new_keys;
    map->values = new_values;
    map->occupied = new_occupied;
    map->count = 0;
    map->deleted_count = 0;

    for (size_t i = 0; i < old_capacity; i++) {
        if (old_occupied[i] != PGY_MAP_RAW_LIVE)
            continue;
        {
            uint32_t h;
            if (map->key_storage_kind == PGY_HASHMAP_KEY_STORAGE_STRING) {
                char **strings = (char **)old_keys;
                if (strings[i] == NULL)
                    continue;
                h = pgy_hash_string_export(strings[i]) % (uint32_t)map->capacity;
            } else if (map->key_storage_kind == PGY_HASHMAP_KEY_STORAGE_I32) {
                h = pgy_hashmap_hash_i32(((int32_t *)old_keys)[i])
                    % (uint32_t)map->capacity;
            } else if (map->key_storage_kind == PGY_HASHMAP_KEY_STORAGE_I64) {
                h = pgy_hashmap_hash_i64(((int64_t *)old_keys)[i])
                    % (uint32_t)map->capacity;
            } else {
                h = pgy_hashmap_hash_bool(((bool *)old_keys)[i])
                    % (uint32_t)map->capacity;
            }
            while (map->occupied[h] == PGY_MAP_RAW_LIVE)
                h = (h + 1) % (uint32_t)map->capacity;
            if (map->key_storage_kind == PGY_HASHMAP_KEY_STORAGE_STRING)
                PGY_MAP_RAW_STRING_KEYS(map)[h] = ((char **)old_keys)[i];
            else if (map->key_storage_kind == PGY_HASHMAP_KEY_STORAGE_I32)
                PGY_MAP_RAW_I32_KEYS(map)[h] = ((int32_t *)old_keys)[i];
            else if (map->key_storage_kind == PGY_HASHMAP_KEY_STORAGE_I64)
                PGY_MAP_RAW_I64_KEYS(map)[h] = ((int64_t *)old_keys)[i];
            else
                PGY_MAP_RAW_BOOL_KEYS(map)[h] = ((bool *)old_keys)[i];
            memcpy((char *)map->values + (h * elem_size),
                   (char *)old_values + (i * elem_size),
                   elem_size);
            map->occupied[h] = PGY_MAP_RAW_LIVE;
            map->count++;
        }
    }

    free(old_keys);
    free(old_values);
    free(old_occupied);
    return true;
}

void
pgy_map_set_raw_export(void *map_ptr, const char *key, void *value_ptr, int64_t value_size)
{
    PgyHashMapRaw *map = (PgyHashMapRaw *)map_ptr;
    uint32_t h;
    uint32_t first_deleted = UINT32_MAX;
    size_t probes = 0;
    char *owned_key = NULL;
    void *owned_value_snapshot = NULL;
    const void *value_source = value_ptr;
    if (map == NULL) {
        pgy_runtime_panic_invalid_collection("map_set", "null map");
        return;
    }
    if (key == NULL) {
        pgy_runtime_panic_invalid_collection("map_set", "null key");
        return;
    }
    if (value_ptr == NULL) {
        pgy_runtime_panic_invalid_collection("map_set", "null value");
        return;
    }
    if (value_size <= 0) {
        pgy_runtime_panic_invalid_collection("map_set", "non-positive value size");
        return;
    }
    if (!pgy_map_raw_is_initialized(map)) {
        pgy_runtime_panic_invalid_collection("map_set", "map is not initialized");
        return;
    }
    pgy_map_raw_require_storage_kind(map, PGY_HASHMAP_KEY_STORAGE_STRING);
    h = pgy_hash_string_export(key) % (uint32_t)map->capacity;
    while (map->occupied[h] && probes < map->capacity) {
        if (map->occupied[h] == PGY_MAP_RAW_LIVE
            && PGY_MAP_RAW_STRING_KEYS(map)[h] != NULL
            && strcmp(PGY_MAP_RAW_STRING_KEYS(map)[h], key) == 0) {
            memmove((char *)map->values + (h * (size_t)value_size),
                    value_ptr, (size_t)value_size);
            return;
        }
        if (map->occupied[h] == PGY_MAP_RAW_DELETED && first_deleted == UINT32_MAX)
            first_deleted = h;
        h = (h + 1) % (uint32_t)map->capacity;
        probes++;
    }
    owned_key = pgy_runtime_strdup_export(key);
    if (owned_key == NULL) {
        pgy_runtime_panic_collection_oom("map_set", "key duplication failed");
        return;
    }
    if ((map->count + map->deleted_count + 1) * 4 > map->capacity * 3) {
        uintptr_t value_address = (uintptr_t)value_ptr;
        uintptr_t values_address = (uintptr_t)map->values;
        size_t values_size = map->capacity * (size_t)value_size;
        if (value_address >= values_address
            && value_address - values_address <= values_size - (size_t)value_size) {
            owned_value_snapshot = malloc((size_t)value_size);
            if (owned_value_snapshot == NULL) {
                free(owned_key);
                pgy_runtime_panic_collection_oom("map_set", "aliased value snapshot failed");
                return;
            }
            memcpy(owned_value_snapshot, value_ptr, (size_t)value_size);
            value_source = owned_value_snapshot;
        }
        if (!pgy_map_grow_raw_export(map, value_size)) {
            free(owned_value_snapshot);
            free(owned_key);
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
        free(owned_value_snapshot);
        free(owned_key);
        pgy_runtime_panic_invalid_collection("map_set", "map is full");
        return;
    }
    PGY_MAP_RAW_STRING_KEYS(map)[h] = owned_key;
    memmove((char *)map->values + (h * (size_t)value_size),
            value_source, (size_t)value_size);
    free(owned_value_snapshot);
    map->occupied[h] = PGY_MAP_RAW_LIVE;
    map->count++;
    if (first_deleted != UINT32_MAX)
        map->deleted_count--;
}

void
pgy_map_get_raw_export(void *map_ptr, const char *key, void *out_ptr, int64_t value_size)
{
    PgyHashMapRaw *map = (PgyHashMapRaw *)map_ptr;
    uint32_t h;
    size_t probes = 0;
    if (out_ptr == NULL) {
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_INTERNAL_INVARIANT,
                          "map get on null output");
    }
    if (value_size <= 0) {
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_INTERNAL_INVARIANT,
                          "map get with invalid value size");
    }
    memset(out_ptr, 0, (size_t)value_size);
    if (map == NULL) {
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_INTERNAL_INVARIANT,
                          "map get on null map");
    }
    if (key == NULL) {
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_INTERNAL_INVARIANT,
                          "map get with null key");
    }
    if (!pgy_map_raw_is_initialized(map)) {
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_INTERNAL_INVARIANT,
                          "map get on uninitialized map");
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
pgy_map_has_raw_export(void *map_ptr, const char *key)
{
    PgyHashMapRaw *map = (PgyHashMapRaw *)map_ptr;
    uint32_t h;
    size_t probes = 0;
    if (map == NULL) {
        pgy_runtime_panic_invalid_collection("map_has", "null map");
        return false;
    }
    if (key == NULL) {
        pgy_runtime_panic_invalid_collection("map_has", "null key");
        return false;
    }
    if (!pgy_map_raw_is_initialized(map)) {
        pgy_runtime_panic_invalid_collection("map_has", "map is not initialized");
        return false;
    }
    pgy_map_raw_require_storage_kind(map, PGY_HASHMAP_KEY_STORAGE_STRING);
    if (map->count == 0)
        return false;
    h = pgy_hash_string_export(key) % (uint32_t)map->capacity;
    while (map->occupied[h] && probes < map->capacity) {
        if (map->occupied[h] == PGY_MAP_RAW_LIVE
            && PGY_MAP_RAW_STRING_KEYS(map)[h] != NULL
            && strcmp(PGY_MAP_RAW_STRING_KEYS(map)[h], key) == 0)
            return true;
        h = (h + 1) % (uint32_t)map->capacity;
        probes++;
    }
    return false;
}

void
pgy_map_remove_raw_export(void *map_ptr, const char *key, int64_t value_size)
{
    PgyHashMapRaw *map = (PgyHashMapRaw *)map_ptr;
    uint32_t h;
    size_t probes = 0;
    if (map == NULL) {
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_INTERNAL_INVARIANT,
                          "map remove on null map");
    }
    if (key == NULL) {
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_INTERNAL_INVARIANT,
                          "map remove with null key");
    }
    if (value_size <= 0) {
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_INTERNAL_INVARIANT,
                          "map remove with invalid value size");
    }
    if (!pgy_map_raw_is_initialized(map)) {
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_INTERNAL_INVARIANT,
                          "map remove on uninitialized map");
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
            free(PGY_MAP_RAW_STRING_KEYS(map)[h]);
            PGY_MAP_RAW_STRING_KEYS(map)[h] = NULL;
            memset((char *)map->values + (h * (size_t)value_size), 0, (size_t)value_size);
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

int32_t
pgy_map_size_raw_export(void *map_ptr)
{
    PgyHashMapRaw *map = (PgyHashMapRaw *)map_ptr;
    if (map == NULL) {
        pgy_runtime_panic_invalid_collection("map_size", "null map");
        return 0;
    }
    if (!pgy_map_raw_is_initialized(map)) {
        pgy_runtime_panic_invalid_collection("map_size", "map is not initialized");
        return 0;
    }
    return (int32_t)map->count;
}
