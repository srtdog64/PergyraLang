/* A UTF-8 byte order mark is skipped as an encoding signature, and a lexer
 * error token stays on the same token stream as every other token. The error
 * token used to leave its stream anchor uninitialized, so the parser reported
 * "token stream anchor changed during parse" (a BOM triggered it on every
 * import) instead of the lexer's own diagnostic. */
static int
run_lexer_anchor_case(const char *name, const char *code, bool expect_error)
{
    Lexer *lexer = lexer_create(code);
    Parser *parser;
    ASTNode *ast;
    const char *error;
    int failed = 0;

    if (lexer == NULL) {
        printf("[FAIL] %s: failed to create lexer\n", name);
        return 1;
    }
    parser = parser_create(lexer);
    if (parser == NULL) {
        printf("[FAIL] %s: failed to create parser\n", name);
        lexer_destroy(lexer);
        return 1;
    }
    ast = parser_parse_program(parser);
    error = parser_get_error(parser);
    if (parser_has_error(parser) != expect_error) {
        printf("[FAIL] %s: parse error expected=%d, got: %s\n", name,
               expect_error ? 1 : 0, error != NULL ? error : "<none>");
        failed = 1;
    } else if (error != NULL
               && strstr(error, "anchor changed during parse") != NULL) {
        printf("[FAIL] %s: lexer error surfaced as a stream-anchor error: %s\n",
               name, error);
        failed = 1;
    } else {
        printf("%s.\n", name);
    }
    ast_destroy(ast);
    parser_destroy(parser);
    lexer_destroy(lexer);
    return failed;
}

static int
run_lexer_stream_anchor_tests(void)
{
    int failures = 0;

    failures += run_lexer_anchor_case(
        "A UTF-8 byte order mark before the first token is skipped",
        "\xEF\xBB\xBF" "func Main() -> Void {\n    let a = 1;\n}\n", false);
    failures += run_lexer_anchor_case(
        "An unexpected byte stays on the token stream as a lexer error",
        "func Main() -> Void {\n    let a = 1 \xC2\xA7;\n}\n", true);
    return failures;
}
