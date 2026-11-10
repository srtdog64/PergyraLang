#include "allocator_routes.h"
#include <stdint.h>
#include <stdlib.h>

int main(void)
{
    PatternHeapPrepare();
    const size_t sizes[] = {1, 17, 33, 257, 1025, 4097};
    unsigned char *pointer = PatternMalloc(sizes[0]);
    if (pointer == NULL) abort();
    pointer[0] = 53;
    for (size_t i = 1; i < sizeof(sizes) / sizeof(sizes[0]); ++i) {
        unsigned char *grown = PatternRealloc(pointer, sizes[i]);
        if (grown == NULL || grown[0] != 53 || (uintptr_t)grown % _Alignof(max_align_t) != 0) abort();
        pointer = grown;
        pointer[sizes[i] - 1] = 97;
    }
    if (PatternCalloc(SIZE_MAX, 2) != NULL) abort();
    unsigned char *zeroed = PatternCalloc(37, 3);
    if (zeroed == NULL) abort();
    for (size_t i = 0; i < 111; ++i) if (zeroed[i] != 0) abort();
    PatternFree(zeroed);
#ifdef PATTERN_TLSF
    /* Fixed pool exhaustion must stay local. No libc fallback; old data lives
     * across a failed realloc and may still be retired exactly once. */
    if (PatternMalloc(32 * 1024 * 1024) != NULL) abort();
    if (PatternRealloc(pointer, 32 * 1024 * 1024) != NULL) abort();
    if (pointer[0] != 53 || pointer[sizes[5] - 1] != 97) abort();
#endif
    PatternFree(pointer);
    PatternHeapFinish();
    return 0;
}
