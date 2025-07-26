/*
 * Copyright (c) 2025 Pergyra Language Project
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the Pergyra Language Project nor the names of
 *    its contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include "slot_manager.h"
#include "slot_security.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>
#include <pthread.h>

/*
 * External assembly function declarations
 */
extern uint32_t SlotHashFunctionAsm(uint32_t slotId);
extern bool     SlotCompareAndSwapAsm(volatile uint32_t *ptr, 
                                      uint32_t expected, uint32_t newVal);
extern void     SlotMemoryBarrierAsm(void);

/*
 * Simple memory pool implementation
 */
typedef struct
{
    void           *poolStart;
    size_t          poolSize;
    size_t          blockSize;
    size_t          totalBlocks;
    bool           *freeBlocks;
    pthread_mutex_t mutex;
} MemoryPool;

/*
 * Create a new slot manager instance
 */
SlotManager *
SlotManagerCreate(size_t maxSlots, size_t memoryPoolSize)
{
    SlotManager *manager;
    MemoryPool  *pool;
    
    manager = malloc(sizeof(SlotManager));
    if (manager == NULL)
        return NULL;
    
    /* Allocate slot table */
    manager->slotTable = calloc(maxSlots, sizeof(SlotEntry));
    if (manager->slotTable == NULL) {
        free(manager);
        return NULL;
    }
    
    manager->tableSize = maxSlots;
    manager->maxSlots = maxSlots;
    manager->nextSlotId = 1; /* 0 is reserved as invalid ID */
    
    /* Create memory pool */
    pool = malloc(sizeof(MemoryPool));
    if (pool == NULL) {
        free(manager->slotTable);
        free(manager);
        return NULL;
    }
    
    pool->poolSize = memoryPoolSize;
    pool->blockSize = 64; /* 64-byte blocks */
    pool->totalBlocks = memoryPoolSize / pool->blockSize;
    pool->poolStart = aligned_alloc(64, memoryPoolSize); /* 64-byte aligned */
    pool->freeBlocks = calloc(pool->totalBlocks, sizeof(bool));
    
    if (pool->poolStart == NULL || pool->freeBlocks == NULL) {
        if (pool->poolStart != NULL)
            free(pool->poolStart);
        if (pool->freeBlocks != NULL)
            free(pool->freeBlocks);
        free(pool);
        free(manager->slotTable);
        free(manager);
        return NULL;
    }
    
    pthread_mutex_init(&pool->mutex, NULL);
    manager->memoryPool = pool;
    
    /* Initialize mutex */
    manager->mutex = malloc(sizeof(pthread_mutex_t));
    pthread_mutex_init((pthread_mutex_t *)manager->mutex, NULL);
    
    /* Initialize statistics */
    manager->totalAllocations = 0;
    manager->totalDeallocations = 0;
    manager->activeSlots = 0;
    manager->cacheHits = 0;
    manager->cacheMisses = 0;
    
    return manager;
}

/*
 * Destroy slot manager and free all resources
 */
void
SlotManagerDestroy(SlotManager *manager)
{
    MemoryPool *pool;
    
    if (manager == NULL)
        return;
    
    /* Free memory pool */
    if (manager->memoryPool != NULL) {
        pool = (MemoryPool *)manager->memoryPool;
        pthread_mutex_destroy(&pool->mutex);
        if (pool->poolStart != NULL)
            free(pool->poolStart);
        if (pool->freeBlocks != NULL)
            free(pool->freeBlocks);
        free(pool);
    }
    
    /* Free mutex */
    if (manager->mutex != NULL) {
        pthread_mutex_destroy((pthread_mutex_t *)manager->mutex);
        free(manager->mutex);
    }
    
    /* Free slot table */
    if (manager->slotTable != NULL)
        free(manager->slotTable);
    
    free(manager);
}

/*
 * Allocate memory block from pool (internal function)
 */
static void *
AllocateMemoryBlock(SlotManager *manager, size_t size)
{
    MemoryPool *pool;
    size_t      blocksNeeded;
    size_t      i, j;
    bool        found;
    void       *result;
    
    if (manager == NULL || manager->memoryPool == NULL)
        return NULL;
    
    pool = (MemoryPool *)manager->memoryPool;
    
    pthread_mutex_lock(&pool->mutex);
    
    /* Calculate required blocks */
    blocksNeeded = (size + pool->blockSize - 1) / pool->blockSize;
    
    /* Find contiguous free blocks */
    for (i = 0; i <= pool->totalBlocks - blocksNeeded; i++) {
        found = true;
        for (j = 0; j < blocksNeeded; j++) {
            if (pool->freeBlocks[i + j]) {
                found = false;
                break;
            }
        }
        
        if (found) {
            /* Mark blocks as used */
            for (j = 0; j < blocksNeeded; j++)
                pool->freeBlocks[i + j] = true;
            
            result = (char *)pool->poolStart + (i * pool->blockSize);
            pthread_mutex_unlock(&pool->mutex);
            return result;
        }
    }
    
    pthread_mutex_unlock(&pool->mutex);
    return NULL; /* Allocation failed */
}

