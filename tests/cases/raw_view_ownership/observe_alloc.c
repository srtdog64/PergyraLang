#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

void *__real_malloc(size_t);
void __real_free(void *);
void *__real_realloc(void *, size_t);
static void *live[4];
static size_t sizes[4], freed_bytes;
static unsigned allocations, releases, reallocations;

void *__wrap_malloc(size_t size)
{
    void *p = __real_malloc(size);
    if ((size == 44 || size == 16) && p != NULL) {
        if (allocations >= 4) _Exit(90);
        sizes[allocations] = size;
        live[allocations++] = p;
    }
    return p;
}

void *__wrap_realloc(void *old, size_t size)
{
    unsigned slot = 4;
    int was_null = old == NULL;
    for (unsigned i = 0; !was_null && i < allocations; ++i)
        if (live[i] == old) { slot = i; break; }
    void *p = __real_realloc(old, size);
    if (was_null && size == 16 && p != NULL) {
        if (allocations >= 4) _Exit(90);
        sizes[allocations] = size;
        live[allocations++] = p;
    } else if (slot < allocations && p != NULL) {
        live[slot] = p;
        sizes[slot] = size;
        ++reallocations;
    }
    return p;
}

void __wrap_free(void *p)
{
    for (unsigned i = 0; i < allocations; ++i)
        if (p != NULL && live[i] == p) {
            live[i] = NULL;
            freed_bytes += sizes[i];
            ++releases;
            break;
        }
    __real_free(p);
}

__attribute__((destructor)) static void verify_backing_release(void)
{
    unsigned remaining = 0;
    for (unsigned i = 0; i < allocations; ++i) remaining += live[i] != NULL;
    fprintf(stderr, "backing: allocations=%u reallocations=%u frees=%u live=%u freed_bytes=%zu\n",
        allocations, reallocations, releases, remaining, freed_bytes);
    fflush(stderr);
    if (allocations != 2 || releases != 2 || remaining != 0) _Exit(89);
}
