static int
run_option_coalesce_ast_print_test(void)
{
    const char *code =
        "func Main() -> Void {\n"
        "    Log(value ?? 0);\n"
        "}\n";
    int failed = 0;
    Lexer *lexer = lexer_create(code);
    Parser *parser = lexer != NULL ? parser_create(lexer) : NULL;
    ASTNode *ast = parser != NULL ? parser_parse_program(parser) : NULL;

    printf("\n=== Test: Option Coalesce AST Print ===\n");

    if (lexer == NULL || parser == NULL || parser_has_error(parser)) {
        printf("[FAIL] option coalesce parse failed: %s\n",
            parser != NULL ? parser_get_error(parser) : "<parser unavailable>");
        failed = 1;
        goto cleanup;
    }

    if (!ast_print_contains(ast, "Log((value ?? 0))")) {
        printf("[FAIL] option coalesce operator was not preserved in AST print\n");
        failed = 1;
    }

cleanup:
    ast_destroy(ast);
    parser_destroy(parser);
    lexer_destroy(lexer);
    return failed;
}

static int
run_ast_program_splice_take_test(void)
{
    ASTNode *program = ast_create_program();
    ASTNode *replacement = ast_create_program();
    ASTNode *empty = ast_create_program();
    ASTNode *import_node = ast_create_import_declaration("part.pgy");
    ASTNode *tail = ast_create_identifier("tail");
    ASTNode *first = ast_create_identifier("first");
    ASTNode *second = ast_create_identifier("second");
    int failed = 0;

    printf("\n=== Test: AST Program Splice Ownership ===\n");

    if (program == NULL || replacement == NULL || empty == NULL
        || import_node == NULL || tail == NULL || first == NULL
        || second == NULL
        || !ast_program_append_statement(program, import_node)
        || !ast_program_append_statement(program, tail)
        || !ast_program_append_statement(replacement, first)
        || !ast_program_append_statement(replacement, second)) {
        printf("[FAIL] failed to build splice fixture\n");
        failed = 1;
        goto cleanup;
    }

    if (ast_program_splice_take(program, 9, replacement)
        || ast_program_statement_count(program) != 2
        || ast_program_statement_count(replacement) != 2) {
        printf("[FAIL] rejected splice mutated either owner\n");
        failed = 1;
        goto cleanup;
    }

    if (!ast_program_splice_take(program, 0, replacement)
        || ast_program_statement_count(program) != 3
        || ast_program_statement_count(replacement) != 0
        || ast_program_statement(program, 0) != first
        || ast_program_statement(program, 1) != second
        || ast_program_statement(program, 2) != tail) {
        printf("[FAIL] replacement statements were not transferred in order\n");
        failed = 1;
        goto cleanup;
    }

    if (!ast_program_splice_take(program, 1, empty)
        || ast_program_statement_count(program) != 2
        || ast_program_statement(program, 0) != first
        || ast_program_statement(program, 1) != tail) {
        printf("[FAIL] empty replacement did not remove one statement\n");
        failed = 1;
    }

cleanup:
    ast_destroy(program);
    ast_destroy(replacement);
    ast_destroy(empty);
    return failed;
}

static int
run_reserved_slice_expression_diagnostic_test(void)
{
    const char *code =
        "func Main() -> Void {\n"
        "    let part = xs[1..3];\n"
        "}\n";
    int failed = 0;
    Lexer *lexer = lexer_create(code);
    Parser *parser = lexer != NULL ? parser_create(lexer) : NULL;
    ASTNode *ast = parser != NULL ? parser_parse_program(parser) : NULL;
    const char *error = parser != NULL ? parser_get_error(parser) : NULL;

    printf("\n=== Test: Slice Range Expression Parses ===\n");

    if (parser == NULL || parser_has_error(parser)) {
        printf("[FAIL] expected slice range expression to parse, got: %s\n",
            error != NULL ? error : "<none>");
        failed = 1;
    }

    ast_destroy(ast);
    parser_destroy(parser);
    lexer_destroy(lexer);
    return failed;
}

