    TEST("ref boundary aggregate parameter reports nested projection path on list store");
    {
        const char *source =
            "class Packet {\n"
            "    let size: Int;\n"
            "}\n"
            "class Wrapper {\n"
            "    let packet: Packet;\n"
            "}\n"
            "class Cargo {\n"
            "    let wrapper: Wrapper;\n"
            "}\n"
            "func BorrowList(ref cargo: Cargo) -> Void {\n"
            "    let items: List<Packet> = ListNew();\n"
            "    ListPush(items, cargo.wrapper.packet);\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticResult *result = semantic_analyze(program);

        EXPECT(!parser_has_error(parser));
        EXPECT(result != NULL && result->error_count > 0);
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "Borrowed ref boundary value 'cargo' cannot escape through list store"));
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "from 'cargo.wrapper.packet'"));

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("ref boundary aggregate parameter reports nested projection path on queue store");
    {
        const char *source =
            "class Packet {\n"
            "    let size: Int;\n"
            "}\n"
            "class Wrapper {\n"
            "    let packet: Packet;\n"
            "}\n"
            "class Cargo {\n"
            "    let wrapper: Wrapper;\n"
            "}\n"
            "func BorrowQueue(ref cargo: Cargo) -> Void {\n"
            "    let items: Queue<Packet> = QueueNew();\n"
            "    QueuePush(items, cargo.wrapper.packet);\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticResult *result = semantic_analyze(program);

        EXPECT(!parser_has_error(parser));
        EXPECT(result != NULL && result->error_count > 0);
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "Borrowed ref boundary value 'cargo' cannot escape through queue store"));
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "from 'cargo.wrapper.packet'"));

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("ref boundary aggregate parameter reports nested projection path on map store");
    {
        const char *source =
            "class Packet {\n"
            "    let size: Int;\n"
            "}\n"
            "class Wrapper {\n"
            "    let packet: Packet;\n"
            "}\n"
            "class Cargo {\n"
            "    let wrapper: Wrapper;\n"
            "}\n"
            "func BorrowMap(ref cargo: Cargo) -> Void {\n"
            "    let items: HashMap<String, Packet> = MapNew();\n"
            "    MapSet(items, \"lead\", cargo.wrapper.packet);\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticResult *result = semantic_analyze(program);

        EXPECT(!parser_has_error(parser));
        EXPECT(result != NULL && result->error_count > 0);
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "Borrowed ref boundary value 'cargo' cannot escape through map store"));
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "from 'cargo.wrapper.packet'"));

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("ref boundary aggregate parameter reports nested projection path on array overwrite");
    {
        const char *source =
            "class Packet {\n"
            "    let size: Int;\n"
            "}\n"
            "class Wrapper {\n"
            "    let packet: Packet;\n"
            "}\n"
            "class Cargo {\n"
            "    let wrapper: Wrapper;\n"
            "}\n"
            "func BorrowArraySet(ref cargo: Cargo) -> Void {\n"
            "    let items: Array<Packet> = [Packet(1)];\n"
            "    ArraySet(items, 0, cargo.wrapper.packet);\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticResult *result = semantic_analyze(program);

        EXPECT(!parser_has_error(parser));
        EXPECT(result != NULL && result->error_count > 0);
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "Borrowed ref boundary value 'cargo' cannot escape through array store"));
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "from 'cargo.wrapper.packet'"));

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("ref boundary aggregate parameter reports nested projection path on set store");
    {
        const char *source =
            "class Packet {\n"
            "    let size: Int;\n"
            "}\n"
            "class Wrapper {\n"
            "    let packet: Packet;\n"
            "}\n"
            "class Cargo {\n"
            "    let wrapper: Wrapper;\n"
            "}\n"
            "func BorrowSet(ref cargo: Cargo) -> Void {\n"
            "    let items: Set<Packet> = SetNew();\n"
            "    SetAdd(items, cargo.wrapper.packet);\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticResult *result = semantic_analyze(program);

        EXPECT(!parser_has_error(parser));
        EXPECT(result != NULL && result->error_count > 0);
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "Borrowed ref boundary value 'cargo' cannot escape through set store"));
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "from 'cargo.wrapper.packet'"));

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("ref boundary aggregate parameter reports nested projection path on array push");
    {
        const char *source =
            "class Packet {\n"
            "    let size: Int;\n"
            "}\n"
            "class Wrapper {\n"
            "    let packet: Packet;\n"
            "}\n"
            "class Cargo {\n"
            "    let wrapper: Wrapper;\n"
            "}\n"
            "func BorrowArrayPush(ref cargo: Cargo) -> Void {\n"
            "    let items: Array<Packet> = [Packet(1)];\n"
            "    ArrayPush(items, cargo.wrapper.packet);\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticResult *result = semantic_analyze(program);

        EXPECT(!parser_has_error(parser));
        EXPECT(result != NULL && result->error_count > 0);
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "Borrowed ref boundary value 'cargo' cannot escape through array store"));
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "from 'cargo.wrapper.packet'"));

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("ref boundary aggregate parameter reports nested projection path on helper return summary");
    {
        const char *source =
            "class Packet {\n"
            "    let size: Int;\n"
            "}\n"
            "class Wrapper {\n"
            "    let packet: Packet;\n"
            "}\n"
            "class Cargo {\n"
            "    let wrapper: Wrapper;\n"
            "}\n"
            "func ReturnPacket(ref packet: Packet) -> Packet {\n"
            "    return packet;\n"
            "}\n"
            "func BorrowReturn(ref cargo: Cargo) -> Void {\n"
            "    let packet = ReturnPacket(cargo.wrapper.packet);\n"
            "    Log(packet.size);\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticResult *result = semantic_analyze(program);

        EXPECT(!parser_has_error(parser));
        EXPECT(result != NULL && result->error_count > 0);
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "Borrowed ref boundary value 'cargo' cannot escape through helper/function call to 'ReturnPacket'"));
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "from 'cargo.wrapper.packet'"));

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("ref boundary aggregate parameter reports nested projection path on direct return");
    {
        const char *source =
            "class Packet {\n"
            "    let size: Int;\n"
            "}\n"
            "class Wrapper {\n"
            "    let packet: Packet;\n"
            "}\n"
            "class Cargo {\n"
            "    let wrapper: Wrapper;\n"
            "}\n"
            "func BorrowDirectReturn(ref cargo: Cargo) -> Packet {\n"
            "    return cargo.wrapper.packet;\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticResult *result = semantic_analyze(program);

        EXPECT(!parser_has_error(parser));
        EXPECT(result != NULL && result->error_count > 0);
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "Borrowed ref boundary value 'cargo' cannot escape through return"));
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "from 'cargo.wrapper.packet'"));

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("ref boundary aggregate parameter reports nested projection path on channel send");
    {
        const char *source =
            "class Packet {\n"
            "    let size: Int;\n"
            "}\n"
            "class Wrapper {\n"
            "    let packet: Packet;\n"
            "}\n"
            "class Cargo {\n"
            "    let wrapper: Wrapper;\n"
            "}\n"
            "func BorrowSend(ref cargo: Cargo) -> Void {\n"
            "    let ch: Channel<Packet> = ChannelNew(4);\n"
            "    ch <- cargo.wrapper.packet;\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticResult *result = semantic_analyze(program);

        EXPECT(!parser_has_error(parser));
        EXPECT(result != NULL && result->error_count > 0);
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "Borrowed ref boundary value 'cargo' cannot escape through channel send"));
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "from 'cargo.wrapper.packet'"));

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("ref boundary aggregate parameter reports nested projection path on transitive helper chain");
    {
        const char *source =
            "class Packet {\n"
            "    let size: Int;\n"
            "}\n"
            "class Wrapper {\n"
            "    let packet: Packet;\n"
            "}\n"
            "class Cargo {\n"
            "    let wrapper: Wrapper;\n"
            "}\n"
            "class Envelope {\n"
            "    let packet: Packet;\n"
            "}\n"
            "func Rebind(env: Envelope, own packet: Packet) -> Void {\n"
            "    env.packet = packet;\n"
            "}\n"
            "func Forward(env: Envelope, own packet: Packet) -> Void {\n"
            "    Rebind(env, packet);\n"
            "}\n"
            "func BorrowForward(ref cargo: Cargo, env: Envelope) -> Void {\n"
            "    Forward(env, cargo.wrapper.packet);\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticResult *result = semantic_analyze(program);

        EXPECT(!parser_has_error(parser));
        EXPECT(result != NULL && result->error_count > 0);
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "Borrowed ref boundary value 'cargo' cannot escape through helper/function call to 'Forward'"));
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "from 'cargo.wrapper.packet'"));

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("ref boundary aggregate parameter reports nested projection path on destructure binding");
    {
        const char *source =
            "class Packet {\n"
            "    let size: Int;\n"
            "}\n"
            "class Wrapper {\n"
            "    let items: Array<Packet>;\n"
            "}\n"
            "class Cargo {\n"
            "    let wrapper: Wrapper;\n"
            "}\n"
            "func BorrowDestructure(ref cargo: Cargo) -> Void {\n"
            "    let (first) = cargo.wrapper.items;\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticResult *result = semantic_analyze(program);

        EXPECT(!parser_has_error(parser));
        EXPECT(result != NULL && result->error_count > 0);
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "Borrowed ref boundary value 'cargo' cannot escape into destructure target binding 'first'"));
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "from 'cargo.wrapper.items'"));

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("ref tuple parameter cannot escape through transitive helper chain");
    {
        const char *source =
            "func Consume(own pair: (Int, Int)) -> Void {\n"
            "    return;\n"
            "}\n"
            "func Forward(own pair: (Int, Int)) -> Void {\n"
            "    Consume(pair);\n"
            "}\n"
            "func BorrowForward(ref pair: (Int, Int)) -> Void {\n"
            "    Forward(pair);\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticResult *result = semantic_analyze(program);

        EXPECT(!parser_has_error(parser));
        EXPECT(result != NULL && result->error_count > 0);
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "Borrowed ref boundary value 'pair' cannot escape through helper/function call to 'Forward'"));
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "forwarding it to 'Forward' as 'own' would create a transitive helper transfer"));

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }
}
