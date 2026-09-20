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

static bool constructor_failure(size_t offset) {
    PgyHashMapRaw map = {0};
    calloc_calls = 0; fail_calloc_at = offset;
    pgy_map_new_raw_export(&map, sizeof(int32_t), PGY_HASHMAP_KEY_STORAGE_STRING);
    fail_calloc_at = 0;
    return map.capacity == 0 && map.keys == NULL && map.values == NULL
        && map.occupied == NULL && map.key_storage_kind == PGY_HASHMAP_KEY_STORAGE_INVALID;
}

static bool scalar_update_and_growth_failure(size_t offset) {
    PgyHashMapRaw map = {0};
    char key[32]; int32_t value, actual = 0;
    void *keys, *values; uint8_t *occupied; size_t capacity, count;
    pgy_map_new_raw_export(&map, sizeof(value), PGY_HASHMAP_KEY_STORAGE_STRING);
    for (int i = 0; i < 12; i++) {
        snprintf(key, sizeof(key), "key-%d", i); value = i + 100;
        pgy_map_set_raw_export(&map, key, &value, sizeof(value));
    }
    {
        size_t c = calloc_calls, m = malloc_calls;
        fail_calloc_at = calloc_calls + 1; fail_malloc_at = malloc_calls + 1;
        value = 9001; pgy_map_set_raw_export(&map, "key-11", &value, sizeof(value));
        fail_calloc_at = 0; fail_malloc_at = 0;
        pgy_map_get_raw_export(&map, "key-11", &actual, sizeof(actual));
        if (calloc_calls != c || malloc_calls != m || actual != 9001) { drop_raw(&map, false); return false; }
    }
    keys = map.keys; values = map.values; occupied = map.occupied;
    capacity = map.capacity; count = map.count;
    fail_calloc_at = calloc_calls + offset; value = 77;
    pgy_map_set_raw_export(&map, "trigger-grow", &value, sizeof(value));
    fail_calloc_at = 0;
    if (map.keys != keys || map.values != values || map.occupied != occupied
        || map.capacity != capacity || map.count != count
        || pgy_map_has_raw_export(&map, "trigger-grow")) { drop_raw(&map, false); return false; }
    drop_raw(&map, false); return true;
}

static bool duplication_failure_before_growth_is_physical_rollback(void) {
    PgyHashMapRaw map = {0}; char key[32]; int32_t value = 0;
    void *keys, *values; uint8_t *occupied; size_t capacity, count, deleted_count;
    pgy_map_new_raw_export(&map, sizeof(value), PGY_HASHMAP_KEY_STORAGE_STRING);
    for (int i = 0; i < 12; i++) {
        snprintf(key, sizeof(key), "key-%d", i); value = i;
        pgy_map_set_raw_export(&map, key, &value, sizeof(value));
    }
    keys = map.keys; values = map.values; occupied = map.occupied;
    capacity = map.capacity; count = map.count; deleted_count = map.deleted_count;
    fail_malloc_at = malloc_calls + 1; value = 77;
    pgy_map_set_raw_export(&map, "dup-before-grow", &value, sizeof(value));
    fail_malloc_at = 0;
    if (map.keys != keys || map.values != values || map.occupied != occupied
        || map.capacity != capacity || map.count != count
        || map.deleted_count != deleted_count
        || pgy_map_has_raw_export(&map, "dup-before-grow")) {
        drop_raw(&map, false); return false;
    }
    drop_raw(&map, false); return true;
}

