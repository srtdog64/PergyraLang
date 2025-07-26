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
    SecurityError result = SecureRandomGenerate(
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
    capability->token.checksum = HardwareFingerprintHash(&context->hwFingerprint) ^
                                capability->token.generation;
    
    /* Securely wipe token material */
    SecureMemoryWipe(tokenMaterial, sizeof(tokenMaterial));
    
    return SECURITY_SUCCESS;
}

SecurityError
TokenValidate(SecurityContext *context, uint32_t slotId,
             const TokenCapability *capability)
{
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
    uint32_t expectedChecksum = HardwareFingerprintHash(&context->hwFingerprint) ^
                               capability->token.generation;
    
    if (capability->token.checksum != expectedChecksum) {
        context->validationFailures++;
        context->securityViolations++;
        return SECURITY_ERROR_INVALID_TOKEN;
    }
    
    /* Re-generate token to validate */
    TokenCapability testCapability;
    SecurityError result = TokenGenerate(context, slotId, capability->level, 
                                       &testCapability);
    if (result != SECURITY_SUCCESS)
        return result;
    
    /* Compare tokens using constant-time comparison */
    if (!TokenCompareSecure(&capability->token, &testCapability.token)) {
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
    if (input == NULL || output == NULL || inputSize == 0)
        return SECURITY_ERROR_INVALID_TOKEN;
    
    SHA256_CTX ctx;
    if (SHA256_Init(&ctx) != 1)
        return SECURITY_ERROR_CRYPTOGRAPHY_FAILED;
    
    if (SHA256_Update(&ctx, input, inputSize) != 1)
        return SECURITY_ERROR_CRYPTOGRAPHY_FAILED;
    
    if (SHA256_Final(output, &ctx) != 1)
        return SECURITY_ERROR_CRYPTOGRAPHY_FAILED;
    
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
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000 + (uint64_t)ts.tv_nsec / 1000;
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
