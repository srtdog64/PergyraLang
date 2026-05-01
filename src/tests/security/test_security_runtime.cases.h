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
 * Test 9: Sealed storage and shadow recovery
 */
void test_sealed_storage_and_shadow_recovery()
{
    SlotManager *manager;
    SlotHandle handle;
    TokenCapability token;
    SlotEntry *entry = NULL;
    int testValue = 0x12345678;
    int readValue = 0;
    size_t bytesRead = 0;
    size_t i;
    SlotError result;

    printf("\n=== Test 9: Sealed Storage And Shadow Recovery ===\n");

    manager = SlotManagerCreateSecure(32, 4096, true, SECURITY_LEVEL_HARDWARE);
    g_pergyraSlotManager = manager;

    result = SlotClaimSecure(manager, TYPE_INT, SECURITY_LEVEL_HARDWARE, &handle, &token);
    TEST_ASSERT(result == SLOT_SUCCESS, "Shadow recovery slot claim");

    result = SlotWriteSecure(manager, &handle, &testValue, sizeof(testValue), &token);
    TEST_ASSERT(result == SLOT_SUCCESS, "Shadow recovery slot write");

    for (i = 0; i < manager->tableSize; i++) {
        if (manager->slotTable[i].occupied &&
            manager->slotTable[i].slotId == handle.slotId) {
            entry = &manager->slotTable[i];
            break;
        }
    }

    TEST_ASSERT(entry != NULL, "Shadow recovery slot lookup");
    TEST_ASSERT(entry != NULL && entry->securityPolicy.storageMode == SECURE_SLOT_STORAGE_SEALED,
                "Secure slot uses sealed storage policy");
    TEST_ASSERT(entry != NULL &&
                SecureSealedPayloadPrimaryBytes(&entry->securePayload) != NULL &&
                memcmp(SecureSealedPayloadPrimaryBytes(&entry->securePayload),
                       &testValue, sizeof(testValue)) != 0,
                "In-memory primary payload is obfuscated");
    TEST_ASSERT(entry != NULL &&
                SecureSealedPayloadShadowBytes(&entry->securePayload) != NULL &&
                memcmp(SecureSealedPayloadShadowBytes(&entry->securePayload),
                       &testValue, sizeof(testValue)) != 0,
                "In-memory shadow payload is obfuscated");

    ((uint8_t *)SecureSealedPayloadPrimaryBytes(&entry->securePayload))[0] ^= 0x5a;

    result = SlotReadSecure(manager, &handle, &readValue, sizeof(readValue), &bytesRead, &token);
    TEST_ASSERT(result == SLOT_SUCCESS, "Shadow copy recovers corrupted primary");
    TEST_ASSERT(readValue == testValue, "Recovered value matches original");

    ((uint8_t *)SecureSealedPayloadPrimaryBytes(&entry->securePayload))[0] ^= 0x33;
    ((uint8_t *)SecureSealedPayloadShadowBytes(&entry->securePayload))[0] ^= 0x77;

    result = SlotReadSecure(manager, &handle, &readValue, sizeof(readValue), &bytesRead, &token);
    TEST_SECURITY_VIOLATION(result != SLOT_SUCCESS,
                            "Read fails when primary and shadow copies are both corrupted");

    SlotReleaseSecure(manager, &handle, &token);
    SlotManagerDestroySecure(manager);
    g_pergyraSlotManager = NULL;
}

