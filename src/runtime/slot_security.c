/*
 * Copyright (c) 2025 Pergyra Language Project
 * All rights reserved.
 *
 * Slot Security System Implementation
 * 
 * This module provides secure token-based access control for slots,
 * preventing external memory manipulation tools from modifying slot values.
 */

#include "slot_security.h"
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <stdio.h>

#ifdef _WIN32
#include <windows.h>
#include <wincrypt.h>
#pragma comment(lib, "advapi32.lib")
#elif defined(__linux__)
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#endif

static void
slot_security_warn(const char *op, SecurityError err, const char *reason)
{
    fprintf(stderr,
            "[pgy][slot-security] %s failed: %s (err=%d)\n",
            op != NULL ? op : "<op>",
            reason != NULL ? reason : "unknown",
            (int)err);
}

/*
 * Global security context (singleton pattern for performance)
 */
static SecurityContext *g_securityContext = NULL;

/*
 * Compile-time security constants
 */
static const uint8_t SECURITY_MAGIC[] = {
    0x50, 0x45, 0x52, 0x47, 0x59, 0x52, 0x41, 0x53,  /* "PERGYRAS" */
    0x45, 0x43, 0x55, 0x52, 0x49, 0x54, 0x59, 0x00   /* "ECURITY\0" */
};

static const uint32_t SECURITY_VERSION = 0x00010002;  /* bumped: IV 8→12 bytes */

#define SECURITY_IV_SIZE 12

static uint32_t
SecurityChecksumBytes(const uint8_t *data, size_t size)
{
    uint32_t checksum = 0;
    size_t i;

    for (i = 0; i < size; i++) {
        checksum = (checksum << 5) | (checksum >> 27);
        checksum ^= data[i];
        checksum += data[i];
    }

    return checksum;
}

static uint32_t
TokenCapabilityChecksum(SecurityContext *context, uint32_t slotId,
                        SecurityLevel level, const SecureToken *token,
                        bool canRead, bool canWrite, bool canTransfer,
                        uint64_t expiryTime)
{
    uint8_t material[64];
    uint8_t digest[32];
    size_t offset = 0;
    uint32_t hwHash;

    if (context == NULL || token == NULL)
        return 0;

    hwHash = HardwareFingerprintHash(&context->hwFingerprint);

    memcpy(material + offset, token->tokenData, sizeof(token->tokenData));
    offset += sizeof(token->tokenData);
    memcpy(material + offset, &slotId, sizeof(slotId));
    offset += sizeof(slotId);
    memcpy(material + offset, &level, sizeof(level));
    offset += sizeof(level);
    memcpy(material + offset, &token->generation, sizeof(token->generation));
    offset += sizeof(token->generation);
    memcpy(material + offset, &expiryTime, sizeof(expiryTime));
    offset += sizeof(expiryTime);
    material[offset++] = canRead ? 1u : 0u;
    material[offset++] = canWrite ? 1u : 0u;
    material[offset++] = canTransfer ? 1u : 0u;
    memcpy(material + offset, &hwHash, sizeof(hwHash));
    offset += sizeof(hwHash);

    if (SecureHashSHA256(material, offset, digest) != SECURITY_SUCCESS)
        return SecurityChecksumBytes(material, offset);

    return ((uint32_t)digest[0] << 24) | ((uint32_t)digest[1] << 16) |
           ((uint32_t)digest[2] << 8) | (uint32_t)digest[3];
}

/*
 * Security context management
 */
SecurityContext *
SecurityContextCreate(SecurityLevel defaultLevel)
{
    SecurityContext *context;
    
    context = malloc(sizeof(SecurityContext));
    if (context == NULL) {
        slot_security_warn("context-create", SECURITY_ERROR_CRYPTOGRAPHY_FAILED,
                           "security context allocation failed");
        return NULL;
    }
    
    memset(context, 0, sizeof(SecurityContext));
    context->defaultLevel = defaultLevel;
    context->initialized = false;
    
    /* Initialize hardware fingerprint */
    if (HardwareFingerprintGenerate(&context->hwFingerprint) != SECURITY_SUCCESS) {
        slot_security_warn("context-create", SECURITY_ERROR_CRYPTOGRAPHY_FAILED,
                           "hardware fingerprint generation failed");
        free(context);
        return NULL;
    }
    
    /* Generate master key from hardware fingerprint */
    context->keySize = 32; /* 256-bit key */
    context->masterKey = malloc(context->keySize);
    if (context->masterKey == NULL) {
        slot_security_warn("context-create", SECURITY_ERROR_CRYPTOGRAPHY_FAILED,
                           "master key allocation failed");
        free(context);
        return NULL;
    }
    
    /* Derive master key from hardware fingerprint and compile-time constants */
    uint8_t keyMaterial[64];
    memcpy(keyMaterial, &context->hwFingerprint, sizeof(HardwareFingerprint));
    memcpy(keyMaterial + sizeof(HardwareFingerprint), SECURITY_MAGIC, 
           sizeof(SECURITY_MAGIC));
    
    SecureHashSHA256(keyMaterial, sizeof(keyMaterial), context->masterKey);
    
    /* Securely wipe key material */
    SecureMemoryWipe(keyMaterial, sizeof(keyMaterial));
    
    /* Lock master key in memory */
    SecureMemoryLock(context->masterKey, context->keySize);
    
    context->initialized = true;
    g_securityContext = context;
    
    return context;
}