static int
run_reserved_cast_type_test_diagnostic_test(void)
{
    const char *code =
        "func Main() -> Void {\n"
        "    let narrowed = value as Player;\n"
        "}\n";
    int failed = 0;
    Lexer *lexer = lexer_create(code);
    Parser *parser = lexer != NULL ? parser_create(lexer) : NULL;
    ASTNode *ast = parser != NULL ? parser_parse_program(parser) : NULL;
    const char *error = parser != NULL ? parser_get_error(parser) : NULL;

    printf("\n=== Test: Cast Expression Parses ===\n");

    /* `expr as Type` is now implemented and builds an AST_CAST node; the
     * parser must accept it (semantic analysis validates the target). */
    if (parser == NULL || parser_has_error(parser) || ast == NULL) {
        printf("[FAIL] expected cast expression to parse, got: %s\n",
            error != NULL ? error : "<none>");
        failed = 1;
    }

    ast_destroy(ast);
    parser_destroy(parser);
    lexer_destroy(lexer);
    return failed;
}

static int
run_type_test_expression_parses_test(void)
{
    const char *code =
        "func Main() -> Void {\n"
        "    let i: Int = 5;\n"
        "    let b = i is Int;\n"
        "}\n";
    int failed = 0;
    Lexer *lexer = lexer_create(code);
    Parser *parser = lexer != NULL ? parser_create(lexer) : NULL;
    ASTNode *ast = parser != NULL ? parser_parse_program(parser) : NULL;
    const char *error = parser != NULL ? parser_get_error(parser) : NULL;

    printf("\n=== Test: Type-Test Expression Parses ===\n");

    /* `expr is Type` is now implemented and builds an AST_TYPE_TEST node; the
     * parser must accept it (semantic analysis validates the target). */
    if (parser == NULL || parser_has_error(parser) || ast == NULL) {
        printf("[FAIL] expected type-test expression to parse, got: %s\n",
            error != NULL ? error : "<none>");
        failed = 1;
    }

    ast_destroy(ast);
    parser_destroy(parser);
    lexer_destroy(lexer);
    return failed;
}

static int
run_reserved_object_literal_diagnostic_test(void)
{
    const char *code =
        "func Main() -> Void {\n"
        "    let value = { hp: 10 };\n"
        "}\n";
    int failed = 0;
    Lexer *lexer = lexer_create(code);
    Parser *parser = lexer != NULL ? parser_create(lexer) : NULL;
    ASTNode *ast = parser != NULL ? parser_parse_program(parser) : NULL;
    const char *error = parser != NULL ? parser_get_error(parser) : NULL;

    printf("\n=== Test: Map Literal Parses ===\n");

    /* Bare `{ key: value }` is now implemented and builds an AST_MAP_LITERAL
     * node, so the parser must accept it. */
    if (parser == NULL || parser_has_error(parser) || ast == NULL) {
        printf("[FAIL] expected map literal to parse, got: %s\n",
            error != NULL ? error : "<none>");
        failed = 1;
    }

    ast_destroy(ast);
    parser_destroy(parser);
    lexer_destroy(lexer);
    return failed;
}

static int
run_reserved_object_initializer_diagnostic_test(void)
{
    const char *code =
        "func Main() -> Void {\n"
        "    let value = Player { hp: 10 };\n"
        "}\n";
    int failed = 0;
    Lexer *lexer = lexer_create(code);
    Parser *parser = lexer != NULL ? parser_create(lexer) : NULL;
    ASTNode *ast = parser != NULL ? parser_parse_program(parser) : NULL;
    const char *error = parser != NULL ? parser_get_error(parser) : NULL;

    printf("\n=== Test: Object Initializer Parses ===\n");

    /* Object initializer `Type { field: value }` is now implemented and
     * lowers to a named-argument constructor call, so it must parse cleanly. */
    if (parser == NULL || parser_has_error(parser) || ast == NULL) {
        printf("[FAIL] expected object initializer to parse, got: %s\n",
            error != NULL ? error : "<none>");
        failed = 1;
    }

    ast_destroy(ast);
    parser_destroy(parser);
    lexer_destroy(lexer);
    return failed;
}

