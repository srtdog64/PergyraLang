#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static size_t calloc_calls;
static size_t malloc_calls;
static size_t free_calls;
static size_t fail_calloc_at;
static size_t fail_malloc_at;

static void *
hashmap_string_test_calloc(size_t count, size_t size)
{
    calloc_calls++;
    if (fail_calloc_at != 0 && calloc_calls == fail_calloc_at)
        return NULL;
    return calloc(count, size);
}

static void *
hashmap_string_test_malloc(size_t size)
{
    malloc_calls++;
    if (fail_malloc_at != 0 && malloc_calls == fail_malloc_at)
        return NULL;
    return malloc(size);
}

static void
hashmap_string_test_free(void *ptr)
{
    if (ptr != NULL)
        free_calls++;
    free(ptr);
}

#define PGY_HASHMAP_CALLOC(count, size) hashmap_string_test_calloc((count), (size))
#define malloc(size) hashmap_string_test_malloc((size))
#define free(ptr) hashmap_string_test_free((ptr))
#include "runtime/pgy_runtime.h"
#undef malloc
#undef free

PGY_HASHMAP_DEFINE(TestValue, uint16_t)
PGY_DEFINE_MAP_KEYS_EXPORTS(TestValue, test_value)

static void
drop_int_map(PgyHashMap_Int *map)
{
    pgy_map_drop_int(map);
}

static void
drop_string_map(PgyHashMap_String *map)
{
    pgy_map_drop_string(map);
}

static bool
constructor_failure_is_closed(size_t offset)
{
    PgyHashMap_Int map;
    calloc_calls = 0; fail_calloc_at = offset;
    map = pgy_map_new_int();
    fail_calloc_at = 0;
    return map.capacity == 0 && map.keys == NULL && map.values == NULL
        && map.occupied == NULL && map.count == 0
        && map.key_storage_kind == PGY_HASHMAP_KEY_STORAGE_INVALID;
}

static bool
generic_constructor_failure_is_closed(size_t offset)
{
    PgyHashMap_TestValue map;
    calloc_calls = 0; fail_calloc_at = offset;
    map = pgy_map_new_TestValue();
    fail_calloc_at = 0;
    return map.capacity == 0 && map.keys == NULL && map.values == NULL
        && map.occupied == NULL && map.count == 0
        && map.key_storage_kind == PGY_HASHMAP_KEY_STORAGE_INVALID;
}

static bool
insert_duplication_failure_is_closed(void)
{
    PgyHashMap_Int map = pgy_map_new_int();
    fail_malloc_at = malloc_calls + 1;
    pgy_map_set_int(&map, "alpha", 1);
    fail_malloc_at = 0;
    if (map.count != 0 || pgy_map_has_int(&map, "alpha")) {
        drop_int_map(&map);
        return false;
    }
    drop_int_map(&map);
    return true;
}

static bool
grow_failure_is_closed(size_t offset)
{
    PgyHashMap_Int map = pgy_map_new_int();
    void *keys; int32_t *values; uint8_t *occupied;
    size_t capacity, count, deleted_count;
    char key[32];
    for (int i = 0; i < 12; i++) {
        snprintf(key, sizeof(key), "key-%d", i);
        pgy_map_set_int(&map, key, i + 100);
    }
    keys = map.keys; values = map.values; occupied = map.occupied;
    capacity = map.capacity; count = map.count; deleted_count = map.deleted_count;
    fail_calloc_at = calloc_calls + offset;
    pgy_map_set_int(&map, "trigger-grow", 77);
    fail_calloc_at = 0;
    if (map.keys != keys || map.values != values || map.occupied != occupied
        || map.capacity != capacity || map.count != count
        || map.deleted_count != deleted_count || pgy_map_has_int(&map, "trigger-grow")) {
        drop_int_map(&map);
        return false;
    }
    for (int i = 0; i < 12; i++) {
        snprintf(key, sizeof(key), "key-%d", i);
        if (pgy_map_get_int(&map, key) != i + 100) {
            drop_int_map(&map);
            return false;
        }
    }
    drop_int_map(&map);
    return true;
}

static bool
duplication_failure_before_growth_is_physical_rollback(void)
{
    PgyHashMap_Int map = pgy_map_new_int();
    void *keys; int32_t *values; uint8_t *occupied;
    size_t capacity, count, deleted_count;
    char key[32];
    for (int i = 0; i < 12; i++) {
        snprintf(key, sizeof(key), "key-%d", i);
        pgy_map_set_int(&map, key, i);
    }
    keys = map.keys; values = map.values; occupied = map.occupied;
    capacity = map.capacity; count = map.count; deleted_count = map.deleted_count;
    fail_malloc_at = malloc_calls + 1;
    pgy_map_set_int(&map, "dup-before-grow", 77);
    fail_malloc_at = 0;
    if (map.keys != keys || map.values != values || map.occupied != occupied
        || map.capacity != capacity || map.count != count
        || map.deleted_count != deleted_count
        || pgy_map_has_int(&map, "dup-before-grow")) {
        drop_int_map(&map);
        return false;
    }
    drop_int_map(&map);
    return true;
}

