/*
 * Copyright (c) 2025 Pergyra Language Project
 * Implementation of security-aware generic type system
 */

#include "security_types.h"
#include "slot_manager.h"
#include <string.h>
#include <time.h>

/* Security trait implementations */

static bool InsecureValidateToken(void* token) {
    /* Zero-cost mode: no validation needed */
    return true;
}

static bool InsecureIsSecure(void) {
    return false;
}

const SecurityModelTrait InsecureModel = {
    .name = "Insecure",
    .ValidateToken = InsecureValidateToken,
    .IsSecure = InsecureIsSecure
};

static bool BasicValidateToken(void* token) {
    if (!token) return false;
    AdaptiveSecurityToken* ast = (AdaptiveSecurityToken*)token;
    return ast->validationTag == EXPECTED_VALIDATION_TAG;
}

static bool BasicIsSecure(void) {
    return true;
}

const SecurityModelTrait BasicSecureModel = {
    .name = "Secure<Basic>",
    .ValidateToken = BasicValidateToken,
    .IsSecure = BasicIsSecure
};

static bool HardwareValidateToken(void* token) {
    if (!token) return false;
    /* Hardware-backed validation would go here */
    return BasicValidateToken(token);
}

static bool HardwareIsSecure(void) {
    return true;
}

const SecurityModelTrait HardwareSecureModel = {
    .name = "Secure<Hardware>",
    .ValidateToken = HardwareValidateToken,
    .IsSecure = HardwareIsSecure
};

static bool EncryptedValidateToken(void* token) {
    if (!token) return false;
    /* Full encryption validation */
    return BasicValidateToken(token);
}

static bool EncryptedIsSecure(void) {
    return true;
}

const SecurityModelTrait EncryptedSecureModel = {
    .name = "Secure<Encrypted>",
    .ValidateToken = EncryptedValidateToken,
    .IsSecure = EncryptedIsSecure
};

/* Get trait implementation for a security model */
const SecurityModelTrait* GetSecurityTrait(SecurityModel model) {
    switch (model) {
        case SECURITY_MODEL_ZERO_COST:
            return &InsecureModel;
        case SECURITY_MODEL_BASIC:
            return &BasicSecureModel;
        case SECURITY_MODEL_HARDWARE:
            return &HardwareSecureModel;
        case SECURITY_MODEL_ENCRYPTED:
            return &EncryptedSecureModel;
        default:
            return &BasicSecureModel;
    }
}

/* Type-safe slot claiming */
GenericSlot ClaimSlotWithSecurity(uint32_t typeHash, SecurityModel security) {
    GenericSlot slot;
    
    /* Claim appropriate slot type based on security level */
    if (security == SECURITY_MODEL_ZERO_COST) {
        slot.slotId = ClaimSlot(typeHash);
    } else {
        /* For secure slots, use the secure API */
        slot.slotId = ClaimSecureSlot(typeHash, (SecurityLevel)security);
    }
    
    slot.security = security;
    slot.typeHash = typeHash;
    slot.securityTrait = GetSecurityTrait(security);
    
    return slot;
}

/* Unified write with compile-time security enforcement */
void GenericWrite(GenericSlot slot, const void* data, size_t size, 
                  AdaptiveSecurityToken* token) {
    /* Validate security requirements */
    if (slot.security != SECURITY_MODEL_ZERO_COST) {
        if (!token || token->level != slot.security) {
            Panic("Security level mismatch in GenericWrite");
        }
        
        if (!slot.securityTrait->ValidateToken(token)) {
            Panic("Invalid security token in GenericWrite");
        }
    }
    
    /* Perform the write */
    if (slot.security == SECURITY_MODEL_ZERO_COST) {
        Write(slot.slotId, data, size);
    } else {
        WriteSecure(slot.slotId, data, size, (SecurityToken*)token->tokenData);
    }
}

