/*
 * Copyright (c) 2025 Pergyra Language Project
 * All rights reserved.
 *
 * Slot manager security audit/statistics helpers.
 */

#include "slot_manager.h"
#include "pgy_runtime_security_log.h"

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
SlotManagerLogSecurityEvent(SlotManager *manager, const char *event,
                            uint32_t slotId, const char *details)
{
    time_t now;
    struct tm tmNow;
    char timestamp[32];

    if (manager == NULL || event == NULL || !SlotManagerIsSecurityEnabled(manager))
        return;

    now = time(NULL);
    if (slot_security_localtime(now, &tmNow))
        strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", &tmNow);
    else
        snprintf(timestamp, sizeof(timestamp), "time-unavailable");

    fputs("{\"component\":\"slot-security\",\"timestamp\":", stdout);
    pgy_runtime_fprint_json_string(stdout, timestamp);
    fprintf(stdout, ",\"slot\":%u,\"event\":", slotId);
    pgy_runtime_fprint_json_string(stdout, event);
    fputs(",\"details\":", stdout);
    pgy_runtime_fprint_json_string(stdout, details != NULL ? details : "n/a");
    fputs("}\n", stdout);

    if (manager->securityContext != NULL)
        SecurityAuditLog(manager->securityContext, event, details);
}

bool
SlotManagerDetectAnomalies(SlotManager *manager)
{
    size_t i;
    bool anomalyDetected = false;
    uint64_t nowUs;

    if (manager == NULL || !SlotManagerIsSecurityEnabled(manager))
        return false;

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

    return anomalyDetected;
}

void
SlotManagerPrintSecurityStats(const SlotManager *manager)
{
    size_t active = 0;
    size_t secure = 0;
    size_t i;

    if (manager == NULL) {
        printf("SlotManager is NULL\n");
        return;
    }

    for (i = 0; i < manager->tableSize; i++) {
        if (!manager->slotTable[i].occupied)
            continue;
        active++;
        if (manager->slotTable[i].securityEnabled)
            secure++;
    }

    printf("=== Slot Manager Security Statistics ===\n");
    printf("Security enabled: %s\n",
           SlotManagerIsSecurityEnabled(manager) ? "Yes" : "No");
    printf("Default level: %d\n", (int)manager->defaultSecurityLevel);
    printf("Security violations: %llu\n",
           (unsigned long long)manager->securityViolations);
    printf("Active slots: %zu\n", active);
    printf("Secure slots: %zu\n", secure);
    if (manager->securityContext != NULL)
        SecurityPrintStatistics(manager->securityContext);
    printf("========================================\n");
}
