#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TEST_ALLOCATION_CAPACITY 4096

typedef struct TestAllocationRecord {
    void *ptr;
    size_t size;
    bool live;
} TestAllocationRecord;

static size_t calloc_calls;
static size_t malloc_calls;
static size_t free_calls;
static size_t fail_calloc_at;
static size_t fail_malloc_at;
static TestAllocationRecord allocations[TEST_ALLOCATION_CAPACITY];
static size_t allocation_count;

static void test_record_allocation(void *ptr, size_t size) {
    if (ptr == NULL) return;
    if (allocation_count == TEST_ALLOCATION_CAPACITY) {
        fputs("test allocation registry exhausted\n", stderr);
        abort();
    }
    allocations[allocation_count++] = (TestAllocationRecord){ptr, size, true};
}

static TestAllocationRecord *test_live_allocation(void *ptr) {
    for (size_t i = allocation_count; i > 0; i--) {
        TestAllocationRecord *record = &allocations[i - 1];
        if (record->ptr == ptr && record->live) return record;
    }
    return NULL;
}

static void *test_calloc(size_t n, size_t s) {
    void *ptr;
    calloc_calls++;
    if (fail_calloc_at != 0 && calloc_calls == fail_calloc_at) return NULL;
    ptr = calloc(n, s);
    test_record_allocation(ptr, n * s);
    return ptr;
}
static void *test_malloc(size_t s) {
    void *ptr;
    malloc_calls++;
    if (fail_malloc_at != 0 && malloc_calls == fail_malloc_at) return NULL;
    ptr = malloc(s);
    test_record_allocation(ptr, s);
    return ptr;
}
static void *test_realloc(void *ptr, size_t size) {
    TestAllocationRecord *record = ptr == NULL
        ? NULL
        : test_live_allocation(ptr);
    void *grown = realloc(ptr, size);
    if (grown == NULL) return NULL;
    if (record == NULL) {
        test_record_allocation(grown, size);
    } else {
        record->ptr = grown;
        record->size = size;
    }
    return grown;
}
static void test_quarantine_free(void *ptr) {
    if (ptr != NULL) {
        TestAllocationRecord *record = test_live_allocation(ptr);
        if (record == NULL) {
            fputs("free of untracked or retired test allocation\n", stderr);
            abort();
        }
        free_calls++;
        memset(ptr, 0xA5, record->size);
        record->live = false;
    }
}

#define PGY_HASHMAP_CALLOC(count, size) test_calloc((count), (size))
#define malloc(size) test_malloc((size))
#define realloc(ptr, size) test_realloc((ptr), (size))
#define free(ptr) test_quarantine_free((ptr))
#define PGY_LLVM_ENABLED
#include "runtime/pgy_runtime_lib.c"
#undef malloc
#undef realloc
#undef free

static void drop_raw(PgyHashMapRaw *map, bool string_values) {
    if (string_values)
        pgy_map_drop_string_value_raw_export(map);
    else
        pgy_map_drop_raw_export(map);
}

static void fill_twelve_scalar_keys(PgyHashMapRaw *map) {
    char key[32]; int32_t value;
    pgy_map_new_raw_export(map, sizeof(value), PGY_HASHMAP_KEY_STORAGE_STRING);
    for (int i = 0; i < 12; i++) {
        snprintf(key, sizeof(key), "key-%d", i); value = i + 100;
        pgy_map_set_raw_export(map, key, &value, sizeof(value));
    }
}

/* Child modes. A failed allocation now panics with class oom
 * (docs/105_runtime_panic_contract.md), so each injected failure runs in its
 * own process and the smoke script checks the exit status and stderr. The
 * physical-rollback checks that used to follow a failed constructor, grow or
 * duplication (same arrays, same count, key absent) described a process that
 * kept running; the abort replaces them. */

static void constructor_oom(size_t offset) {
    PgyHashMapRaw map = {0};
    calloc_calls = 0; fail_calloc_at = offset;
    pgy_map_new_raw_export(&map, sizeof(int32_t), PGY_HASHMAP_KEY_STORAGE_STRING);
}

static void grow_oom(size_t offset) {
    PgyHashMapRaw map = {0}; int32_t value = 77;
    fill_twelve_scalar_keys(&map);
    fail_calloc_at = calloc_calls + offset;
    pgy_map_set_raw_export(&map, "trigger-grow", &value, sizeof(value));
}

