static void
test_qubit_slot_semantics_part_e(void)
{
    TEST("ref boundary value parameter reports member source path on assignment rebind");
    {
        const char *source =
            "object Packet {\n"
            "    let hp: Int;\n"
            "}\n"
            "object Holder {\n"
            "    let packet: Packet;\n"
            "}\n"
            "func Store(ref holder: Holder) -> Void {\n"
            "    let dst = Packet(0);\n"
            "    dst = holder.packet;\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticResult *result = semantic_analyze(program);

        EXPECT(!parser_has_error(parser));
        EXPECT(result != NULL && result->error_count > 0);
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "into 'dst' from 'holder.packet'"));
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "'holder.packet' is derived from that borrowed boundary provenance"));

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("ref boundary value parameter reports array source path on assignment rebind");
    {
        const char *source =
            "object Packet {\n"
            "    let hp: Int;\n"
            "}\n"
            "func Store(ref items: Array<Packet>) -> Void {\n"
            "    let dst = Packet(0);\n"
            "    dst = items[0];\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticResult *result = semantic_analyze(program);

        EXPECT(!parser_has_error(parser));
        EXPECT(result != NULL && result->error_count > 0);
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "into 'dst' from 'items[0]'"));
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "'items[0]' is derived from that borrowed boundary provenance"));

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("ref boundary value parameter rejects TrySend escape");
    {
        const char *source =
            "object Packet {\n"
            "    let hp: Int;\n"
            "}\n"
            "func Store(ref packet: Packet) -> Void {\n"
            "    let ch: Channel<Packet> = Channel(2);\n"
            "    TrySend(ch, packet);\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticResult *result = semantic_analyze(program);

        EXPECT(!parser_has_error(parser));
        EXPECT(result != NULL && result->error_count > 0);
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "cannot escape through channel send"));

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("ref subject parameter rejects SendTimeout escape");
    {
        const char *source =
            "subject Packet {\n"
            "    let hp: Int;\n"
            "}\n"
            "func Store(ref packet: Packet) -> Void {\n"
            "    let ch: Channel<Packet> = Channel(2);\n"
            "    SendTimeout(ch, packet, 1);\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticResult *result = semantic_analyze(program);

        EXPECT(!parser_has_error(parser));
        EXPECT(result != NULL && result->error_count > 0);
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "cannot escape through channel send"));

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("ref boundary value parameter reports member source path on return");
    {
        const char *source =
            "object Packet {\n"
            "    let hp: Int;\n"
            "}\n"
            "object Holder {\n"
            "    let packet: Packet;\n"
            "}\n"
            "func Leak(ref holder: Holder) -> Packet {\n"
            "    return holder.packet;\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticResult *result = semantic_analyze(program);

        EXPECT(!parser_has_error(parser));
        EXPECT(result != NULL && result->error_count > 0);
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "cannot escape through return from 'holder.packet'"));
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "'holder.packet' is derived from that borrowed boundary provenance"));

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("ref boundary value parameter reports member source path on transitive helper forwarding");
    {
        const char *source =
            "object Packet {\n"
            "    let hp: Int;\n"
            "}\n"
            "object Holder {\n"
            "    let packet: Packet;\n"
            "}\n"
            "func Escape(ref packet: Packet) -> Packet {\n"
            "    return packet;\n"
            "}\n"
            "func Proxy(ref packet: Packet) -> Packet {\n"
            "    return Escape(packet);\n"
            "}\n"
            "func Store(ref holder: Holder) -> Packet {\n"
            "    return Proxy(holder.packet);\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticResult *result = semantic_analyze(program);

        EXPECT(!parser_has_error(parser));
        EXPECT(result != NULL && result->error_count > 0);
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "Borrowed ref boundary value 'holder' cannot escape through helper/function call to 'Proxy' from 'holder.packet'"));
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "'holder.packet' is derived from that borrowed boundary provenance"));

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("ref boundary value parameter reports array source path on channel send");
    {
        const char *source =
            "object Packet {\n"
            "    let hp: Int;\n"
            "}\n"
            "func Store(ref packets: Array<Packet>, ch: Channel<Packet>) -> Void {\n"
            "    ch <- packets[0];\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticResult *result = semantic_analyze(program);

        EXPECT(!parser_has_error(parser));
        EXPECT(result != NULL && result->error_count > 0);
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "Borrowed ref boundary value 'packets' cannot escape through channel send from 'packets[0]'"));
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "'packets[0]' is derived from that borrowed boundary provenance"));

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("ref movable-resource parameter reports array source path on channel send");
    {
        const char *source =
            "func Store(ref items: Array<QubitSlot>, ch: Channel<QubitSlot>) -> Void {\n"
            "    ch <- items[0];\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticResult *result = semantic_analyze(program);

        EXPECT(!parser_has_error(parser));
        EXPECT(result != NULL && result->error_count > 0);
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "Borrowed ref slot handle (movable) 'items' cannot escape through channel send from 'items[0]'"));
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "'items[0]' is derived from that borrowed slot-handle (movable) provenance"));

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("world embedding diagnostic reports explicit contract source");
    {
        const char *source =
            "zone BattleZone {\n"
            "}\n"
            "world Arena {\n"
            "    zone battle: BattleZone\n"
            "}\n"
            "func Build() -> Void {\n"
            "    let battle = BattleZone();\n"
            "    let arena = Arena(battle);\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticResult *result = semantic_analyze(program);

        EXPECT(!parser_has_error(parser));
        EXPECT(result != NULL && result->error_count > 0);
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "Contract source:"));
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "ownership/authority after construction belongs to the world-owned slot"));

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("own SecureSlot<subject> parameter rejects aliasing into new binding");
    {
        const char *source =
            "subject Vec2 {\n"
            "    let x: Int;\n"
            "    let y: Int;\n"
            "}\n"
            "func OwnAlias(own s: SecureSlot<Vec2>) -> Void {\n"
            "    let alias: SecureSlot<Vec2> = s;\n"
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

    TEST("own SecureSlot<subject> parameter allows forwarding into own helper");
    {
        const char *source =
            "subject Vec2 {\n"
            "    let x: Int;\n"
            "    let y: Int;\n"
            "}\n"
            "func Consume(own inner: SecureSlot<Vec2>) -> Void {\n"
            "}\n"
            "func Relay(own s: SecureSlot<Vec2>) -> Void {\n"
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

    TEST("local SecureSlot<subject> rejects use after own helper move");
    {
        const char *source =
            "subject Vec2 {\n"
            "    let x: Int;\n"
            "    let y: Int;\n"
            "}\n"
            "func Consume(own inner: SecureSlot<Vec2>) -> Void {\n"
            "}\n"
            "func Main() -> Void {\n"
            "    let s: SecureSlot<Vec2> = Vec2(1, 2);\n"
            "    Consume(s);\n"
            "    Read(s, s_token);\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticResult *result = semantic_analyze(program);

        EXPECT(!parser_has_error(parser));
        EXPECT(result != NULL && result->error_count > 0);
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "released slot"));

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("own SecureSlot<subject> parameter allows transitive forwarding into own helper");
    {
        const char *source =
            "subject Vec2 {\n"
            "    let x: Int;\n"
            "    let y: Int;\n"
            "}\n"
            "func Consume(own inner: SecureSlot<Vec2>) -> Void {\n"
            "}\n"
            "func Middle(own m: SecureSlot<Vec2>) -> Void {\n"
            "    Consume(m);\n"
            "}\n"
            "func Relay(own s: SecureSlot<Vec2>) -> Void {\n"
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

    TEST("own SecureSlot<subject> return is accepted as explicit anchored-handle transfer boundary");
    {
        const char *source =
            "subject Vec2 {\n"
            "    let x: Int;\n"
            "    let y: Int;\n"
            "}\n"
            "func Leak(own s: SecureSlot<Vec2>) -> SecureSlot<Vec2> {\n"
            "    return s;\n"
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

    TEST("own SecureSlot<subject> channel send is accepted as explicit anchored-handle transfer boundary");
    {
        const char *source =
            "subject Vec2 {\n"
            "    let x: Int;\n"
            "    let y: Int;\n"
            "}\n"
            "func SendAway(own s: SecureSlot<Vec2>) -> Void {\n"
            "    let ch: Channel<SecureSlot<Vec2>> = Channel(2);\n"
            "    ch <- s;\n"
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

    TEST("Slot return types are accepted when the boundary is explicit");
    {
        const char *source =
            "subject Vec2 { let x: Int; let y: Int; }\n"
            "func MakeSlot(own s: Slot<Vec2>) -> Slot<Vec2> {\n"
            "    return s;\n"
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


static void
test_qubit_slot_semantics(void)
{
    test_qubit_slot_semantics_part_a();
    test_qubit_slot_semantics_part_b();
    test_qubit_slot_semantics_part_c();
    test_qubit_slot_semantics_part_d();
    test_qubit_slot_semantics_part_e();
}
