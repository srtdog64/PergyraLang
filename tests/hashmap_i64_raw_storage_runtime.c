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

#define CHURN_PAIRS 100000
#define LIVE_KEYS 10000

static void
drop_raw_map(PgyHashMapRaw *map)
{
    free(map->keys);
    free(map->values);
    free(map->occupied);
    memset(map, 0, sizeof(*map));
}

/* Child modes. A failed allocation now panics with class oom
 * (docs/105_runtime_panic_contract.md), so each injected failure runs in its
 * own process and the smoke script checks the exit status and stderr. The
 * checks that used to follow a failed constructor or grow (map fields
 * unchanged, old values still readable, new key absent) described a process
 * that kept running; the abort replaces them. */

static void
constructor_oom(size_t offset)
{
    PgyHashMapRaw map = {0};
    fault_call = 0;
    fail_at = offset;
    pgy_map_new_raw_export(&map, (int64_t)sizeof(int32_t),
                           PGY_HASHMAP_KEY_STORAGE_I64);
}

static void
grow_oom(size_t offset)
{
    PgyHashMapRaw map = {0};
    int32_t value;
    pgy_map_new_raw_export(&map, (int64_t)sizeof(value),
                           PGY_HASHMAP_KEY_STORAGE_I64);
    for (int64_t i = 0; i < 12; i++) {
        value = (int32_t)i + 100;
        pgy_map_set_raw_i64_export(&map,
            i * INT64_C(4294967296) + 7, &value, (int64_t)sizeof(value));
    }
    fail_at = fault_call + offset;
    value = 77;
    pgy_map_set_raw_i64_export(&map, INT64_C(0x4000000000000007),
                               &value, (int64_t)sizeof(value));
}

static void
key_storage_mismatch(size_t offset)
{
    PgyHashMapRaw map = {0};
    int32_t value = 1;
    (void)offset;
    pgy_map_new_raw_export(&map, (int64_t)sizeof(value),
                           PGY_HASHMAP_KEY_STORAGE_STRING);
    pgy_map_set_raw_i64_export(&map, 1, &value, (int64_t)sizeof(value));
}

static void
map_keys_storage_mismatch(size_t offset)
{
    PgyHashMapRaw map = {0};
    PgyArray_Long out = {0};
    (void)offset;
    pgy_map_new_raw_export(&map, (int64_t)sizeof(int32_t),
                           PGY_HASHMAP_KEY_STORAGE_I32);
    pgy_map_keys_raw_i64_export(&map, &out);
}

static void
set_on_invalid_map(size_t offset)
{
    PgyHashMapRaw map = {0};
    int32_t value = 1;
    (void)offset;
    pgy_map_set_raw_i64_export(&map, 1, &value, (int64_t)sizeof(value));
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
    {"grow-oom-1", grow_oom, 1},
    {"grow-oom-2", grow_oom, 2},
    {"grow-oom-3", grow_oom, 3},
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

/* Each pair inserts a new key and removes it again. A rebuild at 75% load
 * counts tombstones, so it must keep the capacity while the live entries fit
 * in half of it; doubling on every rebuild grew such a map without bound. */
static bool
tombstone_churn_keeps_capacity(void)
{
    PgyHashMapRaw map = {0};
    int32_t value;
    bool ok;
    pgy_map_new_raw_export(&map, (int64_t)sizeof(value),
                           PGY_HASHMAP_KEY_STORAGE_I64);
    for (int64_t i = 0; i < CHURN_PAIRS; i++) {
        int64_t key = i * INT64_C(4294967296) + 7;
        value = (int32_t)i;
        pgy_map_set_raw_i64_export(&map, key, &value, (int64_t)sizeof(value));
        pgy_map_remove_raw_i64_export(&map, key, (int64_t)sizeof(value));
    }
    ok = map.capacity <= 64 && map.count == 0;
    if (!ok)
        fprintf(stderr, "raw churn capacity=%zu\n", map.capacity);
    drop_raw_map(&map);
    return ok;
}

static bool
live_keys_still_grow(void)
{
    PgyHashMapRaw map = {0};
    int32_t value;
    bool ok = true;
    pgy_map_new_raw_export(&map, (int64_t)sizeof(value),
                           PGY_HASHMAP_KEY_STORAGE_I64);
    for (int64_t i = 0; i < LIVE_KEYS; i++) {
        value = (int32_t)i + 1;
        pgy_map_set_raw_i64_export(&map,
            i * INT64_C(4294967296) + 7, &value, (int64_t)sizeof(value));
    }
    for (int64_t i = 0; i < LIVE_KEYS && ok; i++) {
        int32_t actual = 0;
        pgy_map_get_raw_i64_export(&map,
            i * INT64_C(4294967296) + 7, &actual, (int64_t)sizeof(actual));
        ok = actual == (int32_t)i + 1;
    }
    ok = ok && map.count == LIVE_KEYS && map.capacity >= 16384;
    drop_raw_map(&map);
    return ok;
}

int
main(int argc, char **argv)
{
    if (argc == 2)
        return run_child_mode(argv[1]);
    if (!existing_update_never_grows()) {
        fputs("raw existing update allocated or was lost\n", stderr);
        return 3;
    }
    if (!tombstone_churn_keeps_capacity() || !live_keys_still_grow()) {
        fputs("raw tombstone rebuild capacity gate failed\n", stderr);
        return 4;
    }
    puts("hashmap raw i64 storage runtime: ok");
    return 0;
}
