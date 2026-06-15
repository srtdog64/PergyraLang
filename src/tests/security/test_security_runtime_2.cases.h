
    PergyraSecureSlot *scopedSlot = pergyra_scope_claim_slot(pscope, "String",
                                                           SECURITY_LEVEL_HARDWARE);
    TEST_ASSERT(scopedSlot != NULL, "Pergyra scoped slot creation");

    /* Cleanup */
    pergyra_slot_release_secure(slot);
    pergyra_scope_end(pscope);
    free(scopedSlot); /* Manual cleanup for test - normally handled by scope */

    SlotManagerDestroySecure(manager);

    /* Print usage example */
    pergyra_security_audit_usage_example();
}

/*
 * Test 8: Performance and stress testing
 */
void test_performance()
{
    printf("\n=== Test 9: Performance Testing ===\n");

    SlotManager *manager = SlotManagerCreateSecure(10000, 1024*1024, true,
                                                  SECURITY_LEVEL_BASIC);

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
}

/*
 * Test 9: Sealed storage and shadow recovery
 */
void test_sealed_storage_and_shadow_recovery()
{
    SlotManager *manager;
    SlotHandle handle;
    SlotHandle otherHandle;
    TokenCapability token;
    TokenCapability otherToken;
    SlotEntry *entry = NULL;
    SlotEntry *otherEntry = NULL;
    SecureSlotPolicy savedPolicy;
    int testValue = 0x12345678;
    int otherValue = 0x55667788;
    int readValue = 0;
    size_t bytesRead = 0;
    size_t i;
    SlotError result;
    SecurityError secResult;

    printf("\n=== Test 10: Sealed Storage And Shadow Recovery ===\n");

    manager = SlotManagerCreateSecure(32, 4096, true, SECURITY_LEVEL_HARDWARE);

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

    savedPolicy = entry->securePayload.policy;
    entry->securePayload.policy.obfuscateInMemory =
        !entry->securePayload.policy.obfuscateInMemory;
    result = SlotReadSecure(manager, &handle, &readValue, sizeof(readValue),
                            &bytesRead, &token);
    TEST_SECURITY_VIOLATION(result != SLOT_SUCCESS,
                            "Sealed payload MAC rejects policy tamper");
    entry->securePayload.policy = savedPolicy;

    secResult = SecureSealedPayloadOpen(manager->securityContext,
                                        handle.slotId,
                                        handle.generation + 1u,
                                        &entry->securePayload,
                                        &readValue,
                                        sizeof(readValue),
                                        &bytesRead,
                                        NULL);
    TEST_SECURITY_VIOLATION(secResult == SECURITY_ERROR_INVALID_TOKEN,
                            "Sealed payload MAC rejects generation mismatch");

    entry->securePayload.primaryAuthTag[0] ^= 0x11u;
    result = SlotReadSecure(manager, &handle, &readValue, sizeof(readValue), &bytesRead, &token);
    TEST_ASSERT(result == SLOT_SUCCESS && readValue == testValue,
                "Shadow copy recovers primary provider auth tag tamper");

    result = SlotClaimSecure(manager, TYPE_INT, SECURITY_LEVEL_HARDWARE,
                             &otherHandle, &otherToken);
    TEST_ASSERT(result == SLOT_SUCCESS, "Sealed payload transplant target claim");
    result = SlotWriteSecure(manager, &otherHandle, &otherValue, sizeof(otherValue),
                             &otherToken);
    TEST_ASSERT(result == SLOT_SUCCESS, "Sealed payload transplant target write");
    for (i = 0; i < manager->tableSize; i++) {
        if (manager->slotTable[i].occupied &&
            manager->slotTable[i].slotId == otherHandle.slotId) {
            otherEntry = &manager->slotTable[i];
            break;
        }
    }
    TEST_ASSERT(otherEntry != NULL, "Sealed payload transplant target lookup");
    if (entry != NULL && otherEntry != NULL &&
        entry->securePayload.size == otherEntry->securePayload.size) {
        memcpy(otherEntry->securePayload.nonce, entry->securePayload.nonce,
               sizeof(otherEntry->securePayload.nonce));
        memcpy(otherEntry->securePayload.primaryData,
               entry->securePayload.primaryData,
               entry->securePayload.size);
        memcpy(otherEntry->securePayload.primaryAuthTag,
               entry->securePayload.primaryAuthTag,
               sizeof(otherEntry->securePayload.primaryAuthTag));
        memcpy(otherEntry->securePayload.primaryMac,
               entry->securePayload.primaryMac,
               sizeof(otherEntry->securePayload.primaryMac));
        memcpy(otherEntry->securePayload.shadowData,
               entry->securePayload.shadowData,
               entry->securePayload.size);
        memcpy(otherEntry->securePayload.shadowAuthTag,
               entry->securePayload.shadowAuthTag,
               sizeof(otherEntry->securePayload.shadowAuthTag));
        memcpy(otherEntry->securePayload.shadowMac,
               entry->securePayload.shadowMac,
               sizeof(otherEntry->securePayload.shadowMac));
    }
    result = SlotReadSecure(manager, &otherHandle, &readValue, sizeof(readValue),
                            &bytesRead, &otherToken);
    TEST_SECURITY_VIOLATION(result != SLOT_SUCCESS,
                            "Sealed payload transplant across slots is rejected");

    ((uint8_t *)SecureSealedPayloadPrimaryBytes(&entry->securePayload))[0] ^= 0x5a;

    result = SlotReadSecure(manager, &handle, &readValue, sizeof(readValue), &bytesRead, &token);
    TEST_ASSERT(result == SLOT_SUCCESS, "Shadow copy recovers corrupted primary");
    TEST_ASSERT(readValue == testValue, "Recovered value matches original");

    ((uint8_t *)SecureSealedPayloadPrimaryBytes(&entry->securePayload))[0] ^= 0x33;
    ((uint8_t *)SecureSealedPayloadShadowBytes(&entry->securePayload))[0] ^= 0x77;

    result = SlotReadSecure(manager, &handle, &readValue, sizeof(readValue), &bytesRead, &token);
    TEST_SECURITY_VIOLATION(result != SLOT_SUCCESS,
                            "Read fails when primary and shadow copies are both corrupted");

    SlotReleaseSecure(manager, &otherHandle, &otherToken);
    SlotReleaseSecure(manager, &handle, &token);
    SlotManagerDestroySecure(manager);
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
    int32_t fd;

    printf("\n=== Test 11: Runtime File I/O Policy ===\n");

    unlink(ok_path);
    unlink(abs_path);
    unlink(escape_path);
    unlink(escape_link);
    unlink("pgy_security_root/inside.txt");
    rmdir(root_dir);
    rmdir(outside_dir);
    pgy_security_test_mkdir(root_dir, 0700);
    pgy_security_test_mkdir(outside_dir, 0700);
    snprintf(rooted_path, sizeof(rooted_path), "%s/%s", root_dir, root_file);
    snprintf(outside_path, sizeof(outside_path), "%s/blocked.txt", outside_dir);

    pgy_write_file(ok_path, "ok");
    content = pgy_read_file(ok_path);
    TEST_ASSERT(content != NULL && strcmp(content, "ok") == 0,
                "Relative runtime file I/O remains allowed");
    free(content);
    unlink(ok_path);

    pgy_security_test_setenv("PGY_IO_ROOT", root_dir, 1);
    pgy_write_file(abs_path, "blocked");
    TEST_SECURITY_VIOLATION(access(abs_path, F_OK) != 0,
                            "Absolute runtime file writes are denied by default");
    fd = pgy_file_open(abs_path, "w");
    TEST_SECURITY_VIOLATION(fd < 0,
                            "Absolute runtime FileOpen writes are denied by default");

    pgy_write_file(root_file, "rooted");
    content = pgy_read_file(root_file);
    TEST_ASSERT(content != NULL && strcmp(content, "rooted") == 0
                && access(rooted_path, F_OK) == 0,
                "Relative runtime file I/O is rooted under PGY_IO_ROOT");
    free(content);
    fd = pgy_file_open("inside_handle.txt", "w");
    TEST_ASSERT(fd >= 0, "Relative runtime FileOpen writes are rooted under PGY_IO_ROOT");
    pgy_file_write(fd, "handle");
    pgy_file_close(fd);
    TEST_ASSERT(pgy_file_exists("inside_handle.txt"),
                "Runtime FileExists shares PGY_IO_ROOT policy");
    unlink("pgy_security_root/inside_handle.txt");

    pgy_write_file(escape_path, "blocked");
    TEST_SECURITY_VIOLATION(access(escape_path, F_OK) != 0,
                            "Parent-traversal runtime file writes are denied");
    fd = pgy_file_open(escape_path, "w");
    TEST_SECURITY_VIOLATION(fd < 0,
                            "Parent-traversal runtime FileOpen writes are denied");
    TEST_SECURITY_VIOLATION(!pgy_file_exists(escape_path),
                            "Parent-traversal runtime FileExists is denied");

    content = pgy_read_file(escape_path);
    TEST_SECURITY_VIOLATION(content != NULL && content[0] == '\0',
                            "Parent-traversal runtime file reads are denied");
    free(content);

#ifndef _WIN32
    TEST_ASSERT(symlink("../pgy_security_outside", escape_link) == 0,
                "Symlink escape test fixture can be created");
    pgy_write_file("escape/blocked.txt", "blocked");
    TEST_SECURITY_VIOLATION(access(outside_path, F_OK) != 0,
                            "Symlink escape outside PGY_IO_ROOT is denied");
#endif

    pgy_security_test_unsetenv("PGY_IO_ROOT");
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

    printf("\n=== Test 12: Runtime Zone Authority Validation ===\n");

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

/*
 * Main test runner
 */
