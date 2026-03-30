/*
 * Copyright (c) 2025 Pergyra Language Project
 * HIR lowering test suite
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lexer/lexer.h"
#include "parser/parser.h"
#include "semantic/semantic.h"
#include "compiler/hir.h"

static int g_pass = 0;
static int g_fail = 0;

#define TEST(name) \
    do { printf("  %-60s", name); } while (0)

#define EXPECT(cond) \
    do { \
        if (cond) { printf("✓\n"); g_pass++; } \
        else      { printf("✗  (line %d)\n", __LINE__); g_fail++; } \
    } while (0)

static HIRProgram *
lower_from_source(const char *source)
{
    Lexer *lexer = lexer_create(source);
    Parser *parser = parser_create(lexer);
    ASTNode *ast = parser_parse_program(parser);
    SemanticResult *sem = semantic_analyze(ast);
    char *hir_error = NULL;
    HIRProgram *hir = NULL;

    if (!parser_has_error(parser) && sem != NULL && sem->success)
        hir = hir_lower(sem->annotated_ast, &hir_error);

    if (hir == NULL && hir_error != NULL)
        fprintf(stderr, "HIR lowering error: %s\n", hir_error);

    free(hir_error);
    semantic_result_destroy(sem);
    ast_destroy(ast);
    parser_destroy(parser);
    lexer_destroy(lexer);
    return hir;
}

static void
test_hir_lowering(void)
{
    printf("\n[hir]\n");

    TEST("mixed top-level program is bucketed correctly");
    {
        const char *src =
            "event OnHit(damage: Int);\n"
            "extern \"C\" { func SDL_Quit(); }\n"
            "func Main() -> Int { return 0; }\n"
            "let boot = 1;\n";
        HIRProgram *hir = lower_from_source(src);
        EXPECT(hir != NULL
               && hir->event_count == 1
               && hir->extern_count == 1
               && hir->function_count == 1
               && hir->executable_count == 1
               && hir->has_main_function);
        hir_destroy(hir);
    }

    TEST("unsupported root is rejected");
    {
        ASTNode *number = ast_create_number("1");
        char *error = NULL;
        HIRProgram *hir = hir_lower(number, &error);
        EXPECT(hir == NULL && error != NULL);
        free(error);
        ast_destroy(number);
    }

    TEST("event lambda subscription lowers through HIR");
    {
        const char *src =
            "event OnHit(damage: Int);\n"
            "func Main() -> Void {\n"
            "    OnHit += (d: Int) => { Log(d); };\n"
            "    OnHit(77);\n"
            "}\n";
        HIRProgram *hir = lower_from_source(src);
        EXPECT(hir != NULL
               && hir->event_count == 1
               && hir->function_count == 1
               && hir->executable_count == 0
               && hir->has_main_function);
        hir_destroy(hir);
    }
}

int
main(void)
{
    printf("=== Pergyra HIR Lowering Test Suite ===\n");
    test_hir_lowering();
    printf("\n=== Results: %d passed, %d failed ===\n", g_pass, g_fail);
    return g_fail == 0 ? 0 : 1;
}
