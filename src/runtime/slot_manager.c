/*
 * Copyright (c) 2025 Pergyra Language Project
 * All rights reserved.
 *
 * Slot manager with secure shadow-copy support.
 */

#include "slot_manager.h"

#include <errno.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

typedef struct
{
    size_t requestedSize;
} MemoryPool;

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

static pthread_mutex_t *
manager_mutex(SlotManager *manager)
{
    return (pthread_mutex_t *)manager->mutex;
}

static uint64_t
slot_now_us(void)
{
    return SecureTimestamp();
}

static const char *
slot_error_name(SlotError err)
{
    switch (err) {
    case SLOT_SUCCESS: return "success";
    case SLOT_ERROR_OUT_OF_MEMORY: return "out-of-memory";
    case SLOT_ERROR_INVALID_HANDLE: return "invalid-handle";
    case SLOT_ERROR_TYPE_MISMATCH: return "type-mismatch";
    case SLOT_ERROR_SLOT_NOT_FOUND: return "slot-not-found";
    case SLOT_ERROR_PERMISSION_DENIED: return "permission-denied";
    case SLOT_ERROR_TTL_EXPIRED: return "ttl-expired";
    case SLOT_ERROR_THREAD_VIOLATION: return "thread-violation";
    default: return "unknown";
    }
}

static void
slot_manager_warn(const char *op, const SlotHandle *handle, SlotError err)
{
    fprintf(stderr,
            "[pgy][slot] %s failed: %s (slot=%u type=%u gen=%u)\n",
            op != NULL ? op : "<op>",
            slot_error_name(err),
            handle != NULL ? handle->slotId : 0u,
            handle != NULL ? handle->typeTag : 0u,
            handle != NULL ? handle->generation : 0u);
}

static uint32_t
current_thread_id(void)
{
    return (uint32_t)((uintptr_t)pthread_self() & 0xffffffffu);
}

static SlotEntry *
find_slot_entry_locked(SlotManager *manager, const SlotHandle *handle)
{
    size_t i;

    if (manager == NULL || handle == NULL)
        return NULL;

    for (i = 0; i < manager->tableSize; i++) {
        SlotEntry *entry = &manager->slotTable[i];
        if (!entry->occupied)
            continue;
        if (entry->slotId != handle->slotId)
            continue;
        return entry;
    }

    return NULL;
}

static SlotEntry *
find_free_entry_locked(SlotManager *manager)
{
    size_t i;

    for (i = 0; i < manager->tableSize; i++) {
        if (!manager->slotTable[i].occupied)
            return &manager->slotTable[i];
    }

    return NULL;
}

static void
slot_wipe_buffer(void *ptr, size_t size)
{
    if (ptr == NULL || size == 0)
        return;

    SecureMemoryWipe(ptr, size);
}

static void
slot_free_plain_buffer(SlotEntry *entry)
{
    if (entry->dataBlockRef != NULL) {
        slot_wipe_buffer(entry->dataBlockRef, entry->dataSize);
        free(entry->dataBlockRef);
        entry->dataBlockRef = NULL;
    }
}

static void
slot_free_buffers(SlotEntry *entry)
{
    slot_free_plain_buffer(entry);
    entry->dataSize = 0;
    SecureSealedPayloadDestroy(&entry->securePayload);
}

static bool
slot_reserve_storage(SlotEntry *entry, size_t size)
{
    void *primary;

    if (size == 0) {
        slot_free_buffers(entry);
        return true;
    }

    /* 과도한 크기 요청 차단 (256MB 제한 - slot은 메모리 슬롯이므로 합리적 제한) */
    if (size > (256UL * 1024UL * 1024UL)) {
        return false;
    }

    if (entry->dataBlockRef != NULL && entry->dataSize == size) {
        return true;
    }

    primary = malloc(size);
    if (primary == NULL) {
        return false;
    }

    slot_free_plain_buffer(entry);
    entry->dataBlockRef = primary;
    entry->dataSize = size;
    return true;
}

static bool
slot_store_plain_payload(SlotEntry *entry, const void *data, size_t size)
{
    if (!slot_reserve_storage(entry, size))
        return false;

    if (size == 0) {
        return true;
    }

    memcpy(entry->dataBlockRef, data, size);
    return true;
}