static bool grow_value_slot_alias_is_snapshotted(void) {
    PgyHashMapRaw map = {0}; char key[32]; int32_t value = 0, actual = 0;
    int32_t *aliased = NULL;
    pgy_map_new_raw_export(&map, sizeof(value), PGY_HASHMAP_KEY_STORAGE_STRING);
    for (int i = 0; i < 12; i++) {
        snprintf(key, sizeof(key), "key-%d", i); value = i + 100;
        pgy_map_set_raw_export(&map, key, &value, sizeof(value));
    }
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

static bool string_value_duplication_failure_precedes_growth(size_t offset) {
    PgyHashMapRaw map = {0}; char key[32];
    void *keys, *values; uint8_t *occupied; size_t capacity, count, deleted_count;
    pgy_map_new_raw_export(&map, sizeof(char *), PGY_HASHMAP_KEY_STORAGE_STRING);
    for (int i = 0; i < 12; i++) {
        snprintf(key, sizeof(key), "key-%d", i);
        pgy_map_set_string_value_raw_export(&map, key, "value");
    }
    keys = map.keys; values = map.values; occupied = map.occupied;
    capacity = map.capacity; count = map.count; deleted_count = map.deleted_count;
    fail_malloc_at = malloc_calls + offset;
    pgy_map_set_string_value_raw_export(&map, "dup-before-grow", "new-value");
    fail_malloc_at = 0;
    if (map.keys != keys || map.values != values || map.occupied != occupied
        || map.capacity != capacity || map.count != count
        || map.deleted_count != deleted_count
        || pgy_map_has_raw_export(&map, "dup-before-grow")) {
        drop_raw(&map, true); return false;
    }
    drop_raw(&map, true); return true;
}

static bool string_value_alias_and_oom(void) {
    PgyHashMapRaw map = {0}; char *borrowed = NULL;
    pgy_map_new_raw_export(&map, sizeof(char *), PGY_HASHMAP_KEY_STORAGE_STRING);
    pgy_map_set_string_value_raw_export(&map, "k", "owned");
    pgy_map_get_string_value_raw_export(&map, "k", &borrowed);
    pgy_map_set_string_value_raw_export(&map, "k", borrowed);
    borrowed = NULL; pgy_map_get_string_value_raw_export(&map, "k", &borrowed);
    if (borrowed == NULL || strcmp(borrowed, "owned") != 0) { drop_raw(&map, true); return false; }
    {
        char *old = borrowed;
        fail_malloc_at = malloc_calls + 1;
        pgy_map_set_string_value_raw_export(&map, "k", "replacement");
        fail_malloc_at = 0; borrowed = NULL;
        pgy_map_get_string_value_raw_export(&map, "k", &borrowed);
        if (borrowed != old || strcmp(borrowed, "owned") != 0) { drop_raw(&map, true); return false; }
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
    if (argc == 2 && strcmp(argv[1], "mismatch") == 0) {
        PgyHashMapRaw map = {0}; int32_t value = 1;
        pgy_map_new_raw_export(&map, sizeof(value), PGY_HASHMAP_KEY_STORAGE_I32);
        fail_malloc_at = malloc_calls + 1;
        pgy_map_set_raw_export(&map, "x", &value, sizeof(value)); return 99;
    }
    if (argc == 2 && strcmp(argv[1], "mapkeys-oom") == 0) {
        PgyHashMapRaw map = {0}; PgyArray_String out = {0}; int32_t value = 1;
        pgy_map_new_raw_export(&map, sizeof(value), PGY_HASHMAP_KEY_STORAGE_STRING);
        pgy_map_set_raw_export(&map, "a", &value, sizeof(value));
        pgy_map_set_raw_export(&map, "b", &value, sizeof(value));
        pgy_map_set_raw_export(&map, "c", &value, sizeof(value));
        fail_malloc_at = malloc_calls + 2;
        pgy_map_keys_raw_export(&map, &out); return 99;
    }
    if (argc == 2 && strcmp(argv[1], "mapkeys-mid-oom") == 0) {
        PgyHashMapRaw map = {0}; PgyArray_String out = {0}; int32_t value = 1;
        pgy_map_new_raw_export(&map, sizeof(value), PGY_HASHMAP_KEY_STORAGE_STRING);
        pgy_map_set_raw_export(&map, "a", &value, sizeof(value));
        pgy_map_set_raw_export(&map, "b", &value, sizeof(value));
        pgy_map_set_raw_export(&map, "c", &value, sizeof(value));
        fail_malloc_at = malloc_calls + 3;
        pgy_map_keys_raw_export(&map, &out); return 99;
    }
    for (size_t i = 1; i <= 3; i++)
        if (!constructor_failure(i) || !scalar_update_and_growth_failure(i)) return 2;
    if (!duplication_failure_before_growth_is_physical_rollback()
        || !grow_value_slot_alias_is_snapshotted()
        || !string_value_duplication_failure_precedes_growth(1)
        || !string_value_duplication_failure_precedes_growth(2)
        || !string_value_alias_and_oom()
        || !production_drop_is_exact_and_idempotent()) return 3;
    puts("hashmap raw string storage runtime: ok"); return 0;
}
