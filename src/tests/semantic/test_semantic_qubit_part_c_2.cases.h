    TEST("ref movable-resource parameter rejects assignment rebind escape");
    {
        const char *source =
            "func Store(ref items: Array<QubitSlot>) -> Void {\n"
            "    let dst: QubitSlot = ClaimQubit();\n"
            "    dst = items[0];\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticResult *result = semantic_analyze(program);

        EXPECT(!parser_has_error(parser));
        EXPECT(result != NULL && result->error_count > 0);
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "Borrowed ref slot handle (movable) 'items' cannot escape through assignment rebind into 'dst' from 'items[0]'"));
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "'items[0]' is derived from that borrowed slot-handle (movable) provenance"));

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("ref movable-resource parameter rejects QueuePush escape with array provenance");
    {
        const char *source =
            "func Store(ref items: Array<QubitSlot>) -> Void {\n"
            "    let queue: Queue<QubitSlot> = QueueNew();\n"
            "    QueuePush(queue, items[0]);\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticResult *result = semantic_analyze(program);

        EXPECT(!parser_has_error(parser));
        EXPECT(result != NULL && result->error_count > 0);
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "Borrowed ref slot handle (movable) 'items' cannot escape through queue store from 'items[0]'"));
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "'items[0]' is derived from that borrowed slot-handle (movable) provenance"));

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("ref movable-resource parameter rejects MapSet escape with array provenance");
    {
        const char *source =
            "func Store(ref items: Array<QubitSlot>) -> Void {\n"
            "    let table: HashMap<String, QubitSlot> = MapNew();\n"
            "    MapSet(table, \"slot\", items[0]);\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticResult *result = semantic_analyze(program);

        EXPECT(!parser_has_error(parser));
        EXPECT(result != NULL && result->error_count > 0);
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "Borrowed ref slot handle (movable) 'items' cannot escape through map store from 'items[0]'"));
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "'items[0]' is derived from that borrowed slot-handle (movable) provenance"));

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("ref movable-resource parameter rejects ListPush escape with array provenance");
    {
        const char *source =
            "func Store(ref items: Array<QubitSlot>) -> Void {\n"
            "    let list: List<QubitSlot> = ListNew();\n"
            "    ListPush(list, items[0]);\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticResult *result = semantic_analyze(program);

        EXPECT(!parser_has_error(parser));
        EXPECT(result != NULL && result->error_count > 0);
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "Borrowed ref slot handle (movable) 'items' cannot escape through list store from 'items[0]'"));
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "'items[0]' is derived from that borrowed slot-handle (movable) provenance"));

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("ref boundary value parameter reports member source path on constructor field store");
    {
        const char *source =
            "object Packet {\n"
            "    let hp: Int;\n"
            "}\n"
            "object HolderView {\n"
            "    let packet: Packet;\n"
            "}\n"
            "class Envelope {\n"
            "    let packet: Packet;\n"
            "}\n"
            "func Store(ref holder: HolderView) -> Void {\n"
            "    let env = Envelope(holder.packet);\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticResult *result = semantic_analyze(program);

        EXPECT(!parser_has_error(parser));
        EXPECT(result != NULL && result->error_count > 0);
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "cannot escape through constructor field store 'Envelope.packet' from 'holder.packet'"));
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "'holder.packet' is derived from that borrowed boundary provenance"));

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("ref boundary value parameter reports array source path on constructor field store");
    {
        const char *source =
            "object Packet {\n"
            "    let hp: Int;\n"
            "}\n"
            "class Envelope {\n"
            "    let packet: Packet;\n"
            "}\n"
            "func Store(ref items: Array<Packet>) -> Void {\n"
            "    let env = Envelope(items[0]);\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticResult *result = semantic_analyze(program);

        EXPECT(!parser_has_error(parser));
        EXPECT(result != NULL && result->error_count > 0);
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "cannot escape through constructor field store 'Envelope.packet' from 'items[0]'"));
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "'items[0]' is derived from that borrowed boundary provenance"));

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("ref movable-resource parameter reports array source path on helper forwarding");
    {
        const char *source =
            "func Consume(own q: QubitSlot) -> Void {\n"
            "    ReleaseQubit(q);\n"
            "}\n"
            "func Store(ref items: Array<QubitSlot>) -> Void {\n"
            "    Consume(items[0]);\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticResult *result = semantic_analyze(program);

        EXPECT(!parser_has_error(parser));
        EXPECT(result != NULL && result->error_count > 0);
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "Borrowed ref slot handle (movable) 'items' cannot escape through helper/function call to 'Consume' from 'items[0]'"));
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "'items[0]' is derived from that borrowed slot-handle (movable) provenance"));

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("ref boundary value parameter reports nested projection path on queue store escape");
    {
        const char *source =
            "object Packet {\n"
            "    let hp: Int;\n"
            "}\n"
            "object Holder {\n"
            "    let packet: Packet;\n"
            "}\n"
            "object Wrapper {\n"
            "    let holder: Holder;\n"
            "}\n"
            "func Store(ref wrapper: Wrapper) -> Void {\n"
            "    let items: Queue<Packet> = QueueNew();\n"
            "    QueuePush(items, wrapper.holder.packet);\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticResult *result = semantic_analyze(program);

        EXPECT(!parser_has_error(parser));
        EXPECT(result != NULL && result->error_count > 0);
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "Borrowed ref boundary value 'wrapper' cannot escape through queue store from 'wrapper.holder.packet'"));
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "'wrapper.holder.packet' is derived from that borrowed boundary provenance"));

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("ref movable-resource parameter reports array source path on transitive helper forwarding");
    {
        const char *source =
            "func Escape(ref q: QubitSlot) -> QubitSlot {\n"
            "    return q;\n"
            "}\n"
            "func Proxy(ref q: QubitSlot) -> QubitSlot {\n"
            "    return Escape(q);\n"
            "}\n"
            "func Store(ref items: Array<QubitSlot>) -> QubitSlot {\n"
            "    return Proxy(items[0]);\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticResult *result = semantic_analyze(program);

        EXPECT(!parser_has_error(parser));
        EXPECT(result != NULL && result->error_count > 0);
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "Borrowed ref slot handle (movable) 'items' cannot escape through helper/function call to 'Proxy' from 'items[0]'"));
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "'items[0]' is derived from that borrowed slot-handle (movable) provenance"));

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }
    TEST("ref boundary value parameter reports nested projection path on transitive helper forwarding");
    {
        const char *source =
            "object Packet {\n"
            "    let hp: Int;\n"
            "}\n"
            "object Holder {\n"
            "    let packet: Packet;\n"
            "}\n"
            "object Wrapper {\n"
            "    let holder: Holder;\n"
            "}\n"
            "func Escape(ref packet: Packet) -> Packet {\n"
            "    return packet;\n"
            "}\n"
            "func Proxy(ref packet: Packet) -> Packet {\n"
            "    return Escape(packet);\n"
            "}\n"
            "func Store(ref wrapper: Wrapper) -> Packet {\n"
            "    return Proxy(wrapper.holder.packet);\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticResult *result = semantic_analyze(program);

        EXPECT(!parser_has_error(parser));
        EXPECT(result != NULL && result->error_count > 0);
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "Borrowed ref boundary value 'wrapper' cannot escape through helper/function call to 'Proxy' from 'wrapper.holder.packet'"));
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "'wrapper.holder.packet' is derived from that borrowed boundary provenance"));

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }


    TEST("ref movable-resource parameter reports array source path on constructor field store");
    {
        const char *source =
            "class Capsule {\n"
            "    let q: QubitSlot;\n"
            "}\n"
            "func Store(ref items: Array<QubitSlot>) -> Void {\n"
            "    let c = Capsule(items[0]);\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticResult *result = semantic_analyze(program);

        EXPECT(!parser_has_error(parser));
        EXPECT(result != NULL && result->error_count > 0);
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "Borrowed ref slot handle (movable) 'items' cannot escape through constructor field store 'Capsule.q' from 'items[0]'"));
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "'items[0]' is derived from that borrowed slot-handle (movable) provenance"));

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("ref boundary value parameter may pass copied snapshot into constructor field");
    {
        const char *source =
            "object Packet {\n"
            "    let hp: Int;\n"
            "}\n"
            "class Holder {\n"
            "    let packet: Packet;\n"
            "}\n"
            "func Store(ref packet: Packet) -> Void {\n"
            "    let holder = Holder(Packet(packet.hp));\n"
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

    TEST("ref boundary value parameter rejects array literal store escape");
    {
        const char *source =
            "object Packet {\n"
            "    let hp: Int;\n"
            "}\n"
            "func Store(ref packet: Packet) -> Void {\n"
            "    let items: Array<Packet> = [packet];\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticResult *result = semantic_analyze(program);

        EXPECT(!parser_has_error(parser));
        EXPECT(result != NULL && result->error_count > 0);
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "cannot escape through array literal store"));

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

}
