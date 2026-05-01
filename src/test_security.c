/*
 * Copyright (c) 2025 Pergyra Language Project
 * All rights reserved.
 *
 * Security System Test Suite
 *
 * This test suite validates the secure slot system including:
 * - Token generation and validation
 * - Hardware binding
 * - Encryption/decryption
 * - Access control
 * - Security violation detection
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <time.h>
#include <unistd.h>
#include <sys/stat.h>

#ifndef _WIN32
extern int setenv(const char *name, const char *value, int overwrite);
extern int unsetenv(const char *name);
extern int symlink(const char *target, const char *linkpath);
#endif

#include "runtime/slot_manager.h"
#include "runtime/slot_security.h"
#include "runtime/pgy_runtime.h"

/* Global test manager */
SlotManager *g_pergyraSlotManager = NULL;

/* Test statistics */
typedef struct {
    int totalTests;
    int passedTests;
    int failedTests;
    int securityViolations;
} TestStats;

static TestStats g_testStats = {0, 0, 0, 0};

/*
 * Test utilities
 */
#define TEST_ASSERT(condition, message) \
    do { \
        g_testStats.totalTests++; \
        if (condition) { \
            g_testStats.passedTests++; \
            printf("[PASS] %s\n", message); \
        } else { \
            g_testStats.failedTests++; \
            printf("[FAIL] %s\n", message); \
        } \
    } while(0)

#define TEST_SECURITY_VIOLATION(condition, message) \
    do { \
        g_testStats.totalTests++; \
        if (condition) { \
            g_testStats.passedTests++; \
            g_testStats.securityViolations++; \
            printf("[SECURITY] %s\n", message); \
        } else { \
            g_testStats.failedTests++; \
            printf("[FAIL] Security test failed: %s\n", message); \
        } \
    } while(0)

static SlotEntry *
test_find_slot_entry(SlotManager *manager, const SlotHandle *handle)
{
    if (manager == NULL || handle == NULL)
        return NULL;

    for (size_t i = 0; i < manager->tableSize; i++) {
        SlotEntry *entry = &manager->slotTable[i];
        if (entry->occupied && entry->slotId == handle->slotId)
            return entry;
    }
    return NULL;
}

void print_test_results()
{
    printf("\n=== TEST RESULTS ===\n");
    printf("Total Tests: %d\n", g_testStats.totalTests);
    printf("Passed: %d\n", g_testStats.passedTests);
    printf("Failed: %d\n", g_testStats.failedTests);
    printf("Security Violations Detected: %d\n", g_testStats.securityViolations);
    printf("Success Rate: %.1f%%\n",
           g_testStats.totalTests > 0 ?
           (g_testStats.passedTests * 100.0 / g_testStats.totalTests) : 0.0);
    printf("====================\n");
}

/*
 * Test 1: Basic security context creation and destruction
 */

#include "tests/security/test_security_runtime.cases.h"

int main(int argc, char *argv[])
{
    (void)argc;
    (void)argv;
    printf("===== Pergyra Security System Test Suite =====\n");
    printf("Testing secure slot-based memory management...\n");

    /* Initialize test environment */
    srand((unsigned int)time(NULL));

    /* Run all tests */
    test_security_context_lifecycle();
    test_hardware_fingerprint();
    test_token_operations();
    test_secure_slot_manager();
    test_security_violations();
    test_scope_based_slots();
    test_pergyra_api();
    test_performance();
    test_sealed_storage_and_shadow_recovery();
    test_runtime_file_io_policy();
    test_runtime_zone_authority_validation();
    test_slot_pin_lease_runtime();

    /* Print final results */
    print_test_results();

    /* Return appropriate exit code */
    return (g_testStats.failedTests == 0) ? 0 : 1;
}
