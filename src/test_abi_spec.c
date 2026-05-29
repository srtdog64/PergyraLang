/*
 * Copyright (c) 2026 Pergyra Language Project
 * All rights reserved.
 *
 * test_abi_spec.c — Pergyra ABI Spec Validation
 *
 * PURPOSE:
 *   Validates that the actual C compiler produces structs matching
 *   the ABI specification defined in pgy_abi_spec.h.
 *
 *   This is the "physical firewall" that prevents AI hallucination
 *   from producing incorrect memory layouts.
 *
 *   If this test fails, the MIR layer is computing WRONG layouts
 *   and the backend is emitting WRONG code.
 *
 * BUILD: make test-abi
 * RUN:   ./bin/test_abi_spec
 *
 * Exit code: 0 = all pass, 1 = at least one failure
 */

#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

/* Include the ABI spec — this pulls in all struct definitions and static asserts */
#include "runtime/pgy_abi_spec.h"

/* -----------------------------------------------------------------
 * Test Runner
 * ----------------------------------------------------------------- */

static int g_pass = 0;
static int g_fail = 0;

#define ABI_TEST(name, cond) \
    do { \
        printf("  %-70s", name); \
        if (cond) { printf("PASS\n"); g_pass++; } \
        else      { printf("FAIL (line %d)\n", __LINE__); g_fail++; } \
    } while (0)

