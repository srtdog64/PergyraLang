/*
 * Copyright (c) 2025 Pergyra Language Project
 * All rights reserved.
 *
 * Secure slot scope and high-level Pergyra slot API wrappers.
 */

#include "slot_manager.h"
#include "slot_manager_internal.h"
#include "pgy_runtime_panic_contract.h"

#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

struct SecureSlotScope;

struct SecureSlotScope
{
    SlotManager *manager;
    SlotHandle *handles;
    TokenCapability *tokens;
    size_t count;
    size_t capacity;
    bool autoCleanup;
};

struct PergyraSlotScope
{
    SecureSlotScope *scope;
    SlotManager *manager;
};

static bool
secure_slot_scope_array_fits(size_t count, size_t elem_size)
{
    return elem_size != 0 && count <= SIZE_MAX / elem_size;
}

SecureSlotScope *
SecureSlotScopeCreate(SlotManager *manager, size_t capacity)
{
    SecureSlotScope *scope;

    if (manager == NULL || !SlotManagerIsSecurityEnabled(manager))
        return NULL;
    if (!secure_slot_scope_array_fits(capacity, sizeof(SlotHandle))
        || !secure_slot_scope_array_fits(capacity, sizeof(TokenCapability))) {
        return NULL;
    }

    scope = calloc(1, sizeof(*scope));
    if (scope == NULL)
        return NULL;

    if (capacity > 0) {
        scope->handles = calloc(capacity, sizeof(*scope->handles));
        scope->tokens = calloc(capacity, sizeof(*scope->tokens));
    }
    if (scope->handles == NULL || scope->tokens == NULL) {
        free(scope->handles);
        free(scope->tokens);
        free(scope);
        return NULL;
    }

    scope->manager = manager;
    scope->capacity = capacity;
    scope->autoCleanup = true;
    return scope;
}

SlotError
SecureSlotScopeClaimSlot(SecureSlotScope *scope, TypeTag type,
                         SecurityLevel level, SlotHandle **handle,
                         TokenCapability **token)
{
    SlotError result;

    if (scope == NULL || scope->count >= scope->capacity)
        return SLOT_ERROR_OUT_OF_MEMORY;

    result = SlotClaimSecure(scope->manager, type, level,
                             &scope->handles[scope->count],
                             &scope->tokens[scope->count]);
    if (result != SLOT_SUCCESS)
        return result;

    if (handle != NULL)
        *handle = &scope->handles[scope->count];
    if (token != NULL)
        *token = &scope->tokens[scope->count];
    scope->count++;
    return SLOT_SUCCESS;
}

void
SecureSlotScopeDestroy(SecureSlotScope *scope)
{
    SlotError result = SecureSlotScopeDestroyChecked(scope);

    if (result != SLOT_SUCCESS) {
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_INTERNAL_INVARIANT,
                          "secure slot scope destroyed while a slot is pinned");
    }
}

static SlotError
secure_scope_validate_destroyable(SecureSlotScope *scope)
{
    SlotError result = SLOT_SUCCESS;

    if (scope == NULL || scope->manager == NULL)
        return SLOT_SUCCESS;

    pthread_mutex_lock(manager_mutex(scope->manager));
    for (size_t i = 0; i < scope->count; i++) {
        SlotEntry *entry = find_slot_entry_locked(scope->manager,
                                                  &scope->handles[i]);
        if (entry != NULL && entry->pinCount > 0) {
            result = SLOT_ERROR_PINNED;
            break;
        }
    }
    pthread_mutex_unlock(manager_mutex(scope->manager));
    return result;
}

SlotError
SecureSlotScopeDestroyChecked(SecureSlotScope *scope)
{
    size_t i;
    SlotError result;

    if (scope == NULL)
        return SLOT_SUCCESS;

    result = secure_scope_validate_destroyable(scope);
    if (result != SLOT_SUCCESS)
        return result;

    if (scope->autoCleanup) {
        for (i = 0; i < scope->count; i++) {
            result = SlotReleaseSecure(scope->manager, &scope->handles[i],
                                       &scope->tokens[i]);
            if (result != SLOT_SUCCESS)
                return result;
        }
    }

    if (scope->tokens != NULL) {
        if (secure_slot_scope_array_fits(scope->capacity, sizeof(*scope->tokens))) {
            SecureMemoryWipe(scope->tokens, scope->capacity * sizeof(*scope->tokens));
        }
        free(scope->tokens);
    }

    free(scope->handles);
    free(scope);
    return SLOT_SUCCESS;
}

PergyraSecureSlot *
pergyra_claim_secure_slot(SlotManager *manager, const char *typeName,
                          SecurityLevel level)
{
    PergyraSecureSlot *slot;
    TypeTag typeTag;

    if (manager == NULL || typeName == NULL)
        return NULL;

    slot = calloc(1, sizeof(*slot));
    if (slot == NULL)
        return NULL;

    typeTag = (TypeTag)TypeTagHash(typeName);
    if (SlotClaimSecure(manager, typeTag, level, &slot->handle, &slot->token) !=
        SLOT_SUCCESS) {
        free(slot);
        return NULL;
    }

    slot->typeTag = typeTag;
    slot->manager = manager;
    slot->isValid = true;
    return slot;
}

bool
pergyra_slot_write_secure(PergyraSecureSlot *slot, const void *data, size_t dataSize)
{
    if (slot == NULL || !slot->isValid || slot->manager == NULL)
        return false;

    return SlotWriteSecure(slot->manager, &slot->handle, data, dataSize,
                           &slot->token) == SLOT_SUCCESS;
}

bool
pergyra_slot_read_secure(PergyraSecureSlot *slot, void *buffer,
                         size_t bufferSize, size_t *bytesRead)
{
    if (slot == NULL || !slot->isValid || slot->manager == NULL)
        return false;

    return SlotReadSecure(slot->manager, &slot->handle, buffer, bufferSize,
                          bytesRead, &slot->token) == SLOT_SUCCESS;
}

void
pergyra_slot_release_secure(PergyraSecureSlot *slot)
{
    if (slot == NULL)
        return;

    if (slot->isValid && slot->manager != NULL)
        SlotReleaseSecure(slot->manager, &slot->handle, &slot->token);

    SecureMemoryWipe(slot, sizeof(*slot));
    free(slot);
}

PergyraSlotScope *
pergyra_scope_begin(SlotManager *manager)
{
    PergyraSlotScope *pscope;

    if (manager == NULL)
        return NULL;

    pscope = calloc(1, sizeof(*pscope));
    if (pscope == NULL)
        return NULL;

    pscope->scope = SecureSlotScopeCreate(manager, 64);
    if (pscope->scope == NULL) {
        free(pscope);
        return NULL;
    }

    pscope->manager = manager;
    return pscope;
}

PergyraSecureSlot *
pergyra_scope_claim_slot(PergyraSlotScope *pscope, const char *typeName,
                         SecurityLevel level)
{
    PergyraSecureSlot *slot;
    SlotHandle *handle;
    TokenCapability *token;
    TypeTag typeTag;

    if (pscope == NULL || pscope->scope == NULL || typeName == NULL)
        return NULL;

    typeTag = (TypeTag)TypeTagHash(typeName);
    if (SecureSlotScopeClaimSlot(pscope->scope, typeTag, level, &handle, &token) !=
        SLOT_SUCCESS) {
        return NULL;
    }

    slot = calloc(1, sizeof(*slot));
    if (slot == NULL)
        return NULL;

    slot->handle = *handle;
    slot->token = *token;
    slot->typeTag = typeTag;
    slot->manager = pscope->manager;
    slot->isValid = true;
    return slot;
}

void
pergyra_scope_end(PergyraSlotScope *pscope)
{
    if (pscope == NULL)
        return;

    SecureSlotScopeDestroy(pscope->scope);
    free(pscope);
}

void
pergyra_security_audit_usage_example(void)
{
    printf("=== Pergyra Secure Slot Usage Example ===\n");
    printf("let slot = claim_secure_slot<Int>(SECURITY_LEVEL_HARDWARE)\n");
    printf("write(slot, 42)\n");
    printf("let value = read(slot)\n");
    printf("release(slot)\n");
    printf("=========================================\n");
}
