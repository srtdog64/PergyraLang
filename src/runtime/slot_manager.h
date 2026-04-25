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

#ifndef PERGYRA_SLOT_MANAGER_H
#define PERGYRA_SLOT_MANAGER_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "slot_security.h"

typedef enum
{
    PGY_SLOT_PIN_READ = 0,
    PGY_SLOT_PIN_WRITE = 1
} PgySlotPinMode;

/*
 * Slot table entry structure
 */
typedef struct
{
    uint32_t slotId;
    uint32_t typeTag;          /* Type identifier hash */
    bool     occupied;
    void    *dataBlockRef;     /* Plain payload for normal slots */
    size_t   dataSize;         /* Actual payload size */
    uint32_t ttl;              /* Time-To-Live in milliseconds */
    uint32_t scopeId;          /* Optional scope owner */
    uint32_t threadAffinity;   /* Assigned thread ID */
    uint64_t allocationTime;   /* Allocation timestamp */
    
    /* Security extensions */
    SecurityLevel securityLevel;     /* Security level for this slot */
    EncryptedToken writeToken;       /* Encrypted write access token */
    uint32_t tokenGeneration;        /* Token generation counter */
    bool     securityEnabled;        /* Whether security is active */
    SecureSlotPolicy securityPolicy; /* Storage policy for secure slots */
    SecureSealedPayload securePayload; /* Sealed storage for secure slots */
    uint64_t lastAccessTime;         /* Last access timestamp */
    uint32_t accessCount;            /* Access counter for anomaly detection */
    uint32_t dataChecksum;           /* Checksum of un-obfuscated data for integrity checks */

    /* Pin/lease state for hot-loop access */
    uint32_t pinCount;
    uint32_t pinMode;
    uint32_t pinThreadAffinity;
    uint32_t pinGeneration;
} SlotEntry;

/*
 * Slot manager structure
 */
typedef struct
{
    SlotEntry *slotTable;
    size_t     tableSize;
    size_t     maxSlots;
    uint32_t   nextSlotId;
    
    /* Memory pool reference */
    void *memoryPool;
    
    /* Concurrency control */
    void *mutex;                /* pthread_mutex_t or equivalent */
    
    /* Security context */
    SecurityContext *securityContext;  /* Security management */
    bool             securityEnabled;  /* Global security toggle */
    SecurityLevel    defaultSecurityLevel; /* Default security level */
    
    /* Statistics */
    uint64_t totalAllocations;
    uint64_t totalDeallocations;
    uint64_t activeSlots;
    uint64_t cacheHits;
    uint64_t cacheMisses;
    uint64_t securityViolations;        /* Security violation counter */
} SlotManager;

/*
 * Slot handle for external reference
 */
typedef struct
{
    uint32_t slotId;
    uint32_t typeTag;
    uint32_t generation;        /* For ABA problem prevention */
} SlotHandle;

typedef struct SecureSlotScope SecureSlotScope;
typedef struct PergyraSlotScope PergyraSlotScope;

/*
 * Type information enumeration
 */
typedef enum
{
    TYPE_INT = 0x1,
    TYPE_LONG = 0x2,
    TYPE_FLOAT = 0x3,
    TYPE_DOUBLE = 0x4,
    TYPE_STRING = 0x5,
    TYPE_BOOL = 0x6,
    TYPE_VECTOR = 0x7,
    TYPE_CUSTOM = 0x1000        /* User-defined types start here */
} TypeTag;

typedef struct
{
    SlotHandle handle;
    TokenCapability token;
    TypeTag typeTag;
    bool isValid;
} PergyraSecureSlot;

typedef struct
{
    void *ptr;
    size_t size;
    uint32_t slotId;
    uint32_t generation;
    PgySlotPinMode mode;
    bool valid;
} PgyPinnedView;

/*
 * Error codes for slot operations
 */
typedef enum
{
    SLOT_SUCCESS = 0,
    SLOT_ERROR_OUT_OF_MEMORY,
    SLOT_ERROR_INVALID_HANDLE,
    SLOT_ERROR_TYPE_MISMATCH,
    SLOT_ERROR_SLOT_NOT_FOUND,
    SLOT_ERROR_PERMISSION_DENIED,
    SLOT_ERROR_TTL_EXPIRED,
    SLOT_ERROR_THREAD_VIOLATION,
    SLOT_ERROR_PINNED,
    SLOT_ERROR_INVALID_PIN
} SlotError;

/*
 * Slot manager lifecycle functions
 */
SlotManager *SlotManagerCreate(size_t maxSlots, size_t memoryPoolSize);
void         SlotManagerDestroy(SlotManager *manager);

/*
 * Core slot operations (assembly-optimized)
 */
SlotError SlotClaim(SlotManager *manager, TypeTag type, SlotHandle *handle);
SlotError SlotWrite(SlotManager *manager, const SlotHandle *handle, 
                    const void *data, size_t dataSize);
SlotError SlotRead(SlotManager *manager, const SlotHandle *handle, 
                   void *buffer, size_t bufferSize, size_t *bytesRead);
SlotError SlotRelease(SlotManager *manager, const SlotHandle *handle);

/*
 * Scoped pin/lease operations for hot-loop access.
 * User-facing language syntax must keep PgyPinnedView scope-bound.
 */
SlotError PergyraSlotPin(SlotManager *manager, const SlotHandle *handle,
                        PgySlotPinMode mode, const TokenCapability *token,
                        PgyPinnedView *outView);
SlotError PergyraSlotUnpin(SlotManager *manager, PgyPinnedView *view);

/*
 * Scope-based management
 */
SlotError SlotClaimScoped(SlotManager *manager, TypeTag type, 
                         uint32_t scopeId, SlotHandle *handle);
SlotError SlotReleaseScope(SlotManager *manager, uint32_t scopeId);

/*
 * Type safety validation (assembly-optimized)
 */
bool SlotValidateType(SlotManager *manager, const SlotHandle *handle, 
                     TypeTag expectedType);
bool SlotIsValid(SlotManager *manager, const SlotHandle *handle);

/*
 * TTL management
 */
SlotError SlotSetTtl(SlotManager *manager, const SlotHandle *handle, 
                    uint32_t ttlMs);
SlotError SlotRefreshTtl(SlotManager *manager, const SlotHandle *handle);
void      SlotCleanupExpired(SlotManager *manager);

/*
 * Concurrency support
 */
SlotError SlotLock(SlotManager *manager, const SlotHandle *handle);
SlotError SlotUnlock(SlotManager *manager, const SlotHandle *handle);
SlotError SlotTryLock(SlotManager *manager, const SlotHandle *handle);

/*
 * Statistics and debugging
 */
void   SlotManagerPrintStats(const SlotManager *manager);
size_t SlotManagerGetActiveCount(const SlotManager *manager);
double SlotManagerGetUtilization(const SlotManager *manager);

/*
 * Type utility functions
 */
uint32_t    TypeTagHash(const char *typeName);
const char *TypeTagToString(TypeTag tag);
bool        TypeIsPrimitive(TypeTag tag);
size_t      TypeGetSize(TypeTag tag);

uint32_t SlotHashFunction(uint32_t slotId);
bool     SlotCompareAndSwap(volatile uint32_t *ptr, 
                           uint32_t expected, uint32_t newVal);
void     SlotMemoryBarrier(void);

/*
 * Secure slot operations with token-based access control
 */
SlotError SlotClaimSecure(SlotManager *manager, TypeTag type, 
                         SecurityLevel level, SlotHandle *handle, 
                         TokenCapability *token);
SlotError SlotWriteSecure(SlotManager *manager, const SlotHandle *handle,
                         const void *data, size_t dataSize, 
                         const TokenCapability *token);
SlotError SlotReadSecure(SlotManager *manager, const SlotHandle *handle,
                        void *buffer, size_t bufferSize, size_t *bytesRead,
                        const TokenCapability *token);
SlotError SlotReleaseSecure(SlotManager *manager, const SlotHandle *handle,
                           const TokenCapability *token);

/*
 * Token validation and management
 */
bool      SlotValidateToken(SlotManager *manager, const SlotHandle *handle,
                           const TokenCapability *token);
SlotError SlotRefreshToken(SlotManager *manager, const SlotHandle *handle,
                          TokenCapability *token);
SlotError SlotRevokeToken(SlotManager *manager, const SlotHandle *handle);

/*
 * Security management functions
 */
SlotError SlotManagerEnableSecurity(SlotManager *manager, SecurityLevel level);
SlotError SlotManagerDisableSecurity(SlotManager *manager);
bool      SlotManagerIsSecurityEnabled(const SlotManager *manager);
SlotError SlotManagerSetDefaultSecurityLevel(SlotManager *manager, SecurityLevel level);

/*
 * Security audit and monitoring
 */
void      SlotManagerLogSecurityEvent(SlotManager *manager, const char *event,
                                     uint32_t slotId, const char *details);
bool      SlotManagerDetectAnomalies(SlotManager *manager);
void      SlotManagerPrintSecurityStats(const SlotManager *manager);

/*
 * Extended secure manager and language-facing APIs
 */
SlotManager *SlotManagerCreateSecure(size_t maxSlots, size_t memoryPoolSize,
                                    bool enableSecurity, SecurityLevel defaultLevel);
void         SlotManagerDestroySecure(SlotManager *manager);

SecureSlotScope *SecureSlotScopeCreate(SlotManager *manager, size_t capacity);
SlotError        SecureSlotScopeClaimSlot(SecureSlotScope *scope, TypeTag type,
                                         SecurityLevel level, SlotHandle **handle,
                                         TokenCapability **token);
void             SecureSlotScopeDestroy(SecureSlotScope *scope);

PergyraSecureSlot *pergyra_claim_secure_slot(SlotManager *manager, const char *typeName,
                                            SecurityLevel level);
bool               pergyra_slot_write_secure(PergyraSecureSlot *slot, const void *data,
                                            size_t dataSize);
bool               pergyra_slot_read_secure(PergyraSecureSlot *slot, void *buffer,
                                           size_t bufferSize, size_t *bytesRead);
void               pergyra_slot_release_secure(PergyraSecureSlot *slot);
PergyraSlotScope  *pergyra_scope_begin(SlotManager *manager);
PergyraSecureSlot *pergyra_scope_claim_slot(PergyraSlotScope *pscope,
                                           const char *typeName, SecurityLevel level);
void               pergyra_scope_end(PergyraSlotScope *pscope);
void               pergyra_security_audit_usage_example(void);

/*
 * Assembly implementation prototypes
 */
extern SlotError SlotClaimFast(SlotManager *manager, TypeTag type, 
                              SlotHandle *handle);
extern SlotError SlotWriteFast(SlotManager *manager, const SlotHandle *handle, 
                              const void *data, size_t dataSize);
extern SlotError SlotReadFast(SlotManager *manager, const SlotHandle *handle, 
                             void *buffer, size_t bufferSize);

#endif /* PERGYRA_SLOT_MANAGER_H */
