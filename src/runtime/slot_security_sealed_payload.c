/*
 * Copyright (c) 2025 Pergyra Language Project
 * All rights reserved.
 *
 * Secure slot sealed payload storage.
 */

#include "slot_security.h"
#include "pgy_runtime_security_log.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void
slot_sealed_payload_warn(const char *op, SecurityError err, const char *reason)
{
    fputs("{\"component\":\"slot-security\",\"operation\":", stderr);
    pgy_runtime_fprint_json_string(stderr, op != NULL ? op : "<op>");
    fputs(",\"event\":", stderr);
    pgy_runtime_fprint_json_string(stderr,
                                   err == SECURITY_SUCCESS
                                       ? "operation_succeeded"
                                       : "operation_failed");
    fputs(",\"reason\":", stderr);
    pgy_runtime_fprint_json_string(stderr,
                                   reason != NULL
                                       ? reason
                                       : (err == SECURITY_SUCCESS
                                             ? "ok"
                                             : "unknown"));
    fprintf(stderr, ",\"err\":%d}\n", (int)err);
}

static SecurityError
SecureComputePayloadMac(SecurityContext *context, uint32_t slotId,
                        uint32_t generation, bool shadowCopy,
                        const uint8_t nonce[16], const uint8_t authTag[16],
                        const SecureSlotPolicy *policy,
                        const uint8_t *payload, size_t size,
                        uint8_t outMac[32])
{
    uint8_t *material;
    size_t totalSize;
    size_t offset = 0;
    uint8_t shadowFlag = shadowCopy ? 1u : 0u;
    uint8_t obfuscateFlag;
    uint8_t policyShadowFlag;
    uint8_t isolateFlag;
    uint8_t auditFlag;
    uint32_t storageMode;
    size_t policyFlagSize;

    if (context == NULL || context->masterKey == NULL || nonce == NULL ||
        authTag == NULL || policy == NULL || (payload == NULL && size > 0) ||
        outMac == NULL) {
        return SECURITY_ERROR_CONTEXT_NOT_INITIALIZED;
    }

    storageMode = (uint32_t)policy->storageMode;
    obfuscateFlag = policy->obfuscateInMemory ? 1u : 0u;
    policyShadowFlag = policy->shadowCopy ? 1u : 0u;
    isolateFlag = policy->isolateShadowCopy ? 1u : 0u;
    auditFlag = policy->auditReads ? 1u : 0u;
    policyFlagSize = sizeof(obfuscateFlag) + sizeof(policyShadowFlag) +
                     sizeof(isolateFlag) + sizeof(auditFlag);

    if (size > SIZE_MAX - (16 + 16 + sizeof(slotId) + sizeof(generation) +
                           sizeof(shadowFlag) + sizeof(storageMode) +
                           policyFlagSize + sizeof(size)))
        return SECURITY_ERROR_CRYPTOGRAPHY_FAILED;

    totalSize = 16 + 16 + sizeof(slotId) + sizeof(generation) +
                sizeof(shadowFlag) + sizeof(storageMode) + policyFlagSize +
                sizeof(size) + size;
    material = malloc(totalSize);
    if (material == NULL)
        return SECURITY_ERROR_CRYPTOGRAPHY_FAILED;

    memcpy(material + offset, nonce, 16);
    offset += 16;
    memcpy(material + offset, authTag, 16);
    offset += 16;
    memcpy(material + offset, &slotId, sizeof(slotId));
    offset += sizeof(slotId);
    memcpy(material + offset, &generation, sizeof(generation));
    offset += sizeof(generation);
    memcpy(material + offset, &shadowFlag, sizeof(shadowFlag));
    offset += sizeof(shadowFlag);
    memcpy(material + offset, &storageMode, sizeof(storageMode));
    offset += sizeof(storageMode);
    material[offset++] = obfuscateFlag;
    material[offset++] = policyShadowFlag;
    material[offset++] = isolateFlag;
    material[offset++] = auditFlag;
    memcpy(material + offset, &size, sizeof(size));
    offset += sizeof(size);
    if (size > 0) {
        memcpy(material + offset, payload, size);
        offset += size;
    }

    if (SecureHmacSHA256(context->masterKey, context->keySize,
                         material, offset, outMac) != SECURITY_SUCCESS) {
        SecureMemoryWipe(material, totalSize);
        free(material);
        return SECURITY_ERROR_CRYPTOGRAPHY_FAILED;
    }

    SecureMemoryWipe(material, totalSize);
    free(material);
    return SECURITY_SUCCESS;
}