/*
 * Deallocate memory block (internal function)
 */
static void
DeallocateMemoryBlock(SlotManager *manager, void *ptr, size_t size)
{
    MemoryPool *pool;
    size_t      blockIndex;
    size_t      blocksToFree;
    size_t      i;
    
    if (manager == NULL || manager->memoryPool == NULL || ptr == NULL)
        return;
    
    pool = (MemoryPool *)manager->memoryPool;
    
    pthread_mutex_lock(&pool->mutex);
    
    /* Calculate block index */
    blockIndex = ((char *)ptr - (char *)pool->poolStart) / pool->blockSize;
    blocksToFree = (size + pool->blockSize - 1) / pool->blockSize;
    
    /* Mark blocks as free */
    for (i = 0; i < blocksToFree; i++) {
        if (blockIndex + i < pool->totalBlocks)
            pool->freeBlocks[blockIndex + i] = false;
    }
    
    pthread_mutex_unlock(&pool->mutex);
}

/*
 * Claim a new slot (C implementation for general case)
 */
SlotError
SlotClaim(SlotManager *manager, TypeTag type, SlotHandle *handle)
{
    SlotEntry *entry;
    size_t     i;
    
    if (manager == NULL || handle == NULL)
        return SLOT_ERROR_INVALID_HANDLE;
    
    /* Use assembly optimized version for simple types */
    if (type <= TYPE_CUSTOM)
        return SlotClaimFast(manager, type, handle);
    
    pthread_mutex_lock((pthread_mutex_t *)manager->mutex);
    
    /* Find empty slot */
    for (i = 0; i < manager->tableSize; i++) {
        entry = &manager->slotTable[i];
        
        if (!entry->occupied) {
            /* Initialize slot */
            entry->slotId = manager->nextSlotId++;
            entry->typeTag = type;
            entry->occupied = true;
            entry->dataBlockRef = NULL; /* Allocated later */
            entry->ttl = 0; /* Unlimited */
            entry->threadAffinity = 0; /* Current thread */
            entry->allocationTime = time(NULL);
            
            /* Set handle */
            handle->slotId = entry->slotId;
            handle->typeTag = type;
            handle->generation = 1;
            
            /* Update statistics */
            manager->totalAllocations++;
            manager->activeSlots++;
            
            pthread_mutex_unlock((pthread_mutex_t *)manager->mutex);
            return SLOT_SUCCESS;
        }
    }
    
    pthread_mutex_unlock((pthread_mutex_t *)manager->mutex);
    return SLOT_ERROR_OUT_OF_MEMORY;
}

/*
 * Write data to slot
 */
SlotError
SlotWrite(SlotManager *manager, const SlotHandle *handle, 
          const void *data, size_t dataSize)
{
    SlotEntry *entry = NULL;
    size_t     i;
    
    if (manager == NULL || handle == NULL || data == NULL)
        return SLOT_ERROR_INVALID_HANDLE;
    
    /* Use assembly version for simple types */
    if (dataSize <= 256 && handle->typeTag <= TYPE_CUSTOM)
        return SlotWriteFast(manager, handle, data, dataSize);
    
    pthread_mutex_lock((pthread_mutex_t *)manager->mutex);
    
    /* Find slot */
    for (i = 0; i < manager->tableSize; i++) {
        if (manager->slotTable[i].slotId == handle->slotId &&
            manager->slotTable[i].occupied) {
            entry = &manager->slotTable[i];
            break;
        }
    }
    
    if (entry == NULL) {
        pthread_mutex_unlock((pthread_mutex_t *)manager->mutex);
        return SLOT_ERROR_SLOT_NOT_FOUND;
    }
    
    /* Validate type */
    if (entry->typeTag != handle->typeTag) {
        pthread_mutex_unlock((pthread_mutex_t *)manager->mutex);
        return SLOT_ERROR_TYPE_MISMATCH;
    }
    
    /* Allocate memory block if needed */
    if (entry->dataBlockRef == NULL) {
        entry->dataBlockRef = AllocateMemoryBlock(manager, dataSize);
        if (entry->dataBlockRef == NULL) {
            pthread_mutex_unlock((pthread_mutex_t *)manager->mutex);
            return SLOT_ERROR_OUT_OF_MEMORY;
        }
    }
    
    /* Copy data */
    memcpy(entry->dataBlockRef, data, dataSize);
    
    pthread_mutex_unlock((pthread_mutex_t *)manager->mutex);
    return SLOT_SUCCESS;
}