static bool
existing_update_never_allocates(void)
{
    PgyHashMap_Int map = pgy_map_new_int();
    char key[32];
    size_t before_calloc, before_malloc;
    for (int i = 0; i < 12; i++) {
        snprintf(key, sizeof(key), "key-%d", i);
        pgy_map_set_int(&map, key, i);
    }
    for (int i = 0; i < 8; i++) {
        snprintf(key, sizeof(key), "key-%d", i);
        pgy_map_remove_int(&map, key);
    }
    before_calloc = calloc_calls; before_malloc = malloc_calls;
    fail_calloc_at = calloc_calls + 1; fail_malloc_at = malloc_calls + 1;
    pgy_map_set_int(&map, "key-11", 9001);
    fail_calloc_at = 0; fail_malloc_at = 0;
    if (calloc_calls != before_calloc || malloc_calls != before_malloc
        || pgy_map_get_int(&map, "key-11") != 9001) {
        drop_int_map(&map);
        return false;
    }
    drop_int_map(&map);
    return true;
}

static bool
string_value_update_is_atomic(void)
{
    PgyHashMap_String map = pgy_map_new_string();
    char *borrowed;
    pgy_map_set_string(&map, "k", "owned");
    borrowed = pgy_map_get_string(&map, "k");
    pgy_map_set_string(&map, "k", borrowed);
    if (strcmp(pgy_map_get_string(&map, "k"), "owned") != 0) {
        drop_string_map(&map);
        return false;
    }
    borrowed = pgy_map_get_string(&map, "k");
    fail_malloc_at = malloc_calls + 1;
    pgy_map_set_string(&map, "k", "replacement");
    fail_malloc_at = 0;
    if (pgy_map_get_string(&map, "k") != borrowed
        || strcmp(borrowed, "owned") != 0 || map.count != 1) {
        drop_string_map(&map);
        return false;
    }
    drop_string_map(&map);
    return true;
}

static bool
key_shapes_and_snapshot_are_owned(void)
{
    PgyHashMap_Int map = pgy_map_new_int();
    PgyArray_String snapshot;
    char *mutable_key = (char *)malloc(32);
    char *long_key = (char *)malloc(4097);
    char collision_keys[3][32] = {{0}};
    int collision_count = 0;
    uint32_t bucket = UINT32_MAX;
    if (mutable_key == NULL || long_key == NULL) {
        free(mutable_key); free(long_key); drop_int_map(&map); return false;
    }
    strcpy(mutable_key, "mutable-source");
    memset(long_key, 'x', 4096); long_key[4096] = '\0';
    pgy_map_set_int(&map, "", 1);
    pgy_map_set_int(&map, mutable_key, 2);
    pgy_map_set_int(&map, long_key, 3);
    memset(mutable_key, 'z', strlen(mutable_key));
    free(mutable_key); mutable_key = NULL;
    if (pgy_map_get_int(&map, "") != 1
        || pgy_map_get_int(&map, "mutable-source") != 2
        || pgy_map_get_int(&map, long_key) != 3) {
        free(long_key); drop_int_map(&map); return false;
    }
    for (int i = 0; i < 1000 && collision_count < 3; i++) {
        char candidate[32];
        uint32_t candidate_bucket;
        snprintf(candidate, sizeof(candidate), "collision-%d", i);
        candidate_bucket = pgy_hash_string(candidate) % (uint32_t)map.capacity;
        if (bucket == UINT32_MAX) bucket = candidate_bucket;
        if (candidate_bucket == bucket) {
            strcpy(collision_keys[collision_count], candidate);
            pgy_map_set_int(&map, collision_keys[collision_count], 10 + collision_count);
            collision_count++;
        }
    }
    if (collision_count != 3) { free(long_key); drop_int_map(&map); return false; }
    pgy_map_remove_int(&map, collision_keys[1]);
    pgy_map_set_int(&map, collision_keys[1], 99);
    if (pgy_map_get_int(&map, collision_keys[1]) != 99) {
        free(long_key); drop_int_map(&map); return false;
    }
    snapshot = pgy_map_keys_int(&map);
    pgy_map_remove_int(&map, "mutable-source");
    for (int i = 0; i < 40; i++) {
        char key[32]; snprintf(key, sizeof(key), "growth-%d", i);
        pgy_map_set_int(&map, key, i);
    }
    drop_int_map(&map);
    {
        bool found_empty = false, found_mutable = false, found_long = false;
        for (size_t i = 0; i < snapshot.length; i++) {
            if (strcmp(snapshot.data[i], "") == 0) found_empty = true;
            if (strcmp(snapshot.data[i], "mutable-source") == 0) found_mutable = true;
            if (strcmp(snapshot.data[i], long_key) == 0) found_long = true;
            free(snapshot.data[i]);
        }
        free(snapshot.data);
        free(long_key);
        if (!found_empty || !found_mutable || !found_long) return false;
    }
    return true;
}

