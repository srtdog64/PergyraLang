static int
run_tobject_keyword_test(void)
{
    const char *code =
        "tobject PlayerDto {\n"
        "    hp: Int;\n"
        "    name: String;\n"
        "}\n";
    int failed = 0;
    Lexer *lexer = lexer_create(code);
    Parser *parser = NULL;
    ASTNode *ast = NULL;
    ASTNode *decl = NULL;

    printf("\n=== Test: TObject Keyword ===\n");

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
        printf("[FAIL] Parse error: %s\n", parser_get_error(parser));
        failed = 1;
        goto cleanup;
    }

    if (ast == NULL || ast->type != AST_PROGRAM || ast->data.program.count != 1) {
        printf("[FAIL] Expected program with one declaration\n");
        failed = 1;
        goto cleanup;
    }

    decl = ast->data.program.statements[0];
    if (decl == NULL || decl->type != AST_CLASS_DECL || !decl->data.class_decl.is_struct
        || decl->data.class_decl.nominal_kind != NOMINAL_DECL_TOBJECT) {
        printf("[FAIL] Expected 'tobject' to parse as tobject declaration surface\n");
        failed = 1;
        goto cleanup;
    }

    if (strcmp(decl->data.class_decl.name, "PlayerDto") != 0
        || decl->data.class_decl.field_count != 2) {
        printf("[FAIL] tobject declaration members were not parsed correctly\n");
        failed = 1;
        goto cleanup;
    }

    printf("TObject keyword parsed successfully as tobject declaration surface!\n");

cleanup:
    ast_destroy(ast);
    parser_destroy(parser);
    lexer_destroy(lexer);
    return failed;
}

static int
run_tobject_keyword_alias_test(void)
{
    const char *code =
        "tobject PlayerPacket {\n"
        "    hp: Int;\n"
        "    name: String;\n"
        "}\n";
    int failed = 0;
    Lexer *lexer = lexer_create(code);
    Parser *parser = NULL;
    ASTNode *ast = NULL;
    ASTNode *decl = NULL;

    printf("\n=== Test: TObject Keyword Surface ===\n");

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
        printf("[FAIL] Parse error: %s\n", parser_get_error(parser));
        failed = 1;
        goto cleanup;
    }

    if (ast == NULL || ast->type != AST_PROGRAM || ast->data.program.count != 1) {
        printf("[FAIL] Expected program with one declaration\n");
        failed = 1;
        goto cleanup;
    }

    decl = ast->data.program.statements[0];
    if (decl == NULL || decl->type != AST_CLASS_DECL || !decl->data.class_decl.is_struct
        || decl->data.class_decl.nominal_kind != NOMINAL_DECL_TOBJECT) {
        printf("[FAIL] Expected 'tobject' to parse as transfer object surface\n");
        failed = 1;
        goto cleanup;
    }

    if (strcmp(decl->data.class_decl.name, "PlayerPacket") != 0
        || decl->data.class_decl.field_count != 2) {
        printf("[FAIL] TObject declaration members were not parsed correctly\n");
        failed = 1;
        goto cleanup;
    }

    printf("TObject keyword parsed successfully as transfer object surface!\n");

cleanup:
    ast_destroy(ast);
    parser_destroy(parser);
    lexer_destroy(lexer);
    return failed;
}

static int
run_object_keyword_alias_test(void)
{
    const char *code =
        "object PlayerView {\n"
        "    hp: Int;\n"
        "    name: String;\n"
        "}\n";
    int failed = 0;
    Lexer *lexer = lexer_create(code);
    Parser *parser = NULL;
    ASTNode *ast = NULL;
    ASTNode *decl = NULL;

    printf("\n=== Test: Object Keyword Surface ===\n");

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
        printf("[FAIL] Parse error: %s\n", parser_get_error(parser));
        failed = 1;
        goto cleanup;
    }

    if (ast == NULL || ast->type != AST_PROGRAM || ast->data.program.count != 1) {
        printf("[FAIL] Expected program with one declaration\n");
        failed = 1;
        goto cleanup;
    }

    decl = ast->data.program.statements[0];
    if (decl == NULL || decl->type != AST_CLASS_DECL || !decl->data.class_decl.is_struct
        || decl->data.class_decl.nominal_kind != NOMINAL_DECL_OBJECT) {
        printf("[FAIL] Expected 'object' to parse as object declaration surface\n");
        failed = 1;
        goto cleanup;
    }

    if (strcmp(decl->data.class_decl.name, "PlayerView") != 0
        || decl->data.class_decl.field_count != 2) {
        printf("[FAIL] Object declaration members were not parsed correctly\n");
        failed = 1;
        goto cleanup;
    }

    printf("Object keyword parsed successfully as object declaration surface!\n");

