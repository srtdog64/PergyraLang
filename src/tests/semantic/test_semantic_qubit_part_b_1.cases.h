static void
test_qubit_slot_semantics_part_b(void)
{
    TEST("local Slot<subject> may borrow then move via own helper");
    {
        const char *source =
            "subject Vec2 {\n"
            "    let x: Int;\n"
            "    let y: Int;\n"
            "}\n"
            "func Borrow(ref s: Slot<Vec2>) -> Void {\n"
            "    Write(s, Vec2(1, 1));\n"
            "}\n"
            "func Consume(own s: Slot<Vec2>) -> Void {\n"
            "    Release(s);\n"
            "}\n"
            "func Main() -> Void {\n"
            "    let s: Slot<Vec2> = Vec2(0, 0);\n"
            "    Borrow(s);\n"
            "    Consume(s);\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticResult *result = semantic_analyze(program);

        EXPECT(!parser_has_error(parser));
        EXPECT(result != NULL && result->error_count == 0);

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("local Slot<subject> rejects borrow after own helper move");
    {
        const char *source =
            "subject Vec2 {\n"
            "    let x: Int;\n"
            "    let y: Int;\n"
            "}\n"
            "func Borrow(ref s: Slot<Vec2>) -> Void {\n"
            "    Write(s, Vec2(1, 1));\n"
            "}\n"
            "func Consume(own s: Slot<Vec2>) -> Void {\n"
            "    Release(s);\n"
            "}\n"
            "func Main() -> Void {\n"
            "    let s: Slot<Vec2> = Vec2(0, 0);\n"
            "    Consume(s);\n"
            "    Borrow(s);\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticResult *result = semantic_analyze(program);

        EXPECT(!parser_has_error(parser));
        EXPECT(result != NULL && result->error_count > 0);
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "Cannot borrow released slot"));

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("ref Slot<subject> parameter allows transitive safe ref forwarding");
    {
        const char *source =
            "subject Vec2 {\n"
            "    let x: Int;\n"
            "    let y: Int;\n"
            "}\n"
            "func Inner(ref t: Slot<Vec2>) -> Void {\n"
            "    Write(t, Vec2(2, 2));\n"
            "}\n"
            "func Middle(ref m: Slot<Vec2>) -> Void {\n"
            "    Inner(m);\n"
            "}\n"
            "func Touch(ref s: Slot<Vec2>) -> Void {\n"
            "    Middle(s);\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticResult *result = semantic_analyze(program);

        EXPECT(!parser_has_error(parser));
        EXPECT(result != NULL && result->error_count == 0);

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("ref Slot<subject> parameter rejects transitive forwarding into own helper");
    {
        const char *source =
            "subject Vec2 {\n"
            "    let x: Int;\n"
            "    let y: Int;\n"
            "}\n"
            "func Consume(own inner: Slot<Vec2>) -> Void {\n"
            "    Release(inner);\n"
            "}\n"
            "func Middle(ref m: Slot<Vec2>) -> Void {\n"
            "    Consume(m);\n"
            "}\n"
            "func Touch(ref s: Slot<Vec2>) -> Void {\n"
            "    Middle(s);\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticResult *result = semantic_analyze(program);

        EXPECT(!parser_has_error(parser));
        EXPECT(result != NULL && result->error_count > 0);
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "cannot escape through helper/function call"));

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("ref Slot<subject> parameter rejects conditional transitive forwarding into own helper");
    {
        const char *source =
            "subject Vec2 {\n"
            "    let x: Int;\n"
            "    let y: Int;\n"
            "}\n"
            "func Consume(own inner: Slot<Vec2>) -> Void {\n"
            "    Release(inner);\n"
            "}\n"
            "func Middle(ref m: Slot<Vec2>) -> Void {\n"
            "    if true {\n"
            "        Consume(m);\n"
            "    } else {\n"
            "        Write(m, Vec2(3, 3));\n"
            "    }\n"
            "}\n"
            "func Touch(ref s: Slot<Vec2>) -> Void {\n"
            "    Middle(s);\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticResult *result = semantic_analyze(program);

        EXPECT(!parser_has_error(parser));
        EXPECT(result != NULL && result->error_count > 0);
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "cannot escape through helper/function call"));

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("ref Slot<subject> parameter keeps conditional safe ref forwarding");
    {
        const char *source =
            "subject Vec2 {\n"
            "    let x: Int;\n"
            "    let y: Int;\n"
            "}\n"
            "func Inner(ref t: Slot<Vec2>) -> Void {\n"
            "    Write(t, Vec2(4, 4));\n"
            "}\n"
            "func Middle(ref m: Slot<Vec2>) -> Void {\n"
            "    if true {\n"
            "        Inner(m);\n"
            "    } else {\n"
            "        Write(m, Vec2(5, 5));\n"
            "    }\n"
            "}\n"
            "func Touch(ref s: Slot<Vec2>) -> Void {\n"
            "    Middle(s);\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticResult *result = semantic_analyze(program);

        EXPECT(!parser_has_error(parser));
        EXPECT(result != NULL && result->error_count == 0);

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("ref Slot<subject> parameter rejects return escape");
    {
        const char *source =
            "subject Vec2 {\n"
            "    let x: Int;\n"
            "    let y: Int;\n"
            "}\n"
            "func Leak(ref s: Slot<Vec2>) -> Slot<Vec2> {\n"
            "    return s;\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticResult *result = semantic_analyze(program);

        EXPECT(!parser_has_error(parser));
        EXPECT(result != NULL && result->error_count > 0);
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "cannot escape through return"));

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("ref Slot<subject> parameter channel send stays blocked by anchored-handle rule");
    {
        const char *source =
            "subject Vec2 {\n"
            "    let x: Int;\n"
            "    let y: Int;\n"
            "}\n"
            "func SendAway(ref s: Slot<Vec2>) -> Void {\n"
            "    let ch: Channel<Slot<Vec2>> = Channel(2);\n"
            "    ch <- s;\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticResult *result = semantic_analyze(program);

        EXPECT(!parser_has_error(parser));
        EXPECT(result != NULL && result->error_count > 0);
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "Borrowed ref slot handle (anchored) 's' cannot escape through channel send"));

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("own Slot<Int> parameter is accepted as explicit anchored-handle transfer boundary");
    {
        const char *source =
            "func Store(own s: Slot<Int>) -> Void {\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticResult *result = semantic_analyze(program);

        EXPECT(!parser_has_error(parser));
        EXPECT(result != NULL && result->error_count == 0);

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("ref Int parameter is accepted as value boundary");
    {
        const char *source =
            "func Borrow(ref value: Int) -> Void {\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticResult *result = semantic_analyze(program);

        EXPECT(!parser_has_error(parser));
        EXPECT(result != NULL && result->error_count == 0);

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("own Int parameter is accepted as value boundary");
    {
        const char *source =
            "func Store(own value: Int) -> Void {\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticResult *result = semantic_analyze(program);

        EXPECT(!parser_has_error(parser));
        EXPECT(result != NULL && result->error_count == 0);

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("ref Int parameter may return copied boundary value");
    {
        const char *source =
            "func Echo(ref value: Int) -> Int {\n"
            "    return value;\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticResult *result = semantic_analyze(program);

        EXPECT(!parser_has_error(parser));
        EXPECT(result != NULL && result->error_count == 0);

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("ref Int parameter may be stored into list by copy");
    {
        const char *source =
            "func Snapshot(ref value: Int) -> Void {\n"
            "    let items: List<Int> = ListNew();\n"
            "    ListPush(items, value);\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticResult *result = semantic_analyze(program);

        EXPECT(!parser_has_error(parser));
        EXPECT(result != NULL && result->error_count == 0);

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("ref Int parameter may be sent through channel by copy");
    {
        const char *source =
            "func Publish(ch: Channel<Int>, ref value: Int) -> Void {\n"
            "    ch <- value;\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticResult *result = semantic_analyze(program);

        EXPECT(!parser_has_error(parser));
        EXPECT(result != NULL && result->error_count == 0);

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("ref enum parameter is accepted as copied value boundary");
    {
        const char *source =
            "enum Mode { Idle, Active }\n"
            "func Use(ref mode: Mode) -> Mode {\n"
            "    return mode;\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticResult *result = semantic_analyze(program);

        EXPECT(!parser_has_error(parser));
        EXPECT(result != NULL && result->error_count == 0);

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("ref Int parameter may escape into a new binding by copy");
    {
        const char *source =
            "func Snapshot(ref value: Int) -> Void {\n"
            "    let alias: Int = value;\n"
            "    Log(alias);\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticResult *result = semantic_analyze(program);

        EXPECT(!parser_has_error(parser));
        EXPECT(result != NULL && result->error_count == 0);

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }
