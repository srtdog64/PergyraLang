/*
 * Copyright (c) 2025 Pergyra Language Project
 * All rights reserved.
 *
 * Secure slot manager operations.
 */

#include "slot_manager.h"
#include "slot_manager_internal.h"

#include <pthread.h>
#include <string.h>

SlotError
SlotManagerEnableSecurity(SlotManager *manager, SecurityLevel level)
{
    if (manager == NULL)
        return SLOT_ERROR_INVALID_HANDLE;

    if (!SecurityLevelIsValid(level))
        return SLOT_ERROR_INVALID_HANDLE;

    if (manager->securityContext == NULL) {
        manager->securityContext = SecurityContextCreate(level);
        if (manager->securityContext == NULL)
            return SLOT_ERROR_OUT_OF_MEMORY;
    }

    manager->securityEnabled = true;
    manager->defaultSecurityLevel = level;
    return SLOT_SUCCESS;
}

SlotError
SlotManagerDisableSecurity(SlotManager *manager)
{
    size_t i;

    if (manager == NULL)
        return SLOT_ERROR_INVALID_HANDLE;

    pthread_mutex_lock(manager_mutex(manager));
    for (i = 0; i < manager->tableSize; i++) {
        SlotEntry *entry = &manager->slotTable[i];
        SecureMemoryWipe(&entry->writeToken, sizeof(entry->writeToken));
        SecureSealedPayloadDestroy(&entry->securePayload);
        entry->securityEnabled = false;
        entry->tokenGeneration = 0;
        entry->securityLevel = SECURITY_LEVEL_BASIC;
        memset(&entry->securityPolicy, 0, sizeof(entry->securityPolicy));
    }
    pthread_mutex_unlock(manager_mutex(manager));

    if (manager->securityContext != NULL) {
        SecurityContextDestroy(manager->securityContext);
        manager->securityContext = NULL;
    }

    manager->securityEnabled = false;
    return SLOT_SUCCESS;
}

bool
SlotManagerIsSecurityEnabled(const SlotManager *manager)
{
    return manager != NULL && manager->securityEnabled &&
           manager->securityContext != NULL;
}

SlotError
SlotManagerSetDefaultSecurityLevel(SlotManager *manager, SecurityLevel level)
{
    if (manager == NULL)
        return SLOT_ERROR_INVALID_HANDLE;

    if (!SecurityLevelIsValid(level))
        return SLOT_ERROR_INVALID_HANDLE;

    manager->defaultSecurityLevel = level;
    if (manager->securityContext != NULL)
        manager->securityContext->defaultLevel = level;
    return SLOT_SUCCESS;
}

static SlotError
store_slot_token(SlotManager *manager, SlotEntry *entry,
                 const TokenCapability *token)
{
    SecurityError secResult;

    if (manager->securityContext == NULL || entry == NULL || token == NULL)
        return SLOT_ERROR_PERMISSION_DENIED;

    secResult = TokenEncrypt(manager->securityContext, &token->token,
                             &entry->writeToken);
    if (secResult != SECURITY_SUCCESS)
        return SLOT_ERROR_PERMISSION_DENIED;

    entry->tokenGeneration = token->token.generation;
    return SLOT_SUCCESS;
}

SlotError
SlotClaimSecure(SlotManager *manager, TypeTag type, SecurityLevel level,
                SlotHandle *handle, TokenCapability *token)
{
    SlotEntry *entry;
    SlotError result;
    SecurityError secResult;

    if (manager == NULL || handle == NULL || token == NULL)
        return SLOT_ERROR_INVALID_HANDLE;

    if (!SecurityLevelIsValid(level))
        return SLOT_ERROR_INVALID_HANDLE;

    if (!SlotManagerIsSecurityEnabled(manager))
        return SLOT_ERROR_PERMISSION_DENIED;

    result = SlotClaim(manager, type, handle);
    if (result != SLOT_SUCCESS)
        return result;

    secResult = TokenGenerate(manager->securityContext, handle->slotId, level, token);
    if (secResult != SECURITY_SUCCESS) {
        SlotRelease(manager, handle);
        return SLOT_ERROR_PERMISSION_DENIED;
    }

    pthread_mutex_lock(manager_mutex(manager));
    entry = find_slot_entry_locked(manager, handle);
    if (entry == NULL) {
        pthread_mutex_unlock(manager_mutex(manager));
        SlotRelease(manager, handle);
        return SLOT_ERROR_SLOT_NOT_FOUND;
    }

    entry->securityEnabled = true;
    entry->securityLevel = level;
    entry->securityPolicy = SecurityPolicyForLevel(level);
    entry->lastAccessTime = slot_now_us();
    entry->accessCount = 0;
    result = store_slot_token(manager, entry, token);
    pthread_mutex_unlock(manager_mutex(manager));
    return result;
}

