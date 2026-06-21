/*
 * Copyright (c) 2025 Pergyra Language Project
 * All rights reserved.
 *
 * Regression test for the secure-slot scope safety fixes:
 *   BUG 1  double-release: claim in scope + individual release + scope end
 *   BUG 2  struct leak / exactly-once manager release
 *   BUG 4  fixed capacity: claiming past the initial capacity must grow
 *
 * The exactly-once property is checked via SlotManagerGetActiveCount: after a
 * scope fully ends, the active slot count must return to the pre-scope baseline.
 * A double-release would error/panic; a leak would leave active > baseline.
 */

#include "slot_manager.h"

#include <stdio.h>
#include <string.h>

static int g_fail = 0;

#define CHECK(cond, msg)                                                    \
    do {                                                                    \
        if (cond) {                                                         \
            printf("  [PASS] %s\n", (msg));                                 \
        } else {                                                            \
            printf("  [FAIL] %s\n", (msg));                                 \
            g_fail++;                                                       \
        }                                                                   \
    } while (0)

static SlotManager *
make_manager(void)
{
    return SlotManagerCreateSecure(512, 1u << 20, true, SECURITY_LEVEL_BASIC);
}

/* BUG 1: early individual release inside a scope must not double-release when
 * the scope later ends. */
static void
test_individual_release_then_scope_end(SlotManager *mgr)
{
    size_t baseline = SlotManagerGetActiveCount(mgr);
    PergyraSlotScope *scope = pergyra_scope_begin(mgr);
    PergyraSecureSlot *slot;
    int value = 42;
    int readBack = 0;
    size_t bytes = 0;

    printf("T1 individual-release-then-scope-end (BUG 1)\n");
    CHECK(scope != NULL, "scope begin");

    slot = pergyra_scope_claim_slot(scope, "Int", SECURITY_LEVEL_BASIC);
    CHECK(slot != NULL, "claim slot in scope");
    CHECK(pergyra_slot_write_secure(slot, &value, sizeof(value)), "write");
    CHECK(pergyra_slot_read_secure(slot, &readBack, sizeof(readBack), &bytes)
          && readBack == 42, "read back 42");

    pergyra_slot_release_secure(slot);          /* early individual release */
    pergyra_scope_end(scope);                   /* must NOT double-release   */

    CHECK(SlotManagerGetActiveCount(mgr) == baseline,
          "active count back to baseline (no double-release, no leak)");
}

/* BUG 4: claiming more than the initial capacity (64) must grow, not fail. */
static void
test_growth_past_capacity(SlotManager *mgr)
{
    size_t baseline = SlotManagerGetActiveCount(mgr);
    PergyraSlotScope *scope = pergyra_scope_begin(mgr);
    int allClaimed = 1;
    const int N = 200;
    int i;

    printf("T2 growth-past-initial-capacity (BUG 4)\n");
    CHECK(scope != NULL, "scope begin");
    for (i = 0; i < N; i++) {
        PergyraSecureSlot *s = pergyra_scope_claim_slot(scope, "Int",
                                                        SECURITY_LEVEL_BASIC);
        if (s == NULL) { allClaimed = 0; break; }
    }
    CHECK(allClaimed, "claimed 200 slots (> initial cap 64) via growth");
    pergyra_scope_end(scope);
    CHECK(SlotManagerGetActiveCount(mgr) == baseline,
          "active count back to baseline after growth scope");
}

/* Normal RAII: scope auto-releases every slot exactly once, no individual
 * release, no leak. */
static void
test_scope_autocleanup(SlotManager *mgr)
{
    size_t baseline = SlotManagerGetActiveCount(mgr);
    PergyraSlotScope *scope = pergyra_scope_begin(mgr);
    int i;

    printf("T3 scope auto-cleanup (BUG 2: no leak, exactly-once)\n");
    for (i = 0; i < 5; i++) {
        PergyraSecureSlot *s = pergyra_scope_claim_slot(scope, "Int",
                                                        SECURITY_LEVEL_BASIC);
        int v = i;
        if (s != NULL)
            pergyra_slot_write_secure(s, &v, sizeof(v));
    }
    CHECK(SlotManagerGetActiveCount(mgr) == baseline + 5, "5 slots active");
    pergyra_scope_end(scope);
    CHECK(SlotManagerGetActiveCount(mgr) == baseline,
          "active count back to baseline (all freed exactly once)");
}

/* Mixed: some slots individually released, some left to the scope. */
static void
test_mixed_release(SlotManager *mgr)
{
    size_t baseline = SlotManagerGetActiveCount(mgr);
    PergyraSlotScope *scope = pergyra_scope_begin(mgr);
    PergyraSecureSlot *s[4];
    int i;

    printf("T4 mixed individual + scope release\n");
    for (i = 0; i < 4; i++)
        s[i] = pergyra_scope_claim_slot(scope, "Int", SECURITY_LEVEL_BASIC);
    pergyra_slot_release_secure(s[1]);          /* release some early */
    pergyra_slot_release_secure(s[3]);
    pergyra_scope_end(scope);                   /* scope releases s[0], s[2] */
    CHECK(SlotManagerGetActiveCount(mgr) == baseline,
          "active count back to baseline (mixed release, exactly-once)");
}

int
main(void)
{
    SlotManager *mgr = make_manager();

    printf("=== Secure Slot Scope Safety Test ===\n");
    if (mgr == NULL) {
        printf("  [FAIL] manager create\n");
        return 1;
    }

    test_individual_release_then_scope_end(mgr);
    test_growth_past_capacity(mgr);
    test_scope_autocleanup(mgr);
    test_mixed_release(mgr);

    SlotManagerDestroySecure(mgr);

    if (g_fail == 0)
        printf("ALL PASS (0 failures)\n");
    else
        printf("FAILED (%d)\n", g_fail);
    return g_fail == 0 ? 0 : 1;
}