/* Unified read with security enforcement */
void GenericRead(GenericSlot slot, void* buffer, size_t size,
                 AdaptiveSecurityToken* token) {
    /* Validate security requirements */
    if (slot.security != SECURITY_MODEL_ZERO_COST) {
        if (!token || token->level != slot.security) {
            Panic("Security level mismatch in GenericRead");
        }
        
        if (!slot.securityTrait->ValidateToken(token)) {
            Panic("Invalid security token in GenericRead");
        }
    }
    
    /* Perform the read */
    if (slot.security == SECURITY_MODEL_ZERO_COST) {
        Read(slot.slotId, buffer, size);
    } else {
        ReadSecure(slot.slotId, buffer, size, (SecurityToken*)token->tokenData);
    }
}

/* Release slot */
void GenericRelease(GenericSlot slot, AdaptiveSecurityToken* token) {
    if (slot.security == SECURITY_MODEL_ZERO_COST) {
        Release(slot.slotId);
    } else {
        if (!token || !slot.securityTrait->ValidateToken(token)) {
            Panic("Invalid token for secure slot release");
        }
        ReleaseSecure(slot.slotId, (SecurityToken*)token->tokenData);
    }
}

/* Security downgrade with audit */
DowngradeResult SecurityDowngradeWithAudit(GenericSlot secureSlot, 
                                          AdaptiveSecurityToken* token,
                                          const char* auditReason) {
    DowngradeResult result;
    
    /* Validate that we're actually downgrading */
    if (secureSlot.security == SECURITY_MODEL_ZERO_COST) {
        Panic("Cannot downgrade an already insecure slot");
    }
    
    /* Validate token */
    if (!token || !secureSlot.securityTrait->ValidateToken(token)) {
        Panic("Invalid token for security downgrade");
    }
    
    /* Log the audit */
    result.auditReason = auditReason;
    result.auditTimestamp = (uint64_t)time(NULL);
    
    /* Create new insecure slot and copy data */
    result.slot.slotId = ClaimSlot(secureSlot.typeHash);
    result.slot.security = SECURITY_MODEL_ZERO_COST;
    result.slot.typeHash = secureSlot.typeHash;
    result.slot.securityTrait = &InsecureModel;
    
    /* Copy data from secure to insecure slot */
    uint8_t buffer[MAX_SLOT_SIZE];
    size_t size = GetSlotSize(secureSlot.slotId);
    GenericRead(secureSlot, buffer, size, token);
    GenericWrite(result.slot, buffer, size, NULL);
    
    /* Log the downgrade for audit */
    LogSecurityEvent("DOWNGRADE", auditReason, secureSlot.slotId, result.slot.slotId);
    
    return result;
}

/* Security upgrade */
UpgradeResult SecurityUpgrade(GenericSlot insecureSlot, 
                             SecurityModel targetLevel) {
    UpgradeResult result;
    
    /* Validate that we're actually upgrading */
    if (insecureSlot.security != SECURITY_MODEL_ZERO_COST) {
        Panic("Slot is already secure");
    }
    
    if (targetLevel == SECURITY_MODEL_ZERO_COST) {
        Panic("Cannot upgrade to insecure level");
    }
    
    /* Create new secure slot */
    result.slot = ClaimSlotWithSecurity(insecureSlot.typeHash, targetLevel);
    
    /* Create security token */
    result.token.level = targetLevel;
    result.token.tokenData = CreateSecurityToken((SecurityLevel)targetLevel);
    result.token.validationTag = EXPECTED_VALIDATION_TAG;
    
    /* Copy data from insecure to secure slot */
    uint8_t buffer[MAX_SLOT_SIZE];
    size_t size = GetSlotSize(insecureSlot.slotId);
    GenericRead(insecureSlot, buffer, size, NULL);
    GenericWrite(result.slot, buffer, size, &result.token);
    
    /* Log the upgrade */
    LogSecurityEvent("UPGRADE", "Security level increased", 
                     insecureSlot.slotId, result.slot.slotId);
    
    return result;
}

/* Validate security context for functions */
bool ValidateSecurityContext(FunctionSecurityContext* ctx, 
                            GenericSlot slot) {
    /* Check if function has required effects */
    if (slot.security != SECURITY_MODEL_ZERO_COST) {
        if (!(ctx->requiredEffects & EFFECT_SECURITY)) {
            return false;  /* Function needs security effect */
        }
        
        if (slot.security > ctx->minimumSecurity) {
            return false;  /* Function can't handle this security level */
        }
    }
    
    return true;
}