/*
 * Read data from slot
 */
SlotError
SlotRead(SlotManager *manager, const SlotHandle *handle, 
         void *buffer, size_t bufferSize, size_t *bytesRead)
{
    SlotEntry *entry = NULL;
    size_t     i;
    size_t     copySize;
    
    if (manager == NULL || handle == NULL || buffer == NULL)
        return SLOT_ERROR_INVALID_HANDLE;
    
    pthread_mutex_lock((pthread_mutex_t *)manager->mutex);
    
    /* Find slot */
    for (i = 0; i < manager->tableSize; i++) {
        if (manager->slotTable[i].slotId == handle->slotId &&
            manager->slotTable[i].occupied) {
            entry = &manager->slotTable[i];
            break;
        }
    }
    
    if (entry == NULL) {
        manager->cacheMisses++;
        pthread_mutex_unlock((pthread_mutex_t *)manager->mutex);
        return SLOT_ERROR_SLOT_NOT_FOUND;
    }
    
    /* Validate type */
    if (entry->typeTag != handle->typeTag) {
        pthread_mutex_unlock((pthread_mutex_t *)manager->mutex);
        return SLOT_ERROR_TYPE_MISMATCH;
    }
    
    /* Check data block */
    if (entry->dataBlockRef == NULL) {
        pthread_mutex_unlock((pthread_mutex_t *)manager->mutex);
        return SLOT_ERROR_SLOT_NOT_FOUND;
    }
    
    /* Copy data (size determined by type) */
    copySize = TypeGetSize(entry->typeTag);
    if (copySize > bufferSize)
        copySize = bufferSize;
    
    memcpy(buffer, entry->dataBlockRef, copySize);
    if (bytesRead != NULL)
        *bytesRead = copySize;
    
    manager->cacheHits++;
    pthread_mutex_unlock((pthread_mutex_t *)manager->mutex);
    return SLOT_SUCCESS;
}

/*
 * Release slot and free resources
 */
SlotError
SlotRelease(SlotManager *manager, const SlotHandle *handle)
{
    SlotEntry *entry;
    size_t     i;
    size_t     blockSize;
    
    if (manager == NULL || handle == NULL)
        return SLOT_ERROR_INVALID_HANDLE;
    
    pthread_mutex_lock((pthread_mutex_t *)manager->mutex);
    
    /* Find slot */
    for (i = 0; i < manager->tableSize; i++) {
        entry = &manager->slotTable[i];
        
        if (entry->slotId == handle->slotId && entry->occupied) {
            /* Free memory block */
            if (entry->dataBlockRef != NULL) {
                blockSize = TypeGetSize(entry->typeTag);
                DeallocateMemoryBlock(manager, entry->dataBlockRef, blockSize);
            }
            
            /* Clear slot entry */
            memset(entry, 0, sizeof(SlotEntry));
            
            /* Update statistics */
            manager->totalDeallocations++;
            manager->activeSlots--;
            
            pthread_mutex_unlock((pthread_mutex_t *)manager->mutex);
            return SLOT_SUCCESS;
        }
    }
    
    pthread_mutex_unlock((pthread_mutex_t *)manager->mutex);
    return SLOT_ERROR_SLOT_NOT_FOUND;
}

/*
 * Print slot manager statistics
 */
void
SlotManagerPrintStats(const SlotManager *manager)
{
    if (manager == NULL)
        return;
    
    printf("=== Pergyra Slot Manager Statistics ===\n");
    printf("Total allocations: %lu\n", manager->totalAllocations);
    printf("Total deallocations: %lu\n", manager->totalDeallocations);
    printf("Active slots: %lu\n", manager->activeSlots);
    printf("Cache hits: %lu\n", manager->cacheHits);
    printf("Cache misses: %lu\n", manager->cacheMisses);
    printf("Table size: %zu\n", manager->tableSize);
    printf("Utilization: %.2f%%\n", SlotManagerGetUtilization(manager) * 100.0);
}

/*
 * Get active slot count
 */
size_t
SlotManagerGetActiveCount(const SlotManager *manager)
{
    return manager != NULL ? manager->activeSlots : 0;
}

/*
 * Get slot table utilization ratio
 */