static bool
production_drop_is_exact_and_idempotent(void)
{
    PgyHashMap_Int ints = pgy_map_new_int();
    PgyHashMap_String strings = pgy_map_new_string();
    PgyHashMap_String scalar_keys = pgy_map_new_i32_string();
    PgyHashMap_Int snapshot_source = pgy_map_new_int();
    PgyArray_String snapshot;
    size_t before;
    pgy_map_set_int(&ints, "a", 1);
    pgy_map_set_int(&ints, "b", 2);
    pgy_map_set_int(&ints, "c", 3);
    before = free_calls;
    pgy_map_drop_int(&ints);
    if (free_calls - before != 6 || ints.key_storage_kind != 0
        || ints.keys != NULL || ints.values != NULL || ints.occupied != NULL)
        return false;
    pgy_map_drop_int(&ints);
    if (free_calls - before != 6) return false;

    pgy_map_set_string(&strings, "a", "one");
    pgy_map_set_string(&strings, "b", "two");
    before = free_calls;
    pgy_map_drop_string(&strings);
    if (free_calls - before != 7 || strings.key_storage_kind != 0)
        return false;
    pgy_map_drop_string(&strings);
    if (free_calls - before != 7) return false;

    pgy_map_set_i32_string(&scalar_keys, 1, "one");
    pgy_map_set_i32_string(&scalar_keys, 2, "two");
    before = free_calls;
    pgy_map_drop_string(&scalar_keys);
    if (free_calls - before != 5 || scalar_keys.key_storage_kind != 0)
        return false;

    pgy_map_set_int(&snapshot_source, "left", 1);
    pgy_map_set_int(&snapshot_source, "middle", 2);
    pgy_map_set_int(&snapshot_source, "right", 3);
    snapshot = pgy_map_keys_int(&snapshot_source);
    pgy_map_drop_int(&snapshot_source);
    before = free_calls;
    pgy_array_drop_owned_String(&snapshot);
    if (free_calls - before != 4 || snapshot.data != NULL
        || snapshot.length != 0 || snapshot.capacity != 0)
        return false;
    pgy_array_drop_owned_String(&snapshot);
    return free_calls - before == 4;
}

int
main(int argc, char **argv)
{
    if (argc == 2 && strcmp(argv[1], "mismatch") == 0) {
        PgyHashMap_Int map = pgy_map_new_i32_int();
        fail_malloc_at = malloc_calls + 1;
        pgy_map_set_int(&map, "x", 1);
        return 99;
    }
    if (argc == 2 && strcmp(argv[1], "mapkeys-oom") == 0) {
        PgyHashMap_Int map = pgy_map_new_int();
        pgy_map_set_int(&map, "a", 1);
        pgy_map_set_int(&map, "b", 2);
        pgy_map_set_int(&map, "c", 3);
        fail_malloc_at = malloc_calls + 2;
        (void)pgy_map_keys_int(&map);
        return 99;
    }
    if (argc == 2 && strcmp(argv[1], "mapkeys-mid-oom") == 0) {
        PgyHashMap_Int map = pgy_map_new_int();
        pgy_map_set_int(&map, "a", 1);
        pgy_map_set_int(&map, "b", 2);
        pgy_map_set_int(&map, "c", 3);
        fail_malloc_at = malloc_calls + 3;
        (void)pgy_map_keys_int(&map);
        return 99;
    }
    if (argc == 2 && strcmp(argv[1], "generic-invalid-mapkeys") == 0) {
        PgyHashMap_TestValue map;
        calloc_calls = 0; fail_calloc_at = 1;
        map = pgy_map_new_TestValue();
        fail_calloc_at = 0;
        (void)pgy_map_keys_test_value(&map);
        return 99;
    }
    for (size_t i = 1; i <= 3; i++) {
        if (!constructor_failure_is_closed(i)
            || !generic_constructor_failure_is_closed(i)
            || !grow_failure_is_closed(i)) {
            fprintf(stderr, "allocation failure case %zu did not fail closed\n", i);
            return 2;
        }
    }
    if (!insert_duplication_failure_is_closed()
        || !duplication_failure_before_growth_is_physical_rollback()
        || !existing_update_never_allocates()
        || !string_value_update_is_atomic()
        || !key_shapes_and_snapshot_are_owned()
        || !production_drop_is_exact_and_idempotent()) {
        fputs("String ownership/update gate failed\n", stderr);
        return 3;
    }
    puts("hashmap string storage runtime: ok");
    return 0;
}