static bool
slot_is_expired_locked(const SlotEntry *entry)
{
    uint64_t nowUs;
    uint64_t expiryUs;

    if (entry == NULL || !entry->occupied || entry->ttl == 0)
        return false;

    nowUs = slot_now_us();
    expiryUs = entry->allocationTime + ((uint64_t)entry->ttl * 1000ULL);
    return nowUs > expiryUs;
}

static void
slot_reset_entry_locked(SlotEntry *entry)
{
    slot_free_buffers(entry);
    SecureMemoryWipe(&entry->writeToken, sizeof(entry->writeToken));
    memset(entry, 0, sizeof(*entry));
}

static SlotError
slot_release_entry_locked(SlotManager *manager, SlotEntry *entry)
{
    if (entry == NULL || !entry->occupied)
        return SLOT_ERROR_SLOT_NOT_FOUND;

    slot_reset_entry_locked(entry);
    manager->totalDeallocations++;
    if (manager->activeSlots > 0)
        manager->activeSlots--;
    return SLOT_SUCCESS;
}

static SlotError
slot_claim_common(SlotManager *manager, TypeTag type, uint32_t scopeId,
                  SlotHandle *handle)
{
    SlotEntry *entry;
    SlotError err;

    if (manager == NULL || handle == NULL) {
        err = SLOT_ERROR_INVALID_HANDLE;
        slot_manager_warn("claim", handle, err);
        return err;
    }

    pthread_mutex_lock(manager_mutex(manager));

    entry = find_free_entry_locked(manager);
    if (entry == NULL) {
        pthread_mutex_unlock(manager_mutex(manager));
        err = SLOT_ERROR_OUT_OF_MEMORY;
        slot_manager_warn("claim", handle, err);
        return err;
    }

    memset(entry, 0, sizeof(*entry));
    entry->occupied = true;
    entry->slotId = manager->nextSlotId++;
    entry->typeTag = (uint32_t)type;
    entry->scopeId = scopeId;
    entry->threadAffinity = 0;
    entry->allocationTime = slot_now_us();
    entry->lastAccessTime = entry->allocationTime;
    entry->securityLevel = manager->defaultSecurityLevel;
    SecureSealedPayloadInit(&entry->securePayload);

    handle->slotId = entry->slotId;
    handle->typeTag = entry->typeTag;
    handle->generation = 1;

    manager->totalAllocations++;
    manager->activeSlots++;

    pthread_mutex_unlock(manager_mutex(manager));
    return SLOT_SUCCESS;
}

static SlotError
slot_write_common(SlotManager *manager, const SlotHandle *handle,
                  const void *data, size_t dataSize)
{
    SlotEntry *entry;
    SlotError err;

    if (manager == NULL || handle == NULL || data == NULL) {
        err = SLOT_ERROR_INVALID_HANDLE;
        slot_manager_warn("write", handle, err);
        return err;
    }

    pthread_mutex_lock(manager_mutex(manager));

    entry = find_slot_entry_locked(manager, handle);
    if (entry == NULL) {
        manager->cacheMisses++;
        pthread_mutex_unlock(manager_mutex(manager));
        err = SLOT_ERROR_SLOT_NOT_FOUND;
        slot_manager_warn("write", handle, err);
        return err;
    }

    if (entry->typeTag != handle->typeTag) {
        pthread_mutex_unlock(manager_mutex(manager));
        err = SLOT_ERROR_TYPE_MISMATCH;
        slot_manager_warn("write", handle, err);
        return err;
    }

    if (entry->securityEnabled) {
        pthread_mutex_unlock(manager_mutex(manager));
        err = SLOT_ERROR_PERMISSION_DENIED;
        slot_manager_warn("write", handle, err);
        return err;
    }

    if (slot_is_expired_locked(entry)) {
        pthread_mutex_unlock(manager_mutex(manager));
        err = SLOT_ERROR_TTL_EXPIRED;
        slot_manager_warn("write", handle, err);
        return err;
    }

    if (!slot_store_plain_payload(entry, data, dataSize)) {
        pthread_mutex_unlock(manager_mutex(manager));
        err = SLOT_ERROR_OUT_OF_MEMORY;
        slot_manager_warn("write", handle, err);
        return err;
    }

    entry->lastAccessTime = slot_now_us();
    entry->accessCount++;
    manager->cacheHits++;

    pthread_mutex_unlock(manager_mutex(manager));
    return SLOT_SUCCESS;
}

