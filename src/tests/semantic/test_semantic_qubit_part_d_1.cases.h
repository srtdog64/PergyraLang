static void
test_qubit_slot_semantics_part_d(void)
{
    TEST("ref boundary value parameter reports array source path on array literal store");
    {
        const char *source =
            "object Packet {\n"
            "    let hp: Int;\n"
            "}\n"
            "func Store(ref items: Array<Packet>) -> Void {\n"
            "    let copies: Array<Packet> = [items[0]];\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticResult *result = semantic_analyze(program);

        EXPECT(!parser_has_error(parser));
        EXPECT(result != NULL && result->error_count > 0);
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "Borrowed ref boundary value 'items' cannot escape through array literal store from 'items[0]'"));
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "'items[0]' is derived from that borrowed boundary provenance"));

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("ref movable-resource parameter reports array source path on array literal store");
    {
        const char *source =
            "func Store(ref items: Array<QubitSlot>) -> Void {\n"
            "    let copies: Array<QubitSlot> = [items[0]];\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticResult *result = semantic_analyze(program);

        EXPECT(!parser_has_error(parser));
        EXPECT(result != NULL && result->error_count > 0);
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "Borrowed ref slot handle (movable) 'items' cannot escape through array literal store from 'items[0]'"));
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "'items[0]' is derived from that borrowed slot-handle (movable) provenance"));

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("ref boundary value parameter may pass copied snapshot into array literal");
    {
        const char *source =
            "object Packet {\n"
            "    let hp: Int;\n"
            "}\n"
            "func Store(ref packet: Packet) -> Void {\n"
            "    let items: Array<Packet> = [Packet(packet.hp)];\n"
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

    TEST("ref boundary value parameter reports nested member assignment target and source path");
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
            "class Envelope {\n"
            "    let packet: Packet;\n"
            "}\n"
            "class State {\n"
            "    let envelope: Envelope;\n"
            "}\n"
            "func Store(ref wrapper: Wrapper) -> Void {\n"
            "    let state = State(Envelope(Packet(0)));\n"
            "    state.envelope.packet = wrapper.holder.packet;\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticResult *result = semantic_analyze(program);

        EXPECT(!parser_has_error(parser));
        EXPECT(result != NULL && result->error_count > 0);
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "into 'state.envelope.packet' from 'wrapper.holder.packet'"));
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "'wrapper.holder.packet' is derived from that borrowed boundary provenance"));

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("ref boundary value parameter reports nested projection path on transitive helper member rebind");
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
            "class Envelope {\n"
            "    let packet: Packet;\n"
            "}\n"
            "class State {\n"
            "    let envelope: Envelope;\n"
            "}\n"
            "func Store(ref wrapper: Wrapper) -> Void {\n"
            "    let state = State(Envelope(Packet(0)));\n"
            "    state.envelope.packet = Proxy(wrapper.holder.packet);\n"
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

    TEST("ref movable-resource parameter reports nested member source path on member rebind");
    {
        const char *source =
            "object Holder {\n"
            "    let q: QubitSlot;\n"
            "}\n"
            "object Wrapper {\n"
            "    let holder: Holder;\n"
            "}\n"
            "class Capsule {\n"
            "    let q: QubitSlot;\n"
            "}\n"
            "class State {\n"
            "    let capsule: Capsule;\n"
            "}\n"
            "func Store(ref wrapper: Wrapper) -> Void {\n"
            "    let state = State(Capsule(ClaimQubit()));\n"
            "    state.capsule.q = wrapper.holder.q;\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticResult *result = semantic_analyze(program);

        EXPECT(!parser_has_error(parser));
        EXPECT(result != NULL && result->error_count > 0);
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "Borrowed ref slot handle (movable) 'wrapper' cannot escape through member assignment rebind into 'state.capsule.q' from 'wrapper.holder.q'"));
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "'wrapper.holder.q' is derived from that borrowed slot-handle (movable) provenance"));

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("ref movable-resource parameter reports nested projection path on transitive helper member rebind");
    {
        const char *source =
            "object Holder {\n"
            "    let q: QubitSlot;\n"
            "}\n"
            "object Wrapper {\n"
            "    let holder: Holder;\n"
            "}\n"
            "func Escape(ref q: QubitSlot) -> QubitSlot {\n"
            "    return q;\n"
            "}\n"
            "func Proxy(ref q: QubitSlot) -> QubitSlot {\n"
            "    return Escape(q);\n"
            "}\n"
            "class Capsule {\n"
            "    let q: QubitSlot;\n"
            "}\n"
            "class State {\n"
            "    let capsule: Capsule;\n"
            "}\n"
            "func Store(ref wrapper: Wrapper) -> Void {\n"
            "    let state = State(Capsule(ClaimQubit()));\n"
            "    state.capsule.q = Proxy(wrapper.holder.q);\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticResult *result = semantic_analyze(program);

        EXPECT(!parser_has_error(parser));
        EXPECT(result != NULL && result->error_count > 0);
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "Borrowed ref slot handle (movable) 'wrapper' cannot escape through helper/function call to 'Proxy' from 'wrapper.holder.q'"));
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "'wrapper.holder.q' is derived from that borrowed slot-handle (movable) provenance"));

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("ref movable-resource parameter reports nested projection path on transitive helper array overwrite");
    {
        const char *source =
            "object Holder {\n"
            "    let q: QubitSlot;\n"
            "}\n"
            "object Wrapper {\n"
            "    let holder: Holder;\n"
            "}\n"
            "func Escape(ref q: QubitSlot) -> QubitSlot {\n"
            "    return q;\n"
            "}\n"
            "func Proxy(ref q: QubitSlot) -> QubitSlot {\n"
            "    return Escape(q);\n"
            "}\n"
            "func Store(ref wrapper: Wrapper) -> Void {\n"
            "    let items: Array<QubitSlot> = [ClaimQubit()];\n"
            "    items[0] = Proxy(wrapper.holder.q);\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticResult *result = semantic_analyze(program);

        EXPECT(!parser_has_error(parser));
        EXPECT(result != NULL && result->error_count > 0);
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "Borrowed ref slot handle (movable) 'wrapper' cannot escape through helper/function call to 'Proxy' from 'wrapper.holder.q'"));
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "'wrapper.holder.q' is derived from that borrowed slot-handle (movable) provenance"));

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("ref movable-resource parameter reports nested projection path on transitive helper queue store");
    {
        const char *source =
            "object Holder {\n"
            "    let q: QubitSlot;\n"
            "}\n"
            "object Wrapper {\n"
            "    let holder: Holder;\n"
            "}\n"
            "func Escape(ref q: QubitSlot) -> QubitSlot {\n"
            "    return q;\n"
            "}\n"
            "func Proxy(ref q: QubitSlot) -> QubitSlot {\n"
            "    return Escape(q);\n"
            "}\n"
            "func Store(ref wrapper: Wrapper) -> Void {\n"
            "    let queue: Queue<QubitSlot> = QueueNew();\n"
            "    QueuePush(queue, Proxy(wrapper.holder.q));\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticResult *result = semantic_analyze(program);

        EXPECT(!parser_has_error(parser));
        EXPECT(result != NULL && result->error_count > 0);
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "Borrowed ref slot handle (movable) 'wrapper' cannot escape through helper/function call to 'Proxy' from 'wrapper.holder.q'"));
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "'wrapper.holder.q' is derived from that borrowed slot-handle (movable) provenance"));

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("ref movable-resource parameter reports nested projection path on transitive helper map store");
    {
        const char *source =
            "object Holder {\n"
            "    let q: QubitSlot;\n"
            "}\n"
            "object Wrapper {\n"
            "    let holder: Holder;\n"
            "}\n"
            "func Escape(ref q: QubitSlot) -> QubitSlot {\n"
            "    return q;\n"
            "}\n"
            "func Proxy(ref q: QubitSlot) -> QubitSlot {\n"
            "    return Escape(q);\n"
            "}\n"
            "func Store(ref wrapper: Wrapper) -> Void {\n"
            "    let table: HashMap<String, QubitSlot> = MapNew();\n"
            "    MapSet(table, \"slot\", Proxy(wrapper.holder.q));\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticResult *result = semantic_analyze(program);

        EXPECT(!parser_has_error(parser));
        EXPECT(result != NULL && result->error_count > 0);
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "Borrowed ref slot handle (movable) 'wrapper' cannot escape through helper/function call to 'Proxy' from 'wrapper.holder.q'"));
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "'wrapper.holder.q' is derived from that borrowed slot-handle (movable) provenance"));

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }
