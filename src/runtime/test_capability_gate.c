/*
 * Copyright (c) 2025 Pergyra Language Project
 * All rights reserved.
 *
 * Capability gate test (the runtime-enforced effect boundary).
 *
 *   no arg      : granted-path unit tests (manifest transitions + a gated op
 *                 succeeds when its capability is granted). Exits 0.
 *   "deny-clock": restrict the manifest to omit CLOCK, then call a clock op,
 *                 which must panic fail-closed (class capability-denied). The
 *                 harness runs this mode and asserts the abort + message.
 */

#include "pgy_runtime.h"

#include <stdio.h>
#include <string.h>

int
main(int argc, char **argv)
{
    int fail = 0;
    int t;

    if (argc > 1 && strcmp(argv[1], "deny-clock") == 0) {
        /* Sandbox: grant file-read only; CLOCK is NOT granted. */
        pgy_cap_set_manifest_export(PGY_CAP_IO_READ);
        printf("calling clock op under restricted manifest"
               " (expect capability-denied)\n");
        fflush(stdout);
        (void)pgy_now_ms();           /* must panic: CLOCK not granted */
        printf("ERROR: clock op returned without a capability panic\n");
        return 1;                     /* unreachable on a correct gate */
    }

    if (argc > 1 && strcmp(argv[1], "deny-render") == 0) {
        /* Sandbox: audio+input only; RENDER is NOT granted. */
        pgy_cap_set_manifest_export(PGY_CAP_AUDIO | PGY_CAP_INPUT);
        printf("calling render op under no-RENDER manifest"
               " (expect capability-denied)\n");
        fflush(stdout);
        pgy_render_clear(0);          /* must panic: RENDER not granted */
        printf("ERROR: render op returned without a capability panic\n");
        return 1;
    }

    printf("=== Capability Gate Test ===\n");

    pgy_cap_grant_all_export();
    if (pgy_cap_granted_export() == PGY_CAP_ALL)
        printf("  [PASS] default/grant_all -> ALL\n");
    else { printf("  [FAIL] grant_all\n"); fail++; }

    pgy_cap_set_manifest_export(PGY_CAP_IO_READ | PGY_CAP_CLOCK);
    if (pgy_cap_granted_export() == (uint32_t)(PGY_CAP_IO_READ | PGY_CAP_CLOCK))
        printf("  [PASS] set_manifest -> restricted mask\n");
    else { printf("  [FAIL] set_manifest\n"); fail++; }

    /* CLOCK is granted in the current manifest, so the gated op must NOT panic. */
    t = pgy_now_ms();
    printf("  [PASS] clock op under CLOCK grant returned %d (not denied)\n", t);

    /* Media gate (headless stub): granted -> the call records, not denied. */
    pgy_cap_grant_all_export();
    pgy_render_clear(0x000000ff);
    pgy_audio_play_tone(440, 100);
    (void)pgy_input_poll_key();
    if (pgy_media_call_count(0) >= 1 && pgy_media_call_count(1) >= 1
        && pgy_media_call_count(2) >= 1)
        printf("  [PASS] media ops under grant recorded (render/audio/input)\n");
    else { printf("  [FAIL] media ops\n"); fail++; }

    pgy_cap_grant_all_export();

    if (fail == 0)
        printf("ALL PASS (0 failures)\n");
    else
        printf("FAILED (%d)\n", fail);
    return fail == 0 ? 0 : 1;
}