static SlotError
slot_read_common(SlotManager *manager, const SlotHandle *handle, void *buffer,
                 size_t bufferSize, size_t *bytesRead)
{
    SlotEntry *entry;
    size_t copySize;
    SlotError err;
    if (manager == NULL || handle == NULL || buffer == NULL) {
        err = SLOT_ERROR_INVALID_HANDLE;
        slot_manager_warn("read", handle, err);
        return err;
    }

    pthread_mutex_lock(manager_mutex(manager));

    entry = find_slot_entry_locked(manager, handle);
    if (entry == NULL) {
        manager->cacheMisses++;
        pthread_mutex_unlock(manager_mutex(manager));
        err = SLOT_ERROR_SLOT_NOT_FOUND;
        slot_manager_warn("read", handle, err);
        return err;
    }

    if (entry->typeTag != handle->typeTag) {
        pthread_mutex_unlock(manager_mutex(manager));
        err = SLOT_ERROR_TYPE_MISMATCH;
        slot_manager_warn("read", handle, err);
        return err;
    }

    if (entry->securityEnabled) {
        pthread_mutex_unlock(manager_mutex(manager));
        err = SLOT_ERROR_PERMISSION_DENIED;
        slot_manager_warn("read", handle, err);
        return err;
    }

    if (slot_is_expired_locked(entry)) {
        pthread_mutex_unlock(manager_mutex(manager));
        err = SLOT_ERROR_TTL_EXPIRED;
        slot_manager_warn("read", handle, err);
        return err;
    }

    if (entry->dataBlockRef == NULL || entry->dataSize == 0) {
        if (bytesRead != NULL)
            *bytesRead = 0;
        pthread_mutex_unlock(manager_mutex(manager));
        return SLOT_SUCCESS;
    }

    copySize = entry->dataSize < bufferSize ? entry->dataSize : bufferSize;
    memcpy(buffer, entry->dataBlockRef, copySize);
    if (bytesRead != NULL)
        *bytesRead = copySize;

    entry->lastAccessTime = slot_now_us();
    entry->accessCount++;
    manager->cacheHits++;

    pthread_mutex_unlock(manager_mutex(manager));
    return SLOT_SUCCESS;
}

SlotManager *
SlotManagerCreate(size_t maxSlots, size_t memoryPoolSize)
{
    SlotManager *manager;
    MemoryPool *pool;
    pthread_mutex_t *mutex;

    manager = calloc(1, sizeof(*manager));
    if (manager == NULL)
        return NULL;

    manager->slotTable = calloc(maxSlots, sizeof(SlotEntry));
    if (manager->slotTable == NULL) {
        free(manager);
        return NULL;
    }

    pool = calloc(1, sizeof(*pool));
    mutex = malloc(sizeof(*mutex));
    if (pool == NULL || mutex == NULL) {
        free(pool);
        free(mutex);
        free(manager->slotTable);
        free(manager);
        return NULL;
    }

    pool->requestedSize = memoryPoolSize;
    pthread_mutex_init(mutex, NULL);

    manager->tableSize = maxSlots;
    manager->maxSlots = maxSlots;
    manager->nextSlotId = 1;
    manager->memoryPool = pool;
    manager->mutex = mutex;
    manager->defaultSecurityLevel = SECURITY_LEVEL_BASIC;
    return manager;
}

void
SlotManagerDestroy(SlotManager *manager)
{
    size_t i;

    if (manager == NULL)
        return;

    if (manager->securityContext != NULL)
        SlotManagerDisableSecurity(manager);

    if (manager->slotTable != NULL) {
        for (i = 0; i < manager->tableSize; i++) {
            if (manager->slotTable[i].occupied)
                slot_reset_entry_locked(&manager->slotTable[i]);
        }
        free(manager->slotTable);
    }

    if (manager->mutex != NULL) {
        pthread_mutex_destroy(manager_mutex(manager));
        free(manager->mutex);
    }

    free(manager->memoryPool);
    free(manager);
}

SlotError
SlotClaim(SlotManager *manager, TypeTag type, SlotHandle *handle)
{
    return slot_claim_common(manager, type, 0, handle);
}

