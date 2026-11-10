/* Existing manager-handle ABI only, distinct from source Slot<T>'s ABI.
 * A pin is a temporary checked lease; it never owns the manager or payload.
 */
#include "slot_manager.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

int32_t PatternIterations(void);
void PatternStart(void);
void PatternFinish(int32_t);

static void require(SlotError actual, SlotError expected)
{
    if (actual != expected) { fprintf(stderr, "slot status %d, expected %d\n", actual, expected); abort(); }
}

int main(void)
{
    int32_t count = PatternIterations();
    PatternStart();
    uint64_t checksum = 0;
#ifdef PATTERN_RAW_SLOT
    volatile int32_t *pointer = malloc(sizeof(*pointer));
    if (pointer == NULL) abort();
    *pointer = 0;
#else
    SlotManager *manager = SlotManagerCreate(8, 4096);
    SlotManager *foreign = SlotManagerCreate(8, 4096);
    if (manager == NULL || foreign == NULL) abort();
    SlotHandle handle;
    int32_t value = 0;
    size_t bytes;
    require(SlotClaim(manager, TYPE_INT, &handle), SLOT_SUCCESS);
    require(SlotWrite(manager, &handle, &value, sizeof(value)), SLOT_SUCCESS);
    SlotHandle wrong = handle;
    wrong.generation += 1;
    if (SlotRead(manager, &wrong, &value, sizeof(value), &bytes) == SLOT_SUCCESS) abort();
    if (SlotRead(foreign, &handle, &value, sizeof(value), &bytes) == SLOT_SUCCESS) abort();
    PgyPinnedView view;
    require(PergyraSlotPin(manager, &handle, PGY_SLOT_PIN_WRITE, NULL, &view), SLOT_SUCCESS);
    require(SlotRelease(manager, &handle), SLOT_ERROR_PINNED);
    require(SlotWrite(manager, &handle, &value, sizeof(value)), SLOT_ERROR_PINNED);
    if (!view.valid || view.size != sizeof(value)) abort();
#ifdef PATTERN_PIN_ONCE
    volatile int32_t *pointer = view.ptr;
#else
    require(PergyraSlotUnpin(manager, &view), SLOT_SUCCESS);
#endif
#endif
    for (int32_t i = 0; i < count; ++i) {
#if defined(PATTERN_RAW_SLOT) || defined(PATTERN_PIN_ONCE)
        *pointer = (*pointer + i % 17 + 1) % 1024;
        checksum += (uint32_t)*pointer;
#else
        require(SlotRead(manager, &handle, &value, sizeof(value), &bytes), SLOT_SUCCESS);
        if (bytes != sizeof(value)) abort();
        value = (value + i % 17 + 1) % 1024;
        require(SlotWrite(manager, &handle, &value, sizeof(value)), SLOT_SUCCESS);
        checksum += (uint32_t)value;
#endif
    }
#ifdef PATTERN_RAW_SLOT
    free((void *)pointer);
#else
#ifdef PATTERN_PIN_ONCE
    /* Drop the local pointer before retiring its lease and owner. */
    pointer = NULL;
    require(PergyraSlotUnpin(manager, &view), SLOT_SUCCESS);
#endif
    require(PergyraSlotUnpin(manager, &view), SLOT_ERROR_INVALID_PIN);
    SlotHandle old = handle;
    require(SlotRelease(manager, &handle), SLOT_SUCCESS);
    require(SlotClaim(manager, TYPE_INT, &handle), SLOT_SUCCESS);
    value = 71;
    require(SlotWrite(manager, &handle, &value, sizeof(value)), SLOT_SUCCESS);
    if (SlotRead(manager, &old, &value, sizeof(value), &bytes) == SLOT_SUCCESS) abort();
    require(SlotRelease(manager, &handle), SLOT_SUCCESS);
    SlotManagerDestroy(foreign);
    SlotManagerDestroy(manager);
#endif
    PatternFinish((int32_t)(checksum % 1000003));
    return 0;
}
