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

static void
fill_twelve_int_keys(PgyHashMap_Int *map)
{
    char key[32];
    for (int i = 0; i < 12; i++) {
        snprintf(key, sizeof(key), "key-%d", i);
        pgy_map_set_int(map, key, i + 100);
    }
}

/* Child modes. A failed allocation now panics with class oom
 * (docs/105_runtime_panic_contract.md), so each injected failure runs in its
 * own process and the smoke script checks the exit status and stderr. The
 * rollback checks that used to follow a failed constructor, grow or
 * duplication (map left unchanged, key absent) described a process that kept
 * running; the abort replaces them. */

static void
constructor_oom(size_t offset)
{
    calloc_calls = 0; fail_calloc_at = offset;
    (void)pgy_map_new_int();
}

static void
generic_constructor_oom(size_t offset)
{
    calloc_calls = 0; fail_calloc_at = offset;
    (void)pgy_map_new_TestValue();
}

static void
grow_oom(size_t offset)
{
    PgyHashMap_Int map = pgy_map_new_int();
    fill_twelve_int_keys(&map);
    fail_calloc_at = calloc_calls + offset;
    pgy_map_set_int(&map, "trigger-grow", 77);
}

static void
insert_duplication_oom(size_t offset)
{
    PgyHashMap_Int map = pgy_map_new_int();
    (void)offset;
    fail_malloc_at = malloc_calls + 1;
    pgy_map_set_int(&map, "alpha", 1);
}

/* The 13th key reaches the grow threshold. Both the key copy and the first
 * grow allocation are armed; the key copy must fail first. */
static void
duplication_before_grow_oom(size_t offset)
{
    PgyHashMap_Int map = pgy_map_new_int();
    (void)offset;
    fill_twelve_int_keys(&map);
    fail_malloc_at = malloc_calls + 1;
    fail_calloc_at = calloc_calls + 1;
    pgy_map_set_int(&map, "dup-before-grow", 77);
}

static void
string_value_update_oom(size_t offset)
{
    PgyHashMap_String map = pgy_map_new_string();
    (void)offset;
    pgy_map_set_string(&map, "k", "owned");
    fail_malloc_at = malloc_calls + 1;
    pgy_map_set_string(&map, "k", "replacement");
}

static void
key_storage_mismatch(size_t offset)
{
    PgyHashMap_Int map = pgy_map_new_i32_int();
    (void)offset;
    fail_malloc_at = malloc_calls + 1;
    pgy_map_set_int(&map, "x", 1);
}

static void
set_on_invalid_map(size_t offset)
{
    PgyHashMap_Int map = {0};
    (void)offset;
    pgy_map_set_int(&map, "x", 1);
}

static void
map_keys_oom(size_t offset)
{
    PgyHashMap_Int map = pgy_map_new_int();
    pgy_map_set_int(&map, "a", 1);
    pgy_map_set_int(&map, "b", 2);
    pgy_map_set_int(&map, "c", 3);
    fail_malloc_at = malloc_calls + offset;
    (void)pgy_map_keys_int(&map);
}

static void
generic_map_keys_on_invalid_map(size_t offset)
{
    PgyHashMap_TestValue map = {0};
    (void)offset;
    (void)pgy_map_keys_test_value(&map);
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
    {"generic-ctor-oom-1", generic_constructor_oom, 1},
    {"generic-ctor-oom-2", generic_constructor_oom, 2},
    {"generic-ctor-oom-3", generic_constructor_oom, 3},
    {"grow-oom-1", grow_oom, 1},
    {"grow-oom-2", grow_oom, 2},
    {"grow-oom-3", grow_oom, 3},
    {"insert-dup-oom", insert_duplication_oom, 0},
    {"dup-before-grow-oom", duplication_before_grow_oom, 0},
    {"string-update-oom", string_value_update_oom, 0},
    {"mismatch", key_storage_mismatch, 0},
    {"set-invalid-map", set_on_invalid_map, 0},
    {"mapkeys-oom", map_keys_oom, 2},
    {"mapkeys-mid-oom", map_keys_oom, 3},
    {"generic-invalid-mapkeys", generic_map_keys_on_invalid_map, 0},
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

/* A failed value copy on update now panics (child mode string-update-oom);
 * only the self-alias update is checked here. */
static bool
string_value_self_alias_is_owned(void)
{
    PgyHashMap_String map = pgy_map_new_string();
    char *borrowed;
    pgy_map_set_string(&map, "k", "owned");
    borrowed = pgy_map_get_string(&map, "k");
    pgy_map_set_string(&map, "k", borrowed);
    if (strcmp(pgy_map_get_string(&map, "k"), "owned") != 0 || map.count != 1) {
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
    if (argc == 2)
        return run_child_mode(argv[1]);
    if (!existing_update_never_allocates()
        || !string_value_self_alias_is_owned()
        || !key_shapes_and_snapshot_are_owned()
        || !production_drop_is_exact_and_idempotent()) {
        fputs("String ownership/update gate failed\n", stderr);
        return 3;
    }
    puts("hashmap string storage runtime: ok");
    return 0;
}