static int
run_reserved_scoped_unsafe_diagnostic_test(void)
{
    const char *code =
        "func Main() -> Void {\n"
        "    unsafe(raw) {\n"
        "        Log(1);\n"
        "    }\n"
        "}\n";
    int failed = 0;
    Lexer *lexer = lexer_create(code);
    Parser *parser = lexer != NULL ? parser_create(lexer) : NULL;
    ASTNode *ast = parser != NULL ? parser_parse_program(parser) : NULL;
    const char *error = parser != NULL ? parser_get_error(parser) : NULL;

    printf("\n=== Test: Scoped Unsafe Parses ===\n");

    /* `unsafe(capability) { ... }` is now implemented and builds an
     * AST_UNSAFE_BLOCK node carrying the capability label. */
    if (parser == NULL || parser_has_error(parser) || ast == NULL) {
        printf("[FAIL] expected scoped unsafe block to parse, got: %s\n",
            error != NULL ? error : "<none>");
        failed = 1;
    }

    ast_destroy(ast);
    parser_destroy(parser);
    lexer_destroy(lexer);
    return failed;
}

static int
run_reserved_labeled_unsafe_diagnostic_test(void)
{
    const char *code =
        "func Main() -> Void {\n"
        "    unsafe raw {\n"
        "        Log(1);\n"
        "    }\n"
        "}\n";
    int failed = 0;
    Lexer *lexer = lexer_create(code);
    Parser *parser = lexer != NULL ? parser_create(lexer) : NULL;
    ASTNode *ast = parser != NULL ? parser_parse_program(parser) : NULL;
    const char *error = parser != NULL ? parser_get_error(parser) : NULL;

    printf("\n=== Test: Labeled Unsafe Parses ===\n");

    /* `unsafe capability { ... }` is now implemented and builds an
     * AST_UNSAFE_BLOCK node carrying the capability label. */
    if (parser == NULL || parser_has_error(parser) || ast == NULL) {
        printf("[FAIL] expected labeled unsafe block to parse, got: %s\n",
            error != NULL ? error : "<none>");
        failed = 1;
    }

    ast_destroy(ast);
    parser_destroy(parser);
    lexer_destroy(lexer);
    return failed;
}

static int
run_reserved_role_slot_union_diagnostic_test(void)
{
    const char *code =
        "party FlexibleTeam {\n"
        "    role slot support: Healing | Buffing\n"
        "}\n";
    int failed = 0;
    Lexer *lexer = lexer_create(code);
    Parser *parser = lexer != NULL ? parser_create(lexer) : NULL;
    ASTNode *ast = parser != NULL ? parser_parse_program(parser) : NULL;
    const char *error = parser != NULL ? parser_get_error(parser) : NULL;

    printf("\n=== Test: Reserved Role Slot Union Diagnostic ===\n");

    if (parser == NULL || !parser_has_error(parser)
        || error == NULL
        || strstr(error, "Role slot ability union '|' is reserved") == NULL
        || strstr(error, "bind-time ambiguity diagnostics") == NULL) {
        printf("[FAIL] expected role slot union diagnostic, got: %s\n",
            error != NULL ? error : "<none>");
        failed = 1;
    }

    ast_destroy(ast);
    parser_destroy(parser);
    lexer_destroy(lexer);
    return failed;
}

static int
run_reserved_role_slot_boolean_and_diagnostic_test(void)
{
    const char *code =
        "party DungeonTeam {\n"
        "    role slot tank: Damageable && Guardable\n"
        "}\n";
    int failed = 0;
    Lexer *lexer = lexer_create(code);
    Parser *parser = lexer != NULL ? parser_create(lexer) : NULL;
    ASTNode *ast = parser != NULL ? parser_parse_program(parser) : NULL;
    const char *error = parser != NULL ? parser_get_error(parser) : NULL;

    printf("\n=== Test: Role Slot Boolean And Diagnostic ===\n");

    if (parser == NULL || !parser_has_error(parser)
        || error == NULL
        || strstr(error, "uses '&', not '&&'") == NULL
        || strstr(error, "boolean expression operator") == NULL) {
        printf("[FAIL] expected role slot boolean-and diagnostic, got: %s\n",
            error != NULL ? error : "<none>");
        failed = 1;
    }

    ast_destroy(ast);
    parser_destroy(parser);
    lexer_destroy(lexer);
    return failed;
}

