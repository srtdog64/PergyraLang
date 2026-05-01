/*
 * Copyright (c) 2025 Pergyra Language Project
 * All rights reserved.
 *
 * Memory layout and slot lifecycle test suite
 *
 * Verifies:
 *   A. Struct sizes and field offsets for all slot types
 *   B. Slot lifecycle correctness (claim/write/read/release)
 *   C. Secure slot token validation
 *   D. Type safety across all primitive types
 *   E. Panic conditions (double-release, use-after-release)
 *
 * Build: make test-memory
 * Run:   ./bin/test_memory_layout
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stddef.h>
#include <signal.h>
#include <setjmp.h>
#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#define PGY_DUP _dup
#define PGY_DUP2 _dup2
#define PGY_CLOSE _close
#define PGY_OPEN _open
#define PGY_FILENO _fileno
#define PGY_NULL_DEVICE "NUL"
#else
#include <fcntl.h>
#include <unistd.h>
#define PGY_DUP dup
#define PGY_DUP2 dup2
#define PGY_CLOSE close
#define PGY_OPEN open
#define PGY_FILENO fileno
#define PGY_NULL_DEVICE "/dev/null"
#endif

/* Enable debug mode for slot safety checks */
#define PGY_DEBUG
#include "runtime/pgy_runtime.h"

/* -----------------------------------------------------------------
 * Test runner
 * ----------------------------------------------------------------- */

static int g_pass = 0;
static int g_fail = 0;

#define TEST(name) \
    do { printf("  %-60s", name); } while (0)

#define EXPECT(cond) \
    do { \
        if (cond) { printf("pass\n"); g_pass++; } \
        else      { printf("FAIL  (line %d)\n", __LINE__); g_fail++; } \
    } while (0)

/* -----------------------------------------------------------------
 * Panic trap ??catch PGY_PANIC (which calls abort ??SIGABRT)
 * ----------------------------------------------------------------- */

static jmp_buf g_panic_jmp;
static volatile sig_atomic_t g_panic_expected = 0;

static int
pgy_suppress_expected_panic_output(void)
{
    int saved_stderr = PGY_DUP(PGY_FILENO(stderr));
    int null_fd;

    if (saved_stderr < 0)
        return -1;

    null_fd = PGY_OPEN(PGY_NULL_DEVICE, O_WRONLY);
    if (null_fd < 0) {
        PGY_CLOSE(saved_stderr);
        return -1;
    }

    if (PGY_DUP2(null_fd, PGY_FILENO(stderr)) < 0) {
        PGY_CLOSE(null_fd);
        PGY_CLOSE(saved_stderr);
        return -1;
    }

    PGY_CLOSE(null_fd);
    return saved_stderr;
}

static void
pgy_restore_expected_panic_output(int saved_stderr)
{
    if (saved_stderr < 0)
        return;
    fflush(stderr);
    PGY_DUP2(saved_stderr, PGY_FILENO(stderr));
    PGY_CLOSE(saved_stderr);
}

static void
pgy_with_suppressed_stderr(void (*fn)(void *), void *userdata)
{
    int saved_stderr;

    if (fn == NULL)
        return;

    saved_stderr = pgy_suppress_expected_panic_output();
    fn(userdata);
    pgy_restore_expected_panic_output(saved_stderr);
}

typedef struct {
    PgyAllocator alloc;
} AllocatorTraceProbe;

static void
allocator_trace_probe_run(void *userdata)
{
    AllocatorTraceProbe *probe = (AllocatorTraceProbe *)userdata;
    int32_t *data;

    if (probe == NULL)
        return;

    data = (int32_t*)pgy_alloc(&probe->alloc, sizeof(int32_t) * 4, _Alignof(int32_t));
    pgy_free(&probe->alloc, data, sizeof(int32_t) * 4);
}

typedef struct {
    PgyAllocator alloc;
    PgyBoxArray_Int box;
    PgyArray_Int *arr;
} BoxArrayTraceProbe;

static void
box_array_trace_probe_create(void *userdata)
{
    BoxArrayTraceProbe *probe = (BoxArrayTraceProbe *)userdata;

    if (probe == NULL)
        return;

    probe->box = pgy_box_array_new_Int(16, &probe->alloc);
    probe->arr = pgy_box_array_get_Int(&probe->box);
}

static void
box_array_trace_probe_drop(void *userdata)
{
    BoxArrayTraceProbe *probe = (BoxArrayTraceProbe *)userdata;

    if (probe == NULL)
        return;

    pgy_box_array_drop_Int(&probe->box);
}

static void
panic_handler(int sig)
{
    (void)sig;
    if (g_panic_expected) {
        g_panic_expected = 0;
        longjmp(g_panic_jmp, 1);
    }
    /* Unexpected abort ??re-raise */
    signal(SIGABRT, SIG_DFL);
    raise(SIGABRT);
}

/*
 * EXPECT_PANIC(code_block):
 *   Expects `code_block` to trigger PGY_PANIC (abort).
 *   Uses setjmp/longjmp + SIGABRT handler to catch it.
 */
#define EXPECT_PANIC(name, code_block) \
    do { \
        int _saved_stderr = pgy_suppress_expected_panic_output(); \
        printf("  %-60s", name); \
        signal(SIGABRT, panic_handler); \
        g_panic_expected = 1; \
        if (setjmp(g_panic_jmp) == 0) { \
            code_block; \
            /* If we reach here, no panic occurred */ \
            g_panic_expected = 0; \
            printf("FAIL (no panic)\n"); g_fail++; \
        } else { \
            printf("pass\n"); g_pass++; \
        } \
        pgy_restore_expected_panic_output(_saved_stderr); \
        signal(SIGABRT, SIG_DFL); \
    } while (0)

/* -----------------------------------------------------------------
 * A. Struct size and offset tests
 * ----------------------------------------------------------------- */

#include "tests/memory/test_memory_layout.cases.h"

int
main(void)
{
    printf("=== Pergyra Memory Layout Test Suite ===\n");

    test_struct_sizes();
    test_struct_offsets();
    test_secure_slot_sizes();

    test_slot_lifecycle_int();
    test_slot_lifecycle_string();
    test_slot_lifecycle_all_types();
    test_slot_pin_views();

    test_secure_slot_lifecycle();
    test_secure_slot_pin_views();
    test_panic_conditions();
    test_slot_isolation();
    test_allocator_features();
    test_rc_weak_features();
    test_box_array_features();
    test_pointer_lifetime_guards();

    printf("\n=== Results: %d passed, %d failed ===\n", g_pass, g_fail);
    return (g_fail > 0) ? 1 : 0;
}
