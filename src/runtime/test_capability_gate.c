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
#include <stdlib.h>
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

    if (argc > 1 && strcmp(argv[1], "intersect") == 0) {
        /* env INTERSECT manifest (docs/190 A4): the host env restricts to
         * {io_read, clock}, a loader's manifest restricts to {io_read, audio};
         * the effective grant is the intersection {io_read} -- neither side can
         * widen the other (fail-closed). Set the env before the first gated
         * access so the env component latches from it. */
        uint32_t eff;
        uint32_t expect = PGY_CAP_IO_READ;   /* {io_read,clock} & {io_read,audio} */
#ifdef _WIN32
        _putenv_s("PGY_CAP_GRANT", "io_read,clock");
#else
        setenv("PGY_CAP_GRANT", "io_read,clock", 1);
#endif
        pgy_cap_set_manifest_export(PGY_CAP_IO_READ | PGY_CAP_AUDIO);
        eff = pgy_cap_granted_export();
        if (eff != expect) {
            printf("  [FAIL] intersect: got 0x%x expected 0x%x (env & manifest)\n",
                   (unsigned)eff, (unsigned)expect);
            return 1;
        }
        if ((eff & PGY_CAP_CLOCK) != 0 || (eff & PGY_CAP_AUDIO) != 0) {
            printf("  [FAIL] intersect: a one-sided cap leaked past the"
                   " intersection (0x%x)\n", (unsigned)eff);
            return 1;
        }
        printf("  [PASS] intersect: env{io_read,clock} & manifest{io_read,audio}"
               " = 0x%x (clock/audio each excluded)\n", (unsigned)eff);
        return 0;
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
