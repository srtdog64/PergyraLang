/*
 * Copyright (c) 2025 Pergyra Language Project
 * All rights reserved.
 *
 * Slot security hardware fingerprint assembly and fallback identity policy.
 */

#include "slot_security.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <windows.h>
#elif defined(__linux__)
#include <unistd.h>
#include <sys/utsname.h>
extern int gethostname(char *name, size_t len);
#endif

static uint64_t
SecurityHashBytes64(const void *data, size_t size)
{
    uint8_t digest[32];
    uint64_t word;

    if (data == NULL && size > 0)
        return 0ULL;
    if (SecureHashSHA256((const uint8_t *)data, size, digest) !=
        SECURITY_SUCCESS)
        return 0ULL;

    word = ((uint64_t)digest[0] << 56) |
           ((uint64_t)digest[1] << 48) |
           ((uint64_t)digest[2] << 40) |
           ((uint64_t)digest[3] << 32) |
           ((uint64_t)digest[4] << 24) |
           ((uint64_t)digest[5] << 16) |
           ((uint64_t)digest[6] << 8) |
           (uint64_t)digest[7];
    SecureMemoryWipe(digest, sizeof(digest));
    return word;
}

#if defined(__linux__)
static uint64_t
SecurityHashString64(const char *text)
{
    return text != NULL ? SecurityHashBytes64(text, strlen(text)) : 0ULL;
}
#endif

/*
 * Fill missing fingerprint fields from software-derived identity when the
 * hardware identifiers are unavailable (VM / container). Every source here
 * (machine-id, hostname, uname) is software-settable, so this fallback is
 * SPOOFABLE BY DESIGN: the fingerprint is a best-effort binding HINT, not a
 * hardware attestation. Attesting hardware identity would need a hardware
 * root of trust (TPM / secure enclave), which the runtime does not require.
 * See docs/security/findings/2026-07-05_fingerprint_not_attestation.md
 * (threat model tier A5).
 */
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
            fingerprint->boardId =
                SecurityHashBytes64(host, host_len) ^ 0x1234567890ABCDEFULL;
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
        uint64_t material[4];
        material[0] = fingerprint->boardId;
        material[1] = fingerprint->macAddress;
        material[2] = (uint64_t)fingerprint->platformHash;
        material[3] = 0x43505546414c4c42ULL; /* "CPUFALLB" */
        fingerprint->cpuId = SecurityHashBytes64(material, sizeof(material));
        if (fingerprint->cpuId == 0)
            fingerprint->cpuId = material[3];
    }
}

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
#else
    return SECURITY_ERROR_UNSUPPORTED_PLATFORM;
#endif

    SecurityFillFallbackIdentity(fingerprint);

    {
        uint8_t digest[32];
        result = SecureHashSHA256(
            (const uint8_t *)fingerprint,
            sizeof(HardwareFingerprint) - sizeof(uint32_t),
            digest);
        if (result != SECURITY_SUCCESS)
            return result;
        fingerprint->checksum = ((uint32_t)digest[0] << 24) |
                                ((uint32_t)digest[1] << 16) |
                                ((uint32_t)digest[2] << 8) |
                                (uint32_t)digest[3];
        SecureMemoryWipe(digest, sizeof(digest));
    }

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
