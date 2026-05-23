/*
 * Copyright (c) 2026 Pergyra Language Project
 * All rights reserved.
 *
 * test_abi_pipeline.c - ABI Pipeline Integration Test
 *
 * PURPOSE:
 *   Validate the ABI contract through the real compiler pipeline:
 *
 *     source -> semantic -> HIR -> DIR -> RIR -> MIR -> backend -> binary
 *
 *   This complements test_abi_spec.c:
 *   - test_abi_spec.c checks physical layout and runtime type agreement
 *   - test_abi_pipeline.c checks that the compiler emits binaries that
 *     produce correct values and meet a minimal runtime performance floor
 *
 * BUILD: make test-abi
 * RUN:   ./bin/test_abi_pipeline
 */

#ifndef _WIN32
#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif
#endif

#include <errno.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <io.h>
#include <process.h>
#include <windows.h>
#define PGY_DUP _dup
#define PGY_DUP2 _dup2
#define PGY_CLOSE _close
#define PGY_FILENO _fileno
#define PGY_GETPID _getpid
#define PGY_EXEEXT ".exe"
#else
#include <sys/time.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#define PGY_DUP dup
#define PGY_DUP2 dup2
#define PGY_CLOSE close
#define PGY_FILENO fileno
#define PGY_GETPID getpid
#define PGY_EXEEXT ""
#endif

#ifndef _WIN32
extern int setenv(const char *name, const char *value, int overwrite);
extern int unsetenv(const char *name);
#endif

#include "compiler/driver_app.h"
#include "compiler/compiler.h"

static int g_pass = 0;
static int g_fail = 0;
static unsigned g_temp_counter = 0;
static const char *g_case_filter = NULL;
static const char *g_backend_filter = NULL;
static const char *g_case_start = NULL;
static const char *g_case_stop = NULL;
static bool g_case_window_open = false;

#include "tests/abi/test_abi_pipeline_helpers.cases.h"

int
main(void)
{
    setvbuf(stdout, NULL, _IONBF, 0);
    setvbuf(stderr, NULL, _IONBF, 0);
    g_case_filter = getenv("PGY_ABI_PIPELINE_CASE");
    g_backend_filter = getenv("PGY_ABI_PIPELINE_BACKEND");
    g_case_start = getenv("PGY_ABI_PIPELINE_START_AT");
    g_case_stop = getenv("PGY_ABI_PIPELINE_STOP_AFTER");
    g_case_window_open = (g_case_start == NULL || g_case_start[0] == '\0');
    bool perf_mode = abi_perf_mode_enabled();
#include "tests/abi/test_abi_pipeline_fixtures_part_a.cases.h"
#include "tests/abi/test_abi_pipeline_fixtures_part_b.cases.h"
#include "tests/abi/test_abi_pipeline_cases_c.cases.h"
#include "tests/abi/test_abi_pipeline_cases_llvm.cases.h"
    printf("\n=== Results: %d passed, %d failed ===\n", g_pass, g_fail);
    return g_fail > 0 ? 1 : 0;
}
