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

    printf("\n=== Test: Reserved Cast Type-Test Diagnostic ===\n");

    if (parser == NULL || !parser_has_error(parser)
        || error == NULL || strstr(error, "Cast syntax 'expr as Type' is reserved") == NULL) {
        printf("[FAIL] expected reserved cast diagnostic, got: %s\n",
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

    printf("\n=== Test: Reserved Object Literal Diagnostic ===\n");

    if (parser == NULL || !parser_has_error(parser)
        || error == NULL || strstr(error, "Object/map literal syntax") == NULL) {
        printf("[FAIL] expected reserved object literal diagnostic, got: %s\n",
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

    printf("\n=== Test: Reserved Scoped Unsafe Diagnostic ===\n");

    if (parser == NULL || !parser_has_error(parser)
        || error == NULL
        || strstr(error, "Scoped unsafe capability syntax") == NULL
        || strstr(error, "not a universal mode bit") == NULL) {
        printf("[FAIL] expected scoped unsafe diagnostic, got: %s\n",
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

    printf("\n=== Test: Reserved Labeled Unsafe Diagnostic ===\n");

    if (parser == NULL || !parser_has_error(parser)
        || error == NULL
        || strstr(error, "Scoped unsafe capability label syntax") == NULL
        || strstr(error, "not to a loose parser shortcut") == NULL) {
        printf("[FAIL] expected labeled unsafe diagnostic, got: %s\n",
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
