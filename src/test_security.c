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

#include "../runtime/slot_manager.h"
#include "../runtime/slot_security.h"

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
void test_security_context_lifecycle()
{
    printf("\n=== Test 1: Security Context Lifecycle ===\n");
    
    SecurityContext *context = SecurityContextCreate(SECURITY_LEVEL_BASIC);
    TEST_ASSERT(context != NULL, "Security context creation");
    TEST_ASSERT(context->initialized, "Security context initialization");
    TEST_ASSERT(context->defaultLevel == SECURITY_LEVEL_BASIC, 
                "Default security level setting");
    
    SecurityContextDestroy(context);
    printf("Security context destroyed successfully\n");
}

/*
 * Test 2: Hardware fingerprint generation and comparison
 */
void test_hardware_fingerprint()
{
    printf("\n=== Test 2: Hardware Fingerprint ===\n");
    
    HardwareFingerprint fp1, fp2;
    
    SecurityError result1 = HardwareFingerprintGenerate(&fp1);
    SecurityError result2 = HardwareFingerprintGenerate(&fp2);
    
    TEST_ASSERT(result1 == SECURITY_SUCCESS, "Hardware fingerprint generation 1");
    TEST_ASSERT(result2 == SECURITY_SUCCESS, "Hardware fingerprint generation 2");
    
    TEST_ASSERT(HardwareFingerprintCompare(&fp1, &fp2), 
                "Hardware fingerprint consistency");
    
    printf("Hardware fingerprint: CPU=0x%llx, Board=0x%llx, MAC=0x%llx\n",
           (unsigned long long)fp1.cpuId,
           (unsigned long long)fp1.boardId, 
           (unsigned long long)fp1.macAddress);
}

/*
 * Test 3: Token generation and validation
 */
void test_token_operations()
{
    printf("\n=== Test 3: Token Operations ===\n");
    
    SecurityContext *context = SecurityContextCreate(SECURITY_LEVEL_HARDWARE);
    TEST_ASSERT(context != NULL, "Security context for token tests");
    
    TokenCapability token;
    SecurityError result = TokenGenerate(context, 123, SECURITY_LEVEL_HARDWARE, &token);
    TEST_ASSERT(result == SECURITY_SUCCESS, "Token generation");
    TEST_ASSERT(token.slotId == 123, "Token slot ID assignment");
    TEST_ASSERT(token.level == SECURITY_LEVEL_HARDWARE, "Token security level");
    TEST_ASSERT(token.canRead && token.canWrite, "Token default permissions");
    
    /* Validate the same token */
    result = TokenValidate(context, 123, &token);
    TEST_ASSERT(result == SECURITY_SUCCESS, "Token validation (valid token)");
    
    /* Test invalid slot ID */
    result = TokenValidate(context, 456, &token);
    TEST_SECURITY_VIOLATION(result != SECURITY_SUCCESS, 
                           "Token validation rejects wrong slot ID");
    
    SecurityContextDestroy(context);
}

/*
 * Test 4: Secure slot manager operations
 */
void test_secure_slot_manager()
{
    printf("\n=== Test 4: Secure Slot Manager ===\n");
    
    /* Create slot manager with security */
    SlotManager *manager = SlotManagerCreateSecure(1000, 64*1024, true, 
                                                  SECURITY_LEVEL_HARDWARE);
    TEST_ASSERT(manager != NULL, "Secure slot manager creation");
    TEST_ASSERT(SlotManagerIsSecurityEnabled(manager), "Security enabled check");
    
    g_pergyraSlotManager = manager; /* Set global reference */
    
    /* Test secure slot claiming */
    SlotHandle handle;
    TokenCapability token;
    SlotError result = SlotClaimSecure(manager, TYPE_INT, SECURITY_LEVEL_HARDWARE,
                                     &handle, &token);
    TEST_ASSERT(result == SLOT_SUCCESS, "Secure slot claiming");
    
    /* Test secure writing */
    int testValue = 42;
    result = SlotWriteSecure(manager, &handle, &testValue, sizeof(testValue), &token);
    TEST_ASSERT(result == SLOT_SUCCESS, "Secure slot writing");
    
    /* Test secure reading */
    int readValue = 0;
    size_t bytesRead;
    result = SlotReadSecure(manager, &handle, &readValue, sizeof(readValue), 
                          &bytesRead, &token);
    TEST_ASSERT(result == SLOT_SUCCESS, "Secure slot reading");
    TEST_ASSERT(readValue == testValue, "Data integrity verification");
    
    /* Test secure release */
    result = SlotReleaseSecure(manager, &handle, &token);
    TEST_ASSERT(result == SLOT_SUCCESS, "Secure slot release");
    
    SlotManagerDestroySecure(manager);
    g_pergyraSlotManager = NULL;
}

