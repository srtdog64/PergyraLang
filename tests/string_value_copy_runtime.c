#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct TestAllocation {
    void *pointer;
    size_t size, retirements;
    bool live;
} TestAllocation;

#ifdef STRING_VALUE_TEST_CALLER
extern size_t test_malloc_calls, test_fail_malloc_at, test_allocation_count;
extern TestAllocation test_allocations[64];
extern void *test_malloc(size_t size);
extern void *test_realloc(void *pointer, size_t size);
extern void test_free(void *pointer);
extern char *test_system_source(void);
extern void test_system_source_retire(char *source);
extern char *test_region_source(void);
extern void test_region_source_retire(void);
#define PGY_RUNTIME_DECLS_ONLY
#include "runtime/pgy_runtime_string_value_inline.h"
#else
size_t test_malloc_calls, test_fail_malloc_at, test_allocation_count;
TestAllocation test_allocations[64];

void *test_malloc(size_t size)
{
    test_malloc_calls++;
    if (test_fail_malloc_at != 0 && test_malloc_calls == test_fail_malloc_at) return NULL;
    void *pointer = malloc(size);
    if (pointer != NULL) {
        if (test_allocation_count == 64) { fputs("test allocation ledger exhausted\n", stderr); abort(); }
        test_allocations[test_allocation_count++] = (TestAllocation){pointer, size, 0, true};
    }
    return pointer;
}
void *test_realloc(void *pointer, size_t size)
{
    if (pointer == NULL) return test_malloc(size);
    for (size_t i = 0; i < test_allocation_count; i++) {
        TestAllocation *row = &test_allocations[i];
        if (row->pointer == pointer && row->live) {
            void *grown = realloc(pointer, size);
            if (grown != NULL) { row->pointer = grown; row->size = size; }
            return grown;
        }
    }
    fputs("test realloc requires a live allocation\n", stderr); abort();
}
void test_free(void *pointer)
{
    if (pointer == NULL) return;
    for (size_t i = 0; i < test_allocation_count; i++) {
        TestAllocation *row = &test_allocations[i];
        if (row->pointer == pointer && row->live) {
            memset(pointer, 0xA5, row->size);
            row->live = false;
            row->retirements++;
            return;
        }
    }
    fputs("test free requires one live allocation\n", stderr); abort();
}

/* Quarantine matches the established raw-storage harness. It observes actual
   allocator retirement without permitting a stale-source UAF during a test. */
#define malloc(size) test_malloc(size)
#define realloc(pointer, size) test_realloc((pointer), (size))
#define free(pointer) test_free(pointer)
#ifdef STRING_VALUE_TEST_BODY
#define PGY_LLVM_ENABLED
#include "runtime/pgy_runtime_lib.c"
#elif defined(STRING_VALUE_TEST_CEXT_BODY)
#include "runtime/pgy_runtime_cext_lib.c"
#else
#include "runtime/pgy_runtime.h"
#endif
#undef malloc
#undef realloc
#undef free

static PgyRegion test_region;
char *test_system_source(void)
{
    char *source = pgy_alloc(NULL, sizeof("system value"), 1);
    memcpy(source, "system value", sizeof("system value"));
    return source;
}
void test_system_source_retire(char *source)
{
    pgy_free(NULL, source, sizeof("system value"));
}
char *test_region_source(void)
{
    test_region = pgy_region_create(64);
    char *source = pgy_region_alloc(&test_region, sizeof("region value"), 1);
    memcpy(source, "region value", sizeof("region value"));
    return source;
}
void test_region_source_retire(void) { pgy_region_destroy(&test_region); }
#endif

#if !defined(STRING_VALUE_TEST_BODY) && !defined(STRING_VALUE_TEST_CEXT_BODY)
static TestAllocation *allocation_for(const void *pointer)
{
    for (size_t i = 0; i < test_allocation_count; i++)
        if (test_allocations[i].pointer == pointer) return &test_allocations[i];
    return NULL;
}
static bool new_system_copy(const char *source, char **copy)
{
    size_t before = test_allocation_count, calls = test_malloc_calls;
    *copy = pgy_string_result_copy(source);
    if (*copy == NULL || *copy == source || test_allocation_count != before + 1 ||
        test_malloc_calls != calls + 1) return false;
    TestAllocation *row = allocation_for(*copy);
    return row == &test_allocations[before] && row->live && row->retirements == 0 &&
        row->size == strlen(source) + 1 && strcmp(*copy, source) == 0;
}
static bool retire_copy_once(char *copy)
{
    TestAllocation *row = allocation_for(copy);
    if (row == NULL || !row->live || row->retirements != 0) return false;
    pgy_string_result_retire(copy);
    return !row->live && row->retirements == 1;
}
static bool literal_and_empty(void)
{
    const char *sources[] = {"literal value", ""};
    for (size_t i = 0; i < 2; i++) {
        char *first = NULL, *second = NULL;
        if (!new_system_copy(sources[i], &first) || !new_system_copy(sources[i], &second) || first == second)
            return false;
        if (!retire_copy_once(first) || strcmp(second, sources[i]) != 0 || !retire_copy_once(second)) return false;
    }
    return true;
}
static bool system_source_retirement(void)
{
    char *source = test_system_source(), *copy = NULL;
    if (!new_system_copy(source, &copy)) return false; /* Before source retirement. */
    source[0] = 'X';
    if (strcmp(copy, "system value") != 0) return false;
    test_system_source_retire(source);
    TestAllocation *original = allocation_for(source);
    if (original == NULL || original->live || original->retirements != 1 ||
        strcmp(copy, "system value") != 0) return false;
    return retire_copy_once(copy);
}
static bool region_source_retirement(void)
{
    char *source = test_region_source(), *copy = NULL;
    if (!new_system_copy(source, &copy)) return false;
    source[0] = 'X';
    if (strcmp(copy, "region value") != 0) return false;
    size_t before = 0;
    for (size_t i = 0; i < test_allocation_count; i++) before += test_allocations[i].retirements;
    test_region_source_retire();
    size_t after = 0;
    for (size_t i = 0; i < test_allocation_count; i++) after += test_allocations[i].retirements;
    if (after != before + 1 || strcmp(copy, "region value") != 0) return false;
    return retire_copy_once(copy);
}
int main(int argc, char **argv)
{
    if (argc == 2) {
        if (strcmp(argv[1], "null-copy") == 0) (void)pgy_string_result_copy(NULL);
        else if (strcmp(argv[1], "null-retire") == 0) pgy_string_result_retire(NULL);
        else if (strcmp(argv[1], "oom-copy") == 0) {
            test_fail_malloc_at = test_malloc_calls + 1;
            (void)pgy_string_result_copy("allocation failure");
        } else return 2;
        fputs("String leaf failure unexpectedly returned\n", stderr);
        return 3;
    }
    if (argc != 1) return 2;
    if (!literal_and_empty() || !system_source_retirement() || !region_source_retirement()) {
        fputs("String copy/retirement ledger failed\n", stderr);
        return 1;
    }
    for (size_t i = 0; i < test_allocation_count; i++) {
        if (test_allocations[i].live || test_allocations[i].retirements != 1) {
            fputs("String leaf test left an owning allocation unretired\n", stderr);
            return 1;
        }
    }
    /* Quarantined allocations are physically reclaimed once, at test exit. */
    for (size_t i = 0; i < test_allocation_count; i++) free(test_allocations[i].pointer);
    puts("String leaf independent system copies and exact retirement: PASS");
    return 0;
}
#endif