cleanup:
    ast_destroy(ast);
    parser_destroy(parser);
    lexer_destroy(lexer);
    return failed;
}

static int
run_string_literal_surface_test(void)
{
    const char *interpolated_code =
        "func Main() -> Void {\n"
        "    Log(f\"count={1 + 2}\");\n"
        "    Log(\"legacy=${3 + 4}\");\n"
        "}\n";
    const char *escaped_code =
        "func Main() -> Void {\n"
        "    Log(f\"open \\{name}\");\n"
        "}\n";
    const char *escaped_legacy_code =
        "func Main() -> Void {\n"
        "    Log(\"literal \\${name}\");\n"
        "}\n";
    const char *unmatched_code =
        "func Main() -> Void {\n"
        "    Log(f\"prefix {name\");\n"
        "}\n";
    const char *malformed_code =
        "func Main() -> Void {\n"
        "    Log(f\"bad {1 + }\");\n"
        "}\n";
    int failed = 0;
    Lexer *lexer = NULL;
    Parser *parser = NULL;
    ASTNode *ast = NULL;

    printf("\n=== Test: String Literal / Interpolation Stable Surface ===\n");

#define PARSE_STRING_CASE(src) \
    do { \
        lexer = lexer_create(src); \
        parser = lexer != NULL ? parser_create(lexer) : NULL; \
        ast = parser != NULL ? parser_parse_program(parser) : NULL; \
        if (lexer == NULL || parser == NULL || parser_has_error(parser)) { \
            printf("[FAIL] string-surface parse failed: %s\n", \
                parser != NULL ? parser_get_error(parser) : "<parser unavailable>"); \
            failed = 1; \
            goto cleanup; \
        } \
    } while (0)

    PARSE_STRING_CASE(interpolated_code);
    if (!ast_print_contains(ast, "ToString")) {
        printf("[FAIL] f-string / legacy interpolation did not lower through ToString\n");
        failed = 1;
        goto cleanup;
    }
    ast_destroy(ast);
    parser_destroy(parser);
    lexer_destroy(lexer);
    ast = NULL;
    parser = NULL;
    lexer = NULL;

    PARSE_STRING_CASE(escaped_code);
    if (ast_print_contains(ast, "ToString")
        || !ast_print_contains(ast, "open {name}")) {
        printf("[FAIL] escaped f-string brace should remain a literal brace\n");
        failed = 1;
        goto cleanup;
    }
    ast_destroy(ast);
    parser_destroy(parser);
    lexer_destroy(lexer);
    ast = NULL;
    parser = NULL;
    lexer = NULL;

    PARSE_STRING_CASE(escaped_legacy_code);
    if (ast_print_contains(ast, "ToString")) {
        printf("[FAIL] escaped legacy interpolation opener should remain literal\n");
        failed = 1;
        goto cleanup;
    }
    ast_destroy(ast);
    parser_destroy(parser);
    lexer_destroy(lexer);
    ast = NULL;
    parser = NULL;
    lexer = NULL;

    PARSE_STRING_CASE(unmatched_code);
    if (ast_print_contains(ast, "ToString")
        || !ast_print_contains(ast, "prefix {name")) {
        printf("[FAIL] unmatched interpolation brace should preserve literal text\n");
        failed = 1;
        goto cleanup;
    }
    ast_destroy(ast);
    parser_destroy(parser);
    lexer_destroy(lexer);
    ast = NULL;
    parser = NULL;
    lexer = NULL;

    PARSE_STRING_CASE(malformed_code);
    if (ast_print_contains(ast, "ToString")) {
        printf("[FAIL] malformed interpolation expression should preserve literal text\n");
        failed = 1;
        goto cleanup;
    }

cleanup:
#undef PARSE_STRING_CASE
    ast_destroy(ast);
    parser_destroy(parser);
    lexer_destroy(lexer);
    return failed;
}

