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

static bool
SecurityBytesAllZero(const uint8_t *data, size_t size)
{
    for (size_t i = 0; i < size; i++) {
        if (data[i] != 0)
            return false;
    }
    return true;
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
    return SECURITY_ERROR_UNSUPPORTED_PLATFORM;
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
    return SECURITY_ERROR_UNSUPPORTED_PLATFORM;
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
    char host[256];
    DWORD host_len = (DWORD)sizeof(host);

    if (GetComputerNameA(host, &host_len) && host_len > 0)
        *boardId = SecurityHashBytes64(host, host_len) ^ 0x1234567890ABCDEFULL;
    else
        *boardId = 0;
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
            if (s->sll_halen == 6
                && ifa->ifa_name != NULL
                && strcmp(ifa->ifa_name, "lo") != 0
                && !SecurityBytesAllZero(s->sll_addr, 6)) {
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

/* =================================================================
 * AES-256 Implementation (FIPS 197)
 *
 * AES-256 block cipher with CTR mode for streaming encryption
 * and HMAC-SHA256 truncated to 128 bits for authentication.
 * ================================================================= */

/* S-Box lookup table — retained for test verification only */
#ifdef PGY_AES_SBOX_TABLE_FOR_TESTS
static const uint8_t aes_sbox_table[256] = {
    0x63,0x7c,0x77,0x7b,0xf2,0x6b,0x6f,0xc5,0x30,0x01,0x67,0x2b,0xfe,0xd7,0xab,0x76,
    0xca,0x82,0xc9,0x7d,0xfa,0x59,0x47,0xf0,0xad,0xd4,0xa2,0xaf,0x9c,0xa4,0x72,0xc0,
    0xb7,0xfd,0x93,0x26,0x36,0x3f,0xf7,0xcc,0x34,0xa5,0xe5,0xf1,0x71,0xd8,0x31,0x15,
    0x04,0xc7,0x23,0xc3,0x18,0x96,0x05,0x9a,0x07,0x12,0x80,0xe2,0xeb,0x27,0xb2,0x75,
    0x09,0x83,0x2c,0x1a,0x1b,0x6e,0x5a,0xa0,0x52,0x3b,0xd6,0xb3,0x29,0xe3,0x2f,0x84,
    0x53,0xd1,0x00,0xed,0x20,0xfc,0xb1,0x5b,0x6a,0xcb,0xbe,0x39,0x4a,0x4c,0x58,0xcf,
    0xd0,0xef,0xaa,0xfb,0x43,0x4d,0x33,0x85,0x45,0xf9,0x02,0x7f,0x50,0x3c,0x9f,0xa8,
    0x51,0xa3,0x40,0x8f,0x92,0x9d,0x38,0xf5,0xbc,0xb6,0xda,0x21,0x10,0xff,0xf3,0xd2,
    0xcd,0x0c,0x13,0xec,0x5f,0x97,0x44,0x17,0xc4,0xa7,0x7e,0x3d,0x64,0x5d,0x19,0x73,
    0x60,0x81,0x4f,0xdc,0x22,0x2a,0x90,0x88,0x46,0xee,0xb8,0x14,0xde,0x5e,0x0b,0xdb,
    0xe0,0x32,0x3a,0x0a,0x49,0x06,0x24,0x5c,0xc2,0xd3,0xac,0x62,0x91,0x95,0xe4,0x79,
    0xe7,0xc8,0x37,0x6d,0x8d,0xd5,0x4e,0xa9,0x6c,0x56,0xf4,0xea,0x65,0x7a,0xae,0x08,
    0xba,0x78,0x25,0x2e,0x1c,0xa6,0xb4,0xc6,0xe8,0xdd,0x74,0x1f,0x4b,0xbd,0x8b,0x8a,
    0x70,0x3e,0xb5,0x66,0x48,0x03,0xf6,0x0e,0x61,0x35,0x57,0xb9,0x86,0xc1,0x1d,0x9e,
    0xe1,0xf8,0x98,0x11,0x69,0xd9,0x8e,0x94,0x9b,0x1e,0x87,0xe9,0xce,0x55,0x28,0xdf,
    0x8c,0xa1,0x89,0x0d,0xbf,0xe6,0x42,0x68,0x41,0x99,0x2d,0x0f,0xb0,0x54,0xbb,0x16
};
#endif /* PGY_AES_SBOX_TABLE_FOR_TESTS */

/* =================================================================
 * Constant-time AES S-Box (algebraic computation)
 *
 * Eliminates cache-timing side-channel by computing S(x) = A * inv(x) + 0x63
 * in GF(2^8) without any secret-dependent memory access.
 * ================================================================= */

/* Constant-time GF(2^8) multiplication (irreducible: x^8+x^4+x^3+x+1 = 0x11B) */
static uint8_t
gf256_mul(uint8_t a, uint8_t b)
{
    uint8_t result = 0;
    int i;
    for (i = 0; i < 8; i++) {
        /* Constant-time conditional XOR: add a if low bit of b is set */
        result ^= a & (uint8_t)(-(int8_t)(b & 1));
        /* Constant-time xtime: reduce if high bit set */
        uint8_t hi = (uint8_t)(-(int8_t)((a >> 7) & 1));
        a = (uint8_t)((a << 1) ^ (hi & 0x1B));
        b >>= 1;
    }
    return result;
}

/* Constant-time GF(2^8) inversion via Fermat's little theorem: x^{-1} = x^{254} */
static uint8_t
gf256_inv(uint8_t x)
{
    uint8_t x2  = gf256_mul(x, x);        /* x^2   */
    uint8_t x3  = gf256_mul(x2, x);       /* x^3   */
    uint8_t x6  = gf256_mul(x3, x3);      /* x^6   */
    uint8_t x12 = gf256_mul(x6, x6);      /* x^12  */
    uint8_t x15 = gf256_mul(x12, x3);     /* x^15  */
    uint8_t x30 = gf256_mul(x15, x15);    /* x^30  */
    uint8_t x60 = gf256_mul(x30, x30);    /* x^60  */
    uint8_t x63 = gf256_mul(x60, x3);     /* x^63  */
    uint8_t x126 = gf256_mul(x63, x63);   /* x^126 */
    uint8_t x252 = gf256_mul(x126, x126); /* x^252 */
    uint8_t x254 = gf256_mul(x252, x2);   /* x^254 */
    return x254;
}

/* Constant-time AES S-Box: affine transform of GF(2^8) inverse */
static uint8_t
aes_sbox_compute(uint8_t x)
{
    uint8_t inv = gf256_inv(x);
    /* Affine transform: S = A * inv + 0x63
     * A is the AES affine matrix (circular left shifts and XOR) */
    uint8_t s = inv;
    s ^= (uint8_t)((inv << 1) | (inv >> 7));
    s ^= (uint8_t)((inv << 2) | (inv >> 6));
    s ^= (uint8_t)((inv << 3) | (inv >> 5));
    s ^= (uint8_t)((inv << 4) | (inv >> 4));
    s ^= 0x63;
    return s;
}

static const uint8_t aes_rcon[11] = {
    0x00, 0x01, 0x02, 0x04, 0x08, 0x10, 0x20, 0x40, 0x80, 0x1b, 0x36
};

/* GF(2^8) multiplication by 2 */
static inline uint8_t
aes_xtime(uint8_t x)
{
    return (uint8_t)((x << 1) ^ (((x >> 7) & 1) * 0x1b));
}

/* AES-256 key expansion: 32-byte key → 240 bytes (15 round keys) */
static void
aes256_key_expand(const uint8_t key[32], uint8_t rk[240])
{
    uint8_t temp[4];
    int i;

    memcpy(rk, key, 32);

    for (i = 8; i < 60; i++) {
        memcpy(temp, rk + (i - 1) * 4, 4);

        if (i % 8 == 0) {
            /* RotWord + SubWord + Rcon */
            uint8_t t = temp[0];
            temp[0] = aes_sbox_compute(temp[1]) ^ aes_rcon[i / 8];
            temp[1] = aes_sbox_compute(temp[2]);
            temp[2] = aes_sbox_compute(temp[3]);
            temp[3] = aes_sbox_compute(t);
        } else if (i % 8 == 4) {
            /* SubWord only */
            temp[0] = aes_sbox_compute(temp[0]);
            temp[1] = aes_sbox_compute(temp[1]);
            temp[2] = aes_sbox_compute(temp[2]);
            temp[3] = aes_sbox_compute(temp[3]);
        }

        rk[i * 4 + 0] = rk[(i - 8) * 4 + 0] ^ temp[0];
        rk[i * 4 + 1] = rk[(i - 8) * 4 + 1] ^ temp[1];
        rk[i * 4 + 2] = rk[(i - 8) * 4 + 2] ^ temp[2];
        rk[i * 4 + 3] = rk[(i - 8) * 4 + 3] ^ temp[3];
    }
}

/* Encrypt a single 16-byte block in-place using AES-256 */
static void
aes256_encrypt_block(const uint8_t rk[240], uint8_t block[16])
{
    uint8_t s[16], t[4];
    int r, i;

    memcpy(s, block, 16);

    /* AddRoundKey(0) */
    for (i = 0; i < 16; i++)
        s[i] ^= rk[i];

    for (r = 1; r <= 14; r++) {
        /* SubBytes */
        for (i = 0; i < 16; i++)
            s[i] = aes_sbox_compute(s[i]);

        /* ShiftRows */
        t[0] = s[1]; s[1] = s[5]; s[5] = s[9]; s[9]  = s[13]; s[13] = t[0];
        t[0] = s[2]; t[1] = s[6]; s[2] = s[10]; s[6] = s[14]; s[10] = t[0]; s[14] = t[1];
        t[0] = s[15]; s[15] = s[11]; s[11] = s[7]; s[7] = s[3]; s[3] = t[0];

        /* MixColumns (skip on last round) */
        if (r < 14) {
            for (i = 0; i < 4; i++) {
                int c = i * 4;
                uint8_t a0 = s[c], a1 = s[c+1], a2 = s[c+2], a3 = s[c+3];
                uint8_t x0 = aes_xtime(a0), x1 = aes_xtime(a1);
                uint8_t x2 = aes_xtime(a2), x3 = aes_xtime(a3);
                s[c]   = x0 ^ a1 ^ x1 ^ a2 ^ a3;
                s[c+1] = a0 ^ x1 ^ a2 ^ x2 ^ a3;
                s[c+2] = a0 ^ a1 ^ x2 ^ a3 ^ x3;
                s[c+3] = a0 ^ x0 ^ a1 ^ a2 ^ x3;
            }
        }

        /* AddRoundKey */
        for (i = 0; i < 16; i++)
            s[i] ^= rk[r * 16 + i];
    }

    memcpy(block, s, 16);
}

/* AES-256-CTR: encrypt/decrypt arbitrary-length data */
static void
aes256_ctr(const uint8_t key[32], const uint8_t iv[16],
           const uint8_t *in, uint8_t *out, size_t len)
{
    uint8_t rk[240];
    uint8_t ctr[16], keystream[16];
    size_t i, block_idx;

    aes256_key_expand(key, rk);
    memcpy(ctr, iv, 16);

    for (block_idx = 0; block_idx < len; block_idx += 16) {
        memcpy(keystream, ctr, 16);
        aes256_encrypt_block(rk, keystream);

        size_t chunk = (len - block_idx < 16) ? (len - block_idx) : 16;
        for (i = 0; i < chunk; i++)
            out[block_idx + i] = in[block_idx + i] ^ keystream[i];

        /* Increment counter (big-endian, last 4 bytes) */
        for (int j = 15; j >= 12; j--) {
            if (++ctr[j] != 0) break;
        }
    }

    /* Wipe sensitive data */
    SecureMemoryWipe(rk, sizeof(rk));
    SecureMemoryWipe(keystream, sizeof(keystream));
}

/* =================================================================
 * HMAC-SHA256 per RFC 2104
 *
 * Uses SecureHashSHA256 (OpenSSL EVP) as the underlying hash.
 * SHA-256: block_size = 64 bytes, output_size = 32 bytes.
 * ================================================================= */

#define HMAC_BLOCK_SIZE 64
#define HMAC_HASH_SIZE  32

static SecurityError
hmac_sha256(const uint8_t *key, size_t keyLen,
            const uint8_t *message, size_t messageLen,
            uint8_t output[32])
{
    uint8_t key_prime[HMAC_BLOCK_SIZE];
    uint8_t ipad_key[HMAC_BLOCK_SIZE];
    uint8_t opad_key[HMAC_BLOCK_SIZE];
    uint8_t inner_hash[HMAC_HASH_SIZE];
    uint8_t *buf = NULL;
    SecurityError err;
    size_t i;

    memset(key_prime, 0, HMAC_BLOCK_SIZE);

    /* Step 1: If key > block_size, hash it first */
    if (keyLen > HMAC_BLOCK_SIZE) {
        err = SecureHashSHA256(key, keyLen, key_prime);
        if (err != SECURITY_SUCCESS)
            return SECURITY_ERROR_CRYPTOGRAPHY_FAILED;
        /* key_prime is now 32 bytes, rest stays zero-padded */
    } else {
        memcpy(key_prime, key, keyLen);
    }

    /* Step 2: Compute ipad_key and opad_key */
    for (i = 0; i < HMAC_BLOCK_SIZE; i++) {
        ipad_key[i] = key_prime[i] ^ 0x36;
        opad_key[i] = key_prime[i] ^ 0x5c;
    }

    /* Step 3: Inner hash = H(ipad_key || message) */
    buf = (uint8_t *)malloc(HMAC_BLOCK_SIZE + messageLen);
    if (buf == NULL) {
        SecureMemoryWipe(key_prime, sizeof(key_prime));
        return SECURITY_ERROR_CRYPTOGRAPHY_FAILED;
    }
    memcpy(buf, ipad_key, HMAC_BLOCK_SIZE);
    memcpy(buf + HMAC_BLOCK_SIZE, message, messageLen);
    err = SecureHashSHA256(buf, HMAC_BLOCK_SIZE + messageLen, inner_hash);
    SecureMemoryWipe(buf, HMAC_BLOCK_SIZE + messageLen);
    free(buf);
    if (err != SECURITY_SUCCESS) {
        SecureMemoryWipe(key_prime, sizeof(key_prime));
        return SECURITY_ERROR_CRYPTOGRAPHY_FAILED;
    }

    /* Step 4: Outer hash = H(opad_key || inner_hash) */
    uint8_t outer_buf[HMAC_BLOCK_SIZE + HMAC_HASH_SIZE];
    memcpy(outer_buf, opad_key, HMAC_BLOCK_SIZE);
    memcpy(outer_buf + HMAC_BLOCK_SIZE, inner_hash, HMAC_HASH_SIZE);
    err = SecureHashSHA256(outer_buf, sizeof(outer_buf), output);

    /* Wipe all intermediates */
    SecureMemoryWipe(key_prime, sizeof(key_prime));
    SecureMemoryWipe(ipad_key, sizeof(ipad_key));
    SecureMemoryWipe(opad_key, sizeof(opad_key));
    SecureMemoryWipe(inner_hash, sizeof(inner_hash));
    SecureMemoryWipe(outer_buf, sizeof(outer_buf));

    return (err == SECURITY_SUCCESS) ? SECURITY_SUCCESS
                                     : SECURITY_ERROR_CRYPTOGRAPHY_FAILED;
}

SecurityError
AES256Encrypt(const uint8_t key[32], const uint8_t iv[16],
              const uint8_t *plaintext, size_t plaintextSize,
              uint8_t *ciphertext, uint8_t authTag[16])
{
    uint8_t digest[32];

    if (key == NULL || iv == NULL || plaintext == NULL || ciphertext == NULL ||
        authTag == NULL) {
        return SECURITY_ERROR_INVALID_TOKEN;
    }

    /* Encrypt with AES-256-CTR */
    aes256_ctr(key, iv, plaintext, ciphertext, plaintextSize);

    /* Auth tag: HMAC-SHA256(key, iv || ciphertext) truncated to 128 bits */
    {
        uint8_t *auth_data = (uint8_t *)malloc(16 + plaintextSize);
        if (auth_data == NULL)
            return SECURITY_ERROR_CRYPTOGRAPHY_FAILED;
        memcpy(auth_data, iv, 16);
        memcpy(auth_data + 16, ciphertext, plaintextSize);
        SecurityError err = hmac_sha256(key, 32, auth_data,
                                        16 + plaintextSize, digest);
        SecureMemoryWipe(auth_data, 16 + plaintextSize);
        free(auth_data);
        if (err != SECURITY_SUCCESS)
            return SECURITY_ERROR_CRYPTOGRAPHY_FAILED;
    }

    memcpy(authTag, digest, 16);
    return SECURITY_SUCCESS;
}

SecurityError
AES256Decrypt(const uint8_t key[32], const uint8_t iv[16],
              const uint8_t *ciphertext, size_t ciphertextSize,
              const uint8_t authTag[16], uint8_t *plaintext)
{
    uint8_t digest[32];

    if (key == NULL || iv == NULL || ciphertext == NULL || authTag == NULL ||
        plaintext == NULL) {
        return SECURITY_ERROR_INVALID_TOKEN;
    }

    /* Verify auth tag: HMAC-SHA256(key, iv || ciphertext) */
    {
        uint8_t *auth_data = (uint8_t *)malloc(16 + ciphertextSize);
        if (auth_data == NULL)
            return SECURITY_ERROR_CRYPTOGRAPHY_FAILED;
        memcpy(auth_data, iv, 16);
        memcpy(auth_data + 16, ciphertext, ciphertextSize);
        SecurityError err = hmac_sha256(key, 32, auth_data,
                                        16 + ciphertextSize, digest);
        SecureMemoryWipe(auth_data, 16 + ciphertextSize);
        free(auth_data);
        if (err != SECURITY_SUCCESS)
            return SECURITY_ERROR_CRYPTOGRAPHY_FAILED;
    }

    if (!SecureCompareConstantTime(authTag, digest, 16))
        return SECURITY_ERROR_INVALID_TOKEN;

    /* Decrypt with AES-256-CTR (same operation as encrypt) */
    aes256_ctr(key, iv, ciphertext, plaintext, ciphertextSize);

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

    if (SecureRandomGenerate(iv, SECURITY_IV_SIZE) != SECURITY_SUCCESS)
        return SECURITY_ERROR_CRYPTOGRAPHY_FAILED;

    memcpy(encryptedToken->encryptedToken, iv, SECURITY_IV_SIZE);
    if (AES256Encrypt(context->masterKey, iv, (const uint8_t *)plainToken,
                      sizeof(*plainToken), encryptedToken->encryptedToken + SECURITY_IV_SIZE,
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

    memcpy(iv, encryptedToken->encryptedToken, SECURITY_IV_SIZE);
    return AES256Decrypt(context->masterKey, iv,
                         encryptedToken->encryptedToken + SECURITY_IV_SIZE,
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
