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

#define CHURN_PAIRS 100000
#define LIVE_KEYS 10000

static void
drop_i32_map(PgyHashMap_Int *map)
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
    fault_call = 0;
    fail_at = offset;
    (void)pgy_map_new_i32_int();
}

static void
grow_oom(size_t offset)
{
    PgyHashMap_Int map = pgy_map_new_i32_int();
    for (int32_t i = 0; i < 12; i++)
        pgy_map_set_i32_int(&map, i * 16 + 7, i + 100);
    fail_at = fault_call + offset;
    pgy_map_set_i32_int(&map, 99991, 77);
}

static void
key_storage_mismatch(size_t offset)
{
    PgyHashMap_Int map = pgy_map_new_int();
    (void)offset;
    pgy_map_set_i32_int(&map, 1, 1);
}

static void
set_on_invalid_map(size_t offset)
{
    PgyHashMap_Int map = {0};
    (void)offset;
    pgy_map_set_i32_int(&map, 1, 1);
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

/* Decision table of the rebuild capacity every key kind's grow shares: keep the
 * capacity while live entries are under half of it, double otherwise, and
 * report 0 when doubling overflows size_t. */
static bool
rebuild_capacity_decision_table(void)
{
    const size_t top = (SIZE_MAX >> 1) + 1;
    const struct { size_t capacity, live, expected; } rows[] = {
        {0, 0, 16},
        {16, 0, 16},
        {16, 7, 16},
        {16, 8, 32},
        {16, 12, 32},
        {64, 31, 64},
        {64, 32, 128},
        {top, 0, top},
        {top, top / 2, 0},
    };
    for (size_t i = 0; i < sizeof(rows) / sizeof(rows[0]); i++) {
        size_t actual = pgy_hashmap_rebuild_capacity(rows[i].capacity,
                                                     rows[i].live, 16);
        if (actual != rows[i].expected) {
            fprintf(stderr, "rebuild capacity row %zu: got %zu want %zu\n",
                    i, actual, rows[i].expected);
            return false;
        }
    }
    return true;
}

/* Each pair inserts a new key and removes it again. A rebuild at 75% load
 * counts tombstones, so it must keep the capacity while the live entries fit
 * in half of it; doubling on every rebuild grew such a map without bound. */
static bool
tombstone_churn_keeps_capacity(void)
{
    PgyHashMap_Int ints = pgy_map_new_i32_int();
    PgyHashMap_String strings = pgy_map_new_i32_string();
    bool ok;
    for (int32_t i = 0; i < CHURN_PAIRS; i++) {
        pgy_map_set_i32_int(&ints, i, i);
        pgy_map_remove_i32_int(&ints, i);
        pgy_map_set_i32_string(&strings, i, "value");
        pgy_map_remove_i32_string(&strings, i);
    }
    ok = ints.capacity <= 64 && ints.count == 0
        && strings.capacity <= 64 && strings.count == 0;
    if (!ok)
        fprintf(stderr, "churn capacity int=%zu string=%zu\n",
                ints.capacity, strings.capacity);
    drop_i32_map(&ints);
    pgy_map_drop_string(&strings);
    return ok;
}

static bool
live_keys_still_grow(void)
{
    PgyHashMap_Int map = pgy_map_new_i32_int();
    bool ok = true;
    for (int32_t i = 0; i < LIVE_KEYS; i++)
        pgy_map_set_i32_int(&map, i * 16 + 7, i + 1);
    for (int32_t i = 0; i < LIVE_KEYS && ok; i++)
        ok = pgy_map_get_i32_int(&map, i * 16 + 7) == i + 1;
    ok = ok && map.count == LIVE_KEYS && map.capacity >= 16384;
    drop_i32_map(&map);
    return ok;
}

int
main(int argc, char **argv)
{
    if (argc == 2)
        return run_child_mode(argv[1]);
    if (!collision_delete_grow_extremes()) {
        fputs("collision/delete/grow/extreme gate failed\n", stderr);
        return 3;
    }
    if (!existing_update_never_grows()) {
        fputs("existing update allocated or was lost\n", stderr);
        return 4;
    }
    if (!rebuild_capacity_decision_table()
        || !tombstone_churn_keeps_capacity() || !live_keys_still_grow()) {
        fputs("tombstone rebuild capacity gate failed\n", stderr);
        return 5;
    }
    puts("hashmap i32 storage runtime: ok");
    return 0;
}
