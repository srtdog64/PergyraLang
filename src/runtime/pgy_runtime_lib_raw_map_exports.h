#include "../common/string_compat.h"
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    char    **keys;
    void     *values;
    uint8_t  *occupied;
    size_t    count;
    size_t    capacity;
} PgyHashMapRaw;

#define PGY_MAP_RAW_EMPTY 0u
#define PGY_MAP_RAW_LIVE 1u
#define PGY_MAP_RAW_DELETED 2u

static bool
pgy_map_raw_shape_fits(size_t capacity, size_t elem_size)
{
    return capacity != 0
        && capacity <= (size_t)INT32_MAX
        && capacity <= SIZE_MAX / sizeof(char *)
        && capacity <= SIZE_MAX / sizeof(uint8_t)
        && elem_size != 0
        && elem_size <= SIZE_MAX / capacity;
}

void
pgy_map_new_raw_export(void *map_ptr, int64_t value_size)
{
    PgyHashMapRaw *map = (PgyHashMapRaw *)map_ptr;
    size_t elem_size;
    if (map == NULL) {
        pgy_runtime_warn_invalid_collection("map_new", "null map");
        return;
    }
    if (value_size <= 0) {
        pgy_runtime_warn_invalid_collection("map_new", "non-positive value size");
        return;
    }
    elem_size = (size_t)value_size;
    map->capacity = 16;
    if (!pgy_map_raw_shape_fits(map->capacity, elem_size)) {
        map->capacity = 0;
        pgy_runtime_warn_invalid_collection("map_new", "value size overflow");
        return;
    }
    map->count = 0;
    map->keys = (char **)calloc(map->capacity, sizeof(char *));
    map->values = calloc(map->capacity, elem_size);
    map->occupied = (uint8_t *)calloc(map->capacity, sizeof(uint8_t));
    if (map->keys == NULL || map->values == NULL || map->occupied == NULL) {
        free(map->keys);
        free(map->values);
        free(map->occupied);
        map->keys = NULL;
        map->values = NULL;
        map->occupied = NULL;
        map->capacity = 0;
        pgy_runtime_warn_invalid_collection("map_new", "allocation failed");
    }
}

