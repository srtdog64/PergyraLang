void test_security_context_lifecycle()
{
    printf("\n=== Test 1: Security Context Lifecycle ===\n");

    SecurityContext *context = SecurityContextCreate(SECURITY_LEVEL_BASIC);
    SecurityContext *invalidContext = SecurityContextCreate((SecurityLevel)99);
    TEST_ASSERT(context != NULL, "Security context creation");
    TEST_ASSERT(context->initialized, "Security context initialization");
    TEST_ASSERT(context->defaultLevel == SECURITY_LEVEL_BASIC,
                "Default security level setting");
    TEST_ASSERT(SecurityLevelIsValid(SECURITY_LEVEL_BASIC),
                "Known security level is accepted");
    TEST_SECURITY_VIOLATION(invalidContext == NULL,
                            "Invalid default security level is rejected");

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
    TokenCapability invalidLevelToken;
    TokenCapability tamperedToken;
    TokenCapability expiredToken;
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

    result = TokenGenerate(context, 123, (SecurityLevel)99, &invalidLevelToken);
    TEST_SECURITY_VIOLATION(result == SECURITY_ERROR_INVALID_TOKEN,
                            "Token generation rejects invalid security level");

    invalidLevelToken = token;
    invalidLevelToken.level = (SecurityLevel)99;
    result = TokenValidate(context, 123, &invalidLevelToken);
    TEST_SECURITY_VIOLATION(result == SECURITY_ERROR_INVALID_TOKEN,
                            "Token validation rejects invalid security level");

    tamperedToken = token;
    tamperedToken.canTransfer = !tamperedToken.canTransfer;
    result = TokenValidate(context, 123, &tamperedToken);
    TEST_SECURITY_VIOLATION(result == SECURITY_ERROR_INVALID_TOKEN,
                            "Token validation rejects capability metadata tamper");

    expiredToken = token;
    expiredToken.expiryTime = SecureTimestamp() - 1u;
    result = TokenValidate(context, 123, &expiredToken);
    TEST_SECURITY_VIOLATION(result == SECURITY_ERROR_TOKEN_EXPIRED,
                            "Token validation rejects expired token");

    SecurityContextDestroy(context);
}

/*
 * Test 4: Cryptographic known-answer vectors
 */
void test_crypto_known_vectors()
{
    static const uint8_t expected_sha_abc[32] = {
        0xba, 0x78, 0x16, 0xbf, 0x8f, 0x01, 0xcf, 0xea,
        0x41, 0x41, 0x40, 0xde, 0x5d, 0xae, 0x22, 0x23,
        0xb0, 0x03, 0x61, 0xa3, 0x96, 0x17, 0x7a, 0x9c,
        0xb4, 0x10, 0xff, 0x61, 0xf2, 0x00, 0x15, 0xad
    };
    static const uint8_t expected_sha_empty[32] = {
        0xe3, 0xb0, 0xc4, 0x42, 0x98, 0xfc, 0x1c, 0x14,
        0x9a, 0xfb, 0xf4, 0xc8, 0x99, 0x6f, 0xb9, 0x24,
        0x27, 0xae, 0x41, 0xe4, 0x64, 0x9b, 0x93, 0x4c,
        0xa4, 0x95, 0x99, 0x1b, 0x78, 0x52, 0xb8, 0x55
    };
    static const uint8_t aes_key[32] = {
        0x60, 0x3d, 0xeb, 0x10, 0x15, 0xca, 0x71, 0xbe,
        0x2b, 0x73, 0xae, 0xf0, 0x85, 0x7d, 0x77, 0x81,
        0x1f, 0x35, 0x2c, 0x07, 0x3b, 0x61, 0x08, 0xd7,
        0x2d, 0x98, 0x10, 0xa3, 0x09, 0x14, 0xdf, 0xf4
    };
    static const uint8_t aes_iv[16] = {
        0xf0, 0xf1, 0xf2, 0xf3, 0xf4, 0xf5, 0xf6, 0xf7,
        0xf8, 0xf9, 0xfa, 0xfb, 0xfc, 0xfd, 0xfe, 0xff
    };
    static const uint8_t aes_plain[16] = {
        0x6b, 0xc1, 0xbe, 0xe2, 0x2e, 0x40, 0x9f, 0x96,
        0xe9, 0x3d, 0x7e, 0x11, 0x73, 0x93, 0x17, 0x2a
    };
    static const uint8_t expected_aes_cipher[16] = {
        0x60, 0x1e, 0xc3, 0x13, 0x77, 0x57, 0x89, 0xa5,
        0xb7, 0xa7, 0xf5, 0x04, 0xbb, 0xf3, 0xd2, 0x28
    };

    uint8_t hash[32];
    uint8_t cipher[16];
    uint8_t plain[16];
    uint8_t tag[16];
    uint8_t tamperedTag[16];
    SecurityError result;

    printf("\n=== Test 4: Cryptographic Known Vectors ===\n");

    result = SecureHashSHA256((const uint8_t *)"abc", 3, hash);
    TEST_ASSERT(result == SECURITY_SUCCESS
                    && memcmp(hash, expected_sha_abc, sizeof(hash)) == 0,
                "SHA-256 known vector for 'abc'");

    result = SecureHashSHA256((const uint8_t *)"", 0, hash);
    TEST_ASSERT(result == SECURITY_SUCCESS
                    && memcmp(hash, expected_sha_empty, sizeof(hash)) == 0,
                "SHA-256 known vector for empty input");

    result = AES256Encrypt(aes_key, aes_iv, aes_plain, sizeof(aes_plain),
                           cipher, tag);
    TEST_ASSERT(result == SECURITY_SUCCESS
                    && memcmp(cipher, expected_aes_cipher, sizeof(cipher)) == 0,
                "AES-256-CTR known vector encryption");

    memset(plain, 0, sizeof(plain));
    result = AES256Decrypt(aes_key, aes_iv, cipher, sizeof(cipher), tag, plain);
    TEST_ASSERT(result == SECURITY_SUCCESS
                    && memcmp(plain, aes_plain, sizeof(plain)) == 0,
                "AES-256-CTR authenticated decrypt round trip");

    memcpy(tamperedTag, tag, sizeof(tamperedTag));
    tamperedTag[0] ^= 0x01;
    result = AES256Decrypt(aes_key, aes_iv, cipher, sizeof(cipher),
                           tamperedTag, plain);
    TEST_SECURITY_VIOLATION(result == SECURITY_ERROR_INVALID_TOKEN,
                            "AES-256-CTR authentication tag tamper is rejected");
}