static int
run_reserved_container_role_slot_intersection_diagnostic_test(void)
{
    const char *code =
        "party CityDistrict {\n"
        "    role slot citizens: Array<Living & Economic>\n"
        "}\n";
    int failed = 0;
    Lexer *lexer = lexer_create(code);
    Parser *parser = lexer != NULL ? parser_create(lexer) : NULL;
    ASTNode *ast = parser != NULL ? parser_parse_program(parser) : NULL;
    const char *error = parser != NULL ? parser_get_error(parser) : NULL;

    printf("\n=== Test: Reserved Container Role Slot Intersection Diagnostic ===\n");

    if (parser == NULL || !parser_has_error(parser)
        || error == NULL
        || strstr(error, "Container-internal ability intersections") == NULL
        || strstr(error, "element provenance") == NULL) {
        printf("[FAIL] expected container role-slot diagnostic, got: %s\n",
            error != NULL ? error : "<none>");
        failed = 1;
    }

    ast_destroy(ast);
    parser_destroy(parser);
    lexer_destroy(lexer);
    return failed;
}

/* Witness for the diagnostic-truncation class: parser_error must render the
 * whole message, not a fixed-buffer prefix of it. The old sink clipped the
 * format expansion at 384 bytes and the assembled text at 512, so a parse
 * error naming a long identifier lost its tail -- and the output said
 * nothing about it. A silently lossy diagnostic is a lie about what the
 * compiler saw, so the check is on the FULL text: the long name survives
 * intact, and the Code/Reason/Fix/location suffix still follows it. */
static int
run_parser_error_no_silent_truncation_test(void)
{
    const size_t long_name_len = 4000;
    char *long_name;
    Lexer *lexer;
    Parser *parser;
    const char *rendered;
    int failed = 0;

    printf("\n=== Test: Parser Error Has No Silent Truncation ===\n");

    long_name = malloc(long_name_len + 1);
    if (long_name == NULL) {
        printf("[FAIL] Could not allocate the long identifier\n");
        return 1;
    }
    memset(long_name, 'A', long_name_len);
    long_name[long_name_len] = '\0';

    lexer = lexer_create("func Main() -> Void {}\n");
    parser = lexer != NULL ? parser_create(lexer) : NULL;
    if (parser == NULL) {
        printf("[FAIL] Could not create parser\n");
        free(long_name);
        lexer_destroy(lexer);
        return 1;
    }

    parser_error(parser, "Unexpected token '%s'", long_name);
    rendered = parser_get_error(parser);

    if (!parser_has_error(parser)) {
        printf("[FAIL] parser_error did not raise has_error\n");
        failed++;
    }
    if (strstr(rendered, long_name) == NULL) {
        printf("[FAIL] Long identifier was truncated out of the diagnostic"
               " (rendered %zu bytes)\n", strlen(rendered));
        failed++;
    }
    if (strstr(rendered, "\nCode: " PGY_CODE_PARSE_SYNTAX) == NULL) {
        printf("[FAIL] Diagnostic lost its Code: routing prefix\n");
        failed++;
    }
    if (strstr(rendered, "\nFix: " PGY_FIX_CHECK_SYNTAX) == NULL) {
        printf("[FAIL] Diagnostic lost its Fix: hint\n");
        failed++;
    }
    if (strstr(rendered, " at line ") == NULL) {
        printf("[FAIL] Diagnostic lost its source location suffix\n");
        failed++;
    }
    if (failed == 0)
        printf("[PASS] %zu-byte diagnostic survived whole\n", strlen(rendered));

    free(long_name);
    parser_destroy(parser);
    lexer_destroy(lexer);
    return failed;
}

