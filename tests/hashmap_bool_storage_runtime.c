#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static size_t fault_call;
static size_t fail_at;

static void *
hashmap_bool_test_calloc(size_t count, size_t size)
{
    fault_call++;
    if (fail_at != 0 && fault_call == fail_at)
        return NULL;
    return calloc(count, size);
}

#define PGY_HASHMAP_CALLOC(count, size) hashmap_bool_test_calloc((count), (size))
#include "runtime/pgy_runtime.h"

static void
drop_int_map(PgyHashMap_Int *map)
{
    free(map->keys);
    free(map->values);
    free(map->occupied);
    memset(map, 0, sizeof(*map));
}

static void
drop_string_map(PgyHashMap_String *map)
{
    for (size_t i = 0; i < map->capacity; i++) {
        if (map->occupied != NULL && map->occupied[i] == PGY_HASHMAP_LIVE)
            free(map->values[i]);
    }
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
    map = pgy_map_new_bool_int();
    fail_at = 0;
    return map.capacity == 0 && map.keys == NULL && map.values == NULL
        && map.occupied == NULL && map.count == 0
        && map.deleted_count == 0
        && map.key_storage_kind == PGY_HASHMAP_KEY_STORAGE_INVALID;
}

static bool
identity_update_churn_and_keys(void)
{
    PgyHashMap_Int map = pgy_map_new_bool_int();
    PgyArray_Bool keys;
    size_t capacity;
    size_t allocation_count;
    if (!pgy_map_int_is_initialized(&map))
        return false;
    capacity = map.capacity;
    pgy_map_set_bool_int(&map, false, 10);
    pgy_map_set_bool_int(&map, true, 20);
    allocation_count = fault_call;
    fail_at = fault_call + 1;
    for (size_t i = 0; i < 100000; i++) {
        pgy_map_set_bool_int(&map, true, (int32_t)i);
        pgy_map_remove_bool_int(&map, false);
        pgy_map_set_bool_int(&map, false, 10);
    }
    fail_at = 0;
    if (fault_call != allocation_count || map.capacity != capacity
        || map.count != 2 || map.deleted_count != 0
        || pgy_map_get_bool_int(&map, true) != 99999
        || pgy_map_get_bool_int(&map, false) != 10) {
        drop_int_map(&map);
        return false;
    }
    keys = pgy_map_keys_bool_int(&map);
    if (keys.length != 2 || keys.data[0] || !keys.data[1]) {
        pgy_array_drop_Bool(&keys);
        drop_int_map(&map);
        return false;
    }
    pgy_array_drop_Bool(&keys);
    drop_int_map(&map);
    return true;
}

static bool
string_value_alias_is_owned(void)
{
    PgyHashMap_String map = pgy_map_new_bool_string();
    char *borrowed;
    if (!pgy_map_string_is_initialized(&map))
        return false;
    pgy_map_set_bool_string(&map, true, "owned");
    borrowed = pgy_map_get_bool_string(&map, true);
    pgy_map_set_bool_string(&map, true, borrowed);
    if (strcmp(pgy_map_get_bool_string(&map, true), "owned") != 0
        || map.count != 1) {
        drop_string_map(&map);
        return false;
    }
    drop_string_map(&map);
    return true;
}

int
main(int argc, char **argv)
{
    if (argc == 2 && strcmp(argv[1], "mismatch") == 0) {
        PgyHashMap_Int map = pgy_map_new_int();
        pgy_map_set_bool_int(&map, true, 1);
        return 99;
    }
    if (argc == 2 && strcmp(argv[1], "mapkeys-mismatch") == 0) {
        PgyHashMap_Int map = pgy_map_new_int();
        map.count = 1;
        fail_at = fault_call + 1;
        (void)pgy_map_keys_bool_int(&map);
        return 99;
    }
    for (size_t i = 1; i <= 3; i++) {
        if (!constructor_failure_is_closed(i)) {
            fprintf(stderr, "allocation failure case %zu did not fail closed\n", i);
            return 2;
        }
    }
    fail_at = 0;
    if (!identity_update_churn_and_keys()) {
        fputs("identity/update/churn/MapKeys gate failed\n", stderr);
        return 3;
    }
    if (!string_value_alias_is_owned()) {
        fputs("string ownership gate failed\n", stderr);
        return 4;
    }
    puts("hashmap bool storage runtime: ok");
    return 0;
}