SlotError
SlotWrite(SlotManager *manager, const SlotHandle *handle, const void *data,
          size_t dataSize)
{
    return slot_write_common(manager, handle, data, dataSize);
}

SlotError
SlotRead(SlotManager *manager, const SlotHandle *handle, void *buffer,
         size_t bufferSize, size_t *bytesRead)
{
    return slot_read_common(manager, handle, buffer, bufferSize, bytesRead);
}

SlotError
SlotRelease(SlotManager *manager, const SlotHandle *handle)
{
    SlotEntry *entry;
    SlotError result;

    if (manager == NULL || handle == NULL)
        return SLOT_ERROR_INVALID_HANDLE;

    pthread_mutex_lock(manager_mutex(manager));
    entry = find_slot_entry_locked(manager, handle);
    result = slot_release_entry_locked(manager, entry);
    pthread_mutex_unlock(manager_mutex(manager));
    return result;
}

SlotError
SlotClaimScoped(SlotManager *manager, TypeTag type, uint32_t scopeId,
                SlotHandle *handle)
{
    return slot_claim_common(manager, type, scopeId, handle);
}

SlotError
SlotReleaseScope(SlotManager *manager, uint32_t scopeId)
{
    size_t i;

    if (manager == NULL)
        return SLOT_ERROR_INVALID_HANDLE;

    pthread_mutex_lock(manager_mutex(manager));
    for (i = 0; i < manager->tableSize; i++) {
        SlotEntry *entry = &manager->slotTable[i];
        if (entry->occupied && entry->scopeId == scopeId)
            slot_release_entry_locked(manager, entry);
    }
    pthread_mutex_unlock(manager_mutex(manager));
    return SLOT_SUCCESS;
}

bool
SlotValidateType(SlotManager *manager, const SlotHandle *handle,
                 TypeTag expectedType)
{
    bool valid = false;
    SlotEntry *entry;

    if (manager == NULL || handle == NULL)
        return false;

    pthread_mutex_lock(manager_mutex(manager));
    entry = find_slot_entry_locked(manager, handle);
    if (entry != NULL && entry->typeTag == (uint32_t)expectedType)
        valid = true;
    pthread_mutex_unlock(manager_mutex(manager));
    return valid;
}

bool
SlotIsValid(SlotManager *manager, const SlotHandle *handle)
{
    bool valid = false;
    SlotEntry *entry;

    if (manager == NULL || handle == NULL)
        return false;

    pthread_mutex_lock(manager_mutex(manager));
    entry = find_slot_entry_locked(manager, handle);
    if (entry != NULL && !slot_is_expired_locked(entry))
        valid = true;
    pthread_mutex_unlock(manager_mutex(manager));
    return valid;
}

SlotError
SlotSetTtl(SlotManager *manager, const SlotHandle *handle, uint32_t ttlMs)
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
    entry->ttl = ttlMs;
    entry->allocationTime = slot_now_us();
    pthread_mutex_unlock(manager_mutex(manager));
    return SLOT_SUCCESS;
}

SlotError
SlotRefreshTtl(SlotManager *manager, const SlotHandle *handle)
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
    entry->allocationTime = slot_now_us();
    pthread_mutex_unlock(manager_mutex(manager));
    return SLOT_SUCCESS;
}

void
SlotCleanupExpired(SlotManager *manager)
{
    size_t i;

    if (manager == NULL)
        return;

    pthread_mutex_lock(manager_mutex(manager));
    for (i = 0; i < manager->tableSize; i++) {
        SlotEntry *entry = &manager->slotTable[i];
        if (entry->occupied && slot_is_expired_locked(entry))
            slot_release_entry_locked(manager, entry);
    }
    pthread_mutex_unlock(manager_mutex(manager));
}

SlotError
SlotLock(SlotManager *manager, const SlotHandle *handle)
{
    SlotEntry *entry;
    uint32_t tid = current_thread_id();

    if (manager == NULL || handle == NULL)
        return SLOT_ERROR_INVALID_HANDLE;

    pthread_mutex_lock(manager_mutex(manager));
    entry = find_slot_entry_locked(manager, handle);
    if (entry == NULL) {
        pthread_mutex_unlock(manager_mutex(manager));
        return SLOT_ERROR_SLOT_NOT_FOUND;
    }

    if (entry->threadAffinity != 0 && entry->threadAffinity != tid) {
        pthread_mutex_unlock(manager_mutex(manager));
        return SLOT_ERROR_THREAD_VIOLATION;
    }

    entry->threadAffinity = tid;
    pthread_mutex_unlock(manager_mutex(manager));
    return SLOT_SUCCESS;
}

