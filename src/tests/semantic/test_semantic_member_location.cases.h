/* Member and index access nodes carry the position of their selector (the
 * member name or `[`). They were created without one, so every diagnostic on
 * `a.b`, `a[i]` or a method call `a.M()` (a call copies its callee's
 * position) read 0:0. */
static bool
member_location_diagnostic_at(const SemanticResult *result, const char *needle,
                              uint32_t line, uint32_t col)
{
    if (result == NULL || needle == NULL)
        return false;
    for (size_t i = 0; i < result->diagnostic_count; i++) {
        Diagnostic *diag = result->diagnostics[i];
        if (diag != NULL && diag->message != NULL
            && strstr(diag->message, needle) != NULL)
            return diag->line == line && diag->col == col;
    }
    return false;
}

static void
test_member_access_diagnostic_location(void)
{
    static const struct {
        const char *name;
        const char *source;
        const char *diagnostic;
        uint32_t line;
        uint32_t col;
    } cases[] = {
        {"a world zone read out of its world points at the zone member",
         "zone Desk {\n"
         "    shared count: Int = 0\n"
         "}\n"
         "world Office {\n"
         "    zone desk: Desk\n"
         "    activate desk\n"
         "}\n"
         "func Inspect(d: Desk) -> Int { return 0; }\n"
         "func Main() -> Void {\n"
         "    let office = Office();\n"
         "    let n = Inspect(office.desk);\n"
         "}\n",
         "World-owned zone binding 'office.desk' cannot escape", 11, 28},
        {"an unknown member points at the member name",
         "struct Point {\n"
         "    let x: Int;\n"
         "}\n"
         "func Main() -> Void {\n"
         "    let p = Point(1);\n"
         "    let y: Int = p.y;\n"
         "}\n",
         "Unknown member", 6, 20},
    };

    for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
        TEST(cases[i].name);
        Lexer *lexer = lexer_create(cases[i].source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticResult *result = semantic_analyze(program);

        EXPECT(!parser_has_error(parser));
        EXPECT(result != NULL && result->error_count > 0
            && member_location_diagnostic_at(result, cases[i].diagnostic,
                cases[i].line, cases[i].col));
        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }
}