static SecurityError
SecureVerifyPayloadMac(SecurityContext *context, uint32_t slotId,
                       uint32_t generation, bool shadowCopy,
                       const uint8_t nonce[16], const uint8_t authTag[16],
                       const SecureSlotPolicy *policy,
                       const uint8_t *payload, size_t size,
                       const uint8_t expectedMac[32])
{
    uint8_t computed[32];

    if (SecureComputePayloadMac(context, slotId, generation, shadowCopy, nonce,
                                authTag, policy, payload, size, computed) !=
        SECURITY_SUCCESS) {
        return SECURITY_ERROR_CRYPTOGRAPHY_FAILED;
    }

    if (!SecureCompareConstantTime(expectedMac, computed, sizeof(computed)))
        return SECURITY_ERROR_INVALID_TOKEN;

    return SECURITY_SUCCESS;
}

SecureSlotPolicy
SecurityPolicyForLevel(SecurityLevel level)
{
    SecureSlotPolicy policy;

    memset(&policy, 0, sizeof(policy));
    policy.storageMode = SECURE_SLOT_STORAGE_SEALED;
    policy.obfuscateInMemory = true;

    switch (level) {
    case SECURITY_LEVEL_BASIC:
        policy.shadowCopy = false;
        policy.isolateShadowCopy = false;
        policy.auditReads = false;
        break;
    case SECURITY_LEVEL_HARDWARE:
        policy.shadowCopy = true;
        policy.isolateShadowCopy = true;
        policy.auditReads = true;
        break;
    case SECURITY_LEVEL_ENCRYPTED:
    default:
        policy.shadowCopy = true;
        policy.isolateShadowCopy = true;
        policy.auditReads = true;
        break;
    }

    return policy;
}

void
SecureSealedPayloadInit(SecureSealedPayload *payload)
{
    if (payload == NULL)
        return;

    memset(payload, 0, sizeof(*payload));
    payload->policy.storageMode = SECURE_SLOT_STORAGE_NONE;
}

void
SecureSealedPayloadDestroy(SecureSealedPayload *payload)
{
    if (payload == NULL)
        return;

    if (payload->primaryData != NULL) {
        SecureMemoryWipe(payload->primaryData, payload->size);
        free(payload->primaryData);
    }
    if (payload->shadowData != NULL) {
        SecureMemoryWipe(payload->shadowData, payload->size);
        free(payload->shadowData);
    }

    SecureMemoryWipe(payload, sizeof(*payload));
    payload->policy.storageMode = SECURE_SLOT_STORAGE_NONE;
}

