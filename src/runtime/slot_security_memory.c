/*
 * Copyright (c) 2025 Pergyra Language Project
 * All rights reserved.
 *
 * Slot security memory primitives.
 */

#include "slot_security.h"

#include <stdint.h>
#include <time.h>

#ifdef _WIN32
#include <windows.h>
#elif defined(__linux__) || defined(__APPLE__) || defined(__unix__) \
    || defined(__unix) || defined(BSD)
#include <sys/mman.h>
#define PGY_HAVE_POSIX_MEMLOCK 1
#endif

SecurityError
SecureMemoryLock(void *addr, size_t size)
{
    if (addr == NULL || size == 0)
        return SECURITY_ERROR_INVALID_TOKEN;

#ifdef _WIN32
    return VirtualLock(addr, size) ? SECURITY_SUCCESS :
                                     SECURITY_ERROR_CRYPTOGRAPHY_FAILED;
#elif defined(PGY_HAVE_POSIX_MEMLOCK)
    /* mlock is POSIX.1-2001 and available on Linux, macOS, and the BSDs;
     * memory protection is no longer limited to Linux. */
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
#elif defined(PGY_HAVE_POSIX_MEMLOCK)
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

#if defined(_WIN32)
    SecureZeroMemory(addr, size);
#elif defined(HAVE_EXPLICIT_BZERO)
    explicit_bzero(addr, size);
#elif defined(__STDC_LIB_EXT1__)
    memset_s(addr, size, 0, size);
#else
    volatile uint8_t *ptr = (volatile uint8_t *)addr;
    for (size_t i = 0; i < size; i++)
        ptr[i] = 0;
#endif
}

bool
SecureCompareConstantTime(const void *a, const void *b, size_t size)
{
    const uint8_t *pa = (const uint8_t *)a;
    const uint8_t *pb = (const uint8_t *)b;
    volatile uint8_t result = 0;

    for (size_t i = 0; i < size; i++)
        result = result | (pa[i] ^ pb[i]);

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
