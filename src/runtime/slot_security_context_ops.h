#ifndef PERGYRA_SLOT_SECURITY_CONTEXT_OPS_H
#define PERGYRA_SLOT_SECURITY_CONTEXT_OPS_H

/*
 * Utility functions
 */
uint32_t
HardwareFingerprintHash(const HardwareFingerprint *fingerprint)
{
    if (fingerprint == NULL)
        return 0;

    uint32_t hash = 0x12345678;
    const uint8_t *data = (const uint8_t *)fingerprint;

    for (size_t i = 0; i < sizeof(HardwareFingerprint); i++) {
        hash ^= data[i];
        hash = (hash << 1) | (hash >> 31);
    }

    return hash;
}

SecurityError
SecurityContextInitialize(SecurityContext *context)
{
    uint8_t keyMaterial[64];

    if (context == NULL) {
        slot_security_warn("context-initialize", SECURITY_ERROR_CONTEXT_NOT_INITIALIZED,
                           "context is null");
        return SECURITY_ERROR_CONTEXT_NOT_INITIALIZED;
    }

    if (context->initialized)
        return SECURITY_SUCCESS;

    if (HardwareFingerprintGenerate(&context->hwFingerprint) != SECURITY_SUCCESS) {
        slot_security_warn("context-initialize", SECURITY_ERROR_CRYPTOGRAPHY_FAILED,
                           "hardware fingerprint generation failed");
        return SECURITY_ERROR_CRYPTOGRAPHY_FAILED;
    }

    if (context->masterKey == NULL) {
        context->keySize = 32;
        context->masterKey = malloc(context->keySize);
        if (context->masterKey == NULL) {
            slot_security_warn("context-initialize", SECURITY_ERROR_CRYPTOGRAPHY_FAILED,
                               "master key allocation failed");
            return SECURITY_ERROR_CRYPTOGRAPHY_FAILED;
        }
    }

    memcpy(keyMaterial, &context->hwFingerprint, sizeof(HardwareFingerprint));
    memcpy(keyMaterial + sizeof(HardwareFingerprint), SECURITY_MAGIC,
           sizeof(SECURITY_MAGIC));
    if (SecureHashSHA256(keyMaterial, sizeof(keyMaterial), context->masterKey) !=
        SECURITY_SUCCESS) {
        SecureMemoryWipe(keyMaterial, sizeof(keyMaterial));
        slot_security_warn("context-initialize", SECURITY_ERROR_CRYPTOGRAPHY_FAILED,
                           "master key derivation failed");
        return SECURITY_ERROR_CRYPTOGRAPHY_FAILED;
    }

    SecureMemoryWipe(keyMaterial, sizeof(keyMaterial));
    context->initialized = true;
    return SECURITY_SUCCESS;
}

SecurityError
SecurityContextUpdateHardware(SecurityContext *context)
{
    if (context == NULL)
        return SECURITY_ERROR_CONTEXT_NOT_INITIALIZED;

    context->initialized = false;
    return SecurityContextInitialize(context);
}

SecurityError
TokenRevoke(SecurityContext *context, uint32_t slotId)
{
    (void)slotId;

    if (context == NULL || !context->initialized)
        return SECURITY_ERROR_CONTEXT_NOT_INITIALIZED;

    return SECURITY_SUCCESS;
}

SecurityError
TokenRefresh(SecurityContext *context, TokenCapability *capability)
{
    bool canRead;
    bool canWrite;
    bool canTransfer;

    if (context == NULL || capability == NULL)
        return SECURITY_ERROR_CONTEXT_NOT_INITIALIZED;

    canRead = capability->canRead;
    canWrite = capability->canWrite;
    canTransfer = capability->canTransfer;

    if (TokenGenerate(context, capability->slotId, capability->level, capability) !=
        SECURITY_SUCCESS) {
        return SECURITY_ERROR_CRYPTOGRAPHY_FAILED;
    }

    capability->canRead = canRead;
    capability->canWrite = canWrite;
    capability->canTransfer = canTransfer;
    capability->token.checksum = TokenCapabilityChecksum(
        context,
        capability->slotId,
        capability->level,
        &capability->token,
        capability->canRead,
        capability->canWrite,
        capability->canTransfer,
        capability->expiryTime
    );
    return SECURITY_SUCCESS;
}

void
SecurityAuditLog(SecurityContext *context, const char *event, const char *details)
{
    (void)context;
    fprintf(stderr, "[SECURITY-AUDIT] event=%s details=%s\n",
            event != NULL ? event : "n/a",
            details != NULL ? details : "n/a");
}

void
SecurityPrintStatistics(const SecurityContext *context)
{
    if (context == NULL)
        return;

    printf("Tokens issued: %llu\n", (unsigned long long)context->tokensIssued);
    printf("Tokens validated: %llu\n",
           (unsigned long long)context->tokensValidated);
    printf("Validation failures: %llu\n",
           (unsigned long long)context->validationFailures);
    printf("Security violations: %llu\n",
           (unsigned long long)context->securityViolations);
}

bool
SecurityDetectAnomalies(const SecurityContext *context)
{
    if (context == NULL)
        return false;

    return context->validationFailures > 0 || context->securityViolations > 0;
}

#endif /* PERGYRA_SLOT_SECURITY_CONTEXT_OPS_H */