bool
SlotValidateToken(SlotManager *manager, const SlotHandle *handle,
                  const TokenCapability *token)
{
    SlotEntry *entry;
    bool valid = false;

    if (manager == NULL || handle == NULL || token == NULL)
        return false;

    if (!SlotManagerIsSecurityEnabled(manager))
        return false;

    if (TokenValidate(manager->securityContext, handle->slotId, token) !=
        SECURITY_SUCCESS)
        return false;

    pthread_mutex_lock(manager_mutex(manager));
    entry = find_slot_entry_locked(manager, handle);
    if (entry != NULL && entry->securityEnabled && entry->tokenGeneration != 0 &&
        entry->tokenGeneration == token->token.generation)
        valid = true;
    pthread_mutex_unlock(manager_mutex(manager));
    return valid;
}

SlotError
SlotWriteSecure(SlotManager *manager, const SlotHandle *handle,
                const void *data, size_t dataSize,
                const TokenCapability *token)
{
    SlotEntry *entry;
    SecurityError secResult;

    if (manager == NULL || handle == NULL || data == NULL || token == NULL)
        return SLOT_ERROR_INVALID_HANDLE;

    if (!token->canWrite)
        return SLOT_ERROR_PERMISSION_DENIED;

    if (!SlotValidateToken(manager, handle, token)) {
        slot_manager_record_security_violation(manager, "TOKEN_VALIDATION_FAILED",
                                               handle->slotId,
                                               "Write denied because token validation failed");
        return SLOT_ERROR_PERMISSION_DENIED;
    }

    pthread_mutex_lock(manager_mutex(manager));
    entry = find_slot_entry_locked(manager, handle);
    if (entry == NULL) {
        pthread_mutex_unlock(manager_mutex(manager));
        return SLOT_ERROR_SLOT_NOT_FOUND;
    }
    if (entry->pinCount > 0) {
        pthread_mutex_unlock(manager_mutex(manager));
        return SLOT_ERROR_PINNED;
    }
    secResult = SecureSealedPayloadSeal(manager->securityContext,
                                        handle->slotId,
                                        handle->generation,
                                        data,
                                        dataSize,
                                        &entry->securityPolicy,
                                        &entry->securePayload);
    if (secResult != SECURITY_SUCCESS) {
        pthread_mutex_unlock(manager_mutex(manager));
        return SLOT_ERROR_OUT_OF_MEMORY;
    }
    slot_free_plain_buffer(entry);
    entry->dataSize = dataSize;
    entry->lastAccessTime = slot_now_us();
    entry->accessCount++;
    pthread_mutex_unlock(manager_mutex(manager));
    return SLOT_SUCCESS;
}

SlotError
SlotReadSecure(SlotManager *manager, const SlotHandle *handle,
               void *buffer, size_t bufferSize, size_t *bytesRead,
               const TokenCapability *token)
{
    SlotEntry *entry;
    SecurityError secResult;
    bool usedShadowRecovery = false;

    if (manager == NULL || handle == NULL || buffer == NULL || token == NULL)
        return SLOT_ERROR_INVALID_HANDLE;

    if (!token->canRead)
        return SLOT_ERROR_PERMISSION_DENIED;

    if (!SlotValidateToken(manager, handle, token)) {
        slot_manager_record_security_violation(manager, "TOKEN_VALIDATION_FAILED",
                                               handle->slotId,
                                               "Read denied because token validation failed");
        return SLOT_ERROR_PERMISSION_DENIED;
    }

    pthread_mutex_lock(manager_mutex(manager));
    entry = find_slot_entry_locked(manager, handle);
    if (entry == NULL) {
        pthread_mutex_unlock(manager_mutex(manager));
        return SLOT_ERROR_SLOT_NOT_FOUND;
    }
    if (entry->pinCount > 0) {
        pthread_mutex_unlock(manager_mutex(manager));
        return SLOT_ERROR_PINNED;
    }
    secResult = SecureSealedPayloadOpen(manager->securityContext,
                                        handle->slotId,
                                        handle->generation,
                                        &entry->securePayload,
                                        buffer,
                                        bufferSize,
                                        bytesRead,
                                        &usedShadowRecovery);
    if (usedShadowRecovery) {
        manager->securityViolations++;
        SlotManagerLogSecurityEvent(manager, "SHADOW_RECOVERY_SUCCESS",
                                    handle->slotId,
                                    "Recovered secure slot payload from shadow copy");
    }
    if (secResult != SECURITY_SUCCESS) {
        manager->securityViolations++;
        SlotManagerLogSecurityEvent(manager, "SEALED_PAYLOAD_VERIFY_FAILED",
                                    handle->slotId,
                                    "Secure sealed payload verification failed");
        pthread_mutex_unlock(manager_mutex(manager));
        return SLOT_ERROR_PERMISSION_DENIED;
    }
    entry->dataSize = entry->securePayload.size;
    entry->lastAccessTime = slot_now_us();
    entry->accessCount++;
    pthread_mutex_unlock(manager_mutex(manager));
    return SLOT_SUCCESS;
}

SlotError
SlotReleaseSecure(SlotManager *manager, const SlotHandle *handle,
                  const TokenCapability *token)
{
    SlotEntry *entry;
    SlotError result;

    if (manager == NULL || handle == NULL || token == NULL)
        return SLOT_ERROR_INVALID_HANDLE;

    if (!SlotValidateToken(manager, handle, token)) {
        slot_manager_record_security_violation(manager,
                                               "RELEASE_TOKEN_VALIDATION_FAILED",
                                               handle->slotId,
                                               "Cannot release secure slot without a valid token");
        return SLOT_ERROR_PERMISSION_DENIED;
    }

    pthread_mutex_lock(manager_mutex(manager));
    entry = find_slot_entry_locked(manager, handle);
    result = slot_release_entry_locked(manager, entry, true);
    pthread_mutex_unlock(manager_mutex(manager));
    return result;
}

SlotError
SlotRefreshToken(SlotManager *manager, const SlotHandle *handle,
                 TokenCapability *token)
{
    SlotEntry *entry;
    SecurityError secResult;

    if (manager == NULL || handle == NULL || token == NULL)
        return SLOT_ERROR_INVALID_HANDLE;

    if (!SlotValidateToken(manager, handle, token))
        return SLOT_ERROR_PERMISSION_DENIED;

    secResult = TokenRefresh(manager->securityContext, token);
    if (secResult != SECURITY_SUCCESS)
        return SLOT_ERROR_PERMISSION_DENIED;

    pthread_mutex_lock(manager_mutex(manager));
    entry = find_slot_entry_locked(manager, handle);
    if (entry == NULL) {
        pthread_mutex_unlock(manager_mutex(manager));
        return SLOT_ERROR_SLOT_NOT_FOUND;
    }

    store_slot_token(manager, entry, token);
    pthread_mutex_unlock(manager_mutex(manager));

    SlotManagerLogSecurityEvent(manager, "TOKEN_REFRESHED", handle->slotId,
                                "Token successfully refreshed");
    return SLOT_SUCCESS;
}

SlotError
SlotRevokeToken(SlotManager *manager, const SlotHandle *handle)
{
    SlotEntry *entry;

    if (manager == NULL || handle == NULL)
        return SLOT_ERROR_INVALID_HANDLE;

    pthread_mutex_lock(manager_mutex(manager));
    entry = find_slot_entry_locked(manager, handle);
    if (entry == NULL) {
        pthread_mutex_unlock(manager_mutex(manager));
        return SLOT_ERROR_SLOT_NOT_FOUND;
    }

    SecureMemoryWipe(&entry->writeToken, sizeof(entry->writeToken));
    entry->tokenGeneration = 0;
    pthread_mutex_unlock(manager_mutex(manager));

    SlotManagerLogSecurityEvent(manager, "TOKEN_REVOKED", handle->slotId,
                                "Token revoked by administrator");
    return SLOT_SUCCESS;
}

SlotManager *
SlotManagerCreateSecure(size_t maxSlots, size_t memoryPoolSize,
                        bool enableSecurity, SecurityLevel defaultLevel)
{
    SlotManager *manager = SlotManagerCreate(maxSlots, memoryPoolSize);

    if (manager == NULL)
        return NULL;

    if (enableSecurity &&
        SlotManagerEnableSecurity(manager, defaultLevel) != SLOT_SUCCESS) {
        SlotManagerDestroy(manager);
        return NULL;
    }

    return manager;
}

void
SlotManagerDestroySecure(SlotManager *manager)
{
    SlotManagerDestroy(manager);
}