double
SlotManagerGetUtilization(const SlotManager *manager)
{
    if (manager == NULL || manager->tableSize == 0)
        return 0.0;
    return (double)manager->activeSlots / (double)manager->tableSize;
}

/*
 * Get type size in bytes
 */
size_t
TypeGetSize(TypeTag tag)
{
    switch (tag) {
    case TYPE_INT:    return sizeof(int32_t);
    case TYPE_LONG:   return sizeof(int64_t);
    case TYPE_FLOAT:  return sizeof(float);
    case TYPE_DOUBLE: return sizeof(double);
    case TYPE_BOOL:   return sizeof(bool);
    case TYPE_STRING: return 256; /* Maximum string size */
    case TYPE_VECTOR: return 1024; /* Vector size */
    default:          return 64; /* Default size */
    }
}

/*
 * Convert type tag to string
 */
const char *
TypeTagToString(TypeTag tag)
{
    switch (tag) {
    case TYPE_INT:    return "Int";
    case TYPE_LONG:   return "Long";
    case TYPE_FLOAT:  return "Float";
    case TYPE_DOUBLE: return "Double";
    case TYPE_BOOL:   return "Bool";
    case TYPE_STRING: return "String";
    case TYPE_VECTOR: return "Vector";
    default:          return "Custom";
    }
}

/*
 * Check if type is primitive
 */
bool
TypeIsPrimitive(TypeTag tag)
{
    return tag >= TYPE_INT && tag <= TYPE_BOOL;
}

/*
 * Generate hash for type name
 */
uint32_t
TypeTagHash(const char *typeName)
{
    uint32_t hash = 5381;
    const char *c;
    
    if (typeName == NULL)
        return 0;
    
    /* Simple djb2 hash function */
    for (c = typeName; *c; c++)
        hash = ((hash << 5) + hash) + *c;
    
    return hash;
}

/*
 * Inline function implementations
 */
static inline uint32_t
SlotHashFunction(uint32_t slotId)
{
    return SlotHashFunctionAsm(slotId);
}

static inline bool
SlotCompareAndSwap(volatile uint32_t *ptr, uint32_t expected, uint32_t newVal)
{
    return SlotCompareAndSwapAsm(ptr, expected, newVal);
}

static inline void
SlotMemoryBarrier(void)
{
    SlotMemoryBarrierAsm();
}                                   handle->slotId, "Token lacks read permission");
        return SLOT_ERROR_PERMISSION_DENIED;
    }
    
    /* Validate token */
    secResult = TokenValidate(manager->securityContext, handle->slotId, token);
    if (secResult != SECURITY_SUCCESS) {
        manager->securityViolations++;
        SlotManagerLogSecurityEvent(manager, "TOKEN_VALIDATION_FAILED", 
                                   handle->slotId, "Invalid or expired token");
        
        switch (secResult) {
            case SECURITY_ERROR_TOKEN_EXPIRED:
                return SLOT_ERROR_TTL_EXPIRED;
            case SECURITY_ERROR_HARDWARE_MISMATCH:
            case SECURITY_ERROR_INVALID_TOKEN:
            default:
                return SLOT_ERROR_PERMISSION_DENIED;
        }
    }
    
    /* Update access statistics */
    entry->lastAccessTime = SecureTimestamp();
    entry->accessCount++;
    
    /* Perform the actual read */
    SlotError result = SlotRead(manager, handle, buffer, bufferSize, bytesRead);
    
    if (result == SLOT_SUCCESS) {
        SlotManagerLogSecurityEvent(manager, "SECURE_READ_SUCCESS", 
                                   handle->slotId, "Secure read completed");
    }
    
    return result;
}

/*
 * Release a secure slot with token validation
 */
SlotError
SlotReleaseSecure(SlotManager *manager, const SlotHandle *handle,
                 const TokenCapability *token)
{
    SlotEntry *entry = NULL;
    size_t slotIndex;
    SecurityError secResult;
    
    if (manager == NULL || handle == NULL || token == NULL)
        return SLOT_ERROR_INVALID_HANDLE;
    
    if (!SlotManagerIsSecurityEnabled(manager))
        return SLOT_ERROR_PERMISSION_DENIED;
    
    /* Find the slot entry */
    for (slotIndex = 0; slotIndex < manager->tableSize; slotIndex++) {
        if (manager->slotTable[slotIndex].slotId == handle->slotId) {
            entry = &manager->slotTable[slotIndex];
            break;
        }
    }
    
    if (entry == NULL || !entry->occupied)
        return SLOT_ERROR_SLOT_NOT_FOUND;
    
    if (entry->securityEnabled) {
        /* Validate token for secure release */
        secResult = TokenValidate(manager->securityContext, handle->slotId, token);
        if (secResult != SECURITY_SUCCESS) {
            manager->securityViolations++;
            SlotManagerLogSecurityEvent(manager, "RELEASE_TOKEN_VALIDATION_FAILED", 
                                       handle->slotId, "Cannot release slot without valid token");
            return SLOT_ERROR_PERMISSION_DENIED;
        }
        
        /* Securely wipe token data */
        SecureMemoryWipe(&entry->writeToken, sizeof(EncryptedToken));
        entry->securityEnabled = false;
        entry->tokenGeneration = 0;
        
        SlotManagerLogSecurityEvent(manager, "SECURE_RELEASE_SUCCESS", 
                                   handle->slotId, "Secure slot released");
    }
    
    /* Perform the actual release */
    return SlotRelease(manager, handle);
}