/* Witness for the sinks that still cannot own a heap string (the LLVM ctx
 * error buffer, the intent contract summary, the C-backend reason text):
 * they may clip, but they must announce the clip. A clipped diagnostic that
 * reads as whole is worse than a short one, because the reader has no way to
 * know they are debugging a fragment. */
static int
run_fixed_sink_announces_clipping_test(void)
{
    char sink[64];
    char fits[64];
    int failed = 0;
    int needed;

    printf("\n=== Test: Fixed Diagnostic Sink Announces Clipping ===\n");

    needed = snprintf(sink, sizeof(sink), "%s",
                      "0123456789012345678901234567890123456789"
                      "0123456789012345678901234567890123456789");
    pergyra_str_mark_clipped(sink, sizeof(sink), needed);
    if (strstr(sink, "bytes clipped") == NULL) {
        printf("[FAIL] Clipped sink did not announce the loss: '%s'\n", sink);
        failed++;
    }
    if (strlen(sink) >= sizeof(sink)) {
        printf("[FAIL] Clip marker overran the sink\n");
        failed++;
    }

    needed = snprintf(fits, sizeof(fits), "%s", "short message");
    pergyra_str_mark_clipped(fits, sizeof(fits), needed);
    if (strcmp(fits, "short message") != 0) {
        printf("[FAIL] Marker fired on text that fit: '%s'\n", fits);
        failed++;
    }

    if (failed == 0)
        printf("[PASS] Clip is announced, and untouched when text fits\n");
    return failed;
}

static int
run_typed_intent_tobject_transition_parse_test(void)
{
    const char *code =
        "subject Actor {}\n"
        "intent Workflow(actor: Actor) -> WorkflowOutcome {\n"
        "    step A {\n"
        "        on outcome_a: actor.ForwardA();\n"
        "        success: ACommitted(receipt_a);\n"
        "        failure: ARejected(problem_a);\n"
        "    }\n"
        "    step B after A {\n"
        "        on outcome_b: actor.ForwardB(receipt_a);\n"
        "        success: BCommitted(receipt_b);\n"
        "        failure: BRejected(problem_b);\n"
        "    }\n"
        "    success B: WorkflowCommitted(receipt_b);\n"
        "    failure A: WorkflowFailedA(problem_a);\n"
        "    failure B: WorkflowFailedB(problem_b);\n"
        "}\n";
    Lexer *lexer = lexer_create(code);
    Parser *parser = NULL;
    ASTNode *program = NULL;
    ASTNode *intent = NULL;
    ASTNode **steps = NULL;
    int failed = 0;

    printf("\n=== Test: Typed Intent TObject Transition Parse ===\n");
    if (lexer == NULL)
        return 1;
    parser = parser_create(lexer);
    if (parser == NULL) {
        lexer_destroy(lexer);
        return 1;
    }
    program = parser_parse_program(parser);
    if (program != NULL && ast_program_statement_count(program) == 2)
        intent = ast_program_statement(program, 1);
    if (intent != NULL)
        steps = ast_intent_decl_steps(intent, NULL);

    if (parser_has_error(parser) || intent == NULL
        || !ast_intent_decl_has_typed_result(intent)
        || ast_intent_decl_return_type(intent) == NULL
        || strcmp(ast_type_name(ast_intent_decl_return_type(intent)),
                  "WorkflowOutcome") != 0
        || ast_intent_decl_step_count(intent) != 2
        || steps == NULL
        || ast_intent_step_predecessor_name(steps[0]) != NULL
        || ast_intent_step_predecessor_name(steps[1]) == NULL
        || strcmp(ast_intent_step_predecessor_name(steps[1]), "A") != 0
        || ast_intent_step_success_variant_name(steps[0]) == NULL
        || strcmp(ast_intent_step_success_variant_name(steps[0]),
                  "ACommitted") != 0
        || ast_intent_step_success_payload_name(steps[0]) == NULL
        || strcmp(ast_intent_step_success_payload_name(steps[0]),
                  "receipt_a") != 0
        || ast_intent_step_failure_variant_name(steps[1]) == NULL
        || strcmp(ast_intent_step_failure_variant_name(steps[1]),
                  "BRejected") != 0
        || ast_intent_step_failure_payload_name(steps[1]) == NULL
        || strcmp(ast_intent_step_failure_payload_name(steps[1]),
                  "problem_b") != 0
        || ast_intent_decl_success_terminal_step(intent) == NULL
        || strcmp(ast_intent_decl_success_terminal_step(intent), "B") != 0
        || ast_intent_decl_success_terminal_expr(intent) == NULL
        || ast_intent_decl_failure_terminal_count(intent) != 2
        || ast_intent_decl_failure_terminal_step(intent, 0) == NULL
        || strcmp(ast_intent_decl_failure_terminal_step(intent, 0), "A") != 0
        || ast_intent_decl_failure_terminal_step(intent, 1) == NULL
        || strcmp(ast_intent_decl_failure_terminal_step(intent, 1), "B") != 0
        || !ast_print_contains(intent, "IntentReturns: WorkflowOutcome")
        || !ast_print_contains(intent, "IntentStep: B after A")
        || !ast_print_contains(intent,
            "IntentTerminalFailure: B => WorkflowFailedB(problem_b)")) {
        printf("[FAIL] Typed intent transition AST facts drifted: %s\n",
            parser_get_error(parser) != NULL
                ? parser_get_error(parser) : "<no parser diagnostic>");
        failed = 1;
    }

    ast_destroy(program);
    parser_destroy(parser);
    lexer_destroy(lexer);
    return failed;
}

