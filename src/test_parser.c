/*
 * Copyright (c) 2025 Pergyra Language Project
 * Parser test driver
 */

#ifndef _WIN32
#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lexer/lexer.h"
#include "parser/ast.h"
#include "parser/parser.h"
#include "semantic/type_system.h"

#if defined(_WIN32)
#include <io.h>
#define pgy_dup _dup
#define pgy_dup2 _dup2
#define pgy_fileno _fileno
#define pgy_close _close
#else
#include <unistd.h>
#define pgy_dup dup
#define pgy_dup2 dup2
#define pgy_fileno fileno
#define pgy_close close
#endif

typedef struct
{
    const char *name;
    const char *code;
    int expect_success;
} TestCase;

static int
ast_print_contains(ASTNode *ast, const char *needle)
{
    FILE *capture = NULL;
    int saved_stdout = -1;
    long size;
    char *buffer = NULL;
    int contains = 0;

    if (ast == NULL || needle == NULL)
        return 0;

    capture = tmpfile();
    if (capture == NULL)
        return 0;

    fflush(stdout);
    saved_stdout = pgy_dup(pgy_fileno(stdout));
    if (saved_stdout < 0) {
        fclose(capture);
        return 0;
    }

    if (pgy_dup2(pgy_fileno(capture), pgy_fileno(stdout)) < 0) {
        pgy_close(saved_stdout);
        fclose(capture);
        return 0;
    }

    ast_print(ast, 0);
    fflush(stdout);

    pgy_dup2(saved_stdout, pgy_fileno(stdout));
    pgy_close(saved_stdout);
    saved_stdout = -1;

    if (fseek(capture, 0, SEEK_END) != 0) {
        fclose(capture);
        return 0;
    }
    size = ftell(capture);
    if (size < 0 || fseek(capture, 0, SEEK_SET) != 0) {
        fclose(capture);
        return 0;
    }

    buffer = (char *)malloc((size_t)size + 1);
    if (buffer == NULL) {
        fclose(capture);
        return 0;
    }

    if (size > 0) {
        size_t read_count = fread(buffer, 1, (size_t)size, capture);
        buffer[read_count] = '\0';
    } else {
        buffer[0] = '\0';
    }

    contains = strstr(buffer, needle) != NULL;

    free(buffer);
    fclose(capture);
    return contains;
}

static int
run_parser_test(const TestCase *test)
{
    int failed = 0;
    Lexer *lexer;
    Parser *parser;
    ASTNode *ast;

    printf("\n=== Test: %s ===\n", test->name);
    printf("Code:\n%s\n", test->code);
    printf("---\n");

    lexer = lexer_create(test->code);
    if (lexer == NULL) {
        printf("[FAIL] Failed to create lexer\n");
        return 1;
    }

    parser = parser_create(lexer);
    if (parser == NULL) {
        printf("[FAIL] Failed to create parser\n");
        lexer_destroy(lexer);
        return 1;
    }

    ast = parser_parse_program(parser);
    if (parser_has_error(parser)) {
        printf("Parse error: %s\n", parser_get_error(parser));
        if (test->expect_success) {
            printf("[FAIL] unexpected parse error\n");
            failed = 1;
        }
    } else {
        printf("Parsing successful!\n\n");
        printf("AST:\n");
        ast_print(ast, 0);
        if (!test->expect_success) {
            printf("[FAIL] expected parse failure but succeeded\n");
            failed = 1;
        }
    }

    ast_destroy(ast);
    parser_destroy(parser);
    lexer_destroy(lexer);
    return failed;
}

#include "tests/parser/test_parser_special_part_a.cases.h"
#include "tests/parser/test_parser_special_part_b.cases.h"
#include "tests/parser/test_parser_special_part_c.cases.h"

int
main(void)
{
#include "tests/parser/test_parser_table.cases.h"

    int failures = 0;
    size_t num_tests = sizeof(tests) / sizeof(tests[0]);

    printf("=== Pergyra Parser Test ===\n");

    for (size_t i = 0; i < num_tests; i++) {
        failures += run_parser_test(&tests[i]);
        printf("\n");
    }

    failures += run_doc_comment_attachment_test();
    printf("\n");
    failures += run_signature_effect_clause_test();
    printf("\n");
    failures += run_action_clause_reordering_test();
    printf("\n");
    failures += run_duplicate_action_clause_diagnostic_test();
    printf("\n");
    failures += run_malformed_effect_clause_diagnostic_test();
    printf("\n");
    failures += run_authorized_clause_missing_by_test();
    printf("\n");
    failures += run_intent_step_within_clause_hint_test();
    printf("\n");
    failures += run_intent_step_with_effects_hint_test();
    printf("\n");
    failures += run_intent_step_using_derivation_ast_print_test();
    printf("\n");
    failures += run_intent_header_value_param_ast_test();
    printf("\n");
    failures += run_intent_interleaved_header_binding_order_ast_test();
    printf("\n");
    failures += run_subject_keyword_alias_test();
    printf("\n");
    failures += run_tobject_keyword_alias_test();
    printf("\n");
    failures += run_tobject_keyword_test();
    printf("\n");
    failures += run_object_keyword_alias_test();
    printf("\n");
    failures += run_string_literal_surface_test();
    printf("\n");
    failures += run_pin_block_metadata_test();
    printf("\n");
    failures += run_named_call_argument_ast_print_test();
    printf("\n");
    failures += run_option_coalesce_ast_print_test();
    printf("\n");
    failures += run_reserved_slice_expression_diagnostic_test();
    printf("\n");
    failures += run_reserved_cast_type_test_diagnostic_test();
    printf("\n");
    failures += run_reserved_object_literal_diagnostic_test();
    printf("\n");
    failures += run_reserved_object_initializer_diagnostic_test();
    printf("\n");
    failures += run_reserved_scoped_unsafe_diagnostic_test();
    printf("\n");
    failures += run_reserved_labeled_unsafe_diagnostic_test();
    printf("\n");
    failures += run_vessel_keyword_alias_test();
    printf("\n");
    failures += run_lexical_zone_context_test();
    printf("\n");
    failures += run_parser_reentry_cleanup_test();
    printf("\n");

    printf("\n=== All tests completed ===\n");
    if (failures > 0) {
        printf("Parser test failures: %d\n", failures);
        return 1;
    }

    return 0;
}