/*
 * Validate token for a slot
 */
bool
SlotValidateToken(SlotManager *manager, const SlotHandle *handle,
                 const TokenCapability *token)
{
    if (manager == NULL || handle == NULL || token == NULL)
        return false;
    
    if (!SlotManagerIsSecurityEnabled(manager))
        return false;
    
    SecurityError result = TokenValidate(manager->securityContext, 
                                       handle->slotId, token);
    return result == SECURITY_SUCCESS;
}

/*
 * Refresh token for a slot
 */
SlotError
SlotRefreshToken(SlotManager *manager, const SlotHandle *handle,
                TokenCapability *token)
{
    SlotEntry *entry = NULL;
    size_t slotIndex;
    SecurityError secResult;
    
    if (manager == NULL || handle == NULL || token == NULL)
        return SLOT_ERROR_INVALID_HANDLE;
    
    if (!SlotManagerIsSecurityEnabled(manager))
        return SLOT_ERROR_PERMISSION_DENIED;
    
    /* Find the slot entry */
    for (slotIndex = 0; slotIndex < manager->tableSize; slotIndex++) {
        if (manager->slotTable[slotIndex].slotId == handle->slotId) {
            entry = &manager->slotTable[slotIndex];
            break;
        }
    }
    
    if (entry == NULL || !entry->occupied || !entry->securityEnabled)
        return SLOT_ERROR_SLOT_NOT_FOUND;
    
    /* Validate current token first */
    secResult = TokenValidate(manager->securityContext, handle->slotId, token);
    if (secResult != SECURITY_SUCCESS)
        return SLOT_ERROR_PERMISSION_DENIED;
    
    /* Generate new token */
    TokenCapability newToken;
    secResult = TokenGenerate(manager->securityContext, handle->slotId,
                            entry->securityLevel, &newToken);
    if (secResult != SECURITY_SUCCESS)
        return SLOT_ERROR_OUT_OF_MEMORY;
    
    /* Update stored token */
    SecureToken plainToken = newToken.token;
    secResult = TokenEncrypt(manager->securityContext, &plainToken,
                           &entry->writeToken);
    if (secResult != SECURITY_SUCCESS) {
        SecureMemoryWipe(&newToken, sizeof(TokenCapability));
        return SLOT_ERROR_OUT_OF_MEMORY;
    }
    
    /* Update token generation */
    entry->tokenGeneration++;
    
    /* Copy new token to output */
    *token = newToken;
    
    /* Wipe temporary data */
    SecureMemoryWipe(&plainToken, sizeof(SecureToken));
    
    SlotManagerLogSecurityEvent(manager, "TOKEN_REFRESHED", 
                               handle->slotId, "Token successfully refreshed");
    
    return SLOT_SUCCESS;
}

/*
 * Revoke token for a slot
 */
SlotError
SlotRevokeToken(SlotManager *manager, const SlotHandle *handle)
{
    SlotEntry *entry = NULL;
    size_t slotIndex;
    
    if (manager == NULL || handle == NULL)
        return SLOT_ERROR_INVALID_HANDLE;
    
    if (!SlotManagerIsSecurityEnabled(manager))
        return SLOT_ERROR_PERMISSION_DENIED;
    
    /* Find the slot entry */
    for (slotIndex = 0; slotIndex < manager->tableSize; slotIndex++) {
        if (manager->slotTable[slotIndex].slotId == handle->slotId) {
            entry = &manager->slotTable[slotIndex];
            break;
        }
    }
    
    if (entry == NULL || !entry->occupied)
        return SLOT_ERROR_SLOT_NOT_FOUND;
    
    if (entry->securityEnabled) {
        /* Securely wipe token data */
        SecureMemoryWipe(&entry->writeToken, sizeof(EncryptedToken));
        entry->tokenGeneration = 0;
        
        SlotManagerLogSecurityEvent(manager, "TOKEN_REVOKED", 
                                   handle->slotId, "Token revoked by administrator");
    }
    
    return SLOT_SUCCESS;
}

