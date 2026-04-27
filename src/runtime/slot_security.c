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
#include <iphlpapi.h>
#include <intrin.h>
#pragma comment(lib, "advapi32.lib")
#pragma comment(lib, "iphlpapi.lib")
#elif defined(__linux__)
#include <unistd.h>
#include <fcntl.h>
#include <sys/random.h>
#include <sys/mman.h>
#include <sys/utsname.h>
#include <cpuid.h>
#include <ifaddrs.h>
#include <net/if.h>
#include <netpacket/packet.h>
extern int gethostname(char *name, size_t len);
#endif

#include <openssl/evp.h>
#include <openssl/rand.h>
#include <openssl/sha.h>
#include <openssl/aes.h>

static void
slot_security_warn(const char *op, SecurityError err, const char *reason)
{
    fprintf(stderr,
            "[pgy][slot-security] %s failed: %s (err=%d)\n",
            op != NULL ? op : "<op>",
            reason != NULL ? reason : "unknown",
            (int)err);
}

static uint64_t
SecurityHashBytes64(const void *data, size_t size)
{
    const uint8_t *bytes = (const uint8_t *)data;
    uint64_t hash = 1469598103934665603ULL;

    for (size_t i = 0; i < size; i++) {
        hash ^= (uint64_t)bytes[i];
        hash *= 1099511628211ULL;
    }

    return hash;
}

static uint64_t
SecurityHashString64(const char *text)
{
    return text != NULL ? SecurityHashBytes64(text, strlen(text)) : 0ULL;
}

static void
SecurityFillFallbackIdentity(HardwareFingerprint *fingerprint)
{
#ifdef _WIN32
    char host[256];
    DWORD host_len = (DWORD)sizeof(host);

    if (fingerprint == NULL)
        return;

    if (GetComputerNameA(host, &host_len) && host_len > 0) {
        if (fingerprint->boardId == 0)
            fingerprint->boardId = SecurityHashBytes64(host, host_len) ^ 0x1234567890ABCDEFULL;
        if (fingerprint->macAddress == 0)
            fingerprint->macAddress = SecurityHashBytes64(host, host_len) ^ 0x4d414343ULL;
    }
    if (fingerprint->platformHash == 0)
        fingerprint->platformHash = (uint32_t)SecurityHashBytes64(&host_len, sizeof(host_len));
#elif defined(__linux__)
    char host[256];
    struct utsname uts;
    const char *machine_id_paths[] = {
        "/etc/machine-id",
        "/var/lib/dbus/machine-id"
    };

    if (fingerprint == NULL)
        return;

    if (fingerprint->boardId == 0) {
        for (size_t i = 0; i < sizeof(machine_id_paths) / sizeof(machine_id_paths[0]); i++) {
            FILE *fp = fopen(machine_id_paths[i], "r");
            char buffer[256];
            if (fp == NULL)
                continue;
            if (fgets(buffer, sizeof(buffer), fp) != NULL)
                fingerprint->boardId = SecurityHashString64(buffer);
            fclose(fp);
            if (fingerprint->boardId != 0)
                break;
        }
    }

    if (gethostname(host, sizeof(host)) == 0)
        host[sizeof(host) - 1] = '\0';
    else
        host[0] = '\0';

    if (fingerprint->macAddress == 0 && host[0] != '\0')
        fingerprint->macAddress = SecurityHashString64(host) ^ 0x4d414343ULL;

    if (fingerprint->platformHash == 0) {
        memset(&uts, 0, sizeof(uts));
        if (uname(&uts) == 0) {
            fingerprint->platformHash =
                (uint32_t)SecurityHashString64(uts.sysname) ^
                (uint32_t)SecurityHashString64(uts.release) ^
                (uint32_t)SecurityHashString64(uts.machine);
        } else {
            fingerprint->platformHash = (uint32_t)getuid();
        }
    }
#endif

    if (fingerprint->cpuId == 0) {
        uint64_t entropy = 0;
        (void)SecureRandomGenerate((uint8_t *)&entropy, sizeof(entropy));
        fingerprint->cpuId = entropy != 0 ? entropy : 0x43505546414c4c42ULL;
    }
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
 * Hardware fingerprinting implementation
 */
SecurityError
HardwareFingerprintGenerate(HardwareFingerprint *fingerprint)
{
    SecurityError result = SECURITY_SUCCESS;
    
    if (fingerprint == NULL)
        return SECURITY_ERROR_INVALID_TOKEN;
    
    memset(fingerprint, 0, sizeof(HardwareFingerprint));
    
#ifdef _WIN32
    result = HardwareGetCpuIdWindows(&fingerprint->cpuId);
    if (result != SECURITY_SUCCESS)
        fingerprint->cpuId = 0;
    
    result = HardwareGetBoardIdWindows(&fingerprint->boardId);
    if (result != SECURITY_SUCCESS)
        fingerprint->boardId = 0;
    
    result = HardwareGetMacAddressWindows(&fingerprint->macAddress);
    if (result != SECURITY_SUCCESS)
        fingerprint->macAddress = 0;
    
    fingerprint->platformHash = 0;
#elif defined(__linux__)
    result = HardwareGetCpuIdLinux(&fingerprint->cpuId);
    if (result != SECURITY_SUCCESS)
        fingerprint->cpuId = 0;
    
    result = HardwareGetBoardIdLinux(&fingerprint->boardId);
    if (result != SECURITY_SUCCESS)
        fingerprint->boardId = 0;
    
    result = HardwareGetMacAddressLinux(&fingerprint->macAddress);
    if (result != SECURITY_SUCCESS)
        fingerprint->macAddress = 0;
    
    fingerprint->platformHash = 0;
#endif

    SecurityFillFallbackIdentity(fingerprint);
    
    /* Calculate fingerprint checksum */
    uint32_t checksum = 0;
    uint8_t *data = (uint8_t *)fingerprint;
    for (size_t i = 0; i < sizeof(HardwareFingerprint) - sizeof(uint32_t); i++) {
        checksum ^= data[i];
        checksum = (checksum << 1) | (checksum >> 31);
    }
    fingerprint->checksum = checksum;
    
    return SECURITY_SUCCESS;
}

bool
HardwareFingerprintCompare(const HardwareFingerprint *fp1, 
                          const HardwareFingerprint *fp2)
{
    if (fp1 == NULL || fp2 == NULL)
        return false;
    
    return SecureCompareConstantTime(fp1, fp2, sizeof(HardwareFingerprint));
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

/*
 * Cryptographic utilities
 */
SecurityError
SecureRandomGenerate(uint8_t *buffer, size_t size)
{
    if (buffer == NULL || size == 0)
        return SECURITY_ERROR_INVALID_TOKEN;
    
#ifdef _WIN32
    HCRYPTPROV hCryptProv;
    if (!CryptAcquireContext(&hCryptProv, NULL, NULL, PROV_RSA_FULL, 
                           CRYPT_VERIFYCONTEXT)) {
        return SECURITY_ERROR_CRYPTOGRAPHY_FAILED;
    }
    
    BOOL success = CryptGenRandom(hCryptProv, (DWORD)size, buffer);
    CryptReleaseContext(hCryptProv, 0);
    
    return success ? SECURITY_SUCCESS : SECURITY_ERROR_CRYPTOGRAPHY_FAILED;
#elif defined(__linux__)
    int fd = open("/dev/urandom", O_RDONLY);
    if (fd < 0)
        return SECURITY_ERROR_CRYPTOGRAPHY_FAILED;
    
    ssize_t bytesRead = read(fd, buffer, size);
    close(fd);
    
    return (bytesRead == (ssize_t)size) ? SECURITY_SUCCESS : 
                                        SECURITY_ERROR_CRYPTOGRAPHY_FAILED;
#else
    /* Fallback to OpenSSL */
    return (RAND_bytes(buffer, (int)size) == 1) ? SECURITY_SUCCESS : 
                                                 SECURITY_ERROR_CRYPTOGRAPHY_FAILED;
#endif
}

SecurityError
SecureHashSHA256(const uint8_t *input, size_t inputSize, uint8_t output[32])
{
    EVP_MD_CTX *ctx;
    unsigned int digestLen = 0;

    if (input == NULL || output == NULL || inputSize == 0)
        return SECURITY_ERROR_INVALID_TOKEN;

    ctx = EVP_MD_CTX_new();
    if (ctx == NULL)
        return SECURITY_ERROR_CRYPTOGRAPHY_FAILED;

    if (EVP_DigestInit_ex(ctx, EVP_sha256(), NULL) != 1) {
        EVP_MD_CTX_free(ctx);
        return SECURITY_ERROR_CRYPTOGRAPHY_FAILED;
    }

    if (EVP_DigestUpdate(ctx, input, inputSize) != 1) {
        EVP_MD_CTX_free(ctx);
        return SECURITY_ERROR_CRYPTOGRAPHY_FAILED;
    }

    if (EVP_DigestFinal_ex(ctx, output, &digestLen) != 1 || digestLen != 32) {
        EVP_MD_CTX_free(ctx);
        return SECURITY_ERROR_CRYPTOGRAPHY_FAILED;
    }

    EVP_MD_CTX_free(ctx);
    return SECURITY_SUCCESS;
}

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

SecurityError
TokenEncrypt(SecurityContext *context, const SecureToken *plainToken,
             EncryptedToken *encryptedToken)
{
    uint8_t iv[16] = {0};

    if (context == NULL || plainToken == NULL || encryptedToken == NULL ||
        context->masterKey == NULL) {
        slot_security_warn("token-encrypt", SECURITY_ERROR_CONTEXT_NOT_INITIALIZED,
                           "context, token, output, or master key is null");
        return SECURITY_ERROR_CONTEXT_NOT_INITIALIZED;
    }

    if (SecureRandomGenerate(iv, SECURITY_IV_SIZE) != SECURITY_SUCCESS) {
        slot_security_warn("token-encrypt", SECURITY_ERROR_CRYPTOGRAPHY_FAILED,
                           "iv generation failed");
        return SECURITY_ERROR_CRYPTOGRAPHY_FAILED;
    }

    memcpy(encryptedToken->encryptedToken, iv, SECURITY_IV_SIZE);
    if (AES256Encrypt(context->masterKey, iv, (const uint8_t *)plainToken,
                      sizeof(*plainToken), encryptedToken->encryptedToken + SECURITY_IV_SIZE,
                      encryptedToken->authTag) != SECURITY_SUCCESS) {
        slot_security_warn("token-encrypt", SECURITY_ERROR_CRYPTOGRAPHY_FAILED,
                           "aes-256 encryption failed");
        return SECURITY_ERROR_CRYPTOGRAPHY_FAILED;
    }

    encryptedToken->keyVersion = SECURITY_VERSION;
    return SECURITY_SUCCESS;
}

SecurityError
TokenDecrypt(SecurityContext *context, const EncryptedToken *encryptedToken,
             SecureToken *plainToken)
{
    uint8_t iv[16] = {0};
    SecurityError result;

    if (context == NULL || encryptedToken == NULL || plainToken == NULL ||
        context->masterKey == NULL) {
        slot_security_warn("token-decrypt", SECURITY_ERROR_CONTEXT_NOT_INITIALIZED,
                           "context, encrypted token, output, or master key is null");
        return SECURITY_ERROR_CONTEXT_NOT_INITIALIZED;
    }

    memcpy(iv, encryptedToken->encryptedToken, SECURITY_IV_SIZE);
    result = AES256Decrypt(context->masterKey, iv,
                           encryptedToken->encryptedToken + SECURITY_IV_SIZE,
                           sizeof(*plainToken), encryptedToken->authTag,
                           (uint8_t *)plainToken);
    if (result != SECURITY_SUCCESS) {
        slot_security_warn("token-decrypt", result,
                           "aes-256 decryption or auth verification failed");
    }
    return result;
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
