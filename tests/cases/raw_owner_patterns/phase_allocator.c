/* Runtime-level phase experiment, not a new Pergyra allocator annotation.
 * Payloads contain only plain bytes: reset never substitutes for a destructor.
 */
#include "pgy_runtime.h"
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>

int32_t PatternIterations(void);
void PatternStart(void);
void PatternFinish(int32_t);

#ifdef PHASE_CURSOR
typedef struct {
    PgyRegion owner;
    PgyRegionBlock *active;
    PgyRegionBlock *unvisited;
} PhaseCursor;

/* Test-only next-fit cursor. All blocks still belong to owner; both cursors
 * expire at reset. No production PgyRegion layout or policy is changed. */
static void *phase_cursor_allocate(PhaseCursor *phase, size_t size, size_t alignment)
{
    void *pointer = pgy_region_block_try_alloc(phase->active, size, alignment);
    while (pointer == NULL && phase->unvisited != NULL) {
        PgyRegionBlock *block = phase->unvisited;
        phase->unvisited = block->next;
        pointer = pgy_region_block_try_alloc(block, size, alignment);
        if (pointer != NULL) phase->active = block;
    }
    if (pointer == NULL) {
        phase->active = pgy_region_block_acquire(&phase->owner, size + alignment - 1);
        pointer = pgy_region_block_try_alloc(phase->active, size, alignment);
    }
    if (pointer == NULL) abort();
    if (phase->owner.total_allocated <= SIZE_MAX - size)
        phase->owner.total_allocated += size;
    return pointer;
}
#endif

int main(void)
{
    int32_t phases = PatternIterations();
    uint32_t *values[4096];
    uint64_t checksum = 0;
    size_t first_blocks = 0, final_blocks = 0, retained_bytes = 0;
#if defined(PHASE_REGION)
    PgyRegion region = pgy_region_create(8192);
#elif defined(PHASE_CURSOR)
    PhaseCursor cursor = {pgy_region_create(8192), NULL, NULL};
#endif
    PatternStart();
    for (int32_t phase = 0; phase < phases; ++phase) {
        for (size_t i = 0; i < 4096; ++i) {
#if defined(PHASE_REGION)
            values[i] = pgy_region_alloc(&region, 48, 16);
#elif defined(PHASE_CURSOR)
            values[i] = phase_cursor_allocate(&cursor, 48, 16);
#else
            values[i] = malloc(48);
#endif
            if (values[i] == NULL || (uintptr_t)values[i] % 16 != 0) abort();
            for (size_t word = 0; word < 12; ++word)
                values[i][word] = (uint32_t)((i * 37 + (size_t)phase * 17 + word) % 65536);
        }
        /* All addresses must remain stable until the phase is retired. */
        for (size_t reverse = 4096; reverse > 0; --reverse) {
            size_t i = reverse - 1;
            for (size_t word = 0; word < 12; ++word) {
                uint32_t expected = (uint32_t)((i * 37 + (size_t)phase * 17 + word) % 65536);
                if (values[i][word] != expected) abort();
                checksum += values[i][word];
            }
#if !defined(PHASE_REGION) && !defined(PHASE_CURSOR)
            free(values[i]);
#endif
            values[i] = NULL;
        }
#if defined(PHASE_REGION) || defined(PHASE_CURSOR)
#ifdef PHASE_REGION
        PgyRegion *owner = &region;
#else
        PgyRegion *owner = &cursor.owner;
#endif
        if (owner->total_allocated != 4096 * 48) abort();
        if (phase == 0 || phase == phases - 1) {
            final_blocks = retained_bytes = 0;
            for (PgyRegionBlock *block = owner->current; block != NULL; block = block->next) {
                ++final_blocks;
                retained_bytes += sizeof(*block) + block->capacity;
            }
            if (phase == 0) first_blocks = final_blocks;
        }
        pgy_region_reset(owner);
#ifdef PHASE_CURSOR
        cursor.active = NULL;
        cursor.unvisited = owner->current;
#endif
#endif
    }
#if defined(PHASE_REGION)
    pgy_region_destroy(&region);
#elif defined(PHASE_CURSOR)
    pgy_region_destroy(&cursor.owner);
#endif
    PatternFinish((int32_t)(checksum % 1000003));
    printf("{\"first_phase_blocks\":%zu,\"final_phase_blocks\":%zu,\"retained_bytes_before_destroy\":%zu}\n",
           first_blocks, final_blocks, retained_bytes);
    return 0;
}
