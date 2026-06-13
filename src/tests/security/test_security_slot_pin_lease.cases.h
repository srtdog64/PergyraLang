void test_slot_pin_lease_runtime()
{
    SlotManager *plainManager;
    SlotManager *exhaustManager;
    SlotManager *secureManager;
    SlotHandle plainHandle;
    SlotHandle exhaustHandle;
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

    printf("\n=== Test 13: Slot Pin Lease Runtime ===\n");

    plainManager = SlotManagerCreate(16, 4096);
    TEST_ASSERT(plainManager != NULL, "Plain slot manager for pin tests");
    plainManager->nextSlotId = 0;
    result = SlotClaim(plainManager, TYPE_INT, &plainHandle);
    TEST_SECURITY_VIOLATION(result == SLOT_ERROR_ID_EXHAUSTED,
                            "Zero slot id sentinel is tombstoned before claim");
    plainManager->nextSlotId = UINT32_MAX;
    result = SlotClaim(plainManager, TYPE_INT, &plainHandle);
    TEST_SECURITY_VIOLATION(result == SLOT_ERROR_ID_EXHAUSTED,
                            "Slot id wrap is tombstoned before reuse");
    plainManager->nextSlotId = 1;

    exhaustManager = SlotManagerCreate(1, 1024);
    TEST_ASSERT(exhaustManager != NULL,
                "Generation exhaustion slot manager is created");
    result = SlotClaim(exhaustManager, TYPE_INT, &exhaustHandle);
    TEST_ASSERT(result == SLOT_SUCCESS, "Generation exhaustion slot claim");
    result = SlotRelease(exhaustManager, &exhaustHandle);
    TEST_ASSERT(result == SLOT_SUCCESS, "Generation exhaustion slot release");
    exhaustManager->slotTable[0].generation = UINT32_MAX;
    result = SlotClaim(exhaustManager, TYPE_INT, &exhaustHandle);
    TEST_SECURITY_VIOLATION(result == SLOT_ERROR_ID_EXHAUSTED,
                            "Generation-exhausted recycled slot is not OOM");
    SlotManagerDestroy(exhaustManager);

    result = SlotClaim(plainManager, TYPE_INT, &plainHandle);
    TEST_ASSERT(result == SLOT_SUCCESS, "Plain slot claim for pin");
    value = 42;
    result = SlotWrite(plainManager, &plainHandle, &value, sizeof(value));
    TEST_ASSERT(result == SLOT_SUCCESS, "Plain slot write before pin");
    /* A permissive but unbound token: the permission bits pass, so the pin is
     * rejected by token validation against the plain slot, not by an
     * uninitialized permission read. */
    memset(&token, 0, sizeof(token));
    token.canRead = true;
    token.canWrite = true;
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
    result = PergyraSlotPin(plainManager, &plainHandle, PGY_SLOT_PIN_WRITE,
                            NULL, &view);
    TEST_ASSERT(result == SLOT_SUCCESS && view.valid && view.ptr != NULL,
                "Plain slot write pin succeeds");
    readValue = 0;
    result = SlotRead(plainManager, &plainHandle, &readValue, sizeof(readValue),
                      &bytesRead);
    TEST_SECURITY_VIOLATION(result == SLOT_ERROR_PINNED,
                            "Write-pinned plain slot rejects concurrent read");
    result = PergyraSlotUnpin(plainManager, &view);
    TEST_ASSERT(result == SLOT_SUCCESS && !view.valid,
                "Plain write pin unpin succeeds");
    result = SlotRelease(plainManager, &plainHandle);
    TEST_ASSERT(result == SLOT_SUCCESS, "Plain slot release after unpin succeeds");

    staleHandle = plainHandle;
    result = SlotClaim(plainManager, TYPE_INT, &plainHandle);
    TEST_ASSERT(result == SLOT_SUCCESS, "Generation guard slot claim");
    TEST_ASSERT(plainHandle.slotId == staleHandle.slotId
                    && plainHandle.generation == staleHandle.generation + 1u,
                "Released slot id is recycled with advanced generation");
    result = SlotRead(plainManager, &staleHandle, &readValue, sizeof(readValue),
                      &bytesRead);
    TEST_SECURITY_VIOLATION(result == SLOT_ERROR_SLOT_NOT_FOUND,
                            "Recycled stale generation handle cannot read");
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
