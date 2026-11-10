#ifndef PGY_PATTERN_ALLOCATOR_ROUTES_H
#define PGY_PATTERN_ALLOCATOR_ROUTES_H
#include <stddef.h>
/* Experiment boundary only; no change to the production runtime allocator. */
void PatternHeapPrepare(void);
void PatternHeapFinish(void);
void *PatternMalloc(size_t size);
void *PatternCalloc(size_t count, size_t size);
void *PatternRealloc(void *pointer, size_t size);
void PatternFree(void *pointer);
#endif
