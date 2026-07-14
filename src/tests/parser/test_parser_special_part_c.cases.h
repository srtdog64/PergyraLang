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