/*
 * Security audit and monitoring functions
 */
void
SlotManagerLogSecurityEvent(SlotManager *manager, const char *event,
                           uint32_t slotId, const char *details)
{
    if (manager == NULL || event == NULL)
        return;
    
    if (!SlotManagerIsSecurityEnabled(manager))
        return;
    
    /* Simple logging implementation - in production would use proper logging */
    time_t now = time(NULL);
    struct tm *timeinfo = localtime(&now);
    char timestamp[64];
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", timeinfo);
    
    printf("[SECURITY] %s - SlotID:%u - Event:%s - Details:%s\n",
           timestamp, slotId, event, details ? details : "N/A");
    
    /* Update security context statistics */
    if (manager->securityContext != NULL) {
        SecurityAuditLog(manager->securityContext, event, details);
    }
}

bool
SlotManagerDetectAnomalies(SlotManager *manager)
{
    if (manager == NULL || !SlotManagerIsSecurityEnabled(manager))
        return false;
    
    /* Simple anomaly detection */
    bool anomalyDetected = false;
    
    /* Check for excessive security violations */
    if (manager->securityViolations > SECURITY_MAX_VALIDATION_FAILURES) {
        SlotManagerLogSecurityEvent(manager, "ANOMALY_EXCESSIVE_VIOLATIONS", 
                                   0, "Too many security violations detected");
        anomalyDetected = true;
    }
    
    /* Check individual slots for suspicious access patterns */
    uint64_t currentTime = SecureTimestamp();
    for (size_t i = 0; i < manager->tableSize; i++) {
        SlotEntry *entry = &manager->slotTable[i];
        if (entry->occupied && entry->securityEnabled) {
            /* Check for rapid successive accesses (potential automation) */
            if (entry->accessCount > 1000 && 
                (currentTime - entry->lastAccessTime) < 1000000) { /* 1 second */
                SlotManagerLogSecurityEvent(manager, "ANOMALY_RAPID_ACCESS", 
                                           entry->slotId, "Suspicious rapid access pattern");
                anomalyDetected = true;
            }
            
            /* Check for very old slots that haven't been accessed */
            if ((currentTime - entry->lastAccessTime) > 86400000000ULL) { /* 1 day */
                SlotManagerLogSecurityEvent(manager, "ANOMALY_STALE_SLOT", 
                                           entry->slotId, "Slot not accessed for extended period");
            }
        }
    }
    
    /* Use security context's anomaly detection */
    if (manager->securityContext != NULL) {
        anomalyDetected |= SecurityDetectAnomalies(manager->securityContext);
    }
    
    return anomalyDetected;
}

void
SlotManagerPrintSecurityStats(const SlotManager *manager)
{
    if (manager == NULL) {
        printf("SlotManager is NULL\n");
        return;
    }
    
    printf("=== Slot Manager Security Statistics ===\n");
    printf("Security Enabled: %s\n", 
           SlotManagerIsSecurityEnabled(manager) ? "Yes" : "No");
    
    if (SlotManagerIsSecurityEnabled(manager)) {
        printf("Default Security Level: %d\n", manager->defaultSecurityLevel);
        printf("Security Violations: %llu\n", 
               (unsigned long long)manager->securityViolations);
        
        /* Count secure slots */
        size_t secureSlots = 0;
        size_t totalActiveSlots = 0;
        for (size_t i = 0; i < manager->tableSize; i++) {
            if (manager->slotTable[i].occupied) {
                totalActiveSlots++;
                if (manager->slotTable[i].securityEnabled) {
                    secureSlots++;
                }
            }
        }
        
        printf("Active Slots: %zu\n", totalActiveSlots);
        printf("Secure Slots: %zu\n", secureSlots);
        printf("Security Coverage: %.1f%%\n", 
               totalActiveSlots > 0 ? (secureSlots * 100.0 / totalActiveSlots) : 0.0);
        
        /* Print security context statistics */
        if (manager->securityContext != NULL) {
            printf("\n=== Security Context Statistics ===\n");
            SecurityPrintStatistics(manager->securityContext);
        }
    }
    
    printf("==========================================\n");
}

/*
 * ==================================================================
 * ENHANCED SLOT MANAGER CREATION WITH SECURITY SUPPORT
 * ==================================================================
 */

/*
 * Create slot manager with optional security
 */