SecurityError
SecureSealedPayloadSeal(SecurityContext *context, uint32_t slotId,
                        uint32_t generation, const void *data, size_t size,
                        const SecureSlotPolicy *policy,
                        SecureSealedPayload *payload)
{
    SecureSlotPolicy effectivePolicy;
    const uint8_t *input = (const uint8_t *)data;
    SecurityError result;

    if (context == NULL || data == NULL || payload == NULL) {
        slot_sealed_payload_warn("sealed-payload-seal", SECURITY_ERROR_CONTEXT_NOT_INITIALIZED,
                                 "context, input data, or payload is null");
        return SECURITY_ERROR_CONTEXT_NOT_INITIALIZED;
    }

    effectivePolicy = policy != NULL ? *policy : SecurityPolicyForLevel(context->defaultLevel);

    SecureSealedPayloadDestroy(payload);
    SecureSealedPayloadInit(payload);

    if (size == 0) {
        payload->policy = effectivePolicy;
        payload->initialized = true;
        return SECURITY_SUCCESS;
    }

    payload->primaryData = malloc(size);
    if (payload->primaryData == NULL) {
        slot_sealed_payload_warn("sealed-payload-seal", SECURITY_ERROR_CRYPTOGRAPHY_FAILED,
                                 "primary payload allocation failed");
        return SECURITY_ERROR_CRYPTOGRAPHY_FAILED;
    }
    payload->size = size;

    if (effectivePolicy.shadowCopy) {
        payload->shadowData = malloc(size);
        if (payload->shadowData == NULL) {
            slot_sealed_payload_warn("sealed-payload-seal", SECURITY_ERROR_CRYPTOGRAPHY_FAILED,
                                     "shadow payload allocation failed");
            SecureSealedPayloadDestroy(payload);
            return SECURITY_ERROR_CRYPTOGRAPHY_FAILED;
        }
    }

    if (SecureRandomGenerate(payload->nonce, sizeof(payload->nonce)) != SECURITY_SUCCESS) {
        slot_sealed_payload_warn("sealed-payload-seal", SECURITY_ERROR_CRYPTOGRAPHY_FAILED,
                                 "nonce generation failed");
        SecureSealedPayloadDestroy(payload);
        return SECURITY_ERROR_CRYPTOGRAPHY_FAILED;
    }

    if (effectivePolicy.obfuscateInMemory) {
        result = AES256Encrypt(context->masterKey, payload->nonce, input, size,
                               payload->primaryData, payload->primaryAuthTag);
        if (result != SECURITY_SUCCESS) {
            slot_sealed_payload_warn("sealed-payload-seal", result,
                                     "primary payload encryption failed");
            SecureSealedPayloadDestroy(payload);
            return result;
        }
        if (payload->shadowData != NULL) {
            result = AES256Encrypt(context->masterKey, payload->nonce, input, size,
                                   payload->shadowData, payload->shadowAuthTag);
            if (result != SECURITY_SUCCESS) {
                slot_sealed_payload_warn("sealed-payload-seal", result,
                                         "shadow payload encryption failed");
                SecureSealedPayloadDestroy(payload);
                return result;
            }
        }
    } else {
        memcpy(payload->primaryData, input, size);
        if (payload->shadowData != NULL)
            memcpy(payload->shadowData, input, size);
    }

    if (SecureComputePayloadMac(context, slotId, generation, false, payload->nonce,
                                payload->primaryAuthTag, &effectivePolicy,
                                payload->primaryData, size, payload->primaryMac) !=
        SECURITY_SUCCESS) {
        slot_sealed_payload_warn("sealed-payload-seal", SECURITY_ERROR_CRYPTOGRAPHY_FAILED,
                                 "primary payload mac computation failed");
        SecureSealedPayloadDestroy(payload);
        return SECURITY_ERROR_CRYPTOGRAPHY_FAILED;
    }

    if (payload->shadowData != NULL &&
        SecureComputePayloadMac(context, slotId, generation, true, payload->nonce,
                                payload->shadowAuthTag, &effectivePolicy,
                                payload->shadowData, size, payload->shadowMac) !=
        SECURITY_SUCCESS) {
        slot_sealed_payload_warn("sealed-payload-seal", SECURITY_ERROR_CRYPTOGRAPHY_FAILED,
                                 "shadow payload mac computation failed");
        SecureSealedPayloadDestroy(payload);
        return SECURITY_ERROR_CRYPTOGRAPHY_FAILED;
    }

    payload->policy = effectivePolicy;
    payload->initialized = true;
    return SECURITY_SUCCESS;
}