#define PRINT_LAYOUT(type_name) \
    printf("  %-20s  sizeof=%3zu  align=%3zu\n", \
           #type_name, sizeof(type_name), alignof(type_name))

/* -----------------------------------------------------------------
 * Include the real runtime for cross-checking.
 * The runtime decides Debug vs Release based on PGY_DEBUG.
 * ----------------------------------------------------------------- */
#include "runtime/pgy_runtime.h"

/* Determine which slot mode the runtime is using */
#if defined(PGY_DEBUG) || defined(PGY_SAFE_SLOTS)
    #define PGY_RUNTIME_SLOT_MODE_DEBUG 1
#else
    #define PGY_RUNTIME_SLOT_MODE_DEBUG 0
#endif

int main(void) {
    printf("=== Pergyra ABI Spec Validation ===\n");
    printf("Platform: %s / %s\n",
#if defined(_WIN32)
           "Windows"
#elif defined(__linux__)
           "Linux"
#elif defined(__APPLE__)
           "macOS"
#else
           "Unknown"
#endif
           ,
#if defined(__x86_64__) || defined(_M_X64)
           "x86_64"
#elif defined(__i386__) || defined(_M_IX86)
           "x86"
#elif defined(__aarch64__)
           "ARM64"
#else
           "Unknown arch"
#endif
    );
    printf("\n");

    /* ================================================================
     * 1. Slot<T> Debug
     * ================================================================ */
    printf("[Slot<T> — Debug Mode]\n");

    PRINT_LAYOUT(pgy_abi_slot_int_dbg);
    PRINT_LAYOUT(pgy_abi_slot_long_dbg);
    PRINT_LAYOUT(pgy_abi_slot_float_dbg);
    PRINT_LAYOUT(pgy_abi_slot_double_dbg);
    PRINT_LAYOUT(pgy_abi_slot_bool_dbg);
    PRINT_LAYOUT(pgy_abi_slot_string_dbg);
    printf("\n");

    /* Cross-check against runtime types (PgySlot_* from pgy_runtime.h) */
#if PGY_RUNTIME_SLOT_MODE_DEBUG
    ABI_TEST("Slot<Int> dbg: runtime size matches ABI spec",
             sizeof(PgySlot_Int) == sizeof(pgy_abi_slot_int_dbg));
    ABI_TEST("Slot<Int> dbg: value offset matches",
             offsetof(PgySlot_Int, value) == offsetof(pgy_abi_slot_int_dbg, value));
    ABI_TEST("Slot<Int> dbg: occupied offset matches",
             offsetof(PgySlot_Int, occupied) == offsetof(pgy_abi_slot_int_dbg, occupied));

    ABI_TEST("Slot<Long> dbg: runtime size matches ABI spec",
             sizeof(PgySlot_Long) == sizeof(pgy_abi_slot_long_dbg));
    ABI_TEST("Slot<Long> dbg: value offset matches",
             offsetof(PgySlot_Long, value) == offsetof(pgy_abi_slot_long_dbg, value));

    ABI_TEST("Slot<String> dbg: runtime size matches ABI spec",
             sizeof(PgySlot_String) == sizeof(pgy_abi_slot_string_dbg));
#else
    ABI_TEST("Slot<Int> rel: runtime size matches ABI spec",
             sizeof(PgySlot_Int) == sizeof(pgy_abi_slot_int_rel));
    ABI_TEST("Slot<Long> rel: runtime size matches ABI spec",
             sizeof(PgySlot_Long) == sizeof(pgy_abi_slot_long_rel));
    ABI_TEST("Slot<String> rel: runtime size matches ABI spec",
             sizeof(PgySlot_String) == sizeof(pgy_abi_slot_string_rel));
#endif

    /* ================================================================
     * 2. Slot<T> Release
     * ================================================================ */
    printf("\n[Slot<T> — Release Mode]\n");

    PRINT_LAYOUT(pgy_abi_slot_int_rel);
    PRINT_LAYOUT(pgy_abi_slot_long_rel);
    PRINT_LAYOUT(pgy_abi_slot_float_rel);
    PRINT_LAYOUT(pgy_abi_slot_double_rel);
    PRINT_LAYOUT(pgy_abi_slot_bool_rel);
    PRINT_LAYOUT(pgy_abi_slot_string_rel);
    printf("\n");

    ABI_TEST("Slot<Int> rel: size == 4",
             sizeof(pgy_abi_slot_int_rel) == 4);
    ABI_TEST("Slot<Long> rel: size == 8",
             sizeof(pgy_abi_slot_long_rel) == 8);
    ABI_TEST("Slot<Float> rel: size == 4",
             sizeof(pgy_abi_slot_float_rel) == 4);
    ABI_TEST("Slot<Double> rel: size == 8",
             sizeof(pgy_abi_slot_double_rel) == 8);
    ABI_TEST("Slot<Bool> rel: size == 1",
             sizeof(pgy_abi_slot_bool_rel) == 1);

    /* ================================================================
     * 3. SecureSlot<T>
     * ================================================================ */
    printf("\n[SecureSlot<T> — Debug Mode]\n");

    PRINT_LAYOUT(pgy_abi_secure_slot_int_dbg);
    PRINT_LAYOUT(pgy_abi_secure_slot_string_dbg);
    printf("\n");

    ABI_TEST("SecureSlot<Int> dbg: larger than Slot<Int>",
             sizeof(pgy_abi_secure_slot_int_dbg) > sizeof(pgy_abi_slot_int_dbg));
    ABI_TEST("SecureSlot<Int> dbg: token offset > 4",
             offsetof(pgy_abi_secure_slot_int_dbg, token) > 4);
    ABI_TEST("SecureSlot<Int> dbg: size >= 16",
             sizeof(pgy_abi_secure_slot_int_dbg) >= 16);
    ABI_TEST("SecureSlot<Int>: runtime size matches ABI spec",
             sizeof(PgySecureSlot_Int) == sizeof(pgy_abi_secure_slot_int_dbg));
    ABI_TEST("SecureSlot<Int>: runtime token offset matches",
             offsetof(PgySecureSlot_Int, token) == offsetof(pgy_abi_secure_slot_int_dbg, token));
    ABI_TEST("SecureSlot<Long>: runtime size matches ABI spec",
             sizeof(PgySecureSlot_Long) == sizeof(pgy_abi_secure_slot_long_dbg));
    ABI_TEST("SecureSlot<Long>: runtime token offset matches",
             offsetof(PgySecureSlot_Long, token) == offsetof(pgy_abi_secure_slot_long_dbg, token));
    ABI_TEST("SecureSlot<Float>: runtime size matches ABI spec",
             sizeof(PgySecureSlot_Float) == sizeof(pgy_abi_secure_slot_float_dbg));
    ABI_TEST("SecureSlot<Float>: runtime token offset matches",
             offsetof(PgySecureSlot_Float, token) == offsetof(pgy_abi_secure_slot_float_dbg, token));
    ABI_TEST("SecureSlot<Double>: runtime size matches ABI spec",
             sizeof(PgySecureSlot_Double) == sizeof(pgy_abi_secure_slot_double_dbg));
    ABI_TEST("SecureSlot<Double>: runtime token offset matches",
             offsetof(PgySecureSlot_Double, token) == offsetof(pgy_abi_secure_slot_double_dbg, token));
    ABI_TEST("SecureSlot<Bool>: runtime size matches ABI spec",
             sizeof(PgySecureSlot_Bool) == sizeof(pgy_abi_secure_slot_bool_dbg));
    ABI_TEST("SecureSlot<Bool>: runtime token offset matches",
             offsetof(PgySecureSlot_Bool, token) == offsetof(pgy_abi_secure_slot_bool_dbg, token));
    ABI_TEST("SecureSlot<String>: runtime size matches ABI spec",
             sizeof(PgySecureSlot_String) == sizeof(pgy_abi_secure_slot_string_dbg));
    ABI_TEST("SecureSlot<String>: runtime token offset matches",
             offsetof(PgySecureSlot_String, token) == offsetof(pgy_abi_secure_slot_string_dbg, token));
    ABI_TEST("Token<Int>: runtime size matches stable ABI spec",
             sizeof(PgyToken_Int) == sizeof(pgy_abi_token_int_dbg));
    ABI_TEST("Token<Int>: can_write offset matches",
             offsetof(PgyToken_Int, can_write) == offsetof(pgy_abi_token_int_dbg, can_write));
    ABI_TEST("Token<Int>: can_read offset matches",
             offsetof(PgyToken_Int, can_read) == offsetof(pgy_abi_token_int_dbg, can_read));

    printf("\n[Pin Views]\n");

    PRINT_LAYOUT(pgy_abi_pinned_slot_view_int);
    PRINT_LAYOUT(pgy_abi_pinned_secure_slot_view_int);
    printf("\n");

    ABI_TEST("PinnedSlotView<Int>: runtime size matches ABI spec",
             sizeof(PgyPinnedSlotView_Int) == sizeof(pgy_abi_pinned_slot_view_int));
    ABI_TEST("PinnedSlotView<Int>: slot offset matches",
             offsetof(PgyPinnedSlotView_Int, slot) == offsetof(pgy_abi_pinned_slot_view_int, slot));
    ABI_TEST("PinnedSlotView<Int>: active offset matches",
             offsetof(PgyPinnedSlotView_Int, active) == offsetof(pgy_abi_pinned_slot_view_int, active));
    ABI_TEST("PinnedSlotView<Int>: can_write offset matches",
             offsetof(PgyPinnedSlotView_Int, can_write) == offsetof(pgy_abi_pinned_slot_view_int, can_write));
    ABI_TEST("PinnedSecureSlotView<Int>: runtime size matches ABI spec",
             sizeof(PgyPinnedSecureSlotView_Int) == sizeof(pgy_abi_pinned_secure_slot_view_int));
    ABI_TEST("PinnedSecureSlotView<Int>: slot offset matches",
             offsetof(PgyPinnedSecureSlotView_Int, slot) == offsetof(pgy_abi_pinned_secure_slot_view_int, slot));
    ABI_TEST("PinnedSecureSlotView<Int>: token offset matches",
             offsetof(PgyPinnedSecureSlotView_Int, token) == offsetof(pgy_abi_pinned_secure_slot_view_int, token));
    ABI_TEST("PinnedSecureSlotView<Int>: active offset matches",
             offsetof(PgyPinnedSecureSlotView_Int, active) == offsetof(pgy_abi_pinned_secure_slot_view_int, active));
    ABI_TEST("PinnedSecureSlotView<Int>: can_write offset matches",
             offsetof(PgyPinnedSecureSlotView_Int, can_write) == offsetof(pgy_abi_pinned_secure_slot_view_int, can_write));

    /* ================================================================
     * 4. DeviceSlot<T>
     * ================================================================ */
    printf("\n[DeviceSlot<T>]\n");

    PRINT_LAYOUT(pgy_abi_device_slot_int);
    PRINT_LAYOUT(pgy_abi_device_slot_string);
    printf("\n");

    ABI_TEST("DeviceSlot<Int>: value at offset 0",
             offsetof(pgy_abi_device_slot_int, value) == 0);
    ABI_TEST("DeviceSlot<Int>: size >= 8",
             sizeof(pgy_abi_device_slot_int) >= 8);

    /* ================================================================
     * 5. Option<T>
     * ================================================================ */
    printf("\n[Option<T>]\n");

    PRINT_LAYOUT(pgy_abi_option_int);
    PRINT_LAYOUT(pgy_abi_option_long);
    PRINT_LAYOUT(pgy_abi_option_bool);
    PRINT_LAYOUT(pgy_abi_option_string);
    printf("\n");

    ABI_TEST("Option<Int>: tag at 0, value at 4, size 8",
             offsetof(pgy_abi_option_int, tag) == 0 &&
             offsetof(pgy_abi_option_int, value) == 4 &&
             sizeof(pgy_abi_option_int) == 8);
    ABI_TEST("Option<Long>: size >= 16",
             sizeof(pgy_abi_option_long) >= 16);

    /* Cross-check against runtime */
    ABI_TEST("Option<Int>: runtime size matches ABI spec",
             sizeof(PgyOption_Int) == sizeof(pgy_abi_option_int));
    ABI_TEST("Option<String>: runtime size matches ABI spec",
             sizeof(PgyOption_String) == sizeof(pgy_abi_option_string));

    /* ================================================================
     * 6. Result<T, E>
     * ================================================================ */
    printf("\n[Result<T, E>]\n");

    PRINT_LAYOUT(pgy_abi_result_int);
    PRINT_LAYOUT(pgy_abi_result_bool);
    PRINT_LAYOUT(pgy_abi_result_string);
    printf("\n");

    ABI_TEST("Result<Int>: tag at 0, size >= 16",
             offsetof(pgy_abi_result_int, tag) == 0 &&
             sizeof(pgy_abi_result_int) >= 16);
    ABI_TEST("Result<Bool>: size >= 16",
             sizeof(pgy_abi_result_bool) >= 16);

    /* Cross-check against runtime */
    ABI_TEST("Result<Int>: runtime size matches ABI spec",
             sizeof(PgyResult_Int) == sizeof(pgy_abi_result_int));

    /* ================================================================
     * 7. ZoneChannel<T> / WorldChannel<T> — Opaque Handles
     * ================================================================ */
    printf("\n[ZoneChannel<T> / WorldChannel<T>] (opaque handles, platform-independent)\n");

    PRINT_LAYOUT(pgy_abi_zone_channel_handle);
    PRINT_LAYOUT(pgy_abi_world_channel_handle);
    printf("\n");

    ABI_TEST("ZoneChannelHandle: size == 4",
             sizeof(pgy_abi_zone_channel_handle) == 4);
    ABI_TEST("WorldChannelHandle: size == 4",
             sizeof(pgy_abi_world_channel_handle) == 4);
    ABI_TEST("ZoneChannelHandle: same as uint32_t",
             sizeof(pgy_abi_zone_channel_handle) == sizeof(uint32_t));

    /* Ordinary Channel<T> still uses legacy local storage in the beta backend. */
    printf("  (Ordinary Channel<Int> legacy storage sizeof=%zu; not aggregate-copy ABI)\n",
           sizeof(PgyChannel_Int));

    /* ================================================================
     * 8. Box<T>
     * ================================================================ */
    printf("\n[Box<T>]\n");

    PRINT_LAYOUT(pgy_abi_box_int);
    PRINT_LAYOUT(pgy_abi_box_string);
    printf("\n");

    ABI_TEST("Box<Int>: size == pointer",
             sizeof(pgy_abi_box_int) == sizeof(void*));

    /* ================================================================
     * 8. Rc<T> / Weak<T>
     * ================================================================ */
    printf("\n[Rc<T> / Weak<T>]\n");

    PRINT_LAYOUT(pgy_abi_rc_ctrl_int);
    PRINT_LAYOUT(pgy_abi_rc_int);
    PRINT_LAYOUT(pgy_abi_weak_int);
    printf("\n");

    ABI_TEST("Rc<Int>: handle size matches runtime",
             sizeof(PgyRc_Int) == sizeof(pgy_abi_rc_int));
    ABI_TEST("Weak<Int>: handle size matches runtime",
             sizeof(PgyWeak_Int) == sizeof(pgy_abi_weak_int));
    ABI_TEST("Rc<Int> ctrl: runtime size matches ABI spec",
             sizeof(PgyRcControl_Int) == sizeof(pgy_abi_rc_ctrl_int));
    ABI_TEST("Rc<Int> ctrl: strong offset matches",
             offsetof(PgyRcControl_Int, strong_count) == offsetof(pgy_abi_rc_ctrl_int, strong_count));
    ABI_TEST("Rc<Int> ctrl: weak offset matches",
             offsetof(PgyRcControl_Int, weak_count) == offsetof(pgy_abi_rc_ctrl_int, weak_count));
    ABI_TEST("Rc<Int> ctrl: alive offset matches",
             offsetof(PgyRcControl_Int, alive) == offsetof(pgy_abi_rc_ctrl_int, alive));

    /* ================================================================
     * 9. Array<T>
     * ================================================================ */
    printf("\n[Array<T>]\n");

    PRINT_LAYOUT(pgy_abi_array_int);
    printf("\n");

    ABI_TEST("Array<Int>: data at 0, size >= 24",
             offsetof(pgy_abi_array_int, data) == 0 &&
             sizeof(pgy_abi_array_int) >= 24);

    /* ================================================================
     * 10. Auxiliary Types
     * ================================================================ */
    printf("\n[Auxiliary Types]\n");

    PRINT_LAYOUT(pgy_abi_qubit);
    PRINT_LAYOUT(pgy_abi_task_handle);
    PRINT_LAYOUT(pgy_abi_timer);
    PRINT_LAYOUT(pgy_abi_arena);
    printf("\n");

    ABI_TEST("Qubit: size >= 12",
             sizeof(pgy_abi_qubit) >= 12);
    ABI_TEST("TaskHandle: size >= 8",
             sizeof(pgy_abi_task_handle) >= 8);
    ABI_TEST("Timer: size >= 12",
             sizeof(pgy_abi_timer) >= 12);

    /* ================================================================
     * Summary
     * ================================================================ */
    printf("=== Results: %d passed, %d failed ===\n", g_pass, g_fail);

    if (g_fail > 0) {
        fprintf(stderr, "\n");
        fprintf(stderr, "*************************************************************\n");
        fprintf(stderr, "***  ABI SPEC VIOLATION DETECTED                            ***\n");
        fprintf(stderr, "*************************************************************\n");
        fprintf(stderr, "***  The MIR layer's type layout computation does NOT match ***\n");
        fprintf(stderr, "***  the actual C struct produced by the compiler.          ***\n");
        fprintf(stderr, "***                                                         ***\n");
        fprintf(stderr, "***  Fix one of:                                            ***\n");
        fprintf(stderr, "***    1. pgy_abi_spec.h (if spec is wrong)                ***\n");
        fprintf(stderr, "***    2. pgy_runtime.h macros (if runtime differs)        ***\n");
        fprintf(stderr, "***    3. MIR type layout code (if MIR computes wrong)     ***\n");
        fprintf(stderr, "*************************************************************\n");
        fprintf(stderr, "\n");
    }

    return g_fail > 0 ? 1 : 0;
}
