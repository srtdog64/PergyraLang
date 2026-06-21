/*
 * Copyright (c) 2025 Pergyra Language Project
 * All rights reserved.
 *
 * Secure slot scope and high-level Pergyra slot API wrappers.
 *
 * Scope bookkeeping invariant (single source of truth): each index i in a scope
 * has exactly one manager-level release. `released[i]` records whether that
 * release has happened (individually via pergyra_slot_release_secure, or about
 * to happen in scope auto-cleanup). Auto-cleanup releases only `!released[i]`
 * indices, so an early individual release followed by scope end never
 * double-releases. The heap PergyraSecureSlot struct is owned by whoever
 * releases it: individual release frees its own struct; scope destroy frees the
 * structs that were never individually released.
 *
 * Threading: `lock` guards count/capacity and the parallel arrays. Manager calls
 * (SlotClaimSecure/SlotReleaseSecure) take the manager mutex internally; the lock
 * order is always scope->lock then manager mutex, never the inverse, so there is
 * no deadlock. Precondition (as for any container): a scope must not be destroyed
 * concurrently with an individual release of one of its own slots. Pergyra's
 * structured parallelism guarantees this — a `parallel{}`/spawn region joins
 * before the enclosing scope ends — so concurrent claim/release inside the region
 * is serialized by `lock`, and destroy runs only after the join.
 */

#include "slot_manager.h"
#include "slot_manager_internal.h"
#include "pgy_runtime_panic_contract.h"

#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct SecureSlotScope;