/* The 13th key reaches the grow threshold. Both the key copy and the first
 * grow allocation are armed; the key copy must fail first. */
static void duplication_before_grow_oom(size_t offset) {
    PgyHashMapRaw map = {0}; int32_t value = 77;
    (void)offset;
    fill_twelve_scalar_keys(&map);
    fail_malloc_at = malloc_calls + 1; fail_calloc_at = calloc_calls + 1;
    pgy_map_set_raw_export(&map, "dup-before-grow", &value, sizeof(value));
}

/* offset 1 fails the key copy, offset 2 the value copy; both precede growth. */
static void string_value_duplication_before_grow_oom(size_t offset) {
    PgyHashMapRaw map = {0}; char key[32];
    pgy_map_new_raw_export(&map, sizeof(char *), PGY_HASHMAP_KEY_STORAGE_STRING);
    for (int i = 0; i < 12; i++) {
        snprintf(key, sizeof(key), "key-%d", i);
        pgy_map_set_string_value_raw_export(&map, key, "value");
    }
    fail_malloc_at = malloc_calls + offset; fail_calloc_at = calloc_calls + 1;
    pgy_map_set_string_value_raw_export(&map, "dup-before-grow", "new-value");
}

static void string_value_update_oom(size_t offset) {
    PgyHashMapRaw map = {0};
    (void)offset;
    pgy_map_new_raw_export(&map, sizeof(char *), PGY_HASHMAP_KEY_STORAGE_STRING);
    pgy_map_set_string_value_raw_export(&map, "k", "owned");
    fail_malloc_at = malloc_calls + 1;
    pgy_map_set_string_value_raw_export(&map, "k", "replacement");
}

static void key_storage_mismatch(size_t offset) {
    PgyHashMapRaw map = {0}; int32_t value = 1;
    (void)offset;
    pgy_map_new_raw_export(&map, sizeof(value), PGY_HASHMAP_KEY_STORAGE_I32);
    fail_malloc_at = malloc_calls + 1;
    pgy_map_set_raw_export(&map, "x", &value, sizeof(value));
}

static void set_on_invalid_map(size_t offset) {
    PgyHashMapRaw map = {0}; int32_t value = 1;
    (void)offset;
    pgy_map_set_raw_export(&map, "x", &value, sizeof(value));
}

static void map_keys_oom(size_t offset) {
    PgyHashMapRaw map = {0}; PgyArray_String out = {0}; int32_t value = 1;
    pgy_map_new_raw_export(&map, sizeof(value), PGY_HASHMAP_KEY_STORAGE_STRING);
    pgy_map_set_raw_export(&map, "a", &value, sizeof(value));
    pgy_map_set_raw_export(&map, "b", &value, sizeof(value));
    pgy_map_set_raw_export(&map, "c", &value, sizeof(value));
    fail_malloc_at = malloc_calls + offset;
    pgy_map_keys_raw_export(&map, &out);
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
    {"dup-before-grow-oom", duplication_before_grow_oom, 0},
    {"string-dup-before-grow-oom-1", string_value_duplication_before_grow_oom, 1},
    {"string-dup-before-grow-oom-2", string_value_duplication_before_grow_oom, 2},
    {"string-update-oom", string_value_update_oom, 0},
    {"mismatch", key_storage_mismatch, 0},
    {"set-invalid-map", set_on_invalid_map, 0},
    {"mapkeys-oom", map_keys_oom, 2},
    {"mapkeys-mid-oom", map_keys_oom, 3},
};