void test_runtime_file_io_policy()
{
    const char *ok_path = "pgy_security_io_ok.txt";
    const char *abs_path = "/tmp/pgy_security_io_abs.txt";
    const char *escape_path = "../pgy_security_io_escape.txt";
    const char *root_dir = "pgy_security_root";
    const char *outside_dir = "pgy_security_outside";
    const char *root_file = "inside.txt";
    const char *escape_link = "pgy_security_root/escape";
    char rooted_path[256];
    char outside_path[256];
    char *content;

    printf("\n=== Test 10: Runtime File I/O Policy ===\n");

    unlink(ok_path);
    unlink(abs_path);
    unlink(escape_path);
    unlink(escape_link);
    unlink("pgy_security_root/inside.txt");
    rmdir(root_dir);
    rmdir(outside_dir);
    mkdir(root_dir, 0700);
    mkdir(outside_dir, 0700);
    snprintf(rooted_path, sizeof(rooted_path), "%s/%s", root_dir, root_file);
    snprintf(outside_path, sizeof(outside_path), "%s/blocked.txt", outside_dir);

    pgy_write_file(ok_path, "ok");
    content = pgy_read_file(ok_path);
    TEST_ASSERT(content != NULL && strcmp(content, "ok") == 0,
                "Relative runtime file I/O remains allowed");
    free(content);
    unlink(ok_path);

    setenv("PGY_IO_ROOT", root_dir, 1);
    pgy_write_file(abs_path, "blocked");
    TEST_SECURITY_VIOLATION(access(abs_path, F_OK) != 0,
                            "Absolute runtime file writes are denied by default");

    pgy_write_file(root_file, "rooted");
    content = pgy_read_file(root_file);
    TEST_ASSERT(content != NULL && strcmp(content, "rooted") == 0
                && access(rooted_path, F_OK) == 0,
                "Relative runtime file I/O is rooted under PGY_IO_ROOT");
    free(content);

    pgy_write_file(escape_path, "blocked");
    TEST_SECURITY_VIOLATION(access(escape_path, F_OK) != 0,
                            "Parent-traversal runtime file writes are denied");

    content = pgy_read_file(escape_path);
    TEST_SECURITY_VIOLATION(content != NULL && content[0] == '\0',
                            "Parent-traversal runtime file reads are denied");
    free(content);

#ifndef _WIN32
    symlink("../pgy_security_outside", escape_link);
    pgy_write_file("escape/blocked.txt", "blocked");
    TEST_SECURITY_VIOLATION(access(outside_path, F_OK) != 0,
                            "Symlink escape outside PGY_IO_ROOT is denied");
#endif

    unsetenv("PGY_IO_ROOT");
    unlink(rooted_path);
    unlink(outside_path);
    unlink(escape_link);
    rmdir(root_dir);
    rmdir(outside_dir);
}