SlotManager *
SlotManagerCreateSecure(size_t maxSlots, size_t memoryPoolSize, 
                       bool enableSecurity, SecurityLevel defaultLevel)
{
    SlotManager *manager = SlotManagerCreate(maxSlots, memoryPoolSize);
    if (manager == NULL)
        return NULL;
    
    /* Initialize security fields */
    manager->securityContext = NULL;
    manager->securityEnabled = false;
    manager->defaultSecurityLevel = SECURITY_LEVEL_BASIC;
    manager->securityViolations = 0;
    
    if (enableSecurity) {
        SlotError result = SlotManagerEnableSecurity(manager, defaultLevel);
        if (result != SLOT_SUCCESS) {
            SlotManagerDestroy(manager);
            return NULL;
        }
    }
    
    return manager;
}

/*
 * Enhanced slot manager destruction with security cleanup
 */
void
SlotManagerDestroySecure(SlotManager *manager)
{
    if (manager == NULL)
        return;
    
    /* Disable security first to clean up sensitive data */
    if (SlotManagerIsSecurityEnabled(manager)) {
        SlotManagerDisableSecurity(manager);
    }
    
    /* Call original destroy function */
    SlotManagerDestroy(manager);
}

/*
 * ==================================================================
 * SCOPE-BASED SECURE SLOT MANAGEMENT
 * ==================================================================
 */

/*
 * Scope-based secure slot claiming with automatic cleanup
 */
typedef struct SecureSlotScope {
    SlotManager *manager;
    SlotHandle *handles;
    TokenCapability *tokens;
    size_t count;
    size_t capacity;
    bool autoCleanup;
} SecureSlotScope;

SecureSlotScope *
SecureSlotScopeCreate(SlotManager *manager, size_t capacity)
{
    if (manager == NULL || !SlotManagerIsSecurityEnabled(manager))
        return NULL;
    
    SecureSlotScope *scope = malloc(sizeof(SecureSlotScope));
    if (scope == NULL)
        return NULL;
    
    scope->handles = malloc(capacity * sizeof(SlotHandle));
    scope->tokens = malloc(capacity * sizeof(TokenCapability));
    
    if (scope->handles == NULL || scope->tokens == NULL) {
        free(scope->handles);
        free(scope->tokens);
        free(scope);
        return NULL;
    }
    
    scope->manager = manager;
    scope->count = 0;
    scope->capacity = capacity;
    scope->autoCleanup = true;
    
    return scope;
}

SlotError
SecureSlotScopeClaimSlot(SecureSlotScope *scope, TypeTag type, 
                        SecurityLevel level, SlotHandle **handle, 
                        TokenCapability **token)
{
    if (scope == NULL || scope->count >= scope->capacity)
        return SLOT_ERROR_OUT_OF_MEMORY;
    
    SlotError result = SlotClaimSecure(scope->manager, type, level,
                                     &scope->handles[scope->count],
                                     &scope->tokens[scope->count]);
    
    if (result == SLOT_SUCCESS) {
        if (handle != NULL)
            *handle = &scope->handles[scope->count];
        if (token != NULL)
            *token = &scope->tokens[scope->count];
        scope->count++;
    }
    
    return result;
}

void
SecureSlotScopeDestroy(SecureSlotScope *scope)
{
    if (scope == NULL)
        return;
    
    /* Release all slots if auto-cleanup is enabled */
    if (scope->autoCleanup) {
        for (size_t i = 0; i < scope->count; i++) {
            SlotReleaseSecure(scope->manager, &scope->handles[i], 
                            &scope->tokens[i]);
        }
    }
    
    /* Securely wipe token data */
    if (scope->tokens != NULL) {
        SecureMemoryWipe(scope->tokens, 
                        scope->capacity * sizeof(TokenCapability));
        free(scope->tokens);
    }
    
    free(scope->handles);
    free(scope);
}

/*
 * ==================================================================
 * LANGUAGE-LEVEL API FOR PERGYRA
 * ==================================================================
 */

/*
 * High-level Pergyra language API for secure slots
 * These functions provide the interface that the Pergyra compiler will use
 */

typedef struct PergyraSecureSlot {
    SlotHandle handle;
    TokenCapability token;
    TypeTag typeTag;
    bool isValid;
} PergyraSecureSlot;

/*
 * claim_secure_slot<Type>() equivalent
 */
