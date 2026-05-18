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
        uint64_t entropy = 0;
        (void)SecureRandomGenerate((uint8_t *)&entropy, sizeof(entropy));
        fingerprint->cpuId = entropy != 0 ? entropy : 0x43505546414c4c42ULL;
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
#endif

    SecurityFillFallbackIdentity(fingerprint);

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