SlotError
SlotUnlock(SlotManager *manager, const SlotHandle *handle)
{
    SlotEntry *entry;
    uint32_t tid = current_thread_id();

    if (manager == NULL || handle == NULL)
        return SLOT_ERROR_INVALID_HANDLE;

    pthread_mutex_lock(manager_mutex(manager));
    entry = find_slot_entry_locked(manager, handle);
    if (entry == NULL) {
        pthread_mutex_unlock(manager_mutex(manager));
        return SLOT_ERROR_SLOT_NOT_FOUND;
    }

    if (entry->threadAffinity != 0 && entry->threadAffinity != tid) {
        pthread_mutex_unlock(manager_mutex(manager));
        return SLOT_ERROR_THREAD_VIOLATION;
    }

    entry->threadAffinity = 0;
    pthread_mutex_unlock(manager_mutex(manager));
    return SLOT_SUCCESS;
}

SlotError
SlotTryLock(SlotManager *manager, const SlotHandle *handle)
{
    SlotEntry *entry;
    uint32_t tid = current_thread_id();

    if (manager == NULL || handle == NULL)
        return SLOT_ERROR_INVALID_HANDLE;

    if (pthread_mutex_trylock(manager_mutex(manager)) == EBUSY)
        return SLOT_ERROR_THREAD_VIOLATION;

    entry = find_slot_entry_locked(manager, handle);
    if (entry == NULL) {
        pthread_mutex_unlock(manager_mutex(manager));
        return SLOT_ERROR_SLOT_NOT_FOUND;
    }

    if (entry->threadAffinity != 0 && entry->threadAffinity != tid) {
        pthread_mutex_unlock(manager_mutex(manager));
        return SLOT_ERROR_THREAD_VIOLATION;
    }

    entry->threadAffinity = tid;
    pthread_mutex_unlock(manager_mutex(manager));
    return SLOT_SUCCESS;
}

void
SlotManagerPrintStats(const SlotManager *manager)
{
    if (manager == NULL)
        return;

    printf("=== Pergyra Slot Manager Statistics ===\n");
    printf("Total allocations: %llu\n",
           (unsigned long long)manager->totalAllocations);
    printf("Total deallocations: %llu\n",
           (unsigned long long)manager->totalDeallocations);
    printf("Active slots: %llu\n",
           (unsigned long long)manager->activeSlots);
    printf("Cache hits: %llu\n",
           (unsigned long long)manager->cacheHits);
    printf("Cache misses: %llu\n",
           (unsigned long long)manager->cacheMisses);
    printf("Table size: %zu\n", manager->tableSize);
    printf("Utilization: %.2f%%\n", SlotManagerGetUtilization(manager) * 100.0);
}

size_t
SlotManagerGetActiveCount(const SlotManager *manager)
{
    return manager != NULL ? (size_t)manager->activeSlots : 0;
}

double
SlotManagerGetUtilization(const SlotManager *manager)
{
    if (manager == NULL || manager->tableSize == 0)
        return 0.0;

    return (double)manager->activeSlots / (double)manager->tableSize;
}

uint32_t
TypeTagHash(const char *typeName)
{
    uint32_t hash = 5381u;
    const unsigned char *p;

    if (typeName == NULL)
        return 0;

    if (strcmp(typeName, "Int") == 0)
        return TYPE_INT;
    if (strcmp(typeName, "Long") == 0)
        return TYPE_LONG;
    if (strcmp(typeName, "Float") == 0)
        return TYPE_FLOAT;
    if (strcmp(typeName, "Double") == 0)
        return TYPE_DOUBLE;
    if (strcmp(typeName, "String") == 0)
        return TYPE_STRING;
    if (strcmp(typeName, "Bool") == 0)
        return TYPE_BOOL;
    if (strcmp(typeName, "Vector") == 0)
        return TYPE_VECTOR;

    for (p = (const unsigned char *)typeName; *p != '\0'; p++)
        hash = ((hash << 5) + hash) ^ *p;

    return hash | TYPE_CUSTOM;
}

