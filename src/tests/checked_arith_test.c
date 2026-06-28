/*
 * checked_arith_test.c — proof that the fail-closed integer add/multiply
 * primitives (src/runtime/pgy_runtime_panic_checked_inline.h) panic on overflow
 * instead of wrapping. This is the size-computation-overflow class
 * (`4 + packet_length`, `num_attrs * sizeof(...)`) made fail-closed.
 *
 * The overflow modes panic (abort), so the test is mode-driven: the smoke runs
 * each mode as a subprocess and checks exit + output. Built and run by
 * tests/checked_arith_failclosed_smoke.sh.
 */
#include "pgy_runtime_panic_checked_inline.h"
#include <stdio.h>
#include <string.h>
#include <stdint.h>

int
main(int argc, char **argv)
{
    const char *mode = argc > 1 ? argv[1] : "";

    if (strcmp(mode, "add_ok") == 0) {
        printf("%lld\n", (long long)pgy_checked_add_i64_export(2, 3));
        return 0;
    }
    if (strcmp(mode, "mul_ok") == 0) {
        printf("%lld\n", (long long)pgy_checked_mul_i64_export(6, 7));
        return 0;
    }
    if (strcmp(mode, "mul_zero") == 0) {
        printf("%lld\n", (long long)pgy_checked_mul_i64_export(0, INT64_MAX));
        return 0;
    }
    if (strcmp(mode, "mul_neg") == 0) {
        printf("%lld\n", (long long)pgy_checked_mul_i64_export(-6, 7));
        return 0;
    }
    if (strcmp(mode, "add_of") == 0) {
        (void)pgy_checked_add_i64_export(INT64_MAX, 1);
        printf("UNREACHABLE\n");
        return 0;
    }
    if (strcmp(mode, "mul_of") == 0) {
        (void)pgy_checked_mul_i64_export(INT64_MAX, 2);
        printf("UNREACHABLE\n");
        return 0;
    }
    if (strcmp(mode, "mul_of_neg") == 0) {
        (void)pgy_checked_mul_i64_export(INT64_MIN, -1);
        printf("UNREACHABLE\n");
        return 0;
    }
    fprintf(stderr, "unknown mode: %s\n", mode);
    return 9;
}
