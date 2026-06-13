/*
 * Copyright (c) 2025 Pergyra Language Project
 * All rights reserved.
 *
 * Slot Security System - Secure Token-based Memory Access Control
 * 
 * This module implements a security layer that prevents external memory
 * manipulation tools (like Cheat Engine) from modifying slot values.
 * It uses hardware-bound cryptographic tokens for access control.
 */

#ifndef PERGYRA_SLOT_SECURITY_H
#define PERGYRA_SLOT_SECURITY_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/*
 * Security level enumeration
 */
typedef enum
{
    SECURITY_LEVEL_BASIC = 1,      /* Basic token verification */
    SECURITY_LEVEL_HARDWARE = 2,   /* Hardware-bound tokens */
    SECURITY_LEVEL_ENCRYPTED = 3   /* Encrypted token storage */
} SecurityLevel;

/*
 * Secure token structure (256-bit total)
 * Never stored in plaintext in memory when SECURITY_LEVEL_ENCRYPTED is used
 */
typedef struct
{
    uint8_t  tokenData[32];        /* 256-bit cryptographic token */
    uint32_t generation;           /* Token generation counter */
    uint32_t checksum;             /* Integrity checksum */
} SecureToken;

/*
 * Hardware fingerprint for hardware-bound security
 */
typedef struct
{
    uint64_t cpuId;                /* CPU serial/feature hash */
    uint64_t boardId;              /* Motherboard serial hash */
    uint64_t macAddress;           /* Primary MAC address */
    uint32_t platformHash;         /* Platform-specific hash */
    uint32_t checksum;             /* Fingerprint integrity */
} HardwareFingerprint;

/*
 * Token capability structure - represents access rights to a slot
 */
typedef struct
{
    uint32_t     slotId;           /* Associated slot ID */
    SecureToken  token;            /* Access token */
    SecurityLevel level;           /* Security level */
    uint64_t     issuedTime;       /* Token issue timestamp */
    uint64_t     expiryTime;       /* Token expiry (0 = no expiry) */
    bool         canRead;          /* Read permission */
    bool         canWrite;         /* Write permission */
    bool         canTransfer;      /* Transfer permission to other contexts */
} TokenCapability;

/*
 * Encrypted token storage (only used with SECURITY_LEVEL_ENCRYPTED)
 */
typedef struct
{
    uint8_t  encryptedToken[52];   /* 12 bytes IV + 40 bytes AES-256 ciphertext */
    uint8_t  authTag[16];          /* HMAC-SHA256 authentication tag (128-bit) */
    uint32_t keyVersion;           /* Encryption key version */
} EncryptedToken;

/*
 * Security context for token operations
 */
typedef struct
{
    HardwareFingerprint hwFingerprint;  /* Hardware binding */
    uint8_t            *masterKey;      /* Encryption master key */
    size_t              keySize;        /* Master key size */
    SecurityLevel       defaultLevel;   /* Default security level */
    bool                initialized;    /* Context initialization status */
    bool                masterKeyLocked;/* Master key is locked in RAM */
    
    /* Security statistics */
    uint64_t           tokensIssued;
    uint64_t           tokensValidated;
    uint64_t           validationFailures;
    uint64_t           securityViolations;
} SecurityContext;

typedef enum
{
    SECURE_SLOT_STORAGE_NONE = 0,
    SECURE_SLOT_STORAGE_SEALED = 1
} SecureSlotStorageMode;

typedef struct
{
    SecureSlotStorageMode storageMode;
    bool obfuscateInMemory;
    bool shadowCopy;
    bool isolateShadowCopy;
    bool auditReads;
} SecureSlotPolicy;

typedef struct
{
    uint8_t  *primaryData;
    uint8_t  *shadowData;
    uint8_t   primaryMac[32];
    uint8_t   shadowMac[32];
    uint8_t   nonce[16];
    size_t    size;
    SecureSlotPolicy policy;
    bool      initialized;
} SecureSealedPayload;

/*
 * Security error codes
 */
typedef enum
{
    SECURITY_SUCCESS = 0,
    SECURITY_ERROR_INVALID_TOKEN,
    SECURITY_ERROR_TOKEN_EXPIRED,
    SECURITY_ERROR_PERMISSION_DENIED,
    SECURITY_ERROR_HARDWARE_MISMATCH,
    SECURITY_ERROR_CRYPTOGRAPHY_FAILED,
    SECURITY_ERROR_REPLAY_ATTACK,
    SECURITY_ERROR_INSUFFICIENT_ENTROPY,
    SECURITY_ERROR_CONTEXT_NOT_INITIALIZED,
    SECURITY_ERROR_UNSUPPORTED_PLATFORM
} SecurityError;

/*
 * Security context management
 */
bool             SecurityLevelIsValid(SecurityLevel level);
SecurityContext *SecurityContextCreate(SecurityLevel defaultLevel);
void             SecurityContextDestroy(SecurityContext *context);
SecurityError    SecurityContextInitialize(SecurityContext *context);
SecurityError    SecurityContextUpdateHardware(SecurityContext *context);

/*
 * Hardware fingerprinting
 */
SecurityError HardwareFingerprintGenerate(HardwareFingerprint *fingerprint);
bool          HardwareFingerprintCompare(const HardwareFingerprint *fp1, 
                                       const HardwareFingerprint *fp2);
uint32_t      HardwareFingerprintHash(const HardwareFingerprint *fingerprint);

/*
 * Token generation and management
 */
SecurityError TokenGenerate(SecurityContext *context, uint32_t slotId,
                           SecurityLevel level, TokenCapability *capability);
SecurityError TokenValidate(SecurityContext *context, uint32_t slotId,
                           const TokenCapability *capability);
SecurityError TokenRevoke(SecurityContext *context, uint32_t slotId);
SecurityError TokenRefresh(SecurityContext *context, TokenCapability *capability);

/*
 * Secure token operations (constant-time to prevent timing attacks)
 */
bool          TokenCompareSecure(const SecureToken *token1, const SecureToken *token2);
SecurityError TokenEncrypt(SecurityContext *context, const SecureToken *plainToken,
                          EncryptedToken *encryptedToken);
SecurityError TokenDecrypt(SecurityContext *context, const EncryptedToken *encryptedToken,
                          SecureToken *plainToken);

/*
 * Cryptographic utilities
 */
SecurityError SecureRandomGenerate(uint8_t *buffer, size_t size);
SecurityError SecureHashSHA256(const uint8_t *input, size_t inputSize,
                              uint8_t output[32]);
SecurityError SecureHmacSHA256(const uint8_t *key, size_t keySize,
                              const uint8_t *input, size_t inputSize,
                              uint8_t output[32]);
SecurityError SecureMemoryLock(void *addr, size_t size);
SecurityError SecureMemoryUnlock(void *addr, size_t size);
void          SecureMemoryWipe(void *addr, size_t size);

/*
 * AES-256-CTR encryption with HMAC-SHA256 authentication.
 *
 * Provider-owned AES/HMAC/SHA-256 primitives:
 * Windows CNG/BCrypt on Windows, OpenSSL EVP/HMAC/RAND elsewhere.
 * The runtime owns only the CTR counter composition and token auth layout.
 * Auth tag: HMAC-SHA256(key, iv || ciphertext) truncated to 128 bits (RFC 2104).
 */
SecurityError AES256Encrypt(const uint8_t key[32], const uint8_t iv[16],
                            const uint8_t *plaintext, size_t plaintextSize,
                            uint8_t *ciphertext, uint8_t authTag[16]);
SecurityError AES256Decrypt(const uint8_t key[32], const uint8_t iv[16],
                            const uint8_t *ciphertext, size_t ciphertextSize,
                            const uint8_t authTag[16], uint8_t *plaintext);

/*
 * Security audit and logging
 */
void SecurityAuditLog(SecurityContext *context, const char *event,
                     const char *details);
void SecurityPrintStatistics(const SecurityContext *context);
bool SecurityDetectAnomalies(const SecurityContext *context);

/*
 * Secure slot sealed storage
 */
SecureSlotPolicy SecurityPolicyForLevel(SecurityLevel level);
void             SecureSealedPayloadInit(SecureSealedPayload *payload);
void             SecureSealedPayloadDestroy(SecureSealedPayload *payload);
SecurityError    SecureSealedPayloadSeal(SecurityContext *context,
                                        uint32_t slotId,
                                        uint32_t generation,
                                        const void *data,
                                        size_t size,
                                        const SecureSlotPolicy *policy,
                                        SecureSealedPayload *payload);
SecurityError    SecureSealedPayloadOpen(SecurityContext *context,
                                        uint32_t slotId,
                                        uint32_t generation,
                                        SecureSealedPayload *payload,
                                        void *buffer,
                                        size_t bufferSize,
                                        size_t *bytesRead,
                                        bool *usedShadowRecovery);
const uint8_t   *SecureSealedPayloadPrimaryBytes(const SecureSealedPayload *payload);
const uint8_t   *SecureSealedPayloadShadowBytes(const SecureSealedPayload *payload);

/*
 * Platform-specific implementations
 */
#ifdef _WIN32
SecurityError HardwareGetCpuIdWindows(uint64_t *cpuId);
SecurityError HardwareGetBoardIdWindows(uint64_t *boardId);
SecurityError HardwareGetMacAddressWindows(uint64_t *macAddress);
#elif defined(__linux__)
SecurityError HardwareGetCpuIdLinux(uint64_t *cpuId);
SecurityError HardwareGetBoardIdLinux(uint64_t *boardId);
SecurityError HardwareGetMacAddressLinux(uint64_t *macAddress);
#endif

/*
 * Assembly-optimized security functions
 */
extern bool SecureCompareConstantTime(const void *a, const void *b, size_t size);
extern void SecureMemoryBarrier(void);
extern uint64_t SecureTimestamp(void);

/*
 * Compile-time security configuration
 */
#ifndef SECURITY_TOKEN_ENTROPY_BITS
#define SECURITY_TOKEN_ENTROPY_BITS 256
#endif

#ifndef SECURITY_DEFAULT_TOKEN_TTL_MS
#define SECURITY_DEFAULT_TOKEN_TTL_MS 300000  /* 5 minutes */
#endif

#ifndef SECURITY_MAX_VALIDATION_FAILURES
#define SECURITY_MAX_VALIDATION_FAILURES 10
#endif

/*
 * Security feature flags
 */
#define SECURITY_FEATURE_HARDWARE_BINDING     (1 << 0)
#define SECURITY_FEATURE_TOKEN_ENCRYPTION     (1 << 1)
#define SECURITY_FEATURE_TIMING_PROTECTION    (1 << 2)
#define SECURITY_FEATURE_AUDIT_LOGGING        (1 << 3)
#define SECURITY_FEATURE_REPLAY_PROTECTION    (1 << 4)

#endif /* PERGYRA_SLOT_SECURITY_H */
