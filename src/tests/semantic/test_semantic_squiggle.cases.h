/*
 * Semantic squiggle classification policy (docs/140 §3).
 *
 * Pure function over (is_blocking, DiagnosticLayer, code) -> SquiggleClass.
 * These lock in the mapping table so a code->colour drift is caught (CLAUDE.md
 * §11.2: codes/event names are regression-prone).
 */

#include "common/squiggle_class.h"

static void
test_squiggle_class(void)
{
    TEST("blocking diagnostic is always RED (fail-closed)");
    {
        /* Whatever the dimension, if it blocks it is shown as blocking. */
        EXPECT(squiggle_class_classify(true, DIAG_LAYER_TYPE,
                   "PGY_CODE_SEM_TYPE_MISMATCH") == SQUIGGLE_RED);
        EXPECT(squiggle_class_classify(true, DIAG_LAYER_DOMAIN,
                   "PGY_CODE_SEM_WORLD_CONTRACT_INVALID") == SQUIGGLE_RED);
    }

    TEST("axis-dimension codes are AMBER (advisory)");
    {
        EXPECT(squiggle_class_classify(false, DIAG_LAYER_DOMAIN,
                   "PGY_CODE_SEM_INTENT_BOUNDARY_DRIFT") == SQUIGGLE_AMBER);
        EXPECT(squiggle_class_classify(false, DIAG_LAYER_DOMAIN,
                   "PGY_CODE_SEM_INTENT_STEP_INVALID") == SQUIGGLE_AMBER);
        EXPECT(squiggle_class_classify(false, DIAG_LAYER_DOMAIN,
                   "PGY_CODE_SEM_ROLE_CONTRACT_INVALID") == SQUIGGLE_AMBER);
    }

    TEST("authority-dimension codes are VIOLET (advisory)");
    {
        EXPECT(squiggle_class_classify(false, DIAG_LAYER_DOMAIN,
                   "PGY_CODE_SEM_WORLD_CONTRACT_INVALID") == SQUIGGLE_VIOLET);
        EXPECT(squiggle_class_classify(false, DIAG_LAYER_DOMAIN,
                   "PGY_CODE_SEM_ZONE_CONTRACT_INVALID") == SQUIGGLE_VIOLET);
        EXPECT(squiggle_class_classify(false, DIAG_LAYER_DOMAIN,
                   "PGY_CODE_SEM_VISIBILITY_BOUNDARY") == SQUIGGLE_VIOLET);
        /* pin lives in the RESOURCE layer but is an authority concern: the code
         * term wins over the layer fallback. */
        EXPECT(squiggle_class_classify(false, DIAG_LAYER_RESOURCE,
                   "PGY_CODE_SEM_PIN_ESCAPE") == SQUIGGLE_VIOLET);
    }

    TEST("axis term wins over BOUNDARY (intent boundary is axis, not authority)");
    {
        /* INTENT_BOUNDARY contains both an axis term and "_BOUNDARY_"; axis is
         * checked first so it resolves to amber, while VISIBILITY_BOUNDARY (no
         * axis term) resolves to violet. */
        EXPECT(squiggle_class_classify(false, DIAG_LAYER_DOMAIN,
                   "PGY_CODE_SEM_INTENT_BOUNDARY_EVIDENCE_MISSING")
               == SQUIGGLE_AMBER);
    }

    TEST("erasure codes are BLUE (reserved dimension)");
    {
        EXPECT(squiggle_class_classify(false, DIAG_LAYER_DOMAIN,
                   "PGY_CODE_SEM_MEANING_ERASURE_VISIBLE") == SQUIGGLE_BLUE);
    }

    TEST("layer fallback when code carries no dimension term");
    {
        EXPECT(squiggle_class_classify(false, DIAG_LAYER_DOMAIN, NULL)
               == SQUIGGLE_AMBER);
        EXPECT(squiggle_class_classify(false, DIAG_LAYER_RESOURCE, NULL)
               == SQUIGGLE_AMBER);
        EXPECT(squiggle_class_classify(false, DIAG_LAYER_CONCURRENCY, NULL)
               == SQUIGGLE_VIOLET);
        /* A plain non-blocking diagnostic in a non-meaning layer gets no
         * semantic squiggle. */
        EXPECT(squiggle_class_classify(false, DIAG_LAYER_TYPE,
                   "PGY_CODE_SEM_SOME_WARNING") == SQUIGGLE_NONE);
    }

    TEST("squiggle_class_name maps to stable lowercase strings");
    {
        EXPECT(strcmp(squiggle_class_name(SQUIGGLE_RED), "red") == 0);
        EXPECT(strcmp(squiggle_class_name(SQUIGGLE_AMBER), "amber") == 0);
        EXPECT(strcmp(squiggle_class_name(SQUIGGLE_VIOLET), "violet") == 0);
        EXPECT(strcmp(squiggle_class_name(SQUIGGLE_BLUE), "blue") == 0);
        EXPECT(strcmp(squiggle_class_name(SQUIGGLE_NONE), "none") == 0);
    }
}

static void
test_squiggle_advisory(void)
{
    TEST("shadowing a Subject binding emits a non-blocking amber advisory");
    {
        /* End-to-end "third state" (docs/140): the program COMPILES (0 errors)
         * yet a meaning-axis drift is surfaced as an advisory. The inner `hero`
         * shadows the Subject-typed outer `hero`. */
        const char *source =
            "subject Hero {\n"
            "    let name: String;\n"
            "    let hp: Int;\n"
            "    action Hp(self) -> Int { return hp; }\n"
            "}\n"
            "func Main() -> Void {\n"
            "    let hero = Hero(\"knight\", 100);\n"
            "    if true {\n"
            "        let hero: Int = 7;\n"
            "        Log(hero);\n"
            "    }\n"
            "    Log(hero.Hp());\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticResult *result = semantic_analyze(program);

        EXPECT(!parser_has_error(parser));
        /* Non-blocking: compiles cleanly. */
        EXPECT(result != NULL && result->error_count == 0);
        /* But the meaning drift is shown. */
        EXPECT(result != NULL && result->advisory_count >= 1);
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "Domain identity of 'hero' is shadowed"));

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("no advisory when a plain (non-Subject) binding is shadowed");
    {
        /* Shadowing an Int carries no domain identity -> no squiggle. */
        const char *source =
            "func Main() -> Void {\n"
            "    let x: Int = 1;\n"
            "    if true {\n"
            "        let x: Int = 2;\n"
            "        Log(x);\n"
            "    }\n"
            "    Log(x);\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticResult *result = semantic_analyze(program);

        EXPECT(!parser_has_error(parser));
        EXPECT(result != NULL && result->error_count == 0);
        EXPECT(result != NULL && result->advisory_count == 0);

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }
}