static int run_child_mode(const char *name) {
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

/* A failed grow now panics (child modes grow-oom-N); only the update of an
 * existing key, which must not allocate, is checked here. */
static bool scalar_update_never_allocates(void) {
    PgyHashMapRaw map = {0}; int32_t value = 9001, actual = 0;
    size_t c, m;
    fill_twelve_scalar_keys(&map);
    c = calloc_calls; m = malloc_calls;
    fail_calloc_at = calloc_calls + 1; fail_malloc_at = malloc_calls + 1;
    pgy_map_set_raw_export(&map, "key-11", &value, sizeof(value));
    fail_calloc_at = 0; fail_malloc_at = 0;
    pgy_map_get_raw_export(&map, "key-11", &actual, sizeof(actual));
    if (calloc_calls != c || malloc_calls != m || actual != 9001) { drop_raw(&map, false); return false; }
    drop_raw(&map, false); return true;
}

static bool grow_value_slot_alias_is_snapshotted(void) {
    PgyHashMapRaw map = {0}; int32_t actual = 0;
    int32_t *aliased = NULL;
    fill_twelve_scalar_keys(&map);
    for (size_t i = 0; i < map.capacity; i++) {
        if (map.occupied[i] == PGY_MAP_RAW_LIVE
            && strcmp(PGY_MAP_RAW_STRING_KEYS(&map)[i], "key-0") == 0) {
            aliased = (int32_t *)((char *)map.values + i * sizeof(int32_t));
            break;
        }
    }
    if (aliased == NULL || *aliased != 100) { drop_raw(&map, false); return false; }
    pgy_map_set_raw_export(&map, "alias-trigger-grow", aliased, sizeof(*aliased));
    pgy_map_get_raw_export(&map, "alias-trigger-grow", &actual, sizeof(actual));
    if (actual != 100 || map.capacity != 32) { drop_raw(&map, false); return false; }
    drop_raw(&map, false); return true;
}

/* A failed value copy on update now panics (child mode string-update-oom);
 * only the self-alias update is checked here. */
static bool string_value_self_alias_is_owned(void) {
    PgyHashMapRaw map = {0}; char *borrowed = NULL;
    pgy_map_new_raw_export(&map, sizeof(char *), PGY_HASHMAP_KEY_STORAGE_STRING);
    pgy_map_set_string_value_raw_export(&map, "k", "owned");
    pgy_map_get_string_value_raw_export(&map, "k", &borrowed);
    pgy_map_set_string_value_raw_export(&map, "k", borrowed);
    borrowed = NULL; pgy_map_get_string_value_raw_export(&map, "k", &borrowed);
    if (borrowed == NULL || strcmp(borrowed, "owned") != 0 || map.count != 1) {
        drop_raw(&map, true); return false;
    }
    drop_raw(&map, true); return true;
}

static bool production_drop_is_exact_and_idempotent(void) {
    PgyHashMapRaw scalar = {0}, strings = {0}, scalar_keys = {0};
    int32_t value = 1; size_t before;
    pgy_map_new_raw_export(&scalar, sizeof(value), PGY_HASHMAP_KEY_STORAGE_STRING);
    pgy_map_set_raw_export(&scalar, "a", &value, sizeof(value));
    pgy_map_set_raw_export(&scalar, "b", &value, sizeof(value));
    pgy_map_set_raw_export(&scalar, "c", &value, sizeof(value));
    before = free_calls; pgy_map_drop_raw_export(&scalar);
    if (free_calls - before != 6 || scalar.keys != NULL || scalar.values != NULL
        || scalar.occupied != NULL || scalar.key_storage_kind != 0) return false;
    pgy_map_drop_raw_export(&scalar);
    if (free_calls - before != 6) return false;

    pgy_map_new_raw_export(&strings, sizeof(char *), PGY_HASHMAP_KEY_STORAGE_STRING);
    pgy_map_set_string_value_raw_export(&strings, "a", "one");
    pgy_map_set_string_value_raw_export(&strings, "b", "two");
    before = free_calls; pgy_map_drop_string_value_raw_export(&strings);
    if (free_calls - before != 7 || strings.key_storage_kind != 0) return false;
    pgy_map_drop_string_value_raw_export(&strings);
    if (free_calls - before != 7) return false;

    pgy_map_new_raw_export(&scalar_keys, sizeof(char *), PGY_HASHMAP_KEY_STORAGE_I32);
    pgy_map_set_string_value_raw_i32_export(&scalar_keys, 1, "one");
    pgy_map_set_string_value_raw_i32_export(&scalar_keys, 2, "two");
    before = free_calls; pgy_map_drop_string_value_raw_export(&scalar_keys);
    return free_calls - before == 5 && scalar_keys.key_storage_kind == 0;
}

int main(int argc, char **argv) {
    if (argc == 2)
        return run_child_mode(argv[1]);
    if (!scalar_update_never_allocates()
        || !grow_value_slot_alias_is_snapshotted()
        || !string_value_self_alias_is_owned()
        || !production_drop_is_exact_and_idempotent()) return 3;
    puts("hashmap raw string storage runtime: ok"); return 0;
}
