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
    SecurityContext *context;

    if (manager == NULL)
        return SLOT_ERROR_INVALID_HANDLE;

    if (!SecurityLevelIsValid(level))
        return SLOT_ERROR_INVALID_HANDLE;

    pthread_mutex_lock(manager_mutex(manager));
    if (manager->securityContext == NULL) {
        context = SecurityContextCreate(level);
        if (context == NULL) {
            pthread_mutex_unlock(manager_mutex(manager));
            return SLOT_ERROR_OUT_OF_MEMORY;
        }
        manager->securityContext = context;
    }

    manager->securityEnabled = true;
    manager->defaultSecurityLevel = level;
    manager->securityContext->defaultLevel = level;
    pthread_mutex_unlock(manager_mutex(manager));
    return SLOT_SUCCESS;
}

SlotError
SlotManagerDisableSecurity(SlotManager *manager)
{
    SecurityContext *context;
    size_t i;

    if (manager == NULL)
        return SLOT_ERROR_INVALID_HANDLE;

    pthread_mutex_lock(manager_mutex(manager));
    for (i = 0; i < manager->tableSize; i++) {
        SlotEntry *entry = &manager->slotTable[i];
        SecureMemoryWipe(&entry->writeToken, sizeof(entry->writeToken));
        if (entry->occupied && entry->securePayload.initialized)
            SecureSealedPayloadDestroy(&entry->securePayload);
        entry->securityEnabled = false;
        entry->tokenGeneration = 0;
        entry->securityLevel = SECURITY_LEVEL_BASIC;
        memset(&entry->securityPolicy, 0, sizeof(entry->securityPolicy));
    }
    context = manager->securityContext;
    manager->securityContext = NULL;
    manager->securityEnabled = false;
    pthread_mutex_unlock(manager_mutex(manager));

    if (context != NULL)
        SecurityContextDestroy(context);
    return SLOT_SUCCESS;
}

bool
SlotManagerIsSecurityEnabled(const SlotManager *manager)
{
    pthread_mutex_t *mutex;
    bool enabled;

    if (manager == NULL || manager->mutex == NULL)
        return false;

    mutex = (pthread_mutex_t *)manager->mutex;
    pthread_mutex_lock(mutex);
    enabled = manager->securityEnabled && manager->securityContext != NULL;
    pthread_mutex_unlock(mutex);
    return enabled;
}

