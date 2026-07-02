/*
 * secure_token_reuse_test.c — proof that the inline secure-slot twin
 * (src/runtime/pgy_runtime_slot_macros.h) issues a FRESH token identity on
 * every claim, so a token retained across release/re-claim can never validate
 * against the reclaimed slot. This is the stale-handle / use-after-release
 * class under storage reuse: the previous address-derived token reproduced the
 * same id whenever the claim temp landed at the same address (always true for
 * repeated claims through one call site), silently accepting stale tokens.
 *
 * The reject modes panic (abort), so the test is mode-driven: the smoke runs
 * each mode as a subprocess and checks exit + output. Built and run by
 * tests/secure_token_reuse_failclosed_smoke.sh.
 */
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>

#include "pgy_runtime_slot_status.h"
#include "pgy_runtime_slot_macros.h"

PGY_RUNTIME_SLOT_RESULT_DEFINE(ReuseInt, int32_t)
PGY_SECURE_SLOT_DEFINE(ReuseInt, int32_t)

int
main(int argc, char **argv)
{
    const char *mode = argc > 1 ? argv[1] : "";

    if (strcmp(mode, "fresh_ok") == 0) {
        PgyToken_ReuseInt t;
        PgySecureSlot_ReuseInt s = pgy_claim_secure_ReuseInt(&t);
        pgy_secure_write_ReuseInt(&s, 7, &t);
        printf("%d\n", (int)pgy_secure_read_ReuseInt(&s, &t));
        pgy_secure_release_ReuseInt(&s, &t);
        return 0;
    }
    if (strcmp(mode, "distinct_ids") == 0) {
        /* Two claims through the SAME call site: under the old address-derived
         * scheme both temps landed at the same address and produced the SAME
         * id; the monotonic counter must make them distinct. */
        PgyToken_ReuseInt t1;
        PgyToken_ReuseInt t2;
        PgySecureSlot_ReuseInt a = pgy_claim_secure_ReuseInt(&t1);
        PgySecureSlot_ReuseInt b = pgy_claim_secure_ReuseInt(&t2);
        (void)a;
        (void)b;
        printf("%s\n", t1.id != t2.id ? "DISTINCT" : "COLLIDED");
        return 0;
    }
    if (strcmp(mode, "stale_read") == 0) {
        /* claim -> release -> re-claim into the SAME storage, then read with
         * the token from the FIRST claim: must fail closed, never read the
         * new occupant's value. */
        PgyToken_ReuseInt stale;
        PgyToken_ReuseInt fresh;
        PgySecureSlot_ReuseInt s = pgy_claim_secure_ReuseInt(&stale);
        pgy_secure_release_ReuseInt(&s, &stale);
        s = pgy_claim_secure_ReuseInt(&fresh);
        pgy_secure_write_ReuseInt(&s, 42, &fresh);
        (void)pgy_secure_read_ReuseInt(&s, &stale);
        printf("UNREACHABLE\n");
        return 0;
    }
    if (strcmp(mode, "stale_write") == 0) {
        PgyToken_ReuseInt stale;
        PgyToken_ReuseInt fresh;
        PgySecureSlot_ReuseInt s = pgy_claim_secure_ReuseInt(&stale);
        pgy_secure_release_ReuseInt(&s, &stale);
        s = pgy_claim_secure_ReuseInt(&fresh);
        pgy_secure_write_ReuseInt(&s, 99, &stale);
        printf("UNREACHABLE\n");
        return 0;
    }

    fprintf(stderr, "unknown mode: %s\n", mode);
    return 2;
}
