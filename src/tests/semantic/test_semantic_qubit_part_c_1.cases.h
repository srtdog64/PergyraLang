static void
test_qubit_slot_semantics_part_c(void)
{
    TEST("own DeviceSlot<Int> parameter is accepted as explicit anchored-handle transfer boundary");
    {
        const char *source =
            "func Submit(own device: DeviceSlot<Int>) -> Void {\n"
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

    TEST("Slot<subject> parameter without own/ref requires explicit anchored-handle qualifier");
    {
        const char *source =
            "subject Vec2 {\n"
            "    let x: Int;\n"
            "    let y: Int;\n"
            "}\n"
            "func Read(s: Slot<Vec2>) -> Void {\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticResult *result = semantic_analyze(program);

        EXPECT(!parser_has_error(parser));
        EXPECT(result != NULL && result->error_count > 0);
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "Slot handle (anchored) parameters require explicit 'own' or 'ref'"));

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("ref Slot<subject> parameter rejects aliasing into new binding");
    {
        const char *source =
            "subject Vec2 {\n"
            "    let x: Int;\n"
            "    let y: Int;\n"
            "}\n"
            "func Alias(ref s: Slot<Vec2>) -> Void {\n"
            "    let alias: Slot<Vec2> = s;\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticResult *result = semantic_analyze(program);

        EXPECT(!parser_has_error(parser));
        EXPECT(result != NULL && result->error_count > 0);
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "cannot be copied into a new binding"));

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("ref Slot<subject> parameter rejects rebinding with assignment");
    {
        const char *source =
            "subject Vec2 {\n"
            "    let x: Int;\n"
            "    let y: Int;\n"
            "}\n"
            "func Rebind(ref s: Slot<Vec2>) -> Void {\n"
            "    let dst: Slot<Vec2> = Vec2(0, 0);\n"
            "    dst = s;\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticResult *result = semantic_analyze(program);

        EXPECT(!parser_has_error(parser));
        EXPECT(result != NULL && result->error_count > 0);
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "Slot-handle (anchored) assignment is not allowed"));

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("ref Slot<subject> parameter rejects Move helper forwarding");
    {
        const char *source =
            "subject Vec2 {\n"
            "    let x: Int;\n"
            "    let y: Int;\n"
            "}\n"
            "func BorrowThenMove(ref s: Slot<Vec2>) -> Void {\n"
            "    let moved: Slot<Vec2> = Move(s);\n"
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

    TEST("ref boundary value parameter rejects ArrayPush escape");
    {
        const char *source =
            "object Packet {\n"
            "    let hp: Int;\n"
            "}\n"
            "func Store(ref packet: Packet) -> Void {\n"
            "    let items: Array<Packet> = [Packet(0)];\n"
            "    ArrayPush(items, packet);\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticResult *result = semantic_analyze(program);

        EXPECT(!parser_has_error(parser));
        EXPECT(result != NULL && result->error_count > 0);
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "cannot escape through array store"));

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("ref boundary value parameter rejects MapSet escape");
    {
        const char *source =
            "object Packet {\n"
            "    let hp: Int;\n"
            "}\n"
            "func Store(ref packet: Packet) -> Void {\n"
            "    let table: HashMap<String, Packet> = MapNew();\n"
            "    MapSet(table, \"hp\", packet);\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticResult *result = semantic_analyze(program);

        EXPECT(!parser_has_error(parser));
        EXPECT(result != NULL && result->error_count > 0);
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "cannot escape through map store"));

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

    TEST("ref boundary value parameter reports member source path on QueuePush escape");
    {
        const char *source =
            "object Packet {\n"
            "    let hp: Int;\n"
            "}\n"
            "object Holder {\n"
            "    let packet: Packet;\n"
            "}\n"
            "func Store(ref holder: Holder) -> Void {\n"
            "    let items: Queue<Packet> = QueueNew();\n"
            "    QueuePush(items, holder.packet);\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticResult *result = semantic_analyze(program);

        EXPECT(!parser_has_error(parser));
        EXPECT(result != NULL && result->error_count > 0);
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "Borrowed ref boundary value 'holder' cannot escape through queue store from 'holder.packet'"));

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("ref boundary value parameter reports array source path on MapSet escape");
    {
        const char *source =
            "object Packet {\n"
            "    let hp: Int;\n"
            "}\n"
            "func Store(ref packets: Array<Packet>) -> Void {\n"
            "    let table: HashMap<String, Packet> = MapNew();\n"
            "    MapSet(table, \"hp\", packets[0]);\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticResult *result = semantic_analyze(program);

        EXPECT(!parser_has_error(parser));
        EXPECT(result != NULL && result->error_count > 0);
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "Borrowed ref boundary value 'packets' cannot escape through map store from 'packets[0]'"));

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("ref boundary value parameter rejects QueuePush escape");
    {
        const char *source =
            "object Packet {\n"
            "    let hp: Int;\n"
            "}\n"
            "func Store(ref packet: Packet) -> Void {\n"
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
            "cannot escape through queue store"));

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("ref boundary value parameter may pass copied snapshot into collection");
    {
        const char *source =
            "object Packet {\n"
            "    let hp: Int;\n"
            "}\n"
            "func Store(ref packet: Packet) -> Void {\n"
            "    let items: Array<Packet> = [Packet(0)];\n"
            "    ArrayPush(items, Packet(packet.hp));\n"
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

    TEST("ref boundary value parameter rejects constructor field store escape");
    {
        const char *source =
            "object Packet {\n"
            "    let hp: Int;\n"
            "}\n"
            "class Holder {\n"
            "    let packet: Packet;\n"
            "}\n"
            "func Store(ref packet: Packet) -> Void {\n"
            "    let holder = Holder(packet);\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticResult *result = semantic_analyze(program);

        EXPECT(!parser_has_error(parser));
        EXPECT(result != NULL && result->error_count > 0);
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "cannot escape through constructor field store"));

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


    TEST("ref movable-resource parameter reports array source path on new-binding escape");
    {
        const char *source =
            "func Alias(ref items: Array<QubitSlot>) -> Void {\n"
            "    let copy: QubitSlot = items[0];\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticResult *result = semantic_analyze(program);

        EXPECT(!parser_has_error(parser));
        EXPECT(result != NULL && result->error_count > 0);
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "Borrowed ref slot handle (movable) 'items' cannot escape into new binding 'copy' from 'items[0]'"));
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "'items[0]' is derived from that borrowed slot-handle (movable) provenance"));

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }
