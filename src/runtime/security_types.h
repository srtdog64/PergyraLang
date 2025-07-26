/*
 * Copyright (c) 2025 Pergyra Language Project
 * Security-aware type system with generic security levels
 * BSD Style + C# naming conventions
 */

#ifndef PERGYRA_SECURITY_TYPES_H
#define PERGYRA_SECURITY_TYPES_H

#include <stdbool.h>
#include <stdint.h>

/* 
 * Security model trait representation
 * This is now a type-level concept that can be used with generics
 */
typedef enum {
    SECURITY_MODEL_ZERO_COST = 0,    /* Insecure - no runtime checks */
    SECURITY_MODEL_BASIC = 1,        /* Basic security with token validation */
    SECURITY_MODEL_HARDWARE = 2,     /* Hardware-backed security */
    SECURITY_MODEL_ENCRYPTED = 3,    /* Full encryption */
    SECURITY_MODEL_ANY = 0xFF        /* Wildcard for generic constraints */
} SecurityModel;

/* Security trait for type system */
typedef struct {
    const char* name;
    bool (*ValidateToken)(void* token);
    bool (*IsSecure)(void);
} SecurityModelTrait;

/* Concrete security model implementations */
extern const SecurityModelTrait InsecureModel;
extern const SecurityModelTrait BasicSecureModel;
extern const SecurityModelTrait HardwareSecureModel;
extern const SecurityModelTrait EncryptedSecureModel;

/* Get trait implementation for a security model */
const SecurityModelTrait* GetSecurityTrait(SecurityModel model);

/* Compile-time security properties */
typedef struct {
    SecurityModel model;
    bool isSecure;
    bool needsToken;
} SecurityProperties;

/* Get security properties for a model */
static inline SecurityProperties GetSecurityProperties(SecurityModel model)
{
    switch (model) {
        case SECURITY_MODEL_ZERO_COST:
            return (SecurityProperties){
                .model = model,
                .isSecure = false,
                .needsToken = false
            };
            
        case SECURITY_MODEL_BASIC:
        case SECURITY_MODEL_HARDWARE:
        case SECURITY_MODEL_ENCRYPTED:
            return (SecurityProperties){
                .model = model,
                .isSecure = true,
                .needsToken = true
            };
            
        default:
            // Should never happen
            return (SecurityProperties){0};
    }
}

/* 
 * Generic Slot type with security as a type parameter
 * In Pergyra: Slot<T, S: SecurityModel>
 */
typedef struct {
    uint32_t slotId;
    SecurityModel security;
    uint32_t typeHash;  /* Type information from generic T */
    const SecurityModelTrait* securityTrait;
} GenericSlot;

/* Legacy unified slot for backward compatibility */
typedef GenericSlot UnifiedSlot;

/* 
 * Security token that adapts to the security level
 * Some security levels don't require tokens
 */
typedef struct {
    SecurityModel level;
    void* tokenData;  /* NULL for ZERO_COST */
    uint64_t validationTag;
} AdaptiveSecurityToken;

/* Legacy token for backward compatibility */
typedef AdaptiveSecurityToken OptionalSecurityToken;

/* Type-safe slot claiming with security level */
GenericSlot ClaimSlotWithSecurity(uint32_t typeHash, SecurityModel security);

/* Legacy API for compatibility */
static inline UnifiedSlot UnifiedClaimSlot(uint32_t typeHash, SecurityModel security)
{
    return ClaimSlotWithSecurity(typeHash, security);
}

/* 
 * Unified API that enforces security constraints at compile time
 * In Pergyra, these would have generic constraints:
 * func Write<T, S: SecurityModel>(slot: Slot<T, S>, value: T, token: TokenFor<S>)
 */
void GenericWrite(GenericSlot slot, const void* data, size_t size, 
                  AdaptiveSecurityToken* token);

void GenericRead(GenericSlot slot, void* buffer, size_t size,
                 AdaptiveSecurityToken* token);

void GenericRelease(GenericSlot slot, AdaptiveSecurityToken* token);

/* Legacy API wrappers */
static inline void UnifiedWrite(UnifiedSlot slot, const void* data, size_t size, 
                               OptionalSecurityToken token)
{
    GenericWrite(slot, data, size, &token);
}

static inline void UnifiedRead(UnifiedSlot slot, void* buffer, size_t size,
                              OptionalSecurityToken token)
{
    GenericRead(slot, buffer, size, &token);
}

static inline void UnifiedRelease(UnifiedSlot slot, OptionalSecurityToken token)
{
    GenericRelease(slot, &token);
}

/* 
 * Compile-time security constraints
 * These would be enforced by Pergyra's type system
 */

/* Function signature constraints */
typedef struct {
    SecurityModel minimumSecurity;
    bool requiresToken;
    const char* auditReason;
} SecurityConstraint;

/* 
 * Security conversions with compile-time safety
 * In Pergyra: these would be generic functions with constraints
 */

/* Downgrade: Secure<T> -> Insecure (requires audit) */
typedef struct {
    GenericSlot slot;
    const char* auditReason;
    uint64_t auditTimestamp;
} DowngradeResult;

DowngradeResult SecurityDowngradeWithAudit(GenericSlot secureSlot, 
                                          AdaptiveSecurityToken* token,
                                          const char* auditReason);

/* Legacy downgrade API */
static inline UnifiedSlot SecurityDowngrade(UnifiedSlot secureSlot, 
                                          OptionalSecurityToken token,
                                          const char* auditReason)
{
    DowngradeResult result = SecurityDowngradeWithAudit(secureSlot, &token, auditReason);
    return result.slot;
}

/* Upgrade: Insecure -> Secure<Level> (always safe) */
typedef struct {
    GenericSlot slot;
    AdaptiveSecurityToken token;
} UpgradeResult;

UpgradeResult SecurityUpgrade(GenericSlot insecureSlot, 
                             SecurityModel targetLevel);

/* Legacy upgrade API */
static inline SecureSlotWithToken SecurityUpgrade(UnifiedSlot insecureSlot, 
                                                 SecurityModel targetLevel)
{
    UpgradeResult result = SecurityUpgrade(insecureSlot, targetLevel);
    return (SecureSlotWithToken){result.slot, result.token};
}

/* 
 * Function constraint annotations
 * In Pergyra: func ProcessFinancials(slot: Slot<Decimal, Secure<any>>)
 */
#define REQUIRE_SECURE_SLOT(slot) \
    _Static_assert((slot).security != SECURITY_MODEL_ZERO_COST, \
                   "This function requires a secure slot")

#define REQUIRE_SECURITY_LEVEL(slot, level) \
    _Static_assert((slot).security >= (level), \
                   "This function requires at least " #level " security")

#define REQUIRE_MATCHING_SECURITY(slot, token) \
    do { \
        if ((slot).security != (token).level) { \
            Panic("Security level mismatch between slot and token"); \
        } \
    } while(0)

/* 
 * Performance optimizations for zero-cost mode
 * These would be inlined by the Pergyra compiler
 */
#ifdef PERGYRA_ZERO_COST_MODE
    #define DEFAULT_SECURITY_MODEL SECURITY_MODEL_ZERO_COST
#else
    #define DEFAULT_SECURITY_MODEL SECURITY_MODEL_BASIC
#endif

/* Direct memory operations without checks */
static inline void FastWrite(GenericSlot slot, const void* data, size_t size) 
{
    if (slot.security == SECURITY_MODEL_ZERO_COST) {
        void* ptr = GetSlotPointer(slot.slotId);
        memcpy(ptr, data, size);
    } else {
        Panic("FastWrite called on secure slot");
    }
}

static inline void FastRead(GenericSlot slot, void* buffer, size_t size) 
{
    if (slot.security == SECURITY_MODEL_ZERO_COST) {
        void* ptr = GetSlotPointer(slot.slotId);
        memcpy(buffer, ptr, size);
    } else {
        Panic("FastRead called on secure slot");
    }
}

/* Helper to create type-safe slots in C (for runtime) */
#define SLOT(T, S) \
    ((GenericSlot){ \
        .typeHash = TYPE_HASH_##T, \
        .security = SECURITY_MODEL_##S, \
        .securityTrait = &S##Model \
    })

/* Type-safe token creation */
#define TOKEN_FOR(S) \
    ((AdaptiveSecurityToken){ \
        .level = SECURITY_MODEL_##S, \
        .tokenData = (S == ZERO_COST) ? NULL : AllocateToken() \
    })

/* 
 * Effect system integration
 * These would be Pergyra effects that functions must declare
 */
typedef enum {
    EFFECT_NONE = 0,
    EFFECT_SECURITY = 1 << 0,
    EFFECT_IO = 1 << 1,
    EFFECT_ASYNC = 1 << 2
} EffectType;

/* Function must declare security effect to use secure slots */
typedef struct {
    EffectType requiredEffects;
    SecurityModel minimumSecurity;
} FunctionSecurityContext;

/* Validate that a function has required effects */
bool ValidateSecurityContext(FunctionSecurityContext* ctx, 
                            GenericSlot slot);

#endif /* PERGYRA_SECURITY_TYPES_H */