void
SecurityContextDestroy(SecurityContext *context)
{
    if (context == NULL)
        return;
    
    if (context->masterKey != NULL) {
        SecureMemoryWipe(context->masterKey, context->keySize);
        SecureMemoryUnlock(context->masterKey, context->keySize);
        free(context->masterKey);
    }
    
    SecureMemoryWipe(context, sizeof(SecurityContext));
    free(context);
    
    if (g_securityContext == context)
        g_securityContext = NULL;
}

/*
 * Token generation and validation
 */
SecurityError
TokenGenerate(SecurityContext *context, uint32_t slotId,
             SecurityLevel level, TokenCapability *capability)
{
    SecurityError result;

    if (context == NULL || !context->initialized || capability == NULL)
        return SECURITY_ERROR_CONTEXT_NOT_INITIALIZED;
    
    memset(capability, 0, sizeof(TokenCapability));
    
    capability->slotId = slotId;
    capability->level = level;
    capability->issuedTime = SecureTimestamp();
    
    /* Set expiry time based on security level */
    switch (level) {
        case SECURITY_LEVEL_BASIC:
            capability->expiryTime = capability->issuedTime + 
                                   (SECURITY_DEFAULT_TOKEN_TTL_MS * 1000);
            break;
        case SECURITY_LEVEL_HARDWARE:
            capability->expiryTime = capability->issuedTime + 
                                   (SECURITY_DEFAULT_TOKEN_TTL_MS * 500);
            break;
        case SECURITY_LEVEL_ENCRYPTED:
            capability->expiryTime = capability->issuedTime + 
                                   (SECURITY_DEFAULT_TOKEN_TTL_MS * 200);
            break;
    }
    
    /* Set default permissions */
    capability->canRead = true;
    capability->canWrite = true;
    capability->canTransfer = false;
    
    /* Generate secure token */
    uint8_t tokenMaterial[64];
    
    /* Hardware fingerprint */
    memcpy(tokenMaterial, &context->hwFingerprint, sizeof(HardwareFingerprint));
    
    /* Slot ID and timestamp */
    memcpy(tokenMaterial + sizeof(HardwareFingerprint), &slotId, sizeof(uint32_t));
    memcpy(tokenMaterial + sizeof(HardwareFingerprint) + sizeof(uint32_t), 
           &capability->issuedTime, sizeof(uint64_t));
    
    /* Secure random data */
    result = SecureRandomGenerate(
        tokenMaterial + sizeof(HardwareFingerprint) + sizeof(uint32_t) + sizeof(uint64_t),
        64 - sizeof(HardwareFingerprint) - sizeof(uint32_t) - sizeof(uint64_t)
    );
    
    if (result != SECURITY_SUCCESS)
        return result;
    
    /* Hash the token material */
    result = SecureHashSHA256(tokenMaterial, sizeof(tokenMaterial), 
                            capability->token.tokenData);
    
    if (result != SECURITY_SUCCESS)
        return result;
    
    /* Set token generation and checksum */
    capability->token.generation = ++context->tokensIssued;
    capability->token.checksum = TokenCapabilityChecksum(
        context,
        slotId,
        level,
        &capability->token,
        capability->canRead,
        capability->canWrite,
        capability->canTransfer,
        capability->expiryTime
    );
    
    /* Securely wipe token material */
    SecureMemoryWipe(tokenMaterial, sizeof(tokenMaterial));
    
    return SECURITY_SUCCESS;
}

SecurityError
TokenValidate(SecurityContext *context, uint32_t slotId,
             const TokenCapability *capability)
{
    uint32_t expectedChecksum;

    if (context == NULL || !context->initialized || capability == NULL)
        return SECURITY_ERROR_CONTEXT_NOT_INITIALIZED;
    
    context->tokensValidated++;
    
    /* Check token expiry */
    uint64_t currentTime = SecureTimestamp();
    if (capability->expiryTime > 0 && currentTime > capability->expiryTime) {
        context->validationFailures++;
        return SECURITY_ERROR_TOKEN_EXPIRED;
    }
    
    /* Validate slot ID */
    if (capability->slotId != slotId) {
        context->validationFailures++;
        context->securityViolations++;
        return SECURITY_ERROR_INVALID_TOKEN;
    }
    
    /* Validate hardware binding for HARDWARE level and above */
    if (capability->level >= SECURITY_LEVEL_HARDWARE) {
        HardwareFingerprint currentFingerprint;
        SecurityError result = HardwareFingerprintGenerate(&currentFingerprint);
        if (result != SECURITY_SUCCESS)
            return result;
        
        if (!HardwareFingerprintCompare(&context->hwFingerprint, &currentFingerprint)) {
            context->securityViolations++;
            return SECURITY_ERROR_HARDWARE_MISMATCH;
        }
    }
    
    /* Validate token checksum */
    expectedChecksum = TokenCapabilityChecksum(
        context,
        slotId,
        capability->level,
        &capability->token,
        capability->canRead,
        capability->canWrite,
        capability->canTransfer,
        capability->expiryTime
    );

    if (capability->token.checksum != expectedChecksum) {
        context->validationFailures++;
        context->securityViolations++;
        return SECURITY_ERROR_INVALID_TOKEN;
    }

    return SECURITY_SUCCESS;
}

/*
 * Secure token comparison (constant-time)
 */
bool
TokenCompareSecure(const SecureToken *token1, const SecureToken *token2)
{
    if (token1 == NULL || token2 == NULL)
        return false;
    
    return SecureCompareConstantTime(token1, token2, sizeof(SecureToken));
}

#include "slot_security_crypto_ops.h"
#include "slot_security_context_ops.h"