static int
run_parser_reentry_cleanup_test(void)
{
    const char *valid_code =
        "struct Vec2 {\n"
        "    x: Int;\n"
        "    y: Int;\n"
        "}\n"
        "func Main() -> Vec2 {\n"
        "    return Vec2(1, 2);\n"
        "}\n";
    const char *invalid_code =
        "func Main( -> Void {\n"
        "    return;\n"
        "}\n";
    int failed = 0;

    printf("\n=== Test: Parser Reentry Cleanup ===\n");

    for (int i = 0; i < 64; i++) {
        Lexer *lexer = lexer_create(valid_code);
        Parser *parser = NULL;
        ASTNode *ast = NULL;

        if (lexer == NULL) {
            printf("[FAIL] Failed to create lexer for valid reentry iteration %d\n", i);
            return 1;
        }

        parser = parser_create(lexer);
        if (parser == NULL) {
            printf("[FAIL] Failed to create parser for valid reentry iteration %d\n", i);
            lexer_destroy(lexer);
            return 1;
        }

        ast = parser_parse_program(parser);
        if (parser_has_error(parser) || ast == NULL || ast->type != AST_PROGRAM) {
            printf("[FAIL] Valid parser reentry iteration %d failed\n", i);
            failed = 1;
        }

        ast_destroy(ast);
        parser_destroy(parser);
        lexer_destroy(lexer);

        if (failed)
            return failed;
    }

    for (int i = 0; i < 64; i++) {
        Lexer *lexer = lexer_create(invalid_code);
        Parser *parser = NULL;
        ASTNode *ast = NULL;

        if (lexer == NULL) {
            printf("[FAIL] Failed to create lexer for invalid reentry iteration %d\n", i);
            return 1;
        }

        parser = parser_create(lexer);
        if (parser == NULL) {
            printf("[FAIL] Failed to create parser for invalid reentry iteration %d\n", i);
            lexer_destroy(lexer);
            return 1;
        }

        ast = parser_parse_program(parser);
        if (!parser_has_error(parser) || parser_get_error(parser) == NULL) {
            printf("[FAIL] Invalid parser reentry iteration %d unexpectedly succeeded\n", i);
            failed = 1;
        }

        ast_destroy(ast);
        parser_destroy(parser);
        lexer_destroy(lexer);

        if (failed)
            return failed;
    }

    printf("Parser success/error cleanup stays stable across repeated same-process runs!\n");
    return 0;
}