SlotError
SlotManagerSetDefaultSecurityLevel(SlotManager *manager, SecurityLevel level)
{
    if (manager == NULL)
        return SLOT_ERROR_INVALID_HANDLE;

    if (!SecurityLevelIsValid(level))
        return SLOT_ERROR_INVALID_HANDLE;

    pthread_mutex_lock(manager_mutex(manager));
    manager->defaultSecurityLevel = level;
    if (manager->securityContext != NULL)
        manager->securityContext->defaultLevel = level;
    pthread_mutex_unlock(manager_mutex(manager));
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

bool
slot_token_valid_for_entry_locked(SlotManager *manager,
                                  const SlotHandle *handle,
                                  const TokenCapability *token,
                                  SlotEntry *entry)
{
    SecureToken storedToken;
    bool valid = false;

    if (manager == NULL || handle == NULL || token == NULL || entry == NULL)
        return false;
    if (!manager->securityEnabled || manager->securityContext == NULL)
        return false;
    if (!entry->securityEnabled ||
        entry->securityLevel != token->level ||
        entry->tokenGeneration == 0 ||
        entry->tokenGeneration != token->token.generation)
        return false;
    if (TokenValidate(manager->securityContext, handle->slotId, token) !=
        SECURITY_SUCCESS)
        return false;
    if (TokenDecrypt(manager->securityContext, &entry->writeToken,
                     &storedToken) == SECURITY_SUCCESS) {
        valid = TokenCompareSecure(&storedToken, &token->token);
        SecureMemoryWipe(&storedToken, sizeof(storedToken));
    }
    return valid;
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

    result = SlotClaim(manager, type, handle);
    if (result != SLOT_SUCCESS)
        return result;

    pthread_mutex_lock(manager_mutex(manager));
    entry = find_slot_entry_locked(manager, handle);
    if (entry == NULL) {
        pthread_mutex_unlock(manager_mutex(manager));
        return SLOT_ERROR_SLOT_NOT_FOUND;
    }
    if (!manager->securityEnabled || manager->securityContext == NULL) {
        result = slot_release_entry_locked(manager, entry, false);
        pthread_mutex_unlock(manager_mutex(manager));
        return result == SLOT_SUCCESS ? SLOT_ERROR_PERMISSION_DENIED : result;
    }
    secResult = TokenGenerate(manager->securityContext, handle->slotId, level, token);
    if (secResult != SECURITY_SUCCESS) {
        result = slot_release_entry_locked(manager, entry, false);
        (void)result;
        pthread_mutex_unlock(manager_mutex(manager));
        return SLOT_ERROR_PERMISSION_DENIED;
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

    pthread_mutex_lock(manager_mutex(manager));
    entry = find_slot_entry_locked(manager, handle);
    valid = slot_token_valid_for_entry_locked(manager, handle, token, entry);
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
    bool tokenValid;

    if (manager == NULL || handle == NULL || data == NULL || token == NULL)
        return SLOT_ERROR_INVALID_HANDLE;

    if (!token->canWrite)
        return SLOT_ERROR_PERMISSION_DENIED;

    pthread_mutex_lock(manager_mutex(manager));
    entry = find_slot_entry_locked(manager, handle);
    if (entry == NULL) {
        pthread_mutex_unlock(manager_mutex(manager));
        return SLOT_ERROR_SLOT_NOT_FOUND;
    }
    tokenValid = slot_token_valid_for_entry_locked(manager, handle, token, entry);
    if (!tokenValid) {
        pthread_mutex_unlock(manager_mutex(manager));
        slot_manager_record_security_violation(manager, "TOKEN_VALIDATION_FAILED",
                                               handle->slotId,
                                               "Write denied because token validation failed");
        return SLOT_ERROR_PERMISSION_DENIED;
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
    bool tokenValid;

    if (manager == NULL || handle == NULL || buffer == NULL || token == NULL)
        return SLOT_ERROR_INVALID_HANDLE;

    if (!token->canRead)
        return SLOT_ERROR_PERMISSION_DENIED;

    pthread_mutex_lock(manager_mutex(manager));
    entry = find_slot_entry_locked(manager, handle);
    if (entry == NULL) {
        pthread_mutex_unlock(manager_mutex(manager));
        return SLOT_ERROR_SLOT_NOT_FOUND;
    }
    tokenValid = slot_token_valid_for_entry_locked(manager, handle, token, entry);
    if (!tokenValid) {
        pthread_mutex_unlock(manager_mutex(manager));
        slot_manager_record_security_violation(manager, "TOKEN_VALIDATION_FAILED",
                                               handle->slotId,
                                               "Read denied because token validation failed");
        return SLOT_ERROR_PERMISSION_DENIED;
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
        slot_manager_log_security_event_locked(
            manager, "SHADOW_RECOVERY_SUCCESS", handle->slotId,
            "Recovered secure slot payload from shadow copy");
    }
    if (secResult != SECURITY_SUCCESS) {
        manager->securityViolations++;
        slot_manager_log_security_event_locked(
            manager, "SEALED_PAYLOAD_VERIFY_FAILED", handle->slotId,
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
    bool tokenValid;

    if (manager == NULL || handle == NULL || token == NULL)
        return SLOT_ERROR_INVALID_HANDLE;

    pthread_mutex_lock(manager_mutex(manager));
    entry = find_slot_entry_locked(manager, handle);
    if (entry == NULL) {
        pthread_mutex_unlock(manager_mutex(manager));
        return SLOT_ERROR_SLOT_NOT_FOUND;
    }
    tokenValid = slot_token_valid_for_entry_locked(manager, handle, token, entry);
    if (!tokenValid) {
        pthread_mutex_unlock(manager_mutex(manager));
        slot_manager_record_security_violation(manager,
                                               "RELEASE_TOKEN_VALIDATION_FAILED",
                                               handle->slotId,
                                               "Cannot release secure slot without a valid token");
        return SLOT_ERROR_PERMISSION_DENIED;
    }

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
    bool tokenValid;

    if (manager == NULL || handle == NULL || token == NULL)
        return SLOT_ERROR_INVALID_HANDLE;

    pthread_mutex_lock(manager_mutex(manager));
    entry = find_slot_entry_locked(manager, handle);
    if (entry == NULL) {
        pthread_mutex_unlock(manager_mutex(manager));
        return SLOT_ERROR_SLOT_NOT_FOUND;
    }
    tokenValid = slot_token_valid_for_entry_locked(manager, handle, token, entry);
    if (!tokenValid) {
        pthread_mutex_unlock(manager_mutex(manager));
        return SLOT_ERROR_PERMISSION_DENIED;
    }

    secResult = TokenRefresh(manager->securityContext, token);
    if (secResult != SECURITY_SUCCESS) {
        pthread_mutex_unlock(manager_mutex(manager));
        return SLOT_ERROR_PERMISSION_DENIED;
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
