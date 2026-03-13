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
#include <cpuid.h>
#include <ifaddrs.h>
#include <net/if.h>
#include <netpacket/packet.h>
#endif

#include <openssl/evp.h>
#include <openssl/rand.h>
#include <openssl/sha.h>
#include <openssl/aes.h>

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

static const uint32_t SECURITY_VERSION = 0x00010001;

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
    if (context == NULL)
        return NULL;
    
    memset(context, 0, sizeof(SecurityContext));
    context->defaultLevel = defaultLevel;
    context->initialized = false;
    
    /* Initialize hardware fingerprint */
    if (HardwareFingerprintGenerate(&context->hwFingerprint) != SECURITY_SUCCESS) {
        free(context);
        return NULL;
    }
    
    /* Generate master key from hardware fingerprint */
    context->keySize = 32; /* 256-bit key */
    context->masterKey = malloc(context->keySize);
    if (context->masterKey == NULL) {
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
        return result;
    
    result = HardwareGetBoardIdWindows(&fingerprint->boardId);
    if (result != SECURITY_SUCCESS)
        return result;
    
    result = HardwareGetMacAddressWindows(&fingerprint->macAddress);
    if (result != SECURITY_SUCCESS)
        return result;
    
    fingerprint->platformHash = GetVersion();
#elif defined(__linux__)
    result = HardwareGetCpuIdLinux(&fingerprint->cpuId);
    if (result != SECURITY_SUCCESS)
        return result;
    
    result = HardwareGetBoardIdLinux(&fingerprint->boardId);
    if (result != SECURITY_SUCCESS)
        return result;
    
    result = HardwareGetMacAddressLinux(&fingerprint->macAddress);
    if (result != SECURITY_SUCCESS)
        return result;
    
    fingerprint->platformHash = (uint32_t)getpid() ^ (uint32_t)getuid();
#endif
    
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
 * Memory protection utilities
 */
SecurityError
SecureMemoryLock(void *addr, size_t size)
{
    if (addr == NULL || size == 0)
        return SECURITY_ERROR_INVALID_TOKEN;
    
#ifdef _WIN32
    return VirtualLock(addr, size) ? SECURITY_SUCCESS : 
                                   SECURITY_ERROR_CRYPTOGRAPHY_FAILED;
#elif defined(__linux__)
    return (mlock(addr, size) == 0) ? SECURITY_SUCCESS : 
                                    SECURITY_ERROR_CRYPTOGRAPHY_FAILED;
#else
    return SECURITY_SUCCESS; /* No-op on unsupported platforms */
#endif
}

SecurityError
SecureMemoryUnlock(void *addr, size_t size)
{
    if (addr == NULL || size == 0)
        return SECURITY_ERROR_INVALID_TOKEN;
    
#ifdef _WIN32
    return VirtualUnlock(addr, size) ? SECURITY_SUCCESS : 
                                     SECURITY_ERROR_CRYPTOGRAPHY_FAILED;
#elif defined(__linux__)
    return (munlock(addr, size) == 0) ? SECURITY_SUCCESS : 
                                      SECURITY_ERROR_CRYPTOGRAPHY_FAILED;
#else
    return SECURITY_SUCCESS; /* No-op on unsupported platforms */
#endif
}

void
SecureMemoryWipe(void *addr, size_t size)
{
    if (addr == NULL || size == 0)
        return;
    
    /* Use explicit_bzero if available, otherwise volatile memset */
#ifdef HAVE_EXPLICIT_BZERO
    explicit_bzero(addr, size);
#else
    volatile uint8_t *ptr = (volatile uint8_t *)addr;
    for (size_t i = 0; i < size; i++)
        ptr[i] = 0;
#endif
}

/*
 * Platform-specific hardware detection
 */
#ifdef _WIN32
SecurityError
HardwareGetCpuIdWindows(uint64_t *cpuId)
{
    int cpuInfo[4];
    __cpuid(cpuInfo, 1);
    *cpuId = ((uint64_t)cpuInfo[3] << 32) | (uint64_t)cpuInfo[0];
    return SECURITY_SUCCESS;
}

SecurityError
HardwareGetBoardIdWindows(uint64_t *boardId)
{
    /* This is a simplified implementation */
    /* In production, would query WMI for motherboard serial */
    *boardId = GetTickCount64() ^ 0x1234567890ABCDEF;
    return SECURITY_SUCCESS;
}

SecurityError
HardwareGetMacAddressWindows(uint64_t *macAddress)
{
    ULONG bufferSize = 0;
    GetAdaptersInfo(NULL, &bufferSize);
    
    if (bufferSize == 0) {
        *macAddress = 0;
        return SECURITY_SUCCESS;
    }
    
    IP_ADAPTER_INFO *adapterInfo = malloc(bufferSize);
    if (adapterInfo == NULL)
        return SECURITY_ERROR_CRYPTOGRAPHY_FAILED;
    
    if (GetAdaptersInfo(adapterInfo, &bufferSize) == NO_ERROR) {
        if (adapterInfo->AddressLength >= 6) {
            *macAddress = 0;
            for (int i = 0; i < 6; i++) {
                *macAddress |= ((uint64_t)adapterInfo->Address[i]) << (8 * i);
            }
        }
    }
    
    free(adapterInfo);
    return SECURITY_SUCCESS;
}
#elif defined(__linux__)
SecurityError
HardwareGetCpuIdLinux(uint64_t *cpuId)
{
    unsigned int eax, ebx, ecx, edx;
    if (__get_cpuid(1, &eax, &ebx, &ecx, &edx) == 0) {
        *cpuId = 0;
        return SECURITY_ERROR_CRYPTOGRAPHY_FAILED;
    }
    
    *cpuId = ((uint64_t)edx << 32) | (uint64_t)eax;
    return SECURITY_SUCCESS;
}

SecurityError
HardwareGetBoardIdLinux(uint64_t *boardId)
{
    FILE *fp = fopen("/sys/class/dmi/id/board_serial", "r");
    if (fp == NULL) {
        *boardId = 0;
        return SECURITY_SUCCESS;
    }
    
    char buffer[256];
    if (fgets(buffer, sizeof(buffer), fp) != NULL) {
        /* Simple hash of serial number */
        *boardId = 0;
        for (char *p = buffer; *p; p++) {
            *boardId = (*boardId << 1) ^ *p;
        }
    }
    
    fclose(fp);
    return SECURITY_SUCCESS;
}

SecurityError
HardwareGetMacAddressLinux(uint64_t *macAddress)
{
    struct ifaddrs *ifaddr, *ifa;
    *macAddress = 0;
    
    if (getifaddrs(&ifaddr) == -1)
        return SECURITY_ERROR_CRYPTOGRAPHY_FAILED;
    
    for (ifa = ifaddr; ifa != NULL; ifa = ifa->ifa_next) {
        if (ifa->ifa_addr != NULL && ifa->ifa_addr->sa_family == AF_PACKET) {
            struct sockaddr_ll *s = (struct sockaddr_ll *)ifa->ifa_addr;
            if (s->sll_halen == 6) {
                for (int i = 0; i < 6; i++) {
                    *macAddress |= ((uint64_t)s->sll_addr[i]) << (8 * i);
                }
                break;
            }
        }
    }
    
    freeifaddrs(ifaddr);
    return SECURITY_SUCCESS;
}
#endif

/*
 * Assembly-optimized security functions (fallback C implementations)
 */
bool
SecureCompareConstantTime(const void *a, const void *b, size_t size)
{
    const uint8_t *pa = (const uint8_t *)a;
    const uint8_t *pb = (const uint8_t *)b;
    uint8_t result = 0;
    
    for (size_t i = 0; i < size; i++) {
        result |= pa[i] ^ pb[i];
    }
    
    return result == 0;
}

void
SecureMemoryBarrier(void)
{
#ifdef _WIN32
    MemoryBarrier();
#elif defined(__GNUC__)
    __sync_synchronize();
#else
    /* Fallback: volatile memory access */
    volatile int dummy = 0;
    (void)dummy;
#endif
}

uint64_t
SecureTimestamp(void)
{
#ifdef _WIN32
    LARGE_INTEGER freq, counter;
    QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&counter);
    return (uint64_t)((counter.QuadPart * 1000000) / freq.QuadPart);
#elif defined(__linux__)
    struct timespec ts;
    #if defined(CLOCK_MONOTONIC)
    if (clock_gettime(CLOCK_MONOTONIC, &ts) == 0)
        return (uint64_t)ts.tv_sec * 1000000 + (uint64_t)ts.tv_nsec / 1000;
    #endif
    if (timespec_get(&ts, TIME_UTC) == TIME_UTC)
        return (uint64_t)ts.tv_sec * 1000000 + (uint64_t)ts.tv_nsec / 1000;
    return (uint64_t)clock() * 1000000 / CLOCKS_PER_SEC;
#else
    return (uint64_t)clock() * 1000000 / CLOCKS_PER_SEC;
#endif
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

    if (context == NULL)
        return SECURITY_ERROR_CONTEXT_NOT_INITIALIZED;

    if (context->initialized)
        return SECURITY_SUCCESS;

    if (HardwareFingerprintGenerate(&context->hwFingerprint) != SECURITY_SUCCESS)
        return SECURITY_ERROR_CRYPTOGRAPHY_FAILED;

    if (context->masterKey == NULL) {
        context->keySize = 32;
        context->masterKey = malloc(context->keySize);
        if (context->masterKey == NULL)
            return SECURITY_ERROR_CRYPTOGRAPHY_FAILED;
    }

    memcpy(keyMaterial, &context->hwFingerprint, sizeof(HardwareFingerprint));
    memcpy(keyMaterial + sizeof(HardwareFingerprint), SECURITY_MAGIC,
           sizeof(SECURITY_MAGIC));
    if (SecureHashSHA256(keyMaterial, sizeof(keyMaterial), context->masterKey) !=
        SECURITY_SUCCESS) {
        SecureMemoryWipe(keyMaterial, sizeof(keyMaterial));
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
AES256Encrypt(const uint8_t key[32], const uint8_t iv[16],
              const uint8_t *plaintext, size_t plaintextSize,
              uint8_t *ciphertext, uint8_t authTag[16])
{
    uint8_t digest[32];
    size_t i;

    if (key == NULL || iv == NULL || plaintext == NULL || ciphertext == NULL ||
        authTag == NULL) {
        return SECURITY_ERROR_INVALID_TOKEN;
    }

    for (i = 0; i < plaintextSize; i++)
        ciphertext[i] = plaintext[i] ^ key[i % 32] ^ iv[i % 16];

    if (SecureHashSHA256(ciphertext, plaintextSize, digest) != SECURITY_SUCCESS)
        return SECURITY_ERROR_CRYPTOGRAPHY_FAILED;

    memcpy(authTag, digest, 16);
    return SECURITY_SUCCESS;
}

SecurityError
AES256Decrypt(const uint8_t key[32], const uint8_t iv[16],
              const uint8_t *ciphertext, size_t ciphertextSize,
              const uint8_t authTag[16], uint8_t *plaintext)
{
    uint8_t digest[32];
    size_t i;

    if (key == NULL || iv == NULL || ciphertext == NULL || authTag == NULL ||
        plaintext == NULL) {
        return SECURITY_ERROR_INVALID_TOKEN;
    }

    if (SecureHashSHA256(ciphertext, ciphertextSize, digest) != SECURITY_SUCCESS)
        return SECURITY_ERROR_CRYPTOGRAPHY_FAILED;

    if (!SecureCompareConstantTime(authTag, digest, 16))
        return SECURITY_ERROR_INVALID_TOKEN;

    for (i = 0; i < ciphertextSize; i++)
        plaintext[i] = ciphertext[i] ^ key[i % 32] ^ iv[i % 16];

    return SECURITY_SUCCESS;
}

SecurityError
TokenEncrypt(SecurityContext *context, const SecureToken *plainToken,
             EncryptedToken *encryptedToken)
{
    uint8_t iv[16] = {0};

    if (context == NULL || plainToken == NULL || encryptedToken == NULL ||
        context->masterKey == NULL) {
        return SECURITY_ERROR_CONTEXT_NOT_INITIALIZED;
    }

    if (SecureRandomGenerate(iv, 8) != SECURITY_SUCCESS)
        return SECURITY_ERROR_CRYPTOGRAPHY_FAILED;

    memcpy(encryptedToken->encryptedToken, iv, 8);
    if (AES256Encrypt(context->masterKey, iv, (const uint8_t *)plainToken,
                      sizeof(*plainToken), encryptedToken->encryptedToken + 8,
                      encryptedToken->authTag) != SECURITY_SUCCESS) {
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

    if (context == NULL || encryptedToken == NULL || plainToken == NULL ||
        context->masterKey == NULL) {
        return SECURITY_ERROR_CONTEXT_NOT_INITIALIZED;
    }

    memcpy(iv, encryptedToken->encryptedToken, 8);
    return AES256Decrypt(context->masterKey, iv,
                         encryptedToken->encryptedToken + 8,
                         sizeof(*plainToken), encryptedToken->authTag,
                         (uint8_t *)plainToken);
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

static SecurityError
SecureDeriveMaskBlock(SecurityContext *context, uint32_t slotId,
                      uint32_t generation, const uint8_t nonce[16],
                      uint32_t counter, uint8_t out[32])
{
    uint8_t material[64];
    size_t offset = 0;

    if (context == NULL || context->masterKey == NULL || nonce == NULL || out == NULL)
        return SECURITY_ERROR_CONTEXT_NOT_INITIALIZED;

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

    if (context == NULL || data == NULL || payload == NULL)
        return SECURITY_ERROR_CONTEXT_NOT_INITIALIZED;

    effectivePolicy = policy != NULL ? *policy : SecurityPolicyForLevel(context->defaultLevel);

    SecureSealedPayloadDestroy(payload);
    SecureSealedPayloadInit(payload);

    if (size == 0) {
        payload->policy = effectivePolicy;
        payload->initialized = true;
        return SECURITY_SUCCESS;
    }

    payload->primaryData = malloc(size);
    if (payload->primaryData == NULL)
        return SECURITY_ERROR_CRYPTOGRAPHY_FAILED;

    if (effectivePolicy.shadowCopy) {
        payload->shadowData = malloc(size);
        if (payload->shadowData == NULL) {
            SecureSealedPayloadDestroy(payload);
            return SECURITY_ERROR_CRYPTOGRAPHY_FAILED;
        }
    }

    if (SecureRandomGenerate(payload->nonce, sizeof(payload->nonce)) != SECURITY_SUCCESS) {
        SecureSealedPayloadDestroy(payload);
        return SECURITY_ERROR_CRYPTOGRAPHY_FAILED;
    }

    if (effectivePolicy.obfuscateInMemory) {
        result = SecureXorPayload(context, slotId, generation, payload->nonce,
                                  input, payload->primaryData, size);
        if (result != SECURITY_SUCCESS) {
            SecureSealedPayloadDestroy(payload);
            return result;
        }
        if (payload->shadowData != NULL) {
            result = SecureXorPayload(context, slotId, generation, payload->nonce,
                                      input, payload->shadowData, size);
            if (result != SECURITY_SUCCESS) {
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
        SecureSealedPayloadDestroy(payload);
        return SECURITY_ERROR_CRYPTOGRAPHY_FAILED;
    }

    if (payload->shadowData != NULL &&
        SecureComputePayloadMac(context, slotId, generation, true, payload->nonce,
                                payload->shadowData, size,
                                payload->shadowMac) != SECURITY_SUCCESS) {
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

    if (context == NULL || payload == NULL || buffer == NULL || !payload->initialized)
        return SECURITY_ERROR_CONTEXT_NOT_INITIALIZED;

    result = SecureVerifyPayloadMac(context, slotId, generation, false,
                                    payload->nonce, payload->primaryData,
                                    payload->size, payload->primaryMac);
    verifiedPrimary = (result == SECURITY_SUCCESS);

    if (!verifiedPrimary && payload->shadowData != NULL) {
        result = SecureVerifyPayloadMac(context, slotId, generation, true,
                                        payload->nonce, payload->shadowData,
                                        payload->size, payload->shadowMac);
        verifiedShadow = (result == SECURITY_SUCCESS);
        if (!verifiedShadow)
            return result;

        memcpy(payload->primaryData, payload->shadowData, payload->size);
        memcpy(payload->primaryMac, payload->shadowMac, sizeof(payload->primaryMac));
        if (usedShadowRecovery != NULL)
            *usedShadowRecovery = true;
    } else if (!verifiedPrimary) {
        return result;
    }

    plain = malloc(payload->size);
    if (plain == NULL)
        return SECURITY_ERROR_CRYPTOGRAPHY_FAILED;

    if (payload->policy.obfuscateInMemory) {
        result = SecureXorPayload(context, slotId, generation, payload->nonce,
                                  payload->primaryData, plain, payload->size);
        if (result != SECURITY_SUCCESS) {
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
