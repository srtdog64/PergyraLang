/*
 * Copyright (c) 2025 Pergyra Language Project
 * All rights reserved.
 *
 * Secure slot sealed payload storage.
 */

#include "slot_security.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void
slot_sealed_payload_warn(const char *op, SecurityError err, const char *reason)
{
    if (err == SECURITY_SUCCESS) {
        fprintf(stderr,
                "[pgy][slot-security] %s: %s\n",
                op != NULL ? op : "<op>",
                reason != NULL ? reason : "ok");
        return;
    }

    fprintf(stderr,
            "[pgy][slot-security] %s failed: %s (err=%d)\n",
            op != NULL ? op : "<op>",
            reason != NULL ? reason : "unknown",
            (int)err);
}

static SecurityError
SecureDeriveMaskBlock(SecurityContext *context, uint32_t slotId,
                      uint32_t generation, const uint8_t nonce[16],
                      uint32_t counter, uint8_t out[32])
{
    uint8_t material[64];
    size_t offset = 0;

    if (context == NULL || context->masterKey == NULL || nonce == NULL || out == NULL)
        return SECURITY_ERROR_CONTEXT_NOT_INITIALIZED;

    if (context->keySize + 28 > sizeof(material)) {
        return SECURITY_ERROR_CRYPTOGRAPHY_FAILED;
    }

    memcpy(material + offset, context->masterKey, context->keySize);
    offset += context->keySize;
    memcpy(material + offset, nonce, 16);
    offset += 16;
    memcpy(material + offset, &slotId, sizeof(slotId));
    offset += sizeof(slotId);
    memcpy(material + offset, &generation, sizeof(generation));
    offset += sizeof(generation);
    memcpy(material + offset, &counter, sizeof(counter));
    offset += sizeof(counter);

    return SecureHashSHA256(material, offset, out);
}

static SecurityError
SecureXorPayload(SecurityContext *context, uint32_t slotId, uint32_t generation,
                 const uint8_t nonce[16], const uint8_t *input, uint8_t *output,
                 size_t size)
{
    uint8_t mask[32];
    size_t offset = 0;
    uint32_t counter = 0;

    while (offset < size) {
        size_t blockSize;
        if (SecureDeriveMaskBlock(context, slotId, generation, nonce, counter, mask) !=
            SECURITY_SUCCESS) {
            return SECURITY_ERROR_CRYPTOGRAPHY_FAILED;
        }

        blockSize = (size - offset) < sizeof(mask) ? (size - offset) : sizeof(mask);
        for (size_t i = 0; i < blockSize; i++)
            output[offset + i] = input[offset + i] ^ mask[i];

        offset += blockSize;
        counter++;
    }

    return SECURITY_SUCCESS;
}

static SecurityError
SecureComputePayloadMac(SecurityContext *context, uint32_t slotId,
                        uint32_t generation, bool shadowCopy,
                        const uint8_t nonce[16], const uint8_t *payload,
                        size_t size, uint8_t outMac[32])
{
    uint8_t *material;
    size_t totalSize;
    size_t offset = 0;
    uint8_t shadowFlag = shadowCopy ? 1u : 0u;

    if (context == NULL || context->masterKey == NULL || nonce == NULL ||
        payload == NULL || outMac == NULL) {
        return SECURITY_ERROR_CONTEXT_NOT_INITIALIZED;
    }

    totalSize = context->keySize + 16 + sizeof(slotId) + sizeof(generation) +
                sizeof(shadowFlag) + sizeof(size) + size;
    material = malloc(totalSize);
    if (material == NULL)
        return SECURITY_ERROR_CRYPTOGRAPHY_FAILED;

    memcpy(material + offset, context->masterKey, context->keySize);
    offset += context->keySize;
    memcpy(material + offset, nonce, 16);
    offset += 16;
    memcpy(material + offset, &slotId, sizeof(slotId));
    offset += sizeof(slotId);
    memcpy(material + offset, &generation, sizeof(generation));
    offset += sizeof(generation);
    memcpy(material + offset, &shadowFlag, sizeof(shadowFlag));
    offset += sizeof(shadowFlag);
    memcpy(material + offset, &size, sizeof(size));
    offset += sizeof(size);
    memcpy(material + offset, payload, size);
    offset += size;

    if (SecureHashSHA256(material, offset, outMac) != SECURITY_SUCCESS) {
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
                       const uint8_t nonce[16], const uint8_t *payload,
                       size_t size, const uint8_t expectedMac[32])
{
    uint8_t computed[32];

    if (SecureComputePayloadMac(context, slotId, generation, shadowCopy, nonce,
                                payload, size, computed) != SECURITY_SUCCESS) {
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
        result = SecureXorPayload(context, slotId, generation, payload->nonce,
                                  input, payload->primaryData, size);
        if (result != SECURITY_SUCCESS) {
            slot_sealed_payload_warn("sealed-payload-seal", result,
                                     "primary payload obfuscation failed");
            SecureSealedPayloadDestroy(payload);
            return result;
        }
        if (payload->shadowData != NULL) {
            result = SecureXorPayload(context, slotId, generation, payload->nonce,
                                      input, payload->shadowData, size);
            if (result != SECURITY_SUCCESS) {
                slot_sealed_payload_warn("sealed-payload-seal", result,
                                         "shadow payload obfuscation failed");
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
                                payload->primaryData, size,
                                payload->primaryMac) != SECURITY_SUCCESS) {
        slot_sealed_payload_warn("sealed-payload-seal", SECURITY_ERROR_CRYPTOGRAPHY_FAILED,
                                 "primary payload mac computation failed");
        SecureSealedPayloadDestroy(payload);
        return SECURITY_ERROR_CRYPTOGRAPHY_FAILED;
    }

    if (payload->shadowData != NULL &&
        SecureComputePayloadMac(context, slotId, generation, true, payload->nonce,
                                payload->shadowData, size,
                                payload->shadowMac) != SECURITY_SUCCESS) {
        slot_sealed_payload_warn("sealed-payload-seal", SECURITY_ERROR_CRYPTOGRAPHY_FAILED,
                                 "shadow payload mac computation failed");
        SecureSealedPayloadDestroy(payload);
        return SECURITY_ERROR_CRYPTOGRAPHY_FAILED;
    }

    payload->size = size;
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
                                    payload->nonce, payload->primaryData,
                                    payload->size, payload->primaryMac);
    verifiedPrimary = (result == SECURITY_SUCCESS);

    if (!verifiedPrimary && payload->shadowData != NULL) {
        result = SecureVerifyPayloadMac(context, slotId, generation, true,
                                        payload->nonce, payload->shadowData,
                                        payload->size, payload->shadowMac);
        verifiedShadow = (result == SECURITY_SUCCESS);
        if (!verifiedShadow) {
            slot_sealed_payload_warn("sealed-payload-open", result,
                                     "primary and shadow payload mac verification failed");
            return result;
        }

        memcpy(payload->primaryData, payload->shadowData, payload->size);
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
        result = SecureXorPayload(context, slotId, generation, payload->nonce,
                                  payload->primaryData, plain, payload->size);
        if (result != SECURITY_SUCCESS) {
            slot_sealed_payload_warn("sealed-payload-open", result,
                                     "payload deobfuscation failed");
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
