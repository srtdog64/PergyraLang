    TEST("ref Int parameter may forward to own helper by copy");
    {
        const char *source =
            "func Consume(own value: Int) -> Void {\n"
            "}\n"
            "func Forward(ref value: Int) -> Void {\n"
            "    Consume(value);\n"
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

    TEST("ref tuple parameter is accepted as boundary value");
    {
        const char *source =
            "func Borrow(ref pair: (Int, Int)) -> Void {\n"
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

    TEST("ref tuple parameter rejects channel-send escape");
    {
        const char *source =
            "func Publish(ch: Channel<(Int, Int)>, ref pair: (Int, Int)) -> Void {\n"
            "    ch <- pair;\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticResult *result = semantic_analyze(program);

        EXPECT(!parser_has_error(parser));
        EXPECT(result != NULL && result->error_count > 0);
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "Borrowed ref boundary value 'pair' cannot escape through channel send"));

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("ref tuple parameter rejects new-binding escape");
    {
        const char *source =
            "func Alias(ref pair: (Int, Int)) -> Void {\n"
            "    let alias: (Int, Int) = pair;\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticResult *result = semantic_analyze(program);

        EXPECT(!parser_has_error(parser));
        EXPECT(result != NULL && result->error_count > 0);
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "Borrowed ref boundary value 'pair' cannot escape into new binding 'alias'"));

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("ref tuple parameter rejects assignment rebind escape");
    {
        const char *source =
            "func Rebind(ref pair: (Int, Int)) -> Void {\n"
            "    let dst: (Int, Int) = (0, 0);\n"
            "    dst = pair;\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticResult *result = semantic_analyze(program);

        EXPECT(!parser_has_error(parser));
        EXPECT(result != NULL && result->error_count > 0);
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "Borrowed ref boundary value 'pair' cannot escape through assignment rebind into 'dst'"));

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("ref tuple parameter rejects return escape");
    {
        const char *source =
            "func Echo(ref pair: (Int, Int)) -> (Int, Int) {\n"
            "    return pair;\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticResult *result = semantic_analyze(program);

        EXPECT(!parser_has_error(parser));
        EXPECT(result != NULL && result->error_count > 0);
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "Borrowed ref boundary value 'pair' cannot escape through return"));

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("ref tuple parameter rejects helper forwarding");
    {
        const char *source =
            "func Take(own pair: (Int, Int)) -> Void {\n"
            "    return;\n"
            "}\n"
            "func Forward(ref pair: (Int, Int)) -> Void {\n"
            "    Take(pair);\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticResult *result = semantic_analyze(program);

        EXPECT(!parser_has_error(parser));
        EXPECT(result != NULL && result->error_count > 0);
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "Borrowed ref boundary value 'pair' cannot escape through helper/function call"));

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("ref Array<Int> parameter rejects new-binding escape");
    {
        const char *source =
            "func Alias(ref items: Array<Int>) -> Void {\n"
            "    let copy: Array<Int> = items;\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticResult *result = semantic_analyze(program);

        EXPECT(!parser_has_error(parser));
        EXPECT(result != NULL && result->error_count > 0);
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "Borrowed ref boundary value 'items' cannot escape into new binding 'copy'"));

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("ref boundary value parameter reports member source path on new-binding escape");
    {
        const char *source =
            "object Packet {\n"
            "    let items: Array<Int>;\n"
            "}\n"
            "func Alias(ref packet: Packet) -> Void {\n"
            "    let copy: Array<Int> = packet.items;\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticResult *result = semantic_analyze(program);

        EXPECT(!parser_has_error(parser));
        EXPECT(result != NULL && result->error_count > 0);
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "Borrowed ref boundary value 'packet' cannot escape into new binding 'copy' from 'packet.items'"));

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("ref boundary value parameter reports array source path on new-binding escape");
    {
        const char *source =
            "func Alias(ref items: Array<Array<Int>>) -> Void {\n"
            "    let copy: Array<Int> = items[0];\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticResult *result = semantic_analyze(program);

        EXPECT(!parser_has_error(parser));
        EXPECT(result != NULL && result->error_count > 0);
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "Borrowed ref boundary value 'items' cannot escape into new binding 'copy' from 'items[0]'"));

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("ref Array<Int> parameter rejects assignment rebind escape");
    {
        const char *source =
            "func Rebind(ref items: Array<Int>) -> Void {\n"
            "    let dst: Array<Int> = [0];\n"
            "    dst = items;\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticResult *result = semantic_analyze(program);

        EXPECT(!parser_has_error(parser));
        EXPECT(result != NULL && result->error_count > 0);
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "Borrowed ref boundary value 'items' cannot escape through assignment rebind into 'dst'"));

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("ref Array<Int> parameter rejects helper forwarding");
    {
        const char *source =
            "func Take(own items: Array<Int>) -> Void {\n"
            "    return;\n"
            "}\n"
            "func Forward(ref items: Array<Int>) -> Void {\n"
            "    Take(items);\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticResult *result = semantic_analyze(program);

        EXPECT(!parser_has_error(parser));
        EXPECT(result != NULL && result->error_count > 0);
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "Borrowed ref boundary value 'items' cannot escape through helper/function call"));

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("ref boundary value parameter reports member source path on helper forwarding");
    {
        const char *source =
            "object Packet {\n"
            "    let items: Array<Int>;\n"
            "}\n"
            "func Take(own items: Array<Int>) -> Void {\n"
            "    return;\n"
            "}\n"
            "func Forward(ref packet: Packet) -> Void {\n"
            "    Take(packet.items);\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticResult *result = semantic_analyze(program);

        EXPECT(!parser_has_error(parser));
        EXPECT(result != NULL && result->error_count > 0);
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "Borrowed ref boundary value 'packet' cannot escape through helper/function call to 'Take' from 'packet.items'"));
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "'packet.items' is derived from that borrowed boundary provenance"));

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }
    TEST("ref tuple parameter rejects queue store escape");
    {
        const char *source =
            "func Store(ref pair: (Int, Int)) -> Void {\n"
            "    let items: Queue<(Int, Int)> = QueueNew();\n"
            "    QueuePush(items, pair);\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticResult *result = semantic_analyze(program);

        EXPECT(!parser_has_error(parser));
        EXPECT(result != NULL && result->error_count > 0);
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "Borrowed ref boundary value 'pair' cannot escape through queue store"));

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }


    TEST("ref boundary value parameter reports member source path on TrySend");
    {
        const char *source =
            "object Packet {\n"
            "    let hp: Int;\n"
            "}\n"
            "object Holder {\n"
            "    let packet: Packet;\n"
            "}\n"
            "func Forward(ref holder: Holder, ch: Channel<Packet>) -> Void {\n"
            "    TrySend(ch, holder.packet);\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticResult *result = semantic_analyze(program);

        EXPECT(!parser_has_error(parser));
        EXPECT(result != NULL && result->error_count > 0);
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "Borrowed ref boundary value 'holder' cannot escape through channel send from 'holder.packet'"));
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "'holder.packet' is derived from that borrowed boundary provenance"));

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("ref enum parameter may be stored into list by copy");
    {
        const char *source =
            "enum Mode { Idle, Active }\n"
            "func Snapshot(ref mode: Mode) -> Void {\n"
            "    let states: List<Mode> = ListNew();\n"
            "    ListPush(states, mode);\n"
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

    TEST("ref enum parameter may be sent through channel by copy");
    {
        const char *source =
            "enum Mode { Idle, Active }\n"
            "func Publish(ch: Channel<Mode>, ref mode: Mode) -> Void {\n"
            "    ch <- mode;\n"
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

}