struct SecureSlotScope
{
    SlotManager *manager;
    SlotHandle *handles;
    TokenCapability *tokens;
    PergyraSecureSlot **issuedSlots;
    bool *released;            /* per-index: manager slot already released */
    size_t count;
    size_t capacity;
    bool autoCleanup;
    pthread_mutex_t lock;      /* guards count/capacity and the arrays */
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

/* Grow every parallel array to at least `needed` elements. Caller holds `lock`.
 * New tail entries for issuedSlots/released are zero-initialised; handles/tokens
 * tail entries are written by the claim that triggered the growth. */
static bool
secure_scope_grow_locked(SecureSlotScope *scope, size_t needed)
{
    size_t newCap;
    SlotHandle *nh;
    TokenCapability *nt;
    PergyraSecureSlot **ni;
    bool *nr;

    if (needed <= scope->capacity)
        return true;

    newCap = scope->capacity ? scope->capacity : 8;
    while (newCap < needed) {
        if (newCap > SIZE_MAX / 2)
            return false;
        newCap *= 2;
    }
    if (!secure_slot_scope_array_fits(newCap, sizeof(SlotHandle))
        || !secure_slot_scope_array_fits(newCap, sizeof(TokenCapability))
        || !secure_slot_scope_array_fits(newCap, sizeof(PergyraSecureSlot *))
        || !secure_slot_scope_array_fits(newCap, sizeof(bool))) {
        return false;
    }

    nh = realloc(scope->handles, newCap * sizeof(*scope->handles));
    if (nh == NULL)
        return false;
    scope->handles = nh;
    nt = realloc(scope->tokens, newCap * sizeof(*scope->tokens));
    if (nt == NULL)
        return false;
    scope->tokens = nt;
    ni = realloc(scope->issuedSlots, newCap * sizeof(*scope->issuedSlots));
    if (ni == NULL)
        return false;
    scope->issuedSlots = ni;
    nr = realloc(scope->released, newCap * sizeof(*scope->released));
    if (nr == NULL)
        return false;
    scope->released = nr;

    memset(&scope->issuedSlots[scope->capacity], 0,
           (newCap - scope->capacity) * sizeof(*scope->issuedSlots));
    memset(&scope->released[scope->capacity], 0,
           (newCap - scope->capacity) * sizeof(*scope->released));
    scope->capacity = newCap;
    return true;
}

SecureSlotScope *
SecureSlotScopeCreate(SlotManager *manager, size_t capacity)
{
    SecureSlotScope *scope;

    if (manager == NULL || !SlotManagerIsSecurityEnabled(manager))
        return NULL;
    if (!secure_slot_scope_array_fits(capacity, sizeof(SlotHandle))
        || !secure_slot_scope_array_fits(capacity, sizeof(TokenCapability))
        || !secure_slot_scope_array_fits(capacity, sizeof(PergyraSecureSlot *))
        || !secure_slot_scope_array_fits(capacity, sizeof(bool))) {
        return NULL;
    }

    scope = calloc(1, sizeof(*scope));
    if (scope == NULL)
        return NULL;

    if (pthread_mutex_init(&scope->lock, NULL) != 0) {
        free(scope);
        return NULL;
    }

    if (capacity > 0) {
        scope->handles = calloc(capacity, sizeof(*scope->handles));
        scope->tokens = calloc(capacity, sizeof(*scope->tokens));
        scope->issuedSlots = calloc(capacity, sizeof(*scope->issuedSlots));
        scope->released = calloc(capacity, sizeof(*scope->released));
        if (scope->handles == NULL || scope->tokens == NULL
            || scope->issuedSlots == NULL || scope->released == NULL) {
            free(scope->handles);
            free(scope->tokens);
            free(scope->issuedSlots);
            free(scope->released);
            pthread_mutex_destroy(&scope->lock);
            free(scope);
            return NULL;
        }
    }

    scope->manager = manager;
    scope->capacity = capacity;
    scope->autoCleanup = true;
    return scope;
}

/* Claim one manager slot into the scope and return its stable index. Caller
 * holds scope->lock; this keeps claim + bookkeeping atomic so a concurrent
 * claim cannot shift the index out from under a struct registration. */
static SlotError
secure_scope_claim_locked(SecureSlotScope *scope, TypeTag type,
                          SecurityLevel level, size_t *outIdx)
{
    size_t idx;
    SlotError result;

    if (!secure_scope_grow_locked(scope, scope->count + 1))
        return SLOT_ERROR_OUT_OF_MEMORY;
    idx = scope->count;
    result = SlotClaimSecure(scope->manager, type, level,
                             &scope->handles[idx], &scope->tokens[idx]);
    if (result != SLOT_SUCCESS)
        return result;
    scope->issuedSlots[idx] = NULL;
    scope->released[idx] = false;
    scope->count++;
    *outIdx = idx;
    return SLOT_SUCCESS;
}

SlotError
SecureSlotScopeClaimSlot(SecureSlotScope *scope, TypeTag type,
                         SecurityLevel level, SlotHandle **handle,
                         TokenCapability **token)
{
    SlotError result;
    size_t idx = 0;

    if (scope == NULL)
        return SLOT_ERROR_OUT_OF_MEMORY;

    pthread_mutex_lock(&scope->lock);
    result = secure_scope_claim_locked(scope, type, level, &idx);
    if (result == SLOT_SUCCESS) {
        if (handle != NULL)
            *handle = &scope->handles[idx];
        if (token != NULL)
            *token = &scope->tokens[idx];
    }
    pthread_mutex_unlock(&scope->lock);
    return result;
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
    SlotError firstError = SLOT_SUCCESS;

    if (scope == NULL)
        return SLOT_SUCCESS;

    result = secure_scope_validate_destroyable(scope);
    if (result != SLOT_SUCCESS)
        return result;

    pthread_mutex_lock(&scope->lock);
    for (i = 0; i < scope->count; i++) {
        /* Free the struct that was never individually released (fixes the leak),
         * after invalidating it so any surviving caller handle fails closed. */
        if (scope->issuedSlots != NULL && scope->issuedSlots[i] != NULL) {
            scope->issuedSlots[i]->isValid = false;
            scope->issuedSlots[i]->scope = NULL;
            SecureMemoryWipe(scope->issuedSlots[i],
                             sizeof(*scope->issuedSlots[i]));
            free(scope->issuedSlots[i]);
            scope->issuedSlots[i] = NULL;
        }
        /* Release the manager slot only if it was not already released
         * individually (fixes the double-release). */
        if (scope->autoCleanup && (scope->released == NULL
                                   || !scope->released[i])) {
            result = SlotReleaseSecure(scope->manager, &scope->handles[i],
                                       &scope->tokens[i]);
            if (scope->released != NULL)
                scope->released[i] = true;
            if (result != SLOT_SUCCESS && firstError == SLOT_SUCCESS)
                firstError = result;
        }
    }
    pthread_mutex_unlock(&scope->lock);

    if (firstError != SLOT_SUCCESS)
        return firstError;

    if (scope->tokens != NULL) {
        if (secure_slot_scope_array_fits(scope->capacity, sizeof(*scope->tokens)))
            SecureMemoryWipe(scope->tokens, scope->capacity * sizeof(*scope->tokens));
        free(scope->tokens);
    }
    free(scope->issuedSlots);
    free(scope->released);
    free(scope->handles);
    pthread_mutex_destroy(&scope->lock);
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
    SecureSlotScope *scope;
    bool needManagerRelease = true;

    if (slot == NULL)
        return;

    /* Deregister from the owning scope first, under the scope lock, so the
     * release/auto-cleanup share a single source of truth (`released[]`). After
     * this the slot no longer references the scope. */
    scope = slot->scope;
    if (scope != NULL) {
        pthread_mutex_lock(&scope->lock);
        for (size_t i = 0; i < scope->count; i++) {
            if (scope->issuedSlots != NULL && scope->issuedSlots[i] == slot) {
                if (scope->released != NULL) {
                    if (scope->released[i])
                        needManagerRelease = false; /* already released */
                    else
                        scope->released[i] = true;
                }
                scope->issuedSlots[i] = NULL;
                break;
            }
        }
        slot->scope = NULL;
        pthread_mutex_unlock(&scope->lock);
    }

    if (needManagerRelease && slot->isValid && slot->manager != NULL)
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
    SecureSlotScope *scope;
    PergyraSecureSlot *slot;
    TypeTag typeTag;
    SlotError result;
    size_t idx = 0;

    if (pscope == NULL || pscope->scope == NULL || typeName == NULL)
        return NULL;
    scope = pscope->scope;

    slot = calloc(1, sizeof(*slot));
    if (slot == NULL)
        return NULL;

    typeTag = (TypeTag)TypeTagHash(typeName);

    /* Claim and register the struct under a single lock acquisition so the
     * index, the handle/token copy, and the issuedSlots entry are consistent
     * even under concurrent claims. */
    pthread_mutex_lock(&scope->lock);
    result = secure_scope_claim_locked(scope, typeTag, level, &idx);
    if (result != SLOT_SUCCESS) {
        pthread_mutex_unlock(&scope->lock);
        free(slot);
        return NULL;
    }
    slot->handle = scope->handles[idx];
    slot->token = scope->tokens[idx];
    slot->typeTag = typeTag;
    slot->manager = pscope->manager;
    slot->isValid = true;
    slot->scope = scope;
    scope->issuedSlots[idx] = slot;
    pthread_mutex_unlock(&scope->lock);

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