const char *
TypeTagToString(TypeTag tag)
{
    switch (tag) {
    case TYPE_INT:
        return "Int";
    case TYPE_LONG:
        return "Long";
    case TYPE_FLOAT:
        return "Float";
    case TYPE_DOUBLE:
        return "Double";
    case TYPE_STRING:
        return "String";
    case TYPE_BOOL:
        return "Bool";
    case TYPE_VECTOR:
        return "Vector";
    default:
        return "Custom";
    }
}

bool
TypeIsPrimitive(TypeTag tag)
{
    return tag >= TYPE_INT && tag <= TYPE_BOOL;
}

size_t
TypeGetSize(TypeTag tag)
{
    switch (tag) {
    case TYPE_INT:
        return sizeof(int32_t);
    case TYPE_LONG:
        return sizeof(int64_t);
    case TYPE_FLOAT:
        return sizeof(float);
    case TYPE_DOUBLE:
        return sizeof(double);
    case TYPE_BOOL:
        return sizeof(bool);
    case TYPE_STRING:
        return 256;
    case TYPE_VECTOR:
        return 1024;
    default:
        return 64;
    }
}

uint32_t
SlotHashFunction(uint32_t slotId)
{
    uint32_t hash = 0x811c9dc5u;
    int i;

    for (i = 0; i < 4; i++) {
        hash ^= (slotId >> (i * 8)) & 0xffu;
        hash *= 0x01000193u;
    }

    return hash;
}

bool
SlotCompareAndSwap(volatile uint32_t *ptr, uint32_t expected, uint32_t newVal)
{
    return __sync_bool_compare_and_swap(ptr, expected, newVal);
}

void
SlotMemoryBarrier(void)
{
    __sync_synchronize();
}

