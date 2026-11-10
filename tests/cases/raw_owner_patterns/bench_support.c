#define _POSIX_C_SOURCE 200809L
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static struct timespec started;
static int32_t iterations;

#ifdef PATTERN_USE_ALLOCATOR
#include "allocator_routes.h"
#define BACKING_MALLOC PatternMalloc
#define BACKING_CALLOC PatternCalloc
#define BACKING_REALLOC PatternRealloc
#define BACKING_FREE PatternFree
#else
#define BACKING_MALLOC __real_malloc
#define BACKING_CALLOC __real_calloc
#define BACKING_REALLOC __real_realloc
#define BACKING_FREE __real_free
#endif

#ifdef PATTERN_OBSERVE
typedef struct { void *pointer; size_t size; unsigned state; } Entry;
static Entry entries[8192];
static uint64_t allocations, releases, reallocations, live, peak_bytes, live_bytes;
static int active;
void *__real_malloc(size_t);
void *__real_calloc(size_t, size_t);
void *__real_realloc(void *, size_t);
void __real_free(void *);

static size_t locate(void *pointer, int insertion)
{
    size_t at = ((uintptr_t)pointer >> 4) & 8191, vacant = 8192;
    for (size_t n = 0; n < 8192; ++n, at = (at + 1) & 8191) {
        if (entries[at].state == 1 && entries[at].pointer == pointer) return at;
        if (entries[at].state == 2 && vacant == 8192) vacant = at;
        if (entries[at].state == 0) return insertion ? (vacant < 8192 ? vacant : at) : 8192;
    }
    if (insertion && vacant < 8192) return vacant;
    if (insertion) { fprintf(stderr, "allocation observer capacity exceeded\n"); _Exit(90); }
    return 8192;
}

static void record_allocation(void *pointer, size_t size)
{
    if (!active || pointer == NULL) return;
    size_t at = locate(pointer, 1);
    entries[at] = (Entry){pointer, size, 1};
    ++live;
    live_bytes += size;
    if (live_bytes > peak_bytes) peak_bytes = live_bytes;
}

static void record_retirement(void *pointer)
{
    if (!active || pointer == NULL) return;
    size_t at = locate(pointer, 0);
    if (at == 8192) return;
    --live;
    live_bytes -= entries[at].size;
    entries[at].state = 2;
}

void *__wrap_malloc(size_t size)
{
    void *pointer = BACKING_MALLOC(size);
    if (active && pointer != NULL) ++allocations;
    record_allocation(pointer, size);
    return pointer;
}
void *__wrap_calloc(size_t count, size_t size)
{
    if (size != 0 && count > SIZE_MAX / size) _Exit(91);
    void *pointer = BACKING_CALLOC(count, size);
    if (active && pointer != NULL) ++allocations;
    record_allocation(pointer, count * size);
    return pointer;
}
void *__wrap_realloc(void *old, size_t size)
{
    int was_null = old == NULL;
    size_t at = active && !was_null ? locate(old, 0) : 8192;
    void *pointer = BACKING_REALLOC(old, size);
    if (active) {
        ++reallocations;
        if (was_null && pointer != NULL) ++allocations;
        if (pointer != NULL || size == 0) {
            if (at != 8192) {
                --live;
                live_bytes -= entries[at].size;
                entries[at].state = 2;
            }
            record_allocation(pointer, size);
        }
    }
    return pointer;
}
void __wrap_free(void *pointer)
{
    if (active && pointer != NULL && locate(pointer, 0) != 8192) ++releases;
    record_retirement(pointer);
    BACKING_FREE(pointer);
}
#elif defined(PATTERN_USE_ALLOCATOR)
void *__wrap_malloc(size_t size) { return PatternMalloc(size); }
void *__wrap_calloc(size_t count, size_t size) { return PatternCalloc(count, size); }
void *__wrap_realloc(void *old, size_t size) { return PatternRealloc(old, size); }
void __wrap_free(void *pointer) { PatternFree(pointer); }
#endif

int32_t PatternSeed(void)
{
    const char *raw = getenv("PGY_PATTERN_SEED");
    if (raw == NULL) return 0;
    char *end;
    long parsed = strtol(raw, &end, 10);
    if (*end != '\0' || parsed < 0 || parsed > 250) exit(92);
    return (int32_t)parsed;
}

int32_t PatternIterations(void)
{
    const char *raw = getenv("PGY_PATTERN_ITERATIONS");
    char *end;
    long parsed = raw != NULL ? strtol(raw, &end, 10) : 0;
    if (raw == NULL || *end != '\0' || parsed < 1 || parsed > 2000000) exit(92);
    iterations = (int32_t)parsed;
#ifdef PATTERN_USE_ALLOCATOR
    PatternHeapPrepare();
#endif
    return iterations;
}
void PatternStart(void)
{
#ifdef PATTERN_OBSERVE
    memset(entries, 0, sizeof(entries));
    allocations = releases = reallocations = live = peak_bytes = live_bytes = 0;
    active = 1;
#endif
    if (clock_gettime(CLOCK_MONOTONIC, &started) != 0) exit(93);
}
void PatternFinish(int32_t value)
{
    struct timespec finished;
    if (clock_gettime(CLOCK_MONOTONIC, &finished) != 0) exit(93);
    uint64_t elapsed = (uint64_t)(finished.tv_sec - started.tv_sec) * UINT64_C(1000000000)
                     + (uint64_t)(finished.tv_nsec - started.tv_nsec);
#ifdef PATTERN_OBSERVE
    active = 0;
    printf("{\"iterations\":%d,\"checksum\":%d,\"elapsed_ns\":%llu,\"allocations\":%llu,\"frees\":%llu,\"reallocations\":%llu,\"live_blocks\":%llu,\"peak_requested_bytes\":%llu}\n",
        iterations, value, (unsigned long long)elapsed,
        (unsigned long long)allocations, (unsigned long long)releases,
        (unsigned long long)reallocations, (unsigned long long)live,
        (unsigned long long)peak_bytes);
    /* An observation table must not keep a leak reachable from LeakSanitizer. */
    memset(entries, 0, sizeof(entries));
#else
    printf("{\"iterations\":%d,\"checksum\":%d,\"elapsed_ns\":%llu}\n",
        iterations, value, (unsigned long long)elapsed);
#endif
    if (fflush(stdout) != 0) exit(95);
#ifdef PATTERN_USE_ALLOCATOR
    PatternHeapFinish();
#endif
}
