/*
 * Copyright (c) 2025 Pergyra Language Project
 * All rights reserved.
 *
 * Slot manager security audit/statistics helpers.
 */

#include "slot_manager.h"
#include "slot_manager_internal.h"
#include "pgy_runtime_security_log.h"

#include <pthread.h>
#include <stdio.h>
#include <time.h>

static bool
slot_security_localtime(time_t now, struct tm *out)
{
    if (out == NULL)
        return false;
#if defined(_WIN32)
    return localtime_s(out, &now) == 0;
#elif defined(_POSIX_VERSION)
    return localtime_r(&now, out) != NULL;
#else
    {
        struct tm *tmp = localtime(&now);
        if (tmp == NULL)
            return false;
        *out = *tmp;
        return true;
    }
#endif
}

void
slot_manager_log_security_event_locked(SlotManager *manager, const char *event,
                                       uint32_t slotId, const char *details)
{
    time_t now;
    struct tm tmNow;
    char timestamp[32];

    if (manager == NULL || event == NULL || !manager->securityEnabled
        || manager->securityContext == NULL)
        return;

    now = time(NULL);
    if (slot_security_localtime(now, &tmNow))
        strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", &tmNow);
    else
        snprintf(timestamp, sizeof(timestamp), "time-unavailable");

    fputs("{\"component\":\"slot-security\",\"timestamp\":", stderr);
    pgy_runtime_fprint_json_string(stderr, timestamp);
    fprintf(stderr, ",\"slot\":%u,\"event\":", slotId);
    pgy_runtime_fprint_json_string(stderr, event);
    fputs(",\"details\":", stderr);
    pgy_runtime_fprint_json_string(stderr, details != NULL ? details : "n/a");
    fputs("}\n", stderr);

    SecurityAuditLog(manager->securityContext, event, details);
}

void
SlotManagerLogSecurityEvent(SlotManager *manager, const char *event,
                            uint32_t slotId, const char *details)
{
    if (manager == NULL || event == NULL || manager->mutex == NULL)
        return;

    pthread_mutex_lock(manager_mutex(manager));
    slot_manager_log_security_event_locked(manager, event, slotId, details);
    pthread_mutex_unlock(manager_mutex(manager));
}

bool
SlotManagerDetectAnomalies(SlotManager *manager)
{
    size_t i;
    bool anomalyDetected = false;
    uint64_t nowUs;

    if (manager == NULL || manager->mutex == NULL)
        return false;

    pthread_mutex_lock(manager_mutex(manager));
    if (!manager->securityEnabled || manager->securityContext == NULL) {
        pthread_mutex_unlock(manager_mutex(manager));
        return false;
    }

    if (manager->securityViolations > 0)
        anomalyDetected = true;

    nowUs = SecureTimestamp();
    for (i = 0; i < manager->tableSize; i++) {
        SlotEntry *entry = &manager->slotTable[i];
        if (!entry->occupied || !entry->securityEnabled)
            continue;
        if (entry->accessCount > 1000 &&
            nowUs > entry->lastAccessTime &&
            (nowUs - entry->lastAccessTime) < 1000000ULL) {
            anomalyDetected = true;
        }
    }

    if (manager->securityContext != NULL)
        anomalyDetected |= SecurityDetectAnomalies(manager->securityContext);

    pthread_mutex_unlock(manager_mutex(manager));
    return anomalyDetected;
}

void
SlotManagerPrintSecurityStats(const SlotManager *manager)
{
    size_t active = 0;
    size_t secure = 0;
    size_t i;
    bool securityEnabled;

    if (manager == NULL || manager->mutex == NULL) {
        printf("SlotManager is NULL\n");
        return;
    }

    pthread_mutex_lock((pthread_mutex_t *)manager->mutex);
    securityEnabled = manager->securityEnabled && manager->securityContext != NULL;
    for (i = 0; i < manager->tableSize; i++) {
        if (!manager->slotTable[i].occupied)
            continue;
        active++;
        if (manager->slotTable[i].securityEnabled)
            secure++;
    }

    printf("=== Slot Manager Security Statistics ===\n");
    printf("Security enabled: %s\n", securityEnabled ? "Yes" : "No");
    printf("Default level: %d\n", (int)manager->defaultSecurityLevel);
    printf("Security violations: %llu\n",
           (unsigned long long)manager->securityViolations);
    printf("Active slots: %zu\n", active);
    printf("Secure slots: %zu\n", secure);
    if (manager->securityContext != NULL)
        SecurityPrintStatistics(manager->securityContext);
    printf("========================================\n");
    pthread_mutex_unlock((pthread_mutex_t *)manager->mutex);
}