SecurityError
SecureSealedPayloadOpen(SecurityContext *context, uint32_t slotId,
                        uint32_t generation, SecureSealedPayload *payload,
                        void *buffer, size_t bufferSize, size_t *bytesRead,
                        bool *usedShadowRecovery)
{
    SecurityError result;
    bool verifiedPrimary = false;
    bool verifiedShadow = false;
    uint8_t *plain;
    size_t copySize;

    if (usedShadowRecovery != NULL)
        *usedShadowRecovery = false;

    if (context == NULL || payload == NULL || buffer == NULL || !payload->initialized) {
        slot_sealed_payload_warn("sealed-payload-open", SECURITY_ERROR_CONTEXT_NOT_INITIALIZED,
                                 "context, payload, buffer missing or payload not initialized");
        return SECURITY_ERROR_CONTEXT_NOT_INITIALIZED;
    }

    result = SecureVerifyPayloadMac(context, slotId, generation, false,
                                    payload->nonce, payload->primaryAuthTag,
                                    &payload->policy,
                                    payload->primaryData,
                                    payload->size, payload->primaryMac);
    verifiedPrimary = (result == SECURITY_SUCCESS);

    if (!verifiedPrimary && payload->shadowData != NULL) {
        result = SecureVerifyPayloadMac(context, slotId, generation, true,
                                        payload->nonce, payload->shadowAuthTag,
                                        &payload->policy,
                                        payload->shadowData,
                                        payload->size, payload->shadowMac);
        verifiedShadow = (result == SECURITY_SUCCESS);
        if (!verifiedShadow) {
            slot_sealed_payload_warn("sealed-payload-open", result,
                                     "primary and shadow payload mac verification failed");
            return result;
        }

        memcpy(payload->primaryData, payload->shadowData, payload->size);
        memcpy(payload->primaryAuthTag, payload->shadowAuthTag,
               sizeof(payload->primaryAuthTag));
        memcpy(payload->primaryMac, payload->shadowMac, sizeof(payload->primaryMac));
        if (usedShadowRecovery != NULL)
            *usedShadowRecovery = true;
        slot_sealed_payload_warn("sealed-payload-open", SECURITY_SUCCESS,
                                 "primary payload recovered from verified shadow copy");
    } else if (!verifiedPrimary) {
        slot_sealed_payload_warn("sealed-payload-open", result,
                                 "primary payload mac verification failed");
        return result;
    }

    plain = malloc(payload->size);
    if (plain == NULL) {
        slot_sealed_payload_warn("sealed-payload-open", SECURITY_ERROR_CRYPTOGRAPHY_FAILED,
                                 "plaintext buffer allocation failed");
        return SECURITY_ERROR_CRYPTOGRAPHY_FAILED;
    }

    if (payload->policy.obfuscateInMemory) {
        result = AES256Decrypt(context->masterKey, payload->nonce,
                               payload->primaryData, payload->size,
                               payload->primaryAuthTag, plain);
        if (result != SECURITY_SUCCESS) {
            slot_sealed_payload_warn("sealed-payload-open", result,
                                     "payload decrypt failed");
            SecureMemoryWipe(plain, payload->size);
            free(plain);
            return result;
        }
    } else {
        memcpy(plain, payload->primaryData, payload->size);
    }

    copySize = payload->size < bufferSize ? payload->size : bufferSize;
    memcpy(buffer, plain, copySize);
    if (bytesRead != NULL)
        *bytesRead = copySize;

    SecureMemoryWipe(plain, payload->size);
    free(plain);
    return SECURITY_SUCCESS;
}

const uint8_t *
SecureSealedPayloadPrimaryBytes(const SecureSealedPayload *payload)
{
    return payload != NULL ? payload->primaryData : NULL;
}

const uint8_t *
SecureSealedPayloadShadowBytes(const SecureSealedPayload *payload)
{
    return payload != NULL ? payload->shadowData : NULL;
}
