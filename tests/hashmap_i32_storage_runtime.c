#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static size_t fault_call;
static size_t fail_at;

static void *
hashmap_test_calloc(size_t count, size_t size)
{
    fault_call++;
    if (fail_at != 0 && fault_call == fail_at)
        return NULL;
    return calloc(count, size);
}

#define PGY_HASHMAP_CALLOC(count, size) hashmap_test_calloc((count), (size))
#include "runtime/pgy_runtime.h"

static void
drop_i32_map(PgyHashMap_Int *map)
{
    free(map->keys);
    free(map->values);
    free(map->occupied);
    memset(map, 0, sizeof(*map));
}

static bool
constructor_failure_is_closed(size_t failure)
{
    PgyHashMap_Int map;
    fault_call = 0;
    fail_at = failure;
    map = pgy_map_new_i32_int();
    fail_at = 0;
    return map.capacity == 0 && map.keys == NULL && map.values == NULL
        && map.occupied == NULL && map.count == 0
        && map.deleted_count == 0
        && map.key_storage_kind == PGY_HASHMAP_KEY_STORAGE_INVALID;
}

static bool
grow_failure_is_closed(size_t allocation_offset)
{
    PgyHashMap_Int map;
    void *keys;
    int32_t *values;
    uint8_t *occupied;
    size_t capacity;
    size_t count;
    size_t deleted_count;

    fault_call = 0;
    fail_at = 0;
    map = pgy_map_new_i32_int();
    if (!pgy_map_int_is_initialized(&map))
        return false;
    for (int32_t i = 0; i < 12; i++)
        pgy_map_set_i32_int(&map, i * 16 + 7, i + 100);
    keys = map.keys;
    values = map.values;
    occupied = map.occupied;
    capacity = map.capacity;
    count = map.count;
    deleted_count = map.deleted_count;
    fail_at = fault_call + allocation_offset;
    pgy_map_set_i32_int(&map, 99991, 77);
    fail_at = 0;
    if (map.keys != keys || map.values != values || map.occupied != occupied
        || map.capacity != capacity || map.count != count
        || map.deleted_count != deleted_count
        || pgy_map_has_i32_int(&map, 99991)) {
        drop_i32_map(&map);
        return false;
    }
    for (int32_t i = 0; i < 12; i++) {
        if (pgy_map_get_i32_int(&map, i * 16 + 7) != i + 100) {
            drop_i32_map(&map);
            return false;
        }
    }
    drop_i32_map(&map);
    return true;
}

static bool
collision_delete_grow_extremes(void)
{
    PgyHashMap_Int map = pgy_map_new_i32_int();
    if (!pgy_map_int_is_initialized(&map))
        return false;
    for (int32_t i = 0; i < 512; i++)
        pgy_map_set_i32_int(&map, i * 16 + 7, i + 1);
    pgy_map_set_i32_int(&map, INT32_MIN, 17);
    pgy_map_set_i32_int(&map, INT32_MAX, 29);
    for (int32_t i = 0; i < 256; i += 2)
        pgy_map_remove_i32_int(&map, i * 16 + 7);
    for (int32_t i = 1; i < 512; i += 2) {
        if (pgy_map_get_i32_int(&map, i * 16 + 7) != i + 1) {
            drop_i32_map(&map);
            return false;
        }
    }
    for (int32_t i = 0; i < 256; i += 2)
        pgy_map_set_i32_int(&map, i * 16 + 7, i + 1000);
    if (pgy_map_get_i32_int(&map, INT32_MIN) != 17
        || pgy_map_get_i32_int(&map, INT32_MAX) != 29
        || map.count != 514) {
        drop_i32_map(&map);
        return false;
    }
    drop_i32_map(&map);
    return true;
}

static bool
existing_update_never_grows(void)
{
    PgyHashMap_Int map = pgy_map_new_i32_int();
    size_t calls_before;
    if (!pgy_map_int_is_initialized(&map))
        return false;
    for (int32_t i = 0; i < 12; i++)
        pgy_map_set_i32_int(&map, i * 16 + 7, i);
    for (int32_t i = 0; i < 8; i++)
        pgy_map_remove_i32_int(&map, i * 16 + 7);
    calls_before = fault_call;
    fail_at = fault_call + 1;
    pgy_map_set_i32_int(&map, 11 * 16 + 7, 9001);
    fail_at = 0;
    if (fault_call != calls_before
        || pgy_map_get_i32_int(&map, 11 * 16 + 7) != 9001) {
        drop_i32_map(&map);
        return false;
    }
    drop_i32_map(&map);
    return true;
}

int
main(int argc, char **argv)
{
    if (argc == 2 && strcmp(argv[1], "mismatch") == 0) {
        PgyHashMap_Int map = pgy_map_new_int();
        pgy_map_set_i32_int(&map, 1, 1);
        return 99;
    }
    for (size_t i = 1; i <= 3; i++) {
        if (!constructor_failure_is_closed(i)
            || !grow_failure_is_closed(i)) {
            fprintf(stderr, "allocation failure case %zu did not fail closed\n", i);
            return 2;
        }
    }
    if (!collision_delete_grow_extremes()) {
        fputs("collision/delete/grow/extreme gate failed\n", stderr);
        return 3;
    }
    if (!existing_update_never_grows()) {
        fputs("existing update allocated or was lost\n", stderr);
        return 4;
    }
    puts("hashmap i32 storage runtime: ok");
    return 0;
}
