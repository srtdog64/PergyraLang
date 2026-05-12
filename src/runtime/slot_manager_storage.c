/*
 * Copyright (c) 2025 Pergyra Language Project
 * All rights reserved.
 *
 * Slot manager storage helpers shared by plain, secure, and pin operations.
 */

#include "slot_manager_internal.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

static void
slot_wipe_buffer(void *ptr, size_t size)
{
    if (ptr == NULL || size == 0)
        return;

    SecureMemoryWipe(ptr, size);
}

uint32_t
slot_checksum_bytes(const void *ptr, size_t size)
{
    const uint8_t *bytes = (const uint8_t *)ptr;
    uint32_t checksum = 0;

    if (bytes == NULL)
        return 0;

    for (size_t i = 0; i < size; i++) {
        checksum = (checksum << 5) | (checksum >> 27);
        checksum ^= bytes[i];
        checksum += bytes[i];
    }
    return checksum;
}

void
slot_free_plain_buffer(SlotEntry *entry)
{
    if (entry->dataBlockRef != NULL) {
        slot_wipe_buffer(entry->dataBlockRef, entry->dataSize);
        free(entry->dataBlockRef);
        entry->dataBlockRef = NULL;
    }
}

void
slot_free_buffers(SlotEntry *entry)
{
    slot_free_plain_buffer(entry);
    entry->dataSize = 0;
    SecureSealedPayloadDestroy(&entry->securePayload);
}

bool
slot_reserve_storage(SlotEntry *entry, size_t size)
{
    void *primary;

    if (size == 0) {
        slot_free_buffers(entry);
        return true;
    }

    /* Keep slot payload allocations bounded; large payloads should use storage APIs. */
    if (size > (256UL * 1024UL * 1024UL))
        return false;

    if (entry->dataBlockRef != NULL && entry->dataSize == size)
        return true;

    primary = malloc(size);
    if (primary == NULL)
        return false;

    slot_free_plain_buffer(entry);
    entry->dataBlockRef = primary;
    entry->dataSize = size;
    return true;
}

bool
slot_store_plain_payload(SlotEntry *entry, const void *data, size_t size)
{
    if (!slot_reserve_storage(entry, size))
        return false;

    if (size == 0)
        return true;

    memcpy(entry->dataBlockRef, data, size);
    return true;
}
