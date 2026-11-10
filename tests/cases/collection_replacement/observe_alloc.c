#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

void *__real_malloc(size_t);
void *__real_realloc(void *, size_t);
void __real_free(void *);
static void *live[64];
static size_t sizes[64], freed_bytes;
static unsigned allocations, releases, peak_live;
#ifndef PGY_EXPECT_ALLOCATIONS
#define PGY_EXPECT_ALLOCATIONS 6
#define PGY_EXPECT_BYTES 72
#define PGY_EXPECT_PEAK 3
#endif

static void remember(void *p, size_t size)
{
    if (p == NULL || size == 0) return;
    if (allocations == 64) _Exit(90);
    live[allocations] = p;
    sizes[allocations++] = size;
    unsigned count = 0;
    for (unsigned i = 0; i < allocations; ++i) count += live[i] != NULL;
    if (count > peak_live) peak_live = count;
}

void *__wrap_malloc(size_t size)
{
    void *p = __real_malloc(size);
    remember(p, size);
    return p;
}

void *__wrap_realloc(void *old, size_t size)
{
    unsigned row = 64;
    for (unsigned i = 0; old != NULL && i < allocations; ++i)
        if (live[i] == old) { row = i; break; }
    void *p = __real_realloc(old, size);
    if (row == 64) remember(p, size);
    else if (p != NULL) { live[row] = p; sizes[row] = size; }
    return p;
}

void __wrap_free(void *p)
{
    for (unsigned i = 0; p != NULL && i < allocations; ++i)
        if (live[i] == p) {
            live[i] = NULL;
            freed_bytes += sizes[i];
            ++releases;
            break;
        }
    __real_free(p);
}

__attribute__((destructor)) static void verify_release(void)
{
    unsigned remaining = 0;
    for (unsigned i = 0; i < allocations; ++i) remaining += live[i] != NULL;
    fprintf(stderr, "ownership: allocations=%u frees=%u live=%u freed_bytes=%zu peak=%u\n",
        allocations, releases, remaining, freed_bytes, peak_live);
    fflush(stderr);
    if (allocations != PGY_EXPECT_ALLOCATIONS || releases != allocations || remaining != 0 ||
        freed_bytes != PGY_EXPECT_BYTES || peak_live != PGY_EXPECT_PEAK) _Exit(89);
}
