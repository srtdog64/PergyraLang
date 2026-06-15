    TEST("ref class parameter cannot escape into new binding");
    {
        const char *source =
            "class Packet {\n"
            "    let size: Int;\n"
            "}\n"
            "func BorrowAlias(ref packet: Packet) -> Void {\n"
            "    let alias: Packet = packet;\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticResult *result = semantic_analyze(program);

        EXPECT(!parser_has_error(parser));
        EXPECT(result != NULL && result->error_count > 0);
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "Borrowed ref boundary value 'packet' cannot escape into new binding 'alias'"));

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("ref class parameter cannot escape through helper/function call");
    {
        const char *source =
            "class Packet {\n"
            "    let size: Int;\n"
            "}\n"
            "func UseOwned(own packet: Packet) -> Void {\n"
            "    return;\n"
            "}\n"
            "func BorrowForward(ref packet: Packet) -> Void {\n"
            "    UseOwned(packet);\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticResult *result = semantic_analyze(program);

        EXPECT(!parser_has_error(parser));
        EXPECT(result != NULL && result->error_count > 0);
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "Borrowed ref boundary value 'packet' cannot escape through helper/function call"));

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("ref class parameter cannot escape through return");
    {
        const char *source =
            "class Packet {\n"
            "    let size: Int;\n"
            "}\n"
            "func Echo(ref packet: Packet) -> Packet {\n"
            "    return packet;\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticResult *result = semantic_analyze(program);

        EXPECT(!parser_has_error(parser));
        EXPECT(result != NULL && result->error_count > 0);
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "Borrowed ref boundary value 'packet' cannot escape through return"));

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("ref class parameter cannot escape through channel send");
    {
        const char *source =
            "class Packet {\n"
            "    let size: Int;\n"
            "}\n"
            "func Publish(ch: Channel<Packet>, ref packet: Packet) -> Void {\n"
            "    ch <- packet;\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticResult *result = semantic_analyze(program);

        EXPECT(!parser_has_error(parser));
        EXPECT(result != NULL && result->error_count > 0);
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "Borrowed ref boundary value 'packet' cannot escape through channel send"));

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("ref class parameter cannot escape through list store");
    {
        const char *source =
            "class Packet {\n"
            "    let size: Int;\n"
            "}\n"
            "func BorrowList(ref packet: Packet) -> Void {\n"
            "    let items: List<Packet> = ListNew();\n"
            "    ListPush(items, packet);\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticResult *result = semantic_analyze(program);

        EXPECT(!parser_has_error(parser));
        EXPECT(result != NULL && result->error_count > 0);
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "Borrowed ref boundary value 'packet' cannot escape through list store"));

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("ref class parameter cannot escape through set store");
    {
        const char *source =
            "class Packet {\n"
            "    let size: Int;\n"
            "}\n"
            "func BorrowSet(ref packet: Packet) -> Void {\n"
            "    let items: Set<Packet> = SetNew();\n"
            "    SetAdd(items, packet);\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticResult *result = semantic_analyze(program);

        EXPECT(!parser_has_error(parser));
        EXPECT(result != NULL && result->error_count > 0);
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "Borrowed ref boundary value 'packet' cannot escape through set store"));

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("ref class parameter cannot escape through queue store");
    {
        const char *source =
            "class Packet {\n"
            "    let size: Int;\n"
            "}\n"
            "func BorrowQueue(ref packet: Packet) -> Void {\n"
            "    let items: Queue<Packet> = QueueNew();\n"
            "    QueuePush(items, packet);\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticResult *result = semantic_analyze(program);

        EXPECT(!parser_has_error(parser));
        EXPECT(result != NULL && result->error_count > 0);
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "Borrowed ref boundary value 'packet' cannot escape through queue store"));

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("ref class parameter cannot escape through map store");
    {
        const char *source =
            "class Packet {\n"
            "    let size: Int;\n"
            "}\n"
            "func BorrowMap(ref packet: Packet) -> Void {\n"
            "    let items: HashMap<String, Packet> = MapNew();\n"
            "    MapSet(items, \"lead\", packet);\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticResult *result = semantic_analyze(program);

        EXPECT(!parser_has_error(parser));
        EXPECT(result != NULL && result->error_count > 0);
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "Borrowed ref boundary value 'packet' cannot escape through map store"));

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("ref class parameter cannot escape through array push");
    {
        const char *source =
            "class Packet {\n"
            "    let size: Int;\n"
            "}\n"
            "func BorrowArrayPush(ref packet: Packet) -> Void {\n"
            "    let items: Array<Packet> = [Packet(1)];\n"
            "    ArrayPush(items, packet);\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticResult *result = semantic_analyze(program);

        EXPECT(!parser_has_error(parser));
        EXPECT(result != NULL && result->error_count > 0);
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "Borrowed ref boundary value 'packet' cannot escape through array store"));

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("ref class parameter cannot escape through array overwrite");
    {
        const char *source =
            "class Packet {\n"
            "    let size: Int;\n"
            "}\n"
            "func BorrowArraySet(ref packet: Packet) -> Void {\n"
            "    let items: Array<Packet> = [Packet(1)];\n"
            "    ArraySet(items, 0, packet);\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticResult *result = semantic_analyze(program);

        EXPECT(!parser_has_error(parser));
        EXPECT(result != NULL && result->error_count > 0);
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "Borrowed ref boundary value 'packet' cannot escape through array store"));

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("ref class parameter cannot escape through member assignment rebind");
    {
        const char *source =
            "class Packet {\n"
            "    let size: Int;\n"
            "}\n"
            "struct Envelope {\n"
            "    let packet: Packet;\n"
            "}\n"
            "func BorrowField(ref packet: Packet, env: Envelope) -> Void {\n"
            "    env.packet = packet;\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticResult *result = semantic_analyze(program);

        EXPECT(!parser_has_error(parser));
        EXPECT(result != NULL && result->error_count > 0);
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "Borrowed ref boundary value 'packet' cannot escape through member assignment rebind"));

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("ref class parameter cannot escape through constructor field store");
    {
        const char *source =
            "class Packet {\n"
            "    let size: Int;\n"
            "}\n"
            "class Envelope {\n"
            "    let packet: Packet;\n"
            "}\n"
            "func BorrowCtor(ref packet: Packet) -> Void {\n"
            "    let env: Envelope = Envelope(packet);\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticResult *result = semantic_analyze(program);

        EXPECT(!parser_has_error(parser));
        EXPECT(result != NULL && result->error_count > 0);
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "Borrowed ref boundary value 'packet' cannot escape through constructor field store"));

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("ref class parameter cannot escape transitively through ref helper return");
    {
        const char *source =
            "class Packet {\n"
            "    let size: Int;\n"
            "}\n"
            "func EchoBorrow(ref packet: Packet) -> Packet {\n"
            "    return packet;\n"
            "}\n"
            "func BorrowForward(ref packet: Packet) -> Packet {\n"
            "    return EchoBorrow(packet);\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticResult *result = semantic_analyze(program);

        EXPECT(!parser_has_error(parser));
        EXPECT(result != NULL && result->error_count > 0);
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "Borrowed ref boundary value 'packet' cannot escape through helper/function call"));

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("ref class parameter cannot escape through transitive helper chain");
    {
        const char *source =
            "class Packet {\n"
            "    let size: Int;\n"
            "}\n"
            "func UseOwned(own packet: Packet) -> Void {\n"
            "    return;\n"
            "}\n"
            "func Forward(own packet: Packet) -> Void {\n"
            "    UseOwned(packet);\n"
            "}\n"
            "func BorrowForward(ref packet: Packet) -> Void {\n"
            "    Forward(packet);\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticResult *result = semantic_analyze(program);

        EXPECT(!parser_has_error(parser));
        EXPECT(result != NULL && result->error_count > 0);
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "Borrowed ref boundary value 'packet' cannot escape through helper/function call to 'Forward'"));
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "forwarding it to 'Forward' as 'own' would create a transitive helper transfer"));

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("own class parameter consumes the caller binding");
    {
        const char *source =
            "class Packet {\n"
            "    let size: Int;\n"
            "}\n"
            "func Take(own packet: Packet) -> Void {\n"
            "    return;\n"
            "}\n"
            "func Main() -> Void {\n"
            "    let packet: Packet = Packet(1);\n"
            "    Take(packet);\n"
            "    Log(packet.size);\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticResult *result = semantic_analyze(program);

        EXPECT(!parser_has_error(parser));
        EXPECT(result != NULL && result->error_count > 0);
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "moved"));

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("ref subject parameter cannot escape through return");
    {
        const char *source =
            "subject Hero {\n"
            "    let hp: Int;\n"
            "}\n"
            "func BorrowReturn(ref hero: Hero) -> Hero {\n"
            "    return hero;\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticResult *result = semantic_analyze(program);

        EXPECT(!parser_has_error(parser));
        EXPECT(result != NULL && result->error_count > 0);
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "Borrowed ref subject 'hero' cannot escape through return"));

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("ref subject parameter cannot escape through channel send");
    {
        const char *source =
            "subject Hero {\n"
            "    let hp: Int;\n"
            "}\n"
            "func BorrowSend(ref hero: Hero) -> Void {\n"
            "    let ch: Channel<Hero> = ChannelNew(4);\n"
            "    ch <- hero;\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticResult *result = semantic_analyze(program);

        EXPECT(!parser_has_error(parser));
        EXPECT(result != NULL && result->error_count > 0);
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "Borrowed ref subject 'hero' cannot escape through channel send"));

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }
}