SlotError
SlotManagerEnableSecurity(SlotManager *manager, SecurityLevel level)
{
    if (manager == NULL)
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
    if (manager == NULL || handle == NULL || token == NULL)
        return false;

    if (!SlotManagerIsSecurityEnabled(manager))
        return false;

    return TokenValidate(manager->securityContext, handle->slotId, token) ==
           SECURITY_SUCCESS;
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
        manager->securityViolations++;
        SlotManagerLogSecurityEvent(manager, "TOKEN_VALIDATION_FAILED",
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
        manager->securityViolations++;
        SlotManagerLogSecurityEvent(manager, "TOKEN_VALIDATION_FAILED",
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
    if (manager == NULL || handle == NULL || token == NULL)
        return SLOT_ERROR_INVALID_HANDLE;

    if (!SlotValidateToken(manager, handle, token)) {
        manager->securityViolations++;
        SlotManagerLogSecurityEvent(manager, "RELEASE_TOKEN_VALIDATION_FAILED",
                                    handle->slotId,
                                    "Cannot release secure slot without a valid token");
        return SLOT_ERROR_PERMISSION_DENIED;
    }

    return SlotRelease(manager, handle);
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

void
SlotManagerLogSecurityEvent(SlotManager *manager, const char *event,
                            uint32_t slotId, const char *details)
{
    time_t now;
    struct tm *tmNow;
    char timestamp[32];

    if (manager == NULL || event == NULL || !SlotManagerIsSecurityEnabled(manager))
        return;

    now = time(NULL);
    tmNow = localtime(&now);
    if (tmNow != NULL)
        strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", tmNow);
    else
        snprintf(timestamp, sizeof(timestamp), "time-unavailable");

    printf("[SECURITY] %s slot=%u event=%s details=%s\n",
           timestamp, slotId, event, details != NULL ? details : "n/a");

    if (manager->securityContext != NULL)
        SecurityAuditLog(manager->securityContext, event, details);
}

bool
SlotManagerDetectAnomalies(SlotManager *manager)
{
    size_t i;
    bool anomalyDetected = false;
    uint64_t nowUs;

    if (manager == NULL || !SlotManagerIsSecurityEnabled(manager))
        return false;

    if (manager->securityViolations > 0)
        anomalyDetected = true;

    nowUs = slot_now_us();
    for (i = 0; i < manager->tableSize; i++) {
        SlotEntry *entry = &manager->slotTable[i];
        if (!entry->occupied || !entry->securityEnabled)
            continue;
        if (entry->accessCount > 1000 &&
            nowUs > entry->lastAccessTime &&
            (nowUs - entry->lastAccessTime) < 1000000ULL) {
            anomalyDetected = true;
        }
    }

    if (manager->securityContext != NULL)
        anomalyDetected |= SecurityDetectAnomalies(manager->securityContext);

    return anomalyDetected;
}

void
SlotManagerPrintSecurityStats(const SlotManager *manager)
{
    size_t active = 0;
    size_t secure = 0;
    size_t i;

    if (manager == NULL) {
        printf("SlotManager is NULL\n");
        return;
    }

    for (i = 0; i < manager->tableSize; i++) {
        if (!manager->slotTable[i].occupied)
            continue;
        active++;
        if (manager->slotTable[i].securityEnabled)
            secure++;
    }

    printf("=== Slot Manager Security Statistics ===\n");
    printf("Security enabled: %s\n",
           SlotManagerIsSecurityEnabled(manager) ? "Yes" : "No");
    printf("Default level: %d\n", (int)manager->defaultSecurityLevel);
    printf("Security violations: %llu\n",
           (unsigned long long)manager->securityViolations);
    printf("Active slots: %zu\n", active);
    printf("Secure slots: %zu\n", secure);
    if (manager->securityContext != NULL)
        SecurityPrintStatistics(manager->securityContext);
    printf("========================================\n");
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

SecureSlotScope *
SecureSlotScopeCreate(SlotManager *manager, size_t capacity)
{
    SecureSlotScope *scope;

    if (manager == NULL || !SlotManagerIsSecurityEnabled(manager))
        return NULL;

    scope = calloc(1, sizeof(*scope));
    if (scope == NULL)
        return NULL;

    scope->handles = calloc(capacity, sizeof(*scope->handles));
    scope->tokens = calloc(capacity, sizeof(*scope->tokens));
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
    size_t i;

    if (scope == NULL)
        return;

    if (scope->autoCleanup) {
        for (i = 0; i < scope->count; i++)
            SlotReleaseSecure(scope->manager, &scope->handles[i], &scope->tokens[i]);
    }

    if (scope->tokens != NULL) {
        SecureMemoryWipe(scope->tokens, scope->capacity * sizeof(*scope->tokens));
        free(scope->tokens);
    }

    free(scope->handles);
    free(scope);
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
    slot->isValid = true;
    return slot;
}

bool
pergyra_slot_write_secure(PergyraSecureSlot *slot, const void *data, size_t dataSize)
{
    extern SlotManager *g_pergyraSlotManager;

    if (slot == NULL || !slot->isValid || g_pergyraSlotManager == NULL)
        return false;

    return SlotWriteSecure(g_pergyraSlotManager, &slot->handle, data, dataSize,
                           &slot->token) == SLOT_SUCCESS;
}

bool
pergyra_slot_read_secure(PergyraSecureSlot *slot, void *buffer,
                         size_t bufferSize, size_t *bytesRead)
{
    extern SlotManager *g_pergyraSlotManager;

    if (slot == NULL || !slot->isValid || g_pergyraSlotManager == NULL)
        return false;

    return SlotReadSecure(g_pergyraSlotManager, &slot->handle, buffer, bufferSize,
                          bytesRead, &slot->token) == SLOT_SUCCESS;
}

void
pergyra_slot_release_secure(PergyraSecureSlot *slot)
{
    extern SlotManager *g_pergyraSlotManager;

    if (slot == NULL)
        return;

    if (slot->isValid && g_pergyraSlotManager != NULL)
        SlotReleaseSecure(g_pergyraSlotManager, &slot->handle, &slot->token);

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

SlotError
SlotClaimFast(SlotManager *manager, TypeTag type, SlotHandle *handle)
{
    return slot_claim_common(manager, type, 0, handle);
}

SlotError
SlotWriteFast(SlotManager *manager, const SlotHandle *handle, const void *data,
              size_t dataSize)
{
    return slot_write_common(manager, handle, data, dataSize);
}

SlotError
SlotReadFast(SlotManager *manager, const SlotHandle *handle, void *buffer,
             size_t bufferSize)
{
    return slot_read_common(manager, handle, buffer, bufferSize, NULL);
}