static void
pgy_map_grow_raw_export(PgyHashMapRaw *map, int64_t value_size)
{
    size_t old_capacity = map->capacity;
    char **old_keys = map->keys;
    void *old_values = map->values;
    uint8_t *old_occupied = map->occupied;
    size_t elem_size;

    if (value_size <= 0) {
        pgy_runtime_warn_invalid_collection("map_grow", "non-positive value size");
        return;
    }
    elem_size = (size_t)value_size;
    if (map->capacity > SIZE_MAX / 2) {
        pgy_runtime_warn_invalid_collection("map_grow", "capacity overflow");
        return;
    }
    size_t new_capacity = map->capacity == 0 ? 16 : map->capacity * 2;
    if (!pgy_map_raw_shape_fits(new_capacity, elem_size)) {
        pgy_runtime_warn_invalid_collection("map_grow", "allocation size overflow");
        return;
    }
    char **new_keys = (char **)calloc(new_capacity, sizeof(char *));
    void *new_values = calloc(new_capacity, elem_size);
    uint8_t *new_occupied = (uint8_t *)calloc(new_capacity, sizeof(uint8_t));
    if (new_keys == NULL || new_values == NULL || new_occupied == NULL) {
        free(new_keys);
        free(new_values);
        free(new_occupied);
        pgy_runtime_warn_invalid_collection("map_grow", "allocation failed");
        return;
    }
    map->capacity = new_capacity;
    map->keys = new_keys;
    map->values = new_values;
    map->occupied = new_occupied;
    map->count = 0;

    for (size_t i = 0; i < old_capacity; i++) {
        if (old_occupied[i] != PGY_MAP_RAW_LIVE || old_keys[i] == NULL)
            continue;
        {
            uint32_t h = pgy_hash_string_export(old_keys[i]) % (uint32_t)map->capacity;
            while (map->occupied[h] == PGY_MAP_RAW_LIVE)
                h = (h + 1) % (uint32_t)map->capacity;
            map->keys[h] = old_keys[i];
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
}

void
pgy_map_set_raw_export(void *map_ptr, const char *key, void *value_ptr, int64_t value_size)
{
    PgyHashMapRaw *map = (PgyHashMapRaw *)map_ptr;
    uint32_t h;
    uint32_t first_deleted = UINT32_MAX;
    size_t probes = 0;
    if (map == NULL) {
        pgy_runtime_warn_invalid_collection("map_set", "null map");
        return;
    }
    if (key == NULL) {
        pgy_runtime_warn_invalid_collection("map_set", "null key");
        return;
    }
    if (value_ptr == NULL) {
        pgy_runtime_warn_invalid_collection("map_set", "null value");
        return;
    }
    if (value_size <= 0) {
        pgy_runtime_warn_invalid_collection("map_set", "non-positive value size");
        return;
    }
    if (map->capacity == 0 || map->keys == NULL || map->values == NULL
        || map->occupied == NULL) {
        pgy_runtime_warn_invalid_collection("map_set", "map is not initialized");
        return;
    }
    if ((double)map->count / (double)map->capacity > 0.75)
        pgy_map_grow_raw_export(map, value_size);
    if (map->capacity == 0 || map->keys == NULL || map->values == NULL
        || map->occupied == NULL) {
        pgy_runtime_warn_invalid_collection("map_set", "map growth failed");
        return;
    }
    h = pgy_hash_string_export(key) % (uint32_t)map->capacity;
    while (map->occupied[h] && probes < map->capacity) {
        if (map->occupied[h] == PGY_MAP_RAW_LIVE
            && map->keys[h] != NULL && strcmp(map->keys[h], key) == 0) {
            memcpy((char *)map->values + (h * (size_t)value_size),
                   value_ptr, (size_t)value_size);
            return;
        }
        if (map->occupied[h] == PGY_MAP_RAW_DELETED && first_deleted == UINT32_MAX)
            first_deleted = h;
        h = (h + 1) % (uint32_t)map->capacity;
        probes++;
    }
    if (first_deleted != UINT32_MAX)
        h = first_deleted;
    map->keys[h] = pgy_runtime_strdup_export(key);
    if (map->keys[h] == NULL) {
        pgy_runtime_warn_invalid_collection("map_set", "key duplication failed");
        return;
    }
    memcpy((char *)map->values + (h * (size_t)value_size),
           value_ptr, (size_t)value_size);
    map->occupied[h] = PGY_MAP_RAW_LIVE;
    map->count++;
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
    if (map->capacity == 0 || map->keys == NULL || map->values == NULL
        || map->occupied == NULL) {
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_INTERNAL_INVARIANT,
                          "map get on uninitialized map");
    }
    if (map->count == 0) {
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_OUT_OF_BOUNDS,
                          "map key not found");
    }
    h = pgy_hash_string_export(key) % (uint32_t)map->capacity;
    while (map->occupied[h] && probes < map->capacity) {
        if (map->occupied[h] == PGY_MAP_RAW_LIVE
            && map->keys[h] != NULL && strcmp(map->keys[h], key) == 0) {
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
        pgy_runtime_warn_invalid_collection("map_has", "null map");
        return false;
    }
    if (key == NULL) {
        pgy_runtime_warn_invalid_collection("map_has", "null key");
        return false;
    }
    if (map->count == 0)
        return false;
    h = pgy_hash_string_export(key) % (uint32_t)map->capacity;
    while (map->occupied[h] && probes < map->capacity) {
        if (map->occupied[h] == PGY_MAP_RAW_LIVE
            && map->keys[h] != NULL && strcmp(map->keys[h], key) == 0)
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
    if (map->count == 0) {
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_OUT_OF_BOUNDS,
                          "map remove key not found");
    }
    h = pgy_hash_string_export(key) % (uint32_t)map->capacity;
    while (map->occupied[h] && probes < map->capacity) {
        if (map->occupied[h] == PGY_MAP_RAW_LIVE
            && map->keys[h] != NULL && strcmp(map->keys[h], key) == 0) {
            free(map->keys[h]);
            map->keys[h] = NULL;
            memset((char *)map->values + (h * (size_t)value_size), 0, (size_t)value_size);
            map->occupied[h] = PGY_MAP_RAW_DELETED;
            map->count--;
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
    char *owned = NULL;
    if (map == NULL) {
        pgy_runtime_warn_invalid_collection("map_set_string_value", "null map");
        return;
    }
    if (key == NULL) {
        pgy_runtime_warn_invalid_collection("map_set_string_value", "null key");
        return;
    }
    if (map->capacity == 0 || map->keys == NULL || map->values == NULL
        || map->occupied == NULL) {
        pgy_runtime_warn_invalid_collection("map_set_string_value", "map is not initialized");
        return;
    }
    if ((double)map->count / (double)map->capacity > 0.75)
        pgy_map_grow_raw_export(map, (int64_t)sizeof(char *));
    if (map->capacity == 0 || map->keys == NULL || map->values == NULL
        || map->occupied == NULL) {
        pgy_runtime_warn_invalid_collection("map_set_string_value", "map growth failed");
        return;
    }
    h = pgy_hash_string_export(key) % (uint32_t)map->capacity;
    while (map->occupied[h] && probes < map->capacity) {
        if (map->occupied[h] == PGY_MAP_RAW_LIVE
            && map->keys[h] != NULL && strcmp(map->keys[h], key) == 0) {
            char **slot = (char **)((char *)map->values + (h * sizeof(char *)));
            owned = pgy_runtime_strdup_export(value != NULL ? value : "");
            if (owned == NULL) {
                pgy_runtime_warn_invalid_collection("map_set_string_value", "value duplication failed");
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
    if (first_deleted != UINT32_MAX)
        h = first_deleted;
    map->keys[h] = pgy_runtime_strdup_export(key);
    owned = pgy_runtime_strdup_export(value != NULL ? value : "");
    if (map->keys[h] == NULL || owned == NULL) {
        free(map->keys[h]);
        free(owned);
        map->keys[h] = NULL;
        pgy_runtime_warn_invalid_collection("map_set_string_value", "key/value duplication failed");
        return;
    }
    *(char **)((char *)map->values + (h * sizeof(char *))) = owned;
    map->occupied[h] = PGY_MAP_RAW_LIVE;
    map->count++;
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
    if (map->capacity == 0 || map->keys == NULL || map->values == NULL
        || map->occupied == NULL) {
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_INTERNAL_INVARIANT,
                          "map string get on uninitialized map");
    }
    if (map->count == 0) {
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_OUT_OF_BOUNDS,
                          "map key not found");
    }
    h = pgy_hash_string_export(key) % (uint32_t)map->capacity;
    while (map->occupied[h] && probes < map->capacity) {
        if (map->occupied[h] == PGY_MAP_RAW_LIVE
            && map->keys[h] != NULL && strcmp(map->keys[h], key) == 0) {
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
    if (map->count == 0) {
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_OUT_OF_BOUNDS,
                          "map remove key not found");
    }
    h = pgy_hash_string_export(key) % (uint32_t)map->capacity;
    while (map->occupied[h] && probes < map->capacity) {
        if (map->occupied[h] == PGY_MAP_RAW_LIVE
            && map->keys[h] != NULL && strcmp(map->keys[h], key) == 0) {
            char **slot = (char **)((char *)map->values + (h * sizeof(char *)));
            free(map->keys[h]);
            free(*slot);
            map->keys[h] = NULL;
            *slot = NULL;
            map->occupied[h] = PGY_MAP_RAW_DELETED;
            map->count--;
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
        pgy_runtime_warn_invalid_collection("map_size", "null map");
        return 0;
    }
    return (int32_t)map->count;
}