/*
 * Test 5: Security violation detection
 */
void test_security_violations()
{
    printf("\n=== Test 5: Security Violation Detection ===\n");
    
    SlotManager *manager = SlotManagerCreateSecure(100, 8*1024, true, 
                                                  SECURITY_LEVEL_ENCRYPTED);
    g_pergyraSlotManager = manager;
    
    SlotHandle handle;
    TokenCapability validToken, invalidToken;
    
    /* Create valid slot and token */
    SlotError result = SlotClaimSecure(manager, TYPE_INT, SECURITY_LEVEL_ENCRYPTED,
                                     &handle, &validToken);
    TEST_ASSERT(result == SLOT_SUCCESS, "Valid slot creation for violation tests");
    
    /* Create invalid token (different slot ID) */
    SecurityContext *context = manager->securityContext;
    TokenGenerate(context, 9999, SECURITY_LEVEL_ENCRYPTED, &invalidToken);
    
    /* Test write with invalid token */
    int testValue = 123;
    result = SlotWriteSecure(manager, &handle, &testValue, sizeof(testValue), 
                           &invalidToken);
    TEST_SECURITY_VIOLATION(result != SLOT_SUCCESS, 
                           "Write with invalid token blocked");
    
    /* Test read with invalid token */
    int readValue;
    size_t bytesRead;
    result = SlotReadSecure(manager, &handle, &readValue, sizeof(readValue), 
                          &bytesRead, &invalidToken);
    TEST_SECURITY_VIOLATION(result != SLOT_SUCCESS, 
                           "Read with invalid token blocked");
    
    /* Test token without write permission */
    TokenCapability readOnlyToken = validToken;
    readOnlyToken.canWrite = false;
    
    result = SlotWriteSecure(manager, &handle, &testValue, sizeof(testValue), 
                           &readOnlyToken);
    TEST_SECURITY_VIOLATION(result != SLOT_SUCCESS, 
                           "Write without permission blocked");
    
    /* Check anomaly detection */
    bool anomalies = SlotManagerDetectAnomalies(manager);
    TEST_ASSERT(anomalies, "Anomaly detection identifies violations");
    
    /* Print security statistics */
    SlotManagerPrintSecurityStats(manager);
    
    /* Cleanup */
    SlotReleaseSecure(manager, &handle, &validToken);
    SlotManagerDestroySecure(manager);
    g_pergyraSlotManager = NULL;
}

/*
 * Test 6: Scope-based slot management
 */
void test_scope_based_slots()
{
    printf("\n=== Test 6: Scope-based Slot Management ===\n");
    
    SlotManager *manager = SlotManagerCreateSecure(100, 8*1024, true, 
                                                  SECURITY_LEVEL_BASIC);
    
    /* Create scope */
    SecureSlotScope *scope = SecureSlotScopeCreate(manager, 10);
    TEST_ASSERT(scope != NULL, "Secure slot scope creation");
    
    /* Claim multiple slots in scope */
    SlotHandle *handle1, *handle2;
    TokenCapability *token1, *token2;
    
    SlotError result1 = SecureSlotScopeClaimSlot(scope, TYPE_INT, 
                                               SECURITY_LEVEL_BASIC, 
                                               &handle1, &token1);
    SlotError result2 = SecureSlotScopeClaimSlot(scope, TYPE_FLOAT, 
                                               SECURITY_LEVEL_BASIC, 
                                               &handle2, &token2);
    
    TEST_ASSERT(result1 == SLOT_SUCCESS, "Scope slot claiming 1");
    TEST_ASSERT(result2 == SLOT_SUCCESS, "Scope slot claiming 2");
    
    /* Test writing to scoped slots */
    int intValue = 100;
    float floatValue = 3.14f;
    
    SlotError writeResult1 = SlotWriteSecure(manager, handle1, &intValue, 
                                           sizeof(intValue), token1);
    SlotError writeResult2 = SlotWriteSecure(manager, handle2, &floatValue, 
                                           sizeof(floatValue), token2);
    
    TEST_ASSERT(writeResult1 == SLOT_SUCCESS, "Scoped slot write 1");
    TEST_ASSERT(writeResult2 == SLOT_SUCCESS, "Scoped slot write 2");
    
    /* Destroy scope (should auto-release all slots) */
    SecureSlotScopeDestroy(scope);
    printf("Scope destroyed - all slots auto-released\n");
    
    SlotManagerDestroySecure(manager);
}

/*
 * Test 7: Pergyra language-level API
 */