/*
 * Test 5: Secure slot manager operations
 */
void test_secure_slot_manager()
{
    printf("\n=== Test 5: Secure Slot Manager ===\n");

    /* Create slot manager with security */
    SlotManager *manager = SlotManagerCreateSecure(1000, 64*1024, true,
                                                  SECURITY_LEVEL_HARDWARE);
    TEST_ASSERT(manager != NULL, "Secure slot manager creation");
    TEST_ASSERT(SlotManagerIsSecurityEnabled(manager), "Security enabled check");


    /* Test secure slot claiming */
    SlotHandle handle;
    TokenCapability token;
    TokenCapability oldToken;
    SlotEntry *entry = NULL;
    EncryptedToken savedWriteToken;
    size_t i;
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

    oldToken = token;
    result = SlotRefreshToken(manager, &handle, &token);
    TEST_ASSERT(result == SLOT_SUCCESS, "Secure slot token refresh");
    result = SlotReadSecure(manager, &handle, &readValue, sizeof(readValue),
                          &bytesRead, &oldToken);
    TEST_SECURITY_VIOLATION(result == SLOT_ERROR_PERMISSION_DENIED,
                            "Refreshed secure slot rejects old token replay");
    result = SlotReadSecure(manager, &handle, &readValue, sizeof(readValue),
                          &bytesRead, &token);
    TEST_ASSERT(result == SLOT_SUCCESS, "Refreshed secure slot accepts new token");

    for (i = 0; i < manager->tableSize; i++) {
        if (manager->slotTable[i].occupied &&
            manager->slotTable[i].slotId == handle.slotId) {
            entry = &manager->slotTable[i];
            break;
        }
    }
    TEST_ASSERT(entry != NULL, "Secure slot stored token lookup");
    if (entry != NULL) {
        savedWriteToken = entry->writeToken;
        entry->writeToken.authTag[0] ^= 0x01u;
        result = SlotReadSecure(manager, &handle, &readValue, sizeof(readValue),
                              &bytesRead, &token);
        TEST_SECURITY_VIOLATION(result == SLOT_ERROR_PERMISSION_DENIED,
                                "Stored secure token tamper rejects valid capability");
        entry->writeToken = savedWriteToken;
    }

    result = SlotClaimSecure(manager, TYPE_INT, (SecurityLevel)99, &handle, &oldToken);
    TEST_SECURITY_VIOLATION(result == SLOT_ERROR_INVALID_HANDLE,
                            "Secure slot claim rejects invalid security level");

    /* Test secure release */
    result = SlotReleaseSecure(manager, &handle, &token);
    TEST_ASSERT(result == SLOT_SUCCESS, "Secure slot release");

    SlotManagerDestroySecure(manager);
}

/*
 * Test 5: Security violation detection
 */
void test_security_violations()
{
    printf("\n=== Test 6: Security Violation Detection ===\n");

    SlotManager *manager = SlotManagerCreateSecure(100, 8*1024, true,
                                                  SECURITY_LEVEL_ENCRYPTED);

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
}

/*
 * Test 6: Scope-based slot management
 */
void test_scope_based_slots()
{
    printf("\n=== Test 7: Scope-based Slot Management ===\n");

    SlotManager *manager = SlotManagerCreateSecure(100, 8*1024, true,
                                                  SECURITY_LEVEL_BASIC);

    /* Create scope */
    SecureSlotScope *scope = SecureSlotScopeCreate(manager, 10);
    TEST_ASSERT(scope != NULL, "Secure slot scope creation");

    /* Claim multiple slots in scope */
    SlotHandle *handle1, *handle2;
    TokenCapability *token1, *token2;
    PgyPinnedView scopeView;

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

    memset(&scopeView, 0, sizeof(scopeView));
    SlotError pinResult = PergyraSlotPin(manager, handle1, PGY_SLOT_PIN_READ,
                                         token1, &scopeView);
    TEST_ASSERT(pinResult == SLOT_SUCCESS && scopeView.valid,
                "Scoped secure slot pin succeeds before scope destroy");
    SlotError destroyPinnedResult = SecureSlotScopeDestroyChecked(scope);
    TEST_SECURITY_VIOLATION(destroyPinnedResult == SLOT_ERROR_PINNED,
                            "Scope destroy rejects live pinned secure slots");
    SlotError unpinResult = PergyraSlotUnpin(manager, &scopeView);
    TEST_ASSERT(unpinResult == SLOT_SUCCESS,
                "Scoped secure slot unpin succeeds after destroy rejection");

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
    printf("\n=== Test 8: Pergyra Language API ===\n");

    SlotManager *manager = SlotManagerCreateSecure(100, 8*1024, true,
                                                  SECURITY_LEVEL_HARDWARE);

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