void test_runtime_zone_authority_validation()
{
    int zone = 1;
    int participant = 7;

    printf("\n=== Test 11: Runtime Zone Authority Validation ===\n");

    TEST_ASSERT(pgy_zone_authority_validate(&zone, &participant,
                                            "BattleZone", "owner"),
                "Zone authority validation accepts non-null zone and participant");
    TEST_ASSERT(pgy_zone_authority_last_ok_export(),
                "Zone authority validation records success state");
    TEST_ASSERT(strcmp(pgy_zone_authority_last_zone_export(), "BattleZone") == 0,
                "Zone authority validation records last zone");
    TEST_ASSERT(strcmp(pgy_zone_authority_last_participant_export(), "owner") == 0,
                "Zone authority validation records last participant");
    TEST_ASSERT(strcmp(pgy_zone_authority_last_code_export(), "ok") == 0,
                "Zone authority validation records ok code");
    TEST_ASSERT(strcmp(pgy_zone_authority_last_reason_export(), "") == 0,
                "Zone authority validation clears failure reason after success");
    TEST_SECURITY_VIOLATION(!pgy_zone_authority_validate(NULL, &participant,
                                                         "BattleZone", "owner"),
                            "Zone authority validation rejects null zone");
    TEST_ASSERT(!pgy_zone_authority_last_ok_export(),
                "Zone authority validation records failed state for null zone");
    TEST_ASSERT(strcmp(pgy_zone_authority_last_code_export(), "missing-zone") == 0,
                "Zone authority validation records null-zone failure code");
    TEST_ASSERT(strcmp(pgy_zone_authority_last_reason_export(),
                       "zone authority validation failed: null zone self") == 0,
                "Zone authority validation records null-zone failure reason");
    TEST_SECURITY_VIOLATION(!pgy_zone_authority_validate(&zone, NULL,
                                                         "BattleZone", "owner"),
                            "Zone authority validation rejects null participant");
    TEST_ASSERT(!pgy_zone_authority_last_ok_export(),
                "Zone authority validation records failed state for null participant");
    TEST_ASSERT(strcmp(pgy_zone_authority_last_code_export(), "missing-participant") == 0,
                "Zone authority validation records null-participant failure code");
    TEST_ASSERT(strcmp(pgy_zone_authority_last_reason_export(),
                       "zone authority validation failed: null authority participant") == 0,
                "Zone authority validation records null-participant failure reason");
    TEST_ASSERT(pgy_zone_authority_validate_flags_export(true, true,
                                                         "BattleZone", "owner"),
                "Zone authority flag export accepts present zone and participant");
    TEST_ASSERT(pgy_zone_authority_last_ok_rt_export(),
                "Zone authority flag export records success via runtime accessors");
    TEST_ASSERT(strcmp(pgy_zone_authority_last_zone_rt_export(), "BattleZone") == 0,
                "Zone authority runtime accessor records last zone");
    TEST_ASSERT(strcmp(pgy_zone_authority_last_participant_rt_export(), "owner") == 0,
                "Zone authority runtime accessor records last participant");
    TEST_ASSERT(strcmp(pgy_zone_authority_last_code_rt_export(), "ok") == 0,
                "Zone authority runtime accessor records ok code");
    TEST_ASSERT(strcmp(pgy_zone_authority_last_reason_rt_export(), "") == 0,
                "Zone authority runtime accessor clears failure reason after success");
    TEST_SECURITY_VIOLATION(!pgy_zone_authority_validate_flags_export(false, true,
                                                                      "BattleZone", "owner"),
                            "Zone authority flag export rejects missing zone without aborting");
    TEST_ASSERT(!pgy_zone_authority_last_ok_rt_export(),
                "Zone authority runtime accessor records failed state for missing zone");
    TEST_ASSERT(strcmp(pgy_zone_authority_last_code_rt_export(), "missing-zone") == 0,
                "Zone authority runtime accessor records missing-zone code");
    TEST_ASSERT(strcmp(pgy_zone_authority_last_reason_rt_export(),
                       "zone authority validation failed: null zone self") == 0,
                "Zone authority runtime accessor records missing-zone reason");
    TEST_SECURITY_VIOLATION(!pgy_zone_authority_validate_flags_export(true, false,
                                                                      "BattleZone", "owner"),
                            "Zone authority flag export rejects missing participant without aborting");
    TEST_ASSERT(!pgy_zone_authority_last_ok_rt_export(),
                "Zone authority runtime accessor records failed state for missing participant");
    TEST_ASSERT(strcmp(pgy_zone_authority_last_code_rt_export(), "missing-participant") == 0,
                "Zone authority runtime accessor records missing-participant code");
    TEST_ASSERT(strcmp(pgy_zone_authority_last_reason_rt_export(),
                       "zone authority validation failed: null authority participant") == 0,
                            "Zone authority runtime accessor records missing-participant reason");
    TEST_ASSERT(pgy_zone_authority_validate_token_flags_export(true, true,
                                                               777, 777,
                                                               "BattleZone", "owner"),
                "Zone authority token flag export accepts matching token");
    TEST_ASSERT(strcmp(pgy_zone_authority_last_code_rt_export(), "ok") == 0,
                "Zone authority token flag export records ok code");
    TEST_SECURITY_VIOLATION(!pgy_zone_authority_validate_token_flags_export(true, true,
                                                                            777, 888,
                                                                            "BattleZone", "owner"),
                            "Zone authority token flag export rejects mismatched token");
    TEST_ASSERT(!pgy_zone_authority_last_ok_rt_export(),
                "Zone authority runtime accessor records failed state for token mismatch");
    TEST_ASSERT(strcmp(pgy_zone_authority_last_code_rt_export(), "authority-token-mismatch") == 0,
                "Zone authority runtime accessor records token-mismatch code");
    TEST_ASSERT(strcmp(pgy_zone_authority_last_reason_rt_export(),
                       "zone authority validation failed: authority token mismatch") == 0,
                "Zone authority runtime accessor records token-mismatch reason");
}