void test_pergyra_api()
{
    printf("\n=== Test 7: Pergyra Language API ===\n");
    
    SlotManager *manager = SlotManagerCreateSecure(100, 8*1024, true, 
                                                  SECURITY_LEVEL_HARDWARE);
    g_pergyraSlotManager = manager;
    
    /* Test high-level API */
    PergyraSecureSlot *slot = pergyra_claim_secure_slot(manager, "Int", 
                                                       SECURITY_LEVEL_HARDWARE);
    TEST_ASSERT(slot != NULL, "Pergyra secure slot creation");
    TEST_ASSERT(slot->isValid, "Pergyra slot validity");
    
    /* Test writing and reading */
    int testValue = 2025;
    bool writeSuccess = pergyra_slot_write_secure(slot, &testValue, sizeof(testValue));
    TEST_ASSERT(writeSuccess, "Pergyra secure write");
    
    int readValue;
    size_t bytesRead;
    bool readSuccess = pergyra_slot_read_secure(slot, &readValue, sizeof(readValue), 
                                              &bytesRead);
    TEST_ASSERT(readSuccess, "Pergyra secure read");
    TEST_ASSERT(readValue == testValue, "Pergyra data integrity");
    
    /* Test scope-based API */
    PergyraSlotScope *pscope = pergyra_scope_begin(manager);
    TEST_ASSERT(pscope != NULL, "Pergyra scope creation");
    
    PergyraSecureSlot *scopedSlot = pergyra_scope_claim_slot(pscope, "String", 
                                                           SECURITY_LEVEL_HARDWARE);
    TEST_ASSERT(scopedSlot != NULL, "Pergyra scoped slot creation");
    
    /* Cleanup */
    pergyra_slot_release_secure(slot);
    pergyra_scope_end(pscope);
    free(scopedSlot); /* Manual cleanup for test - normally handled by scope */
    
    SlotManagerDestroySecure(manager);
    g_pergyraSlotManager = NULL;
    
    /* Print usage example */
    pergyra_security_audit_usage_example();
}

/*
 * Test 8: Performance and stress testing
 */
void test_performance()
{
    printf("\n=== Test 8: Performance Testing ===\n");
    
    SlotManager *manager = SlotManagerCreateSecure(10000, 1024*1024, true, 
                                                  SECURITY_LEVEL_BASIC);
    g_pergyraSlotManager = manager;
    
    const int NUM_OPERATIONS = 1000;
    clock_t start, end;
    
    /* Test token generation performance */
    start = clock();
    for (int i = 0; i < NUM_OPERATIONS; i++) {
        TokenCapability token;
        TokenGenerate(manager->securityContext, i, SECURITY_LEVEL_BASIC, &token);
    }
    end = clock();
    
    double tokenGenTime = ((double)(end - start)) / CLOCKS_PER_SEC;
    printf("Token generation: %d operations in %.3f seconds (%.1f ops/sec)\n",
           NUM_OPERATIONS, tokenGenTime, NUM_OPERATIONS / tokenGenTime);
    
    /* Test secure slot operations performance */
    SlotHandle handles[100];
    TokenCapability tokens[100];
    
    start = clock();
    for (int i = 0; i < 100; i++) {
        SlotClaimSecure(manager, TYPE_INT, SECURITY_LEVEL_BASIC, 
                       &handles[i], &tokens[i]);
    }
    end = clock();
    
    double claimTime = ((double)(end - start)) / CLOCKS_PER_SEC;
    printf("Secure slot claiming: 100 operations in %.3f seconds\n", claimTime);
    
    /* Test write/read performance */
    start = clock();
    for (int i = 0; i < 100; i++) {
        int value = i;
        SlotWriteSecure(manager, &handles[i], &value, sizeof(value), &tokens[i]);
        
        int readValue;
        size_t bytesRead;
        SlotReadSecure(manager, &handles[i], &readValue, sizeof(readValue), 
                      &bytesRead, &tokens[i]);
    }
    end = clock();
    
    double rwTime = ((double)(end - start)) / CLOCKS_PER_SEC;
    printf("Secure read/write: 200 operations in %.3f seconds\n", rwTime);
    
    /* Cleanup */
    for (int i = 0; i < 100; i++) {
        SlotReleaseSecure(manager, &handles[i], &tokens[i]);
    }
    
    SlotManagerDestroySecure(manager);
    g_pergyraSlotManager = NULL;
}

/*
 * Main test runner
 */
int main(int argc, char *argv[])
{
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
    
    /* Print final results */
    print_test_results();
    
    /* Return appropriate exit code */
    return (g_testStats.failedTests == 0) ? 0 : 1;
}
