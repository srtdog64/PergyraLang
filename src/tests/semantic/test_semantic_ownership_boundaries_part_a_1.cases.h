static void
test_semantic_ownership_boundaries_part_a(void)
{
    TEST("ref QubitSlot parameter is accepted as borrowed movable-resource boundary");
    {
        const char *source =
            "func Inspect(ref q: QubitSlot) -> Void {\n"
            "    return;\n"
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
    TEST("ref QubitSlot parameter cannot escape through return");
    {
        const char *source =
            "func Echo(ref q: QubitSlot) -> QubitSlot {\n"
            "    return q;\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticResult *result = semantic_analyze(program);

        EXPECT(!parser_has_error(parser));
        EXPECT(result != NULL && result->error_count > 0);
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "Borrowed ref slot handle (movable) 'q' cannot escape through return"));

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("ref QubitSlot parameter cannot escape through channel send");
    {
        const char *source =
            "func Publish(ch: Channel<QubitSlot>, ref q: QubitSlot) -> Void {\n"
            "    ch <- q;\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticResult *result = semantic_analyze(program);

        EXPECT(!parser_has_error(parser));
        EXPECT(result != NULL && result->error_count > 0);
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "Borrowed ref slot handle (movable) 'q' cannot escape through channel send"));

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("ref QubitSlot parameter cannot escape through helper/function call");
    {
        const char *source =
            "func UseOwned(own q: QubitSlot) -> Void {\n"
            "    return;\n"
            "}\n"
            "func BorrowForward(ref q: QubitSlot) -> Void {\n"
            "    UseOwned(q);\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticResult *result = semantic_analyze(program);

        EXPECT(!parser_has_error(parser));
        EXPECT(result != NULL && result->error_count > 0);
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "Borrowed ref slot handle (movable) 'q' cannot escape through helper/function call"));
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "forwarding it to 'UseOwned' as 'own' would create a transitive helper transfer"));

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("ref QubitSlot parameter cannot escape transitively through ref helper return");
    {
        const char *source =
            "func EchoBorrow(ref q: QubitSlot) -> QubitSlot {\n"
            "    return q;\n"
            "}\n"
            "func BorrowForward(ref q: QubitSlot) -> QubitSlot {\n"
            "    return EchoBorrow(q);\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticResult *result = semantic_analyze(program);

        EXPECT(!parser_has_error(parser));
        EXPECT(result != NULL && result->error_count > 0);
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "Borrowed ref slot handle (movable) 'q' cannot escape through helper/function call"));
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "callee 'EchoBorrow' derives a transitive helper-escape path for its borrowed parameter"));

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("ref QubitSlot parameter cannot escape through transitive helper chain");
    {
        const char *source =
            "func UseOwned(own q: QubitSlot) -> Void {\n"
            "    return;\n"
            "}\n"
            "func Forward(own q: QubitSlot) -> Void {\n"
            "    UseOwned(q);\n"
            "}\n"
            "func BorrowForward(ref q: QubitSlot) -> Void {\n"
            "    Forward(q);\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticResult *result = semantic_analyze(program);

        EXPECT(!parser_has_error(parser));
        EXPECT(result != NULL && result->error_count > 0);
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "Borrowed ref slot handle (movable) 'q' cannot escape through helper/function call to 'Forward'"));
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "forwarding it to 'Forward' as 'own' would create a transitive helper transfer"));

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("ref QubitSlot parameter cannot escape into new binding");
    {
        const char *source =
            "func BorrowAlias(ref q: QubitSlot) -> Void {\n"
            "    let alias: QubitSlot = q;\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticResult *result = semantic_analyze(program);

        EXPECT(!parser_has_error(parser));
        EXPECT(result != NULL && result->error_count > 0);
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "cannot escape into new binding"));
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "borrowed 'ref' slot handle (movable)"));
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "Fix:"));

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("ref QubitSlot parameter cannot escape through assignment rebind");
    {
        const char *source =
            "func BorrowAssign(ref q: QubitSlot) -> Void {\n"
            "    let dst: QubitSlot = ClaimQubit();\n"
            "    dst = q;\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticResult *result = semantic_analyze(program);

        EXPECT(!parser_has_error(parser));
        EXPECT(result != NULL && result->error_count > 0);
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "cannot escape through assignment rebind"));
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "borrowed 'ref' slot handle (movable)"));
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "Fix:"));

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("ref QubitSlot parameter cannot escape through list store");
    {
        const char *source =
            "func BorrowList(ref q: QubitSlot) -> Void {\n"
            "    let items: List<QubitSlot> = ListNew();\n"
            "    ListPush(items, q);\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticResult *result = semantic_analyze(program);

        EXPECT(!parser_has_error(parser));
        EXPECT(result != NULL && result->error_count > 0);
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "Borrowed ref slot handle (movable) 'q' cannot escape through list store"));
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "borrowed 'ref' slot handle (movable)"));

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("ref QubitSlot parameter cannot escape through queue store");
    {
        const char *source =
            "func BorrowQueue(ref q: QubitSlot) -> Void {\n"
            "    let items: Queue<QubitSlot> = QueueNew();\n"
            "    QueuePush(items, q);\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticResult *result = semantic_analyze(program);

        EXPECT(!parser_has_error(parser));
        EXPECT(result != NULL && result->error_count > 0);
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "Borrowed ref slot handle (movable) 'q' cannot escape through queue store"));

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("ref QubitSlot parameter cannot escape through array overwrite");
    {
        const char *source =
            "func BorrowArraySet(ref q: QubitSlot) -> Void {\n"
            "    let items: Array<QubitSlot> = [ClaimQubit()];\n"
            "    ArraySet(items, 0, q);\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticResult *result = semantic_analyze(program);

        EXPECT(!parser_has_error(parser));
        EXPECT(result != NULL && result->error_count > 0);
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "Borrowed ref slot handle (movable) 'q' cannot escape through array store"));

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("ref QubitSlot parameter cannot escape through set store");
    {
        const char *source =
            "func BorrowSet(ref q: QubitSlot) -> Void {\n"
            "    let seen: Set<QubitSlot> = SetNew();\n"
            "    SetAdd(seen, q);\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticResult *result = semantic_analyze(program);

        EXPECT(!parser_has_error(parser));
        EXPECT(result != NULL && result->error_count > 0);
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "Borrowed ref slot handle (movable) 'q' cannot escape through set store"));

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("ref QubitSlot parameter cannot escape through map store");
    {
        const char *source =
            "func BorrowMap(ref q: QubitSlot) -> Void {\n"
            "    let slots: HashMap<String, QubitSlot> = MapNew();\n"
            "    MapSet(slots, \"lead\", q);\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticResult *result = semantic_analyze(program);

        EXPECT(!parser_has_error(parser));
        EXPECT(result != NULL && result->error_count > 0);
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "Borrowed ref slot handle (movable) 'q' cannot escape through map store"));

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("ref QubitSlot parameter cannot escape through array push");
    {
        const char *source =
            "func BorrowArrayPush(ref q: QubitSlot) -> Void {\n"
            "    let seed: QubitSlot = ClaimQubit();\n"
            "    let items: Array<QubitSlot> = [seed];\n"
            "    ArrayPush(items, q);\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticResult *result = semantic_analyze(program);

        EXPECT(!parser_has_error(parser));
        EXPECT(result != NULL && result->error_count > 0);
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "Borrowed ref slot handle (movable) 'q' cannot escape through array store"));

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("ref QubitSlot parameter cannot escape through member assignment rebind");
    {
        const char *source =
            "struct Lab {\n"
            "    let current: QubitSlot;\n"
            "}\n"
            "func BorrowField(ref q: QubitSlot, lab: Lab) -> Void {\n"
            "    lab.current = q;\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticResult *result = semantic_analyze(program);

        EXPECT(!parser_has_error(parser));
        EXPECT(result != NULL && result->error_count > 0);
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "Borrowed ref slot handle (movable) 'q' cannot escape through member assignment rebind"));

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("ref array parameter cannot escape through destructure binding");
    {
        const char *source =
            "func BorrowDestructure(ref items: Array<QubitSlot>) -> Void {\n"
            "    let (first) = items;\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticResult *result = semantic_analyze(program);

        EXPECT(!parser_has_error(parser));
        EXPECT(result != NULL && result->error_count > 0);
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "Borrowed ref boundary value 'items' cannot escape into destructure target binding 'first'"));

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("own subject parameter is accepted as explicit transfer boundary");
    {
        const char *source =
            "subject Hero {\n"
            "    let hp: Int;\n"
            "}\n"
            "func Take(own hero: Hero) -> Void {\n"
            "    return;\n"
            "}\n"
            "func Main() -> Void {\n"
            "    let hero: Hero = Hero(10);\n"
            "    Take(hero);\n"
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

    TEST("ref class parameter is accepted as borrowed value boundary");
    {
        const char *source =
            "class Packet {\n"
            "    let size: Int;\n"
            "}\n"
            "func Inspect(ref packet: Packet) -> Void {\n"
            "    Log(packet.size);\n"
            "}\n"
            "func Main() -> Void {\n"
            "    let packet: Packet = Packet(1);\n"
            "    Inspect(packet);\n"
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