PergyraSecureSlot *
pergyra_claim_secure_slot(SlotManager *manager, const char *typeName, 
                         SecurityLevel level)
{
    if (manager == NULL || typeName == NULL)
        return NULL;
    
    if (!SlotManagerIsSecurityEnabled(manager))
        return NULL;
    
    PergyraSecureSlot *slot = malloc(sizeof(PergyraSecureSlot));
    if (slot == NULL)
        return NULL;
    
    TypeTag typeTag = TypeTagHash(typeName);
    
    SlotError result = SlotClaimSecure(manager, typeTag, level, 
                                     &slot->handle, &slot->token);
    
    if (result != SLOT_SUCCESS) {
        free(slot);
        return NULL;
    }
    
    slot->typeTag = typeTag;
    slot->isValid = true;
    
    return slot;
}

/*
 * write(slot, value, token) equivalent
 */
bool
pergyra_slot_write_secure(PergyraSecureSlot *slot, const void *data, 
                         size_t dataSize)
{
    if (slot == NULL || !slot->isValid || data == NULL)
        return false;
    
    /* The manager reference would need to be stored or passed */
    /* For now, assume global manager or retrieve from slot context */
    extern SlotManager *g_pergyraSlotManager; /* Global manager reference */
    
    SlotError result = SlotWriteSecure(g_pergyraSlotManager, &slot->handle,
                                     data, dataSize, &slot->token);
    
    return result == SLOT_SUCCESS;
}

/*
 * read(slot) equivalent
 */
bool
pergyra_slot_read_secure(PergyraSecureSlot *slot, void *buffer, 
                        size_t bufferSize, size_t *bytesRead)
{
    if (slot == NULL || !slot->isValid || buffer == NULL)
        return false;
    
    extern SlotManager *g_pergyraSlotManager;
    
    SlotError result = SlotReadSecure(g_pergyraSlotManager, &slot->handle,
                                    buffer, bufferSize, bytesRead, 
                                    &slot->token);
    
    return result == SLOT_SUCCESS;
}

/*
 * release(slot) equivalent
 */
void
pergyra_slot_release_secure(PergyraSecureSlot *slot)
{
    if (slot == NULL || !slot->isValid)
        return;
    
    extern SlotManager *g_pergyraSlotManager;
    
    SlotReleaseSecure(g_pergyraSlotManager, &slot->handle, &slot->token);
    
    /* Securely wipe slot data */
    SecureMemoryWipe(slot, sizeof(PergyraSecureSlot));
    slot->isValid = false;
    
    free(slot);
}

/*
 * Scope-based syntax: with slot<Type> as s { ... }
 */
typedef struct PergyraSlotScope {
    SecureSlotScope *scope;
    SlotManager *manager;
} PergyraSlotScope;

PergyraSlotScope *
pergyra_scope_begin(SlotManager *manager)
{
    PergyraSlotScope *pscope = malloc(sizeof(PergyraSlotScope));
    if (pscope == NULL)
        return NULL;
    
    pscope->scope = SecureSlotScopeCreate(manager, 64); /* Default capacity */
    pscope->manager = manager;
    
    if (pscope->scope == NULL) {
        free(pscope);
        return NULL;
    }
    
    return pscope;
}

PergyraSecureSlot *
pergyra_scope_claim_slot(PergyraSlotScope *pscope, const char *typeName, 
                        SecurityLevel level)
{
    if (pscope == NULL || pscope->scope == NULL)
        return NULL;
    
    SlotHandle *handle;
    TokenCapability *token;
    TypeTag typeTag = TypeTagHash(typeName);
    
    SlotError result = SecureSlotScopeClaimSlot(pscope->scope, typeTag, level,
                                              &handle, &token);
    
    if (result != SLOT_SUCCESS)
        return NULL;
    
    /* Create wrapper for language-level use */
    PergyraSecureSlot *slot = malloc(sizeof(PergyraSecureSlot));
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
    
    if (pscope->scope != NULL) {
        SecureSlotScopeDestroy(pscope->scope);
    }
    
    free(pscope);
}

/*
 * Example usage tracking for security auditing
 */
void
pergyra_security_audit_usage_example(void)
{
    printf("=== Pergyra Secure Slot Usage Example ===\n");
    printf("// High-level Pergyra syntax:\n");
    printf("let (slot, token) = claim_secure_slot<Int>(SECURITY_LEVEL_HARDWARE)\n");
    printf("write(slot, 42, token)\n");
    printf("let value = read(slot, token)\n");
    printf("release(slot, token)\n");
    printf("\n");
    printf("// Scope-based syntax:\n");
    printf("with secure_slot<Int>(SECURITY_LEVEL_ENCRYPTED) as s {\n");
    printf("    s.write(42)\n");
    printf("    log(s.read())\n");
    printf("} // automatic release with token validation\n");
    printf("==========================================\n");
}
