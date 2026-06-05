/*
 * Copyright (c) 2025 Pergyra Language Project
 * All rights reserved.
 *
 * Transpiler unit test suite
 * Build: make test-transpile
 * Run:   ./bin/test_transpile
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "common/string_compat.h"
#include "codegen/transpiler.h"
#include "codegen/transpiler_let_type_register_emit.h"
#include "codegen/transpiler_symbols.h"
#include "codegen/transpiler_type_declarator.h"
#include "codegen/transpiler_type_mapping.h"
#include "codegen/transpiler_type_render.h"
#include "lexer/lexer.h"
#include "parser/parser.h"
#include "semantic/semantic.h"
#include "compiler/rir.h"
#include "compiler/mir.h"
#include "semantic/type_system.h"
#include "semantic/type_checker.h"

/* -----------------------------------------------------------------
 * Test runner
 * ----------------------------------------------------------------- */

static int g_pass = 0;
static int g_fail = 0;
static MIRProgram *g_last_mir = NULL;

#define TEST(name) \
    do { g_last_mir = NULL; printf("  %-60s", name); } while (0)

#define EXPECT(cond) \
    do { \
        if (cond) { printf("ok\n"); g_pass++; } \
        else      { printf("fail (line %d)\n", __LINE__); g_fail++; } \
    } while (0)

#define EXPECT_STR_CONTAINS(haystack, needle) \
    do { \
        const char *_pgy_haystack = (haystack); \
        const char *_pgy_needle = (needle); \
        EXPECT(_pgy_haystack != NULL && _pgy_needle != NULL \
               && strstr(_pgy_haystack, _pgy_needle) != NULL); \
    } while (0)

#define EXPECT_STR_NOT_CONTAINS(haystack, needle) \
    do { \
        const char *_pgy_haystack = (haystack); \
        const char *_pgy_needle = (needle); \
        EXPECT(_pgy_haystack != NULL && _pgy_needle != NULL \
               && strstr(_pgy_haystack, _pgy_needle) == NULL); \
    } while (0)


#include "tests/transpile/test_transpile_helpers.cases.h"
#include "tests/transpile/test_transpile_core_part_0.cases.h"
#include "tests/transpile/test_transpile_core_part_a.cases.h"
#include "tests/transpile/test_transpile_core_part_b.cases.h"
#include "tests/transpile/test_transpile_program_part_a.cases.h"
#include "tests/transpile/test_transpile_program_part_b.cases.h"
#include "tests/transpile/test_transpile_domain_part_a.cases.h"
#include "tests/transpile/test_transpile_parallel_family.cases.h"
#include "tests/transpile/test_transpile_domain_async_part_a.cases.h"
#include "tests/transpile/test_transpile_domain_async_part_b.cases.h"
#include "tests/transpile/test_transpile_stdlib_part_a.cases.h"
#include "tests/transpile/test_transpile_stdlib_part_b.cases.h"
#include "tests/transpile/test_transpile_mir_part_a.cases.h"
#include "tests/transpile/test_transpile_mir_part_b.cases.h"
#include "tests/transpile/test_transpile_mir_source_order.cases.h"

int
main(void)
{
    printf("=== Pergyra C Transpiler Test Suite ===\n");

    type_system_init();

    test_codebuf();
    test_type_mapping();
    test_expression_emit();
    test_statement_emit();
    test_program_emit();
    test_ability_role_emit();
    test_party_emit();
    test_roster_world_emit();
    test_parallel_family_emit();
    test_parallel_execution_emit();
    test_slot_sugar();
    test_stdlib_and_enum_emit();
    test_transpiler_reentry_stability();
    test_mir_vertical_slice_emit();
    test_mir_select_dispatch_emit();
    test_intent_observability_emit();
    test_source_order_mir_emit();

    printf("\n=== Results: %d passed, %d failed ===\n", g_pass, g_fail);

    type_system_cleanup();
    return (g_fail > 0) ? 1 : 0;
}
