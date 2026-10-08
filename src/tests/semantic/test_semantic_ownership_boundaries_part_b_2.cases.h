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

    TEST("direct synchronous recursion may reborrow the same ref parameter");
    {
        const char *source =
            "class Packet {\n"
            "    let items: Array<Int>;\n"
            "}\n"
            "func Visit(ref packet: Packet, depth: Int) -> Int {\n"
            "    if depth <= 0 {\n"
            "        return ArrayLength(packet.items);\n"
            "    }\n"
            "    return Visit(packet, depth - 1);\n"
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

    TEST("direct recursive reborrow still cannot return borrowed provenance");
    {
        const char *source =
            "class Packet {\n"
            "    let items: Array<Int>;\n"
            "}\n"
            "func Leak(ref packet: Packet, depth: Int) -> Packet {\n"
            "    if depth <= 0 {\n"
            "        return packet;\n"
            "    }\n"
            "    return Leak(packet, depth - 1);\n"
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

    {
        static const struct {
            const char *name;
            const char *body;
            const char *refusal;
        } cases[] = {
            {"owned functional update restores the assigned generation",
             "let packet: Packet = Packet(1); packet = Step(packet); "
             "packet = Step(packet); Log(packet.size);", NULL},
            {"owned functional update restores each loop generation",
             "let packet: Packet = Packet(1); let i: Int = 0; "
             "while i < 3 { packet = Step(packet); i = i + 1; } "
             "Log(packet.size);", NULL},
            {"fresh value can initialize a previously consumed value place",
             "let packet: Packet = Packet(1); let next: Packet = Step(packet); "
             "packet = Packet(7); Log(packet.size); Log(next.size);", NULL},
            {"owned array functional update restores the assigned generation",
             "let values: Array<Int> = [1]; values = StepArray(values); "
             "Log(values[1]);", NULL},
            {"empty literal initializes a consumed array place",
             "let values: Array<Int> = [1]; let next: Array<Int> = StepArray(values); "
             "values = []; Log(ArrayLength(values)); Log(next[1]);", NULL},
            {"terminal move does not poison the continuing branch",
             "let packet: Packet = Packet(1); packet = Choose(packet, false); "
             "Log(packet.size);", NULL},
            {"both continuing branches restore an owned generation",
             "let packet: Packet = Packet(1); packet = ChooseRestored(packet, true); "
             "Log(packet.size);", NULL},
            {"read of old generation without assignment remains rejected",
             "let packet: Packet = Packet(1); let next: Packet = Step(packet); "
             "Log(packet.size);", "was moved or released"},
            {"assignment cannot conceal two moves in its RHS",
             "let packet: Packet = Packet(1); packet = Pair(packet, packet);",
             "was moved or released"},
            {"later call argument cannot read the earlier moved value",
             "let packet: Packet = Packet(1); packet = WithSize(packet, packet.size);",
             "was moved or released"},
            {"assignment cannot read an already consumed RHS",
             "let packet: Packet = Packet(1); let next: Packet = Step(packet); "
             "packet = packet;", "was moved or released"},
            {"functional update does not waive result type admission",
             "let packet: Packet = Packet(1); packet = Count(packet);",
             "cannot assign 'Int' to 'Packet'"},
            {"continuing branch without restoration leaves the old value dead",
             "let packet: Packet = Packet(1); if true { let next: Packet = Step(packet); } "
             "Log(packet.size);", "was moved or released"}
        };
        const char *prefix =
            "struct Packet { size: Int; }\n"
            "func Step(own packet: Packet) -> Packet { return Packet(packet.size + 1); }\n"
            "func Pair(own first: Packet, own second: Packet) -> Packet { return first; }\n"
            "func Count(own packet: Packet) -> Int { return packet.size; }\n"
            "func WithSize(own packet: Packet, size: Int) -> Packet { return Packet(size); }\n"
            "func Choose(own packet: Packet, early: Bool) -> Packet { "
            "if early { return Step(packet); } packet = Step(packet); return packet; }\n"
            "func ChooseRestored(own packet: Packet, choice: Bool) -> Packet { "
            "if choice { packet = Step(packet); } else { packet = Step(packet); } return packet; }\n"
            "func StepArray(own values: Array<Int>) -> Array<Int> { ArrayPush(values, 3); return values; }\n"
            "func Main() -> Void { ";
        for (size_t c = 0; c < sizeof(cases) / sizeof(cases[0]); c++) {
            char source[1400];
            TEST(cases[c].name);
            int length = snprintf(source, sizeof(source), "%s%s }\n",
                prefix, cases[c].body);
            EXPECT(length > 0 && (size_t)length < sizeof(source));
            Lexer *lexer = lexer_create(source);
            Parser *parser = parser_create(lexer);
            ASTNode *program = parser_parse_program(parser);
            SemanticResult *result = semantic_analyze(program);
            EXPECT(!parser_has_error(parser));
            if (cases[c].refusal == NULL) {
                EXPECT(result != NULL && result->error_count == 0);
            } else {
                EXPECT(result != NULL && result->error_count > 0);
                EXPECT(ctx_has_diagnostic_substring_from_result(result,
                    cases[c].refusal));
            }
            semantic_result_destroy(result);
            ast_destroy(program);
            parser_destroy(parser);
            lexer_destroy(lexer);
        }
    }

    TEST("own enum with a runtime handle payload is consumed once");
    {
        const char *source =
            "enum Outcome {\n"
            "    Ready(Rc<Int>),\n"
            "    Rejected(Int),\n"
            "}\n"
            "func Consume(own outcome: Outcome) -> Void {\n"
            "    Log(1);\n"
            "}\n"
            "func Main() -> Void {\n"
            "    let outcome: Outcome = Ready(RcNew(5));\n"
            "    Consume(outcome);\n"
            "    Consume(outcome);\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticResult *result = semantic_analyze(program);

        EXPECT(!parser_has_error(parser));
        EXPECT(result != NULL && result->error_count > 0);
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "was moved or released and cannot be used again"));

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("own enum with a subject payload is consumed once");
    {
        const char *source =
            "subject Hero {\n"
            "    let hp: Int;\n"
            "}\n"
            "enum Outcome {\n"
            "    Ready(Hero),\n"
            "    Rejected(Int),\n"
            "}\n"
            "func Consume(own outcome: Outcome) -> Void {\n"
            "    Log(1);\n"
            "}\n"
            "func Main() -> Void {\n"
            "    let outcome: Outcome = Ready(Hero(3));\n"
            "    Consume(outcome);\n"
            "    Consume(outcome);\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticResult *result = semantic_analyze(program);

        EXPECT(!parser_has_error(parser));
        EXPECT(result != NULL && result->error_count > 0);
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "was moved or released and cannot be used again"));

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("own enum with copy-only payloads stays a copy");
    {
        const char *source =
            "enum Outcome {\n"
            "    Ready(Int),\n"
            "    Rejected(Int),\n"
            "}\n"
            "func Consume(own outcome: Outcome) -> Void {\n"
            "    Log(1);\n"
            "}\n"
            "func Main() -> Void {\n"
            "    let outcome: Outcome = Ready(4);\n"
            "    Consume(outcome);\n"
            "    Consume(outcome);\n"
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

    TEST("parallel read of a self-referential type reaches no storage");
    {
        const char *source =
            "class Node {\n"
            "    let value: Int;\n"
            "    let next: Option<Node>;\n"
            "}\n"
            "func Main() -> Void {\n"
            "    let tail: Node = Node(2, None);\n"
            "    let head: Node = Node(1, Some(tail));\n"
            "    let mut a: Int = 0;\n"
            "    let mut b: Int = 0;\n"
            "    parallel {\n"
            "        { a = head.value; }\n"
            "        { b = head.value * 3; }\n"
            "    }\n"
            "    Log(a + b);\n"
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
