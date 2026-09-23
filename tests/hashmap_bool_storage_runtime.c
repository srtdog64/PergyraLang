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

/* Child modes. A failed allocation now panics with class oom
 * (docs/105_runtime_panic_contract.md), so each injected constructor failure
 * runs in its own process and the smoke script checks the exit status and
 * stderr. The check that a failed constructor returned an empty invalid map
 * described a process that kept running; the abort replaces it. */

static void
constructor_oom(size_t offset)
{
    fault_call = 0;
    fail_at = offset;
    (void)pgy_map_new_bool_int();
}

static void
key_storage_mismatch(size_t offset)
{
    PgyHashMap_Int map = pgy_map_new_int();
    (void)offset;
    pgy_map_set_bool_int(&map, true, 1);
}

static void
map_keys_storage_mismatch(size_t offset)
{
    PgyHashMap_Int map = pgy_map_new_int();
    (void)offset;
    map.count = 1;
    fail_at = fault_call + 1;
    (void)pgy_map_keys_bool_int(&map);
}

static void
set_on_invalid_map(size_t offset)
{
    PgyHashMap_Int map = {0};
    (void)offset;
    pgy_map_set_bool_int(&map, true, 1);
}

typedef struct {
    const char *name;
    void (*run)(size_t offset);
    size_t offset;
} ChildMode;

static const ChildMode child_modes[] = {
    {"ctor-oom-1", constructor_oom, 1},
    {"ctor-oom-2", constructor_oom, 2},
    {"ctor-oom-3", constructor_oom, 3},
    {"mismatch", key_storage_mismatch, 0},
    {"mapkeys-mismatch", map_keys_storage_mismatch, 0},
    {"set-invalid-map", set_on_invalid_map, 0},
};

static int
run_child_mode(const char *name)
{
    for (size_t i = 0; i < sizeof(child_modes) / sizeof(child_modes[0]); i++) {
        if (strcmp(child_modes[i].name, name) != 0)
            continue;
        child_modes[i].run(child_modes[i].offset);
        fprintf(stderr, "child mode %s returned instead of panicking\n", name);
        return 99;
    }
    fprintf(stderr, "unknown child mode %s\n", name);
    return 98;
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
    if (argc == 2)
        return run_child_mode(argv[1]);
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
