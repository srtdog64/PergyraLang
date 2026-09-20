#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static size_t fault_call;
static size_t fail_at;

static void *
hashmap_raw_test_calloc(size_t count, size_t size)
{
    fault_call++;
    if (fail_at != 0 && fault_call == fail_at)
        return NULL;
    return calloc(count, size);
}

#define PGY_HASHMAP_CALLOC(count, size) hashmap_raw_test_calloc((count), (size))
#define PGY_LLVM_ENABLED
#include "runtime/pgy_runtime_lib.c"

static void
drop_raw_map(PgyHashMapRaw *map)
{
    free(map->keys);
    free(map->values);
    free(map->occupied);
    memset(map, 0, sizeof(*map));
}

static bool
constructor_failure_is_closed(size_t failure)
{
    PgyHashMapRaw map = {0};
    fault_call = 0;
    fail_at = failure;
    pgy_map_new_raw_export(&map, (int64_t)sizeof(int32_t),
                           PGY_HASHMAP_KEY_STORAGE_I64);
    fail_at = 0;
    return map.capacity == 0 && map.keys == NULL && map.values == NULL
        && map.occupied == NULL && map.count == 0
        && map.deleted_count == 0
        && map.key_storage_kind == PGY_HASHMAP_KEY_STORAGE_INVALID;
}

static bool
grow_failure_is_closed(size_t allocation_offset)
{
    PgyHashMapRaw map = {0};
    void *keys;
    void *values;
    uint8_t *occupied;
    size_t capacity;
    size_t count;
    size_t deleted_count;
    int32_t value;

    fault_call = 0;
    fail_at = 0;
    pgy_map_new_raw_export(&map, (int64_t)sizeof(value),
                           PGY_HASHMAP_KEY_STORAGE_I64);
    if (!pgy_map_raw_is_initialized(&map))
        return false;
    for (int64_t i = 0; i < 12; i++) {
        value = (int32_t)i + 100;
        pgy_map_set_raw_i64_export(&map,
            i * INT64_C(4294967296) + 7, &value, (int64_t)sizeof(value));
    }
    keys = map.keys; values = map.values; occupied = map.occupied;
    capacity = map.capacity; count = map.count; deleted_count = map.deleted_count;
    fail_at = fault_call + allocation_offset;
    value = 77;
    pgy_map_set_raw_i64_export(&map, INT64_C(0x4000000000000007),
                               &value, (int64_t)sizeof(value));
    fail_at = 0;
    if (map.keys != keys || map.values != values || map.occupied != occupied
        || map.capacity != capacity || map.count != count
        || map.deleted_count != deleted_count
        || pgy_map_has_raw_i64_export(&map, INT64_C(0x4000000000000007))) {
        drop_raw_map(&map);
        return false;
    }
    for (int64_t i = 0; i < 12; i++) {
        int32_t actual = 0;
        pgy_map_get_raw_i64_export(&map,
            i * INT64_C(4294967296) + 7, &actual, (int64_t)sizeof(actual));
        if (actual != (int32_t)i + 100) {
            drop_raw_map(&map);
            return false;
        }
    }
    drop_raw_map(&map);
    return true;
}

static bool
existing_update_never_grows(void)
{
    PgyHashMapRaw map = {0};
    int32_t value;
    int32_t actual = 0;
    size_t calls_before;
    pgy_map_new_raw_export(&map, (int64_t)sizeof(value),
                           PGY_HASHMAP_KEY_STORAGE_I64);
    if (!pgy_map_raw_is_initialized(&map))
        return false;
    for (int64_t i = 0; i < 12; i++) {
        value = (int32_t)i;
        pgy_map_set_raw_i64_export(&map,
            i * INT64_C(4294967296) + 7, &value, (int64_t)sizeof(value));
    }
    for (int64_t i = 0; i < 8; i++)
        pgy_map_remove_raw_i64_export(&map,
            i * INT64_C(4294967296) + 7, (int64_t)sizeof(value));
    calls_before = fault_call;
    fail_at = fault_call + 1;
    value = 9001;
    pgy_map_set_raw_i64_export(&map,
        INT64_C(11) * INT64_C(4294967296) + 7,
        &value, (int64_t)sizeof(value));
    fail_at = 0;
    pgy_map_get_raw_i64_export(&map,
        INT64_C(11) * INT64_C(4294967296) + 7,
        &actual, (int64_t)sizeof(actual));
    if (fault_call != calls_before || actual != 9001) {
        drop_raw_map(&map);
        return false;
    }
    drop_raw_map(&map);
    return true;
}

int
main(int argc, char **argv)
{
    if (argc == 2 && strcmp(argv[1], "mismatch") == 0) {
        PgyHashMapRaw map = {0};
        int32_t value = 1;
        pgy_map_new_raw_export(&map, (int64_t)sizeof(value),
                               PGY_HASHMAP_KEY_STORAGE_STRING);
        pgy_map_set_raw_i64_export(&map, 1, &value, (int64_t)sizeof(value));
        return 99;
    }
    if (argc == 2 && strcmp(argv[1], "mapkeys-mismatch") == 0) {
        PgyHashMapRaw map = {0};
        PgyArray_Long out = {0};
        pgy_map_new_raw_export(&map, (int64_t)sizeof(int32_t),
                               PGY_HASHMAP_KEY_STORAGE_I32);
        pgy_map_keys_raw_i64_export(&map, &out);
        return 99;
    }
    for (size_t i = 1; i <= 3; i++) {
        if (!constructor_failure_is_closed(i) || !grow_failure_is_closed(i)) {
            fprintf(stderr, "raw allocation failure case %zu did not fail closed\n", i);
            return 2;
        }
    }
    if (!existing_update_never_grows()) {
        fputs("raw existing update allocated or was lost\n", stderr);
        return 3;
    }
    puts("hashmap raw i64 storage runtime: ok");
    return 0;
}
