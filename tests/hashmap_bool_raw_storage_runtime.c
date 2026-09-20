#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static size_t fault_call;
static size_t fail_at;

static void *
hashmap_bool_raw_test_calloc(size_t count, size_t size)
{
    fault_call++;
    if (fail_at != 0 && fault_call == fail_at)
        return NULL;
    return calloc(count, size);
}

#define PGY_HASHMAP_CALLOC(count, size) hashmap_bool_raw_test_calloc((count), (size))
#define PGY_LLVM_ENABLED
#include "runtime/pgy_runtime_lib.c"

static void
drop_raw_map(PgyHashMapRaw *map, bool string_values)
{
    if (string_values && map->values != NULL && map->occupied != NULL) {
        for (size_t i = 0; i < map->capacity; i++) {
            if (map->occupied[i] == PGY_MAP_RAW_LIVE)
                free(*(char **)((char *)map->values + i * sizeof(char *)));
        }
    }
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
                           PGY_HASHMAP_KEY_STORAGE_BOOL);
    fail_at = 0;
    return map.capacity == 0 && map.keys == NULL && map.values == NULL
        && map.occupied == NULL && map.count == 0
        && map.deleted_count == 0
        && map.key_storage_kind == PGY_HASHMAP_KEY_STORAGE_INVALID;
}

static bool
scalar_and_keys(void)
{
    PgyHashMapRaw map = {0};
    PgyArray_Bool keys = {0};
    int32_t value;
    int32_t actual = 0;
    size_t allocation_count;
    pgy_map_new_raw_export(&map, (int64_t)sizeof(value),
                           PGY_HASHMAP_KEY_STORAGE_BOOL);
    if (!pgy_map_raw_is_initialized(&map))
        return false;
    value = 10;
    pgy_map_set_raw_bool_export(&map, false, &value, (int64_t)sizeof(value));
    value = 20;
    pgy_map_set_raw_bool_export(&map, true, &value, (int64_t)sizeof(value));
    allocation_count = fault_call;
    fail_at = fault_call + 1;
    value = 30;
    pgy_map_set_raw_bool_export(&map, true, &value, (int64_t)sizeof(value));
    fail_at = 0;
    pgy_map_get_raw_bool_export(&map, true, &actual, (int64_t)sizeof(actual));
    if (fault_call != allocation_count || actual != 30 || map.count != 2) {
        drop_raw_map(&map, false);
        return false;
    }
    pgy_map_remove_raw_bool_export(&map, false, (int64_t)sizeof(value));
    value = 40;
    pgy_map_set_raw_bool_export(&map, false, &value, (int64_t)sizeof(value));
    pgy_map_keys_raw_bool_export(&map, &keys);
    if (keys.length != 2 || keys.data[0] || !keys.data[1]
        || map.deleted_count != 0) {
        free(keys.data);
        drop_raw_map(&map, false);
        return false;
    }
    free(keys.data);
    drop_raw_map(&map, false);
    return true;
}

static bool
string_value_alias_is_owned(void)
{
    PgyHashMapRaw map = {0};
    char *borrowed = NULL;
    pgy_map_new_raw_export(&map, (int64_t)sizeof(char *),
                           PGY_HASHMAP_KEY_STORAGE_BOOL);
    if (!pgy_map_raw_is_initialized(&map))
        return false;
    pgy_map_set_string_value_raw_bool_export(&map, true, "owned");
    pgy_map_get_string_value_raw_bool_export(&map, true, &borrowed);
    pgy_map_set_string_value_raw_bool_export(&map, true, borrowed);
    borrowed = NULL;
    pgy_map_get_string_value_raw_bool_export(&map, true, &borrowed);
    if (borrowed == NULL || strcmp(borrowed, "owned") != 0 || map.count != 1) {
        drop_raw_map(&map, true);
        return false;
    }
    drop_raw_map(&map, true);
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
        pgy_map_set_raw_bool_export(&map, true, &value, (int64_t)sizeof(value));
        return 99;
    }
    if (argc == 2 && strcmp(argv[1], "mapkeys-mismatch") == 0) {
        PgyHashMapRaw map = {0};
        PgyArray_Bool out = {0};
        pgy_map_new_raw_export(&map, (int64_t)sizeof(int32_t),
                               PGY_HASHMAP_KEY_STORAGE_I32);
        map.count = 1;
        fail_at = fault_call + 1;
        pgy_map_keys_raw_bool_export(&map, &out);
        return 99;
    }
    for (size_t i = 1; i <= 3; i++) {
        if (!constructor_failure_is_closed(i)) {
            fprintf(stderr, "raw allocation failure case %zu did not fail closed\n", i);
            return 2;
        }
    }
    fail_at = 0;
    if (!scalar_and_keys()) {
        fputs("raw scalar/MapKeys gate failed\n", stderr);
        return 3;
    }
    if (!string_value_alias_is_owned()) {
        fputs("raw string ownership gate failed\n", stderr);
        return 4;
    }
    puts("hashmap raw bool storage runtime: ok");
    return 0;
}
