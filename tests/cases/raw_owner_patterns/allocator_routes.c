#include "allocator_routes.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(PATTERN_TLSF)
#include "tlsf.h"
static tlsf_t heap;
static void *pool;
void *__real_malloc(size_t);
void __real_free(void *);
#elif defined(PATTERN_MIMALLOC)
#include "mimalloc.h"
#else
void *__real_malloc(size_t);
void *__real_calloc(size_t, size_t);
void *__real_realloc(void *, size_t);
void __real_free(void *);
#endif

void PatternHeapPrepare(void)
{
#if defined(PATTERN_TLSF)
    pool = __real_malloc(16 * 1024 * 1024);
    if (pool == NULL) { fputs("TLSF pool allocation failed\n", stderr); exit(94); }
    heap = tlsf_create_with_pool(pool, 16 * 1024 * 1024);
    if (heap == NULL) { fputs("TLSF initialization failed\n", stderr); exit(94); }
#elif defined(PATTERN_MIMALLOC)
    mi_process_init();
#endif
}

void PatternHeapFinish(void)
{
#if defined(PATTERN_TLSF)
    if (tlsf_check(heap) != 0 || tlsf_check_pool(tlsf_get_pool(heap)) != 0) {
        fputs("TLSF integrity check failed\n", stderr);
        exit(94);
    }
    tlsf_destroy(heap);
    __real_free(pool);
    heap = NULL;
    pool = NULL;
#elif defined(PATTERN_MIMALLOC)
    mi_collect(true);
#endif
}

void *PatternMalloc(size_t size)
{
#if defined(PATTERN_TLSF)
    return tlsf_memalign(heap, _Alignof(max_align_t), size);
#elif defined(PATTERN_MIMALLOC)
    return mi_malloc(size);
#else
    return __real_malloc(size);
#endif
}

void PatternFree(void *pointer)
{
#if defined(PATTERN_TLSF)
    if (pointer != NULL) tlsf_free(heap, pointer);
#elif defined(PATTERN_MIMALLOC)
    mi_free(pointer);
#else
    __real_free(pointer);
#endif
}

void *PatternCalloc(size_t count, size_t size)
{
    if (size != 0 && count > SIZE_MAX / size) return NULL;
#if defined(PATTERN_TLSF)
    void *pointer = PatternMalloc(count * size);
    if (pointer != NULL) memset(pointer, 0, count * size);
    return pointer;
#elif defined(PATTERN_MIMALLOC)
    return mi_calloc(count, size);
#else
    return __real_calloc(count, size);
#endif
}

void *PatternRealloc(void *old, size_t size)
{
#if defined(PATTERN_TLSF)
    /* tlsf_realloc only promises its native alignment. Preserve malloc's
     * max_align_t contract even when a resize moves the block. */
    if (old == NULL) return PatternMalloc(size);
    if (size == 0) { PatternFree(old); return NULL; }
    void *pointer = PatternMalloc(size);
    if (pointer == NULL) return NULL;
    size_t previous = tlsf_block_size(old);
    memcpy(pointer, old, previous < size ? previous : size);
    PatternFree(old);
    return pointer;
#elif defined(PATTERN_MIMALLOC)
    return mi_realloc(old, size);
#else
    return __real_realloc(old, size);
#endif
}
