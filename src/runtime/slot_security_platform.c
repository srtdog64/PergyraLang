/*
 * Copyright (c) 2025 Pergyra Language Project
 * All rights reserved.
 *
 * Slot security platform fingerprint helpers.
 */

#include "slot_security.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#ifdef _WIN32
#include <windows.h>
#include <wincrypt.h>
#include <iphlpapi.h>
#include <intrin.h>
#pragma comment(lib, "advapi32.lib")
#pragma comment(lib, "iphlpapi.lib")
#elif defined(__linux__)
#include <cpuid.h>
#include <ifaddrs.h>
#include <net/if.h>
#include <netpacket/packet.h>
#endif

#ifdef _WIN32
static uint64_t
slot_security_platform_hash_bytes64(const void *data, size_t size)
{
    const uint8_t *bytes = (const uint8_t *)data;
    uint64_t hash = 1469598103934665603ULL;

    for (size_t i = 0; i < size; i++) {
        hash ^= (uint64_t)bytes[i];
        hash *= 1099511628211ULL;
    }

    return hash;
}
#endif

#if defined(__linux__)
static bool
slot_security_platform_bytes_all_zero(const uint8_t *data, size_t size)
{
    for (size_t i = 0; i < size; i++) {
        if (data[i] != 0)
            return false;
    }
    return true;
}
#endif

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
        *boardId = slot_security_platform_hash_bytes64(host, host_len) ^ 0x1234567890ABCDEFULL;
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
            for (int i = 0; i < 6; i++)
                *macAddress |= ((uint64_t)adapterInfo->Address[i]) << (8 * i);
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
        *boardId = 0;
        for (char *p = buffer; *p; p++)
            *boardId = (*boardId << 1) ^ *p;
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
                && !slot_security_platform_bytes_all_zero(s->sll_addr, 6)) {
                for (int i = 0; i < 6; i++)
                    *macAddress |= ((uint64_t)s->sll_addr[i]) << (8 * i);
                break;
            }
        }
    }

    freeifaddrs(ifaddr);
    return SECURITY_SUCCESS;
}
#endif
