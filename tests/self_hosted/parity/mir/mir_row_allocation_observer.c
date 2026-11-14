/* Observe requested allocation traffic without replacing pointer lifetimes. */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

void *__real_malloc(size_t);
void *__real_calloc(size_t, size_t);
void *__real_realloc(void *, size_t);
static uint64_t requested_bytes;

void *__wrap_malloc(size_t size)
{
    requested_bytes += size;
    return __real_malloc(size);
}

void *__wrap_calloc(size_t count, size_t size)
{
    if (size != 0 && count > SIZE_MAX / size) _Exit(90);
    requested_bytes += count * size;
    return __real_calloc(count, size);
}

void *__wrap_realloc(void *pointer, size_t size)
{
    requested_bytes += size;
    return __real_realloc(pointer, size);
}

__attribute__((destructor)) static void report_cost(void)
{
    fprintf(stderr, "row_allocation_bytes=%llu\n",
        (unsigned long long)requested_bytes);
}