static int
run_pin_block_metadata_test(void)
{
    const char *code =
        "func Main() -> Void {\n"
        "    let scores: Slot<Int> = ClaimSlot<Int>();\n"
        "    pin scores as view: WriteView<Int> {\n"
        "        Write(view, 1);\n"
        "    }\n"
        "}\n";
    int failed = 0;
    Lexer *lexer = lexer_create(code);
    Parser *parser = lexer != NULL ? parser_create(lexer) : NULL;
    ASTNode *ast = parser != NULL ? parser_parse_program(parser) : NULL;
    ASTNode *func = NULL;
    ASTNode *body = NULL;
    ASTNode *pin_block = NULL;

    printf("\n=== Test: Pin Block Metadata In AST ===\n");

    if (lexer == NULL || parser == NULL || parser_has_error(parser)) {
        printf("[FAIL] pin block metadata parse failed: %s\n",
            parser != NULL ? parser_get_error(parser) : "<parser unavailable>");
        failed = 1;
        goto cleanup;
    }

    if (ast == NULL || ast->type != AST_PROGRAM || ast->data.program.count == 0)
        failed = 1;
    else
        func = ast->data.program.statements[0];
    if (!failed && (func == NULL || func->type != AST_FUNC_DECL))
        failed = 1;
    else if (!failed)
        body = func->data.func_decl.body;
    if (!failed && (body == NULL || body->type != AST_BLOCK
        || body->data.block.count < 2)) {
        failed = 1;
    } else if (!failed) {
        pin_block = body->data.block.statements[1];
    }

    if (!failed && (pin_block == NULL || pin_block->type != AST_BLOCK
        || !pin_block->data.block.is_pin_block
        || !pin_block->data.block.pin_view_is_write
        || pin_block->data.block.pin_source_name == NULL
        || strcmp(pin_block->data.block.pin_source_name, "scores") != 0
        || pin_block->data.block.pin_view_name == NULL
        || strcmp(pin_block->data.block.pin_view_name, "view") != 0)) {
        failed = 1;
    }

    if (!failed && !ast_print_contains(ast, "Pin Block: scores as view (WriteView)"))
        failed = 1;

    if (failed)
        printf("[FAIL] pin block metadata was not preserved in AST\n");

cleanup:
    ast_destroy(ast);
    parser_destroy(parser);
    lexer_destroy(lexer);
    return failed;
}

static int
run_named_call_argument_ast_print_test(void)
{
    const char *code =
        "func Main() -> Void {\n"
        "    Log(value: 1);\n"
        "}\n";
    int failed = 0;
    Lexer *lexer = lexer_create(code);
    Parser *parser = lexer != NULL ? parser_create(lexer) : NULL;
    ASTNode *ast = parser != NULL ? parser_parse_program(parser) : NULL;

    printf("\n=== Test: Named Call Argument AST Print ===\n");

    if (lexer == NULL || parser == NULL || parser_has_error(parser)) {
        printf("[FAIL] named call argument parse failed: %s\n",
            parser != NULL ? parser_get_error(parser) : "<parser unavailable>");
        failed = 1;
        goto cleanup;
    }

    if (!ast_print_contains(ast, "Log(value: 1)")) {
        printf("[FAIL] named call argument was not preserved in AST print\n");
        failed = 1;
    }

cleanup:
    ast_destroy(ast);
    parser_destroy(parser);
    lexer_destroy(lexer);
    return failed;
}

static int
run_integral_float_ast_print_test(void)
{
    const char *code =
        "func Main() -> Void {\n"
        "    let f: Float = 16.0;\n"
        "    let i: Int = 16;\n"
        "}\n";
    int failed = 0;
    Lexer *lexer = lexer_create(code);
    Parser *parser = lexer != NULL ? parser_create(lexer) : NULL;
    ASTNode *ast = parser != NULL ? parser_parse_program(parser) : NULL;

    printf("\n=== Test: Integral Float AST Print ===\n");

    if (lexer == NULL || parser == NULL || parser_has_error(parser)) {
        printf("[FAIL] integral float parse failed: %s\n",
            parser != NULL ? parser_get_error(parser) : "<parser unavailable>");
        failed = 1;
        goto cleanup;
    }

    if (!ast_print_contains(ast, "Let: f : Float = 16.0")
        || !ast_print_contains(ast, "Let: i : Int = 16")) {
        printf("[FAIL] AST print erased integral Float spelling\n");
        failed = 1;
    }

cleanup:
    ast_destroy(ast);
    parser_destroy(parser);
    lexer_destroy(lexer);
    return failed;
}
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
