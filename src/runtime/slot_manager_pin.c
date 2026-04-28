/*
 * Copyright (c) 2025 Pergyra Language Project
 * All rights reserved.
 *
 * Pinned slot views.  This owner keeps the Pin/Unpin ABI and secure payload
 * open/seal path separate from the core claim/read/write/release lifecycle.
 */

#include "slot_manager_internal.h"

#include <pthread.h>
#include <string.h>

SlotError
PergyraSlotPin(SlotManager *manager, const SlotHandle *handle,
               PgySlotPinMode mode, const TokenCapability *token,
               PgyPinnedView *outView)
{
    SlotEntry *entry;
    SlotError result = SLOT_SUCCESS;
    uint32_t tid = current_thread_id();

    if (manager == NULL || handle == NULL || outView == NULL)
        return SLOT_ERROR_INVALID_HANDLE;

    memset(outView, 0, sizeof(*outView));
    if (mode != PGY_SLOT_PIN_READ && mode != PGY_SLOT_PIN_WRITE)
        return SLOT_ERROR_INVALID_PIN;

    if (token != NULL && mode == PGY_SLOT_PIN_READ && !token->canRead)
        return SLOT_ERROR_PERMISSION_DENIED;
    if (token != NULL && mode == PGY_SLOT_PIN_WRITE
        && (!token->canRead || !token->canWrite))
        return SLOT_ERROR_PERMISSION_DENIED;

    if (token != NULL && !SlotValidateToken(manager, handle, token)) {
        slot_manager_record_security_violation(manager,
                                               "PIN_TOKEN_VALIDATION_FAILED",
                                               handle->slotId,
                                               "Pin denied because token validation failed");
        return SLOT_ERROR_PERMISSION_DENIED;
    }

    pthread_mutex_lock(manager_mutex(manager));
    entry = find_slot_entry_locked(manager, handle);
    if (entry == NULL) {
        manager->cacheMisses++;
        result = SLOT_ERROR_SLOT_NOT_FOUND;
        goto done;
    }
    if (entry->typeTag != handle->typeTag) {
        result = SLOT_ERROR_TYPE_MISMATCH;
        goto done;
    }
    if (slot_is_expired_locked(entry)) {
        result = SLOT_ERROR_TTL_EXPIRED;
        goto done;
    }
    if (entry->pinCount > 0) {
        result = SLOT_ERROR_PINNED;
        goto done;
    }
    if (entry->securityEnabled && token == NULL) {
        result = SLOT_ERROR_PERMISSION_DENIED;
        goto done;
    }
    if (!entry->securityEnabled && token != NULL) {
        result = SLOT_ERROR_PERMISSION_DENIED;
        goto done;
    }

    if (entry->securityEnabled) {
        SecurityError secResult;
        bool usedShadowRecovery = false;

        if (!slot_reserve_storage(entry, entry->securePayload.size)) {
            result = SLOT_ERROR_OUT_OF_MEMORY;
            goto done;
        }
        secResult = SecureSealedPayloadOpen(manager->securityContext,
                                            handle->slotId,
                                            handle->generation,
                                            &entry->securePayload,
                                            entry->dataBlockRef,
                                            entry->dataSize,
                                            &entry->dataSize,
                                            &usedShadowRecovery);
        if (usedShadowRecovery) {
            manager->securityViolations++;
            SlotManagerLogSecurityEvent(manager, "PIN_SHADOW_RECOVERY_SUCCESS",
                                        handle->slotId,
                                        "Recovered secure slot payload while pinning");
        }
        if (secResult != SECURITY_SUCCESS) {
            manager->securityViolations++;
            SlotManagerLogSecurityEvent(manager, "PIN_SEALED_PAYLOAD_VERIFY_FAILED",
                                        handle->slotId,
                                        "Secure sealed payload verification failed while pinning");
            result = SLOT_ERROR_PERMISSION_DENIED;
            goto done;
        }
    }

    entry->pinCount = 1;
    entry->pinMode = (uint32_t)mode;
    entry->pinThreadAffinity = tid;
    entry->pinGeneration = handle->generation;
    entry->threadAffinity = tid;
    entry->lastAccessTime = slot_now_us();
    entry->accessCount++;
    manager->cacheHits++;

    outView->ptr = entry->dataBlockRef;
    outView->size = entry->dataSize;
    outView->slotId = handle->slotId;
    outView->generation = handle->generation;
    outView->mode = mode;
    outView->valid = true;

done:
    pthread_mutex_unlock(manager_mutex(manager));
    return result;
}

SlotError
PergyraSlotUnpin(SlotManager *manager, PgyPinnedView *view)
{
    SlotEntry *entry;
    SlotError result = SLOT_SUCCESS;
    uint32_t tid = current_thread_id();

    if (manager == NULL || view == NULL || !view->valid)
        return SLOT_ERROR_INVALID_PIN;

    pthread_mutex_lock(manager_mutex(manager));
    entry = NULL;
    for (size_t i = 0; i < manager->tableSize; i++) {
        SlotEntry *candidate = &manager->slotTable[i];
        if (candidate->occupied && candidate->slotId == view->slotId) {
            entry = candidate;
            break;
        }
    }

    if (entry == NULL) {
        result = SLOT_ERROR_SLOT_NOT_FOUND;
        goto done;
    }
    if (entry->pinCount == 0 || entry->pinThreadAffinity != tid
        || entry->pinGeneration != view->generation
        || entry->pinMode != (uint32_t)view->mode
        || view->ptr != entry->dataBlockRef) {
        result = SLOT_ERROR_INVALID_PIN;
        goto done;
    }
    if (entry->securityEnabled && view->mode == PGY_SLOT_PIN_WRITE) {
        SecurityError secResult = SecureSealedPayloadSeal(manager->securityContext,
                                                          entry->slotId,
                                                          view->generation,
                                                          entry->dataBlockRef,
                                                          entry->dataSize,
                                                          &entry->securityPolicy,
                                                          &entry->securePayload);
        if (secResult != SECURITY_SUCCESS) {
            result = SLOT_ERROR_PERMISSION_DENIED;
            goto done;
        }
        entry->dataChecksum = slot_checksum_bytes(entry->dataBlockRef,
                                                  entry->dataSize);
    }

    entry->pinCount = 0;
    entry->pinMode = 0;
    entry->pinThreadAffinity = 0;
    entry->pinGeneration = 0;
    entry->threadAffinity = 0;
    entry->lastAccessTime = slot_now_us();
    if (entry->securityEnabled)
        slot_free_plain_buffer(entry);
    memset(view, 0, sizeof(*view));

done:
    pthread_mutex_unlock(manager_mutex(manager));
    return result;
}