void test_slot_pin_lease_runtime()
{
    SlotManager *plainManager;
    SlotManager *secureManager;
    SlotHandle plainHandle;
    SlotHandle staleHandle;
    SlotHandle secureHandle;
    SlotHandle revokedHandle;
    TokenCapability token;
    TokenCapability revokedToken;
    TokenCapability invalidToken;
    PgyPinnedView view;
    PgyPinnedView tamperedView;
    SlotEntry *entry;
    int value;
    int readValue;
    size_t bytesRead = 0;
    SlotError result;

    printf("\n=== Test 12: Slot Pin Lease Runtime ===\n");

    plainManager = SlotManagerCreate(16, 4096);
    TEST_ASSERT(plainManager != NULL, "Plain slot manager for pin tests");
    plainManager->nextSlotId = 0;
    result = SlotClaim(plainManager, TYPE_INT, &plainHandle);
    TEST_SECURITY_VIOLATION(result == SLOT_ERROR_OUT_OF_MEMORY,
                            "Zero slot id sentinel is tombstoned before claim");
    plainManager->nextSlotId = UINT32_MAX;
    result = SlotClaim(plainManager, TYPE_INT, &plainHandle);
    TEST_SECURITY_VIOLATION(result == SLOT_ERROR_OUT_OF_MEMORY,
                            "Slot id wrap is tombstoned before reuse");
    plainManager->nextSlotId = 1;
    result = SlotClaim(plainManager, TYPE_INT, &plainHandle);
    TEST_ASSERT(result == SLOT_SUCCESS, "Plain slot claim for pin");
    value = 42;
    result = SlotWrite(plainManager, &plainHandle, &value, sizeof(value));
    TEST_ASSERT(result == SLOT_SUCCESS, "Plain slot write before pin");
    result = PergyraSlotPin(plainManager, &plainHandle, PGY_SLOT_PIN_WRITE,
                            &token, &view);
    TEST_SECURITY_VIOLATION(result == SLOT_ERROR_PERMISSION_DENIED
                                && !view.valid,
                            "Plain slot rejects token-bearing pin");
    result = PergyraSlotPin(plainManager, &plainHandle, PGY_SLOT_PIN_READ,
                            NULL, &view);
    TEST_ASSERT(result == SLOT_SUCCESS && view.valid && view.ptr != NULL,
                "Plain slot read pin succeeds");
    TEST_ASSERT(*(int *)view.ptr == 42, "Plain pinned read view exposes payload");
    tamperedView = view;
    tamperedView.generation++;
    result = PergyraSlotUnpin(plainManager, &tamperedView);
    TEST_SECURITY_VIOLATION(result == SLOT_ERROR_INVALID_PIN,
                            "Tampered pinned view generation cannot unpin");
    result = SlotRelease(plainManager, &plainHandle);
    TEST_SECURITY_VIOLATION(result == SLOT_ERROR_PINNED,
                            "Tampered unpin leaves plain slot pinned");
    result = PergyraSlotUnpin(plainManager, &view);
    TEST_ASSERT(result == SLOT_SUCCESS && !view.valid, "Plain slot unpin succeeds");
    result = PergyraSlotUnpin(plainManager, &view);
    TEST_SECURITY_VIOLATION(result == SLOT_ERROR_INVALID_PIN,
                            "Plain slot double unpin is invalid");
    result = SlotRelease(plainManager, &plainHandle);
    TEST_ASSERT(result == SLOT_SUCCESS, "Plain slot release after unpin succeeds");

    result = SlotClaim(plainManager, TYPE_INT, &plainHandle);
    TEST_ASSERT(result == SLOT_SUCCESS, "Generation guard slot claim");
    value = 77;
    result = SlotWrite(plainManager, &plainHandle, &value, sizeof(value));
    TEST_ASSERT(result == SLOT_SUCCESS, "Generation guard slot write");
    staleHandle = plainHandle;
    staleHandle.generation++;
    readValue = 0;
    result = SlotRead(plainManager, &staleHandle, &readValue, sizeof(readValue),
                      &bytesRead);
    TEST_SECURITY_VIOLATION(result == SLOT_ERROR_SLOT_NOT_FOUND,
                            "Stale generation handle cannot read");
    value = 88;
    result = SlotWrite(plainManager, &staleHandle, &value, sizeof(value));
    TEST_SECURITY_VIOLATION(result == SLOT_ERROR_SLOT_NOT_FOUND,
                            "Stale generation handle cannot write");
    result = PergyraSlotPin(plainManager, &staleHandle, PGY_SLOT_PIN_READ,
                            NULL, &view);
    TEST_SECURITY_VIOLATION(result == SLOT_ERROR_SLOT_NOT_FOUND && !view.valid,
                            "Stale generation handle cannot pin");
    TEST_ASSERT(!SlotIsValid(plainManager, &staleHandle),
                "Stale generation handle is not valid");
    result = SlotRelease(plainManager, &staleHandle);
    TEST_SECURITY_VIOLATION(result == SLOT_ERROR_SLOT_NOT_FOUND,
                            "Stale generation handle cannot release");
    result = SlotRelease(plainManager, &plainHandle);
    TEST_ASSERT(result == SLOT_SUCCESS,
                "Generation guard original handle still releases");

    result = SlotClaimScoped(plainManager, TYPE_INT, 77, &plainHandle);
    TEST_ASSERT(result == SLOT_SUCCESS, "Scoped slot claim for pin");
    value = 12;
    result = SlotWrite(plainManager, &plainHandle, &value, sizeof(value));
    TEST_ASSERT(result == SLOT_SUCCESS, "Scoped slot write before pin");
    result = PergyraSlotPin(plainManager, &plainHandle, PGY_SLOT_PIN_READ,
                            NULL, &view);
    TEST_ASSERT(result == SLOT_SUCCESS && view.valid,
                "Scoped slot pin succeeds");
    result = SlotReleaseScope(plainManager, 77);
    TEST_SECURITY_VIOLATION(result == SLOT_ERROR_PINNED,
                            "Scope release reports pinned slot");
    result = PergyraSlotUnpin(plainManager, &view);
    TEST_ASSERT(result == SLOT_SUCCESS, "Scoped slot unpin succeeds");
    result = SlotReleaseScope(plainManager, 77);
    TEST_ASSERT(result == SLOT_SUCCESS, "Scope release succeeds after unpin");

    result = SlotClaim(plainManager, TYPE_INT, &plainHandle);
    TEST_ASSERT(result == SLOT_SUCCESS, "TTL slot claim for pin cleanup");
    value = 13;
    result = SlotWrite(plainManager, &plainHandle, &value, sizeof(value));
    TEST_ASSERT(result == SLOT_SUCCESS, "TTL slot write before pin");
    result = SlotSetTtl(plainManager, &plainHandle, 1);
    TEST_ASSERT(result == SLOT_SUCCESS, "TTL slot gets short lease");
    result = PergyraSlotPin(plainManager, &plainHandle, PGY_SLOT_PIN_READ,
                            NULL, &view);
    TEST_ASSERT(result == SLOT_SUCCESS && view.valid, "TTL slot pin succeeds");
    entry = test_find_slot_entry(plainManager, &plainHandle);
    TEST_ASSERT(entry != NULL, "TTL pinned slot entry is visible to test");
    if (entry != NULL)
        entry->allocationTime = 0;
    SlotCleanupExpired(plainManager);
    result = SlotRelease(plainManager, &plainHandle);
    TEST_SECURITY_VIOLATION(result == SLOT_ERROR_PINNED,
                            "Expired pinned slot is not cleaned while pinned");
    result = PergyraSlotUnpin(plainManager, &view);
    TEST_ASSERT(result == SLOT_SUCCESS, "Expired pinned slot unpin succeeds");
    SlotCleanupExpired(plainManager);
    result = SlotRelease(plainManager, &plainHandle);
    TEST_SECURITY_VIOLATION(result == SLOT_ERROR_SLOT_NOT_FOUND,
                            "Expired unpinned slot is cleaned after unpin");
    SlotManagerDestroy(plainManager);

    secureManager = SlotManagerCreateSecure(16, 4096, true, SECURITY_LEVEL_BASIC);
    TEST_ASSERT(secureManager != NULL, "Secure slot manager for pin tests");
    result = SlotClaimSecure(secureManager, TYPE_INT, SECURITY_LEVEL_BASIC,
                             &secureHandle, &token);
    TEST_ASSERT(result == SLOT_SUCCESS, "Secure slot claim for pin");
    value = 7;
    result = SlotWriteSecure(secureManager, &secureHandle, &value, sizeof(value),
                             &token);
    TEST_ASSERT(result == SLOT_SUCCESS, "Secure slot write before pin");
    invalidToken = token;
    invalidToken.slotId += 1;
    result = PergyraSlotPin(secureManager, &secureHandle, PGY_SLOT_PIN_READ,
                            &invalidToken, &view);
    TEST_SECURITY_VIOLATION(result == SLOT_ERROR_PERMISSION_DENIED
                                && !view.valid,
                            "Secure slot pin rejects invalid token");
    invalidToken = token;
    invalidToken.canRead = false;
    result = PergyraSlotPin(secureManager, &secureHandle, PGY_SLOT_PIN_READ,
                            &invalidToken, &view);
    TEST_SECURITY_VIOLATION(result == SLOT_ERROR_PERMISSION_DENIED
                                && !view.valid,
                            "Secure slot read pin requires read capability");
    result = PergyraSlotPin(secureManager, &secureHandle, PGY_SLOT_PIN_WRITE,
                            &token, &view);
    TEST_ASSERT(result == SLOT_SUCCESS && view.valid && view.ptr != NULL,
                "Secure slot write pin succeeds with token");
    *(int *)view.ptr = 99;
    result = SlotWriteSecure(secureManager, &secureHandle, &value, sizeof(value),
                             &token);
    TEST_SECURITY_VIOLATION(result == SLOT_ERROR_PINNED,
                            "Pinned secure slot rejects concurrent secure write");
    result = SlotReleaseSecure(secureManager, &secureHandle, &token);
    TEST_SECURITY_VIOLATION(result == SLOT_ERROR_PINNED,
                            "Pinned secure slot cannot be released");
    result = PergyraSlotUnpin(secureManager, &view);
    TEST_ASSERT(result == SLOT_SUCCESS && !view.valid, "Secure slot unpin succeeds");
    readValue = 0;
    result = SlotReadSecure(secureManager, &secureHandle, &readValue,
                            sizeof(readValue), &bytesRead, &token);
    TEST_ASSERT(result == SLOT_SUCCESS && bytesRead == sizeof(readValue),
                "Secure slot read after pin succeeds");
    TEST_ASSERT(readValue == 99, "Secure write pin persists modified payload");
    result = SlotRelease(secureManager, &secureHandle);
    TEST_SECURITY_VIOLATION(result == SLOT_ERROR_PERMISSION_DENIED,
                            "Raw release cannot release secure slot");
    result = SlotReleaseSecure(secureManager, &secureHandle, &token);
    TEST_ASSERT(result == SLOT_SUCCESS, "Secure slot release after unpin succeeds");

    result = SlotClaimSecure(secureManager, TYPE_INT, SECURITY_LEVEL_BASIC,
                             &revokedHandle, &revokedToken);
    TEST_ASSERT(result == SLOT_SUCCESS, "Revocation guard secure slot claim");
    value = 111;
    result = SlotWriteSecure(secureManager, &revokedHandle, &value, sizeof(value),
                             &revokedToken);
    TEST_ASSERT(result == SLOT_SUCCESS, "Revocation guard secure slot write");
    result = SlotRevokeToken(secureManager, &revokedHandle);
    TEST_ASSERT(result == SLOT_SUCCESS, "Revocation guard token revoke");
    readValue = 0;
    result = SlotReadSecure(secureManager, &revokedHandle, &readValue,
                            sizeof(readValue), &bytesRead, &revokedToken);
    TEST_SECURITY_VIOLATION(result == SLOT_ERROR_PERMISSION_DENIED,
                            "Revoked token cannot read secure slot");
    value = 222;
    result = SlotWriteSecure(secureManager, &revokedHandle, &value, sizeof(value),
                             &revokedToken);
    TEST_SECURITY_VIOLATION(result == SLOT_ERROR_PERMISSION_DENIED,
                            "Revoked token cannot write secure slot");
    result = PergyraSlotPin(secureManager, &revokedHandle, PGY_SLOT_PIN_READ,
                            &revokedToken, &view);
    TEST_SECURITY_VIOLATION(result == SLOT_ERROR_PERMISSION_DENIED && !view.valid,
                            "Revoked token cannot pin secure slot");
    result = SlotReleaseSecure(secureManager, &revokedHandle, &revokedToken);
    TEST_SECURITY_VIOLATION(result == SLOT_ERROR_PERMISSION_DENIED,
                            "Revoked token cannot release secure slot");
    result = SlotRelease(secureManager, &revokedHandle);
    TEST_SECURITY_VIOLATION(result == SLOT_ERROR_PERMISSION_DENIED,
                            "Raw release cannot bypass secure token revocation");
    SlotManagerDestroySecure(secureManager);
}

/*
 * Main test runner
 */
