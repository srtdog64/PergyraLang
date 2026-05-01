static void
test_slot_sugar(void)
{
    printf("\n[slot_sugar]\n");

    TranspilerCtx *ctx;

    TEST("let x: Slot<Int> = 42 -> claim + write");
    {
        ASTNode *node = make_let("x", make_type_node("Slot<Int>"),
                                  make_number(42, 1), 1);
        const char *out = emit_stmt_to_str(node, &ctx);
        EXPECT(strstr(out, "pgy_claim_Int()") != NULL);
        EXPECT(strstr(out, "pgy_write_Int(&x, 42)") != NULL);
        transpiler_ctx_destroy(ctx);
    }

    TEST("slot sugar: identifier auto-read via Log");
    {
        /* Build: func Main() -> Void { let x: Slot<Int> = 42; Log(x); } */
        ASTNode *let_node = make_let("x", make_type_node("Slot<Int>"),
                                      make_number(42, 1), 1);

        ASTNode *x_ident = calloc(1, sizeof(ASTNode));
        x_ident->type = AST_IDENTIFIER; x_ident->line = 2;
        x_ident->data.identifier.name = pergyra_strdup("x");

        ASTNode *log_call = calloc(1, sizeof(ASTNode));
        log_call->type = AST_CALL; log_call->line = 2;
        ASTNode *log_id = calloc(1, sizeof(ASTNode));
        log_id->type = AST_IDENTIFIER; log_id->data.identifier.name = pergyra_strdup("Log");
        log_call->data.call.callee = log_id;
        log_call->data.call.arguments = malloc(sizeof(ASTNode*));
        log_call->data.call.arguments[0] = x_ident;
        log_call->data.call.arg_count = 1;
        ctx = transpiler_ctx_create();
        emit_statement(let_node, ctx);
        emit_statement(log_call, ctx);
        EXPECT(strstr(ctx->out->data, "pgy_read_Int(&") != NULL);
        transpiler_ctx_destroy(ctx);
    }

    TEST("slot sugar: x = 5 auto-write");
    {
        ASTNode *let_node = make_let("x", make_type_node("Slot<Int>"),
                                      make_number(42, 1), 1);

        ASTNode *assign = calloc(1, sizeof(ASTNode));
        assign->type = AST_ASSIGNMENT; assign->line = 2;
        ASTNode *tgt = calloc(1, sizeof(ASTNode));
        tgt->type = AST_IDENTIFIER; tgt->data.identifier.name = pergyra_strdup("x");
        assign->data.assignment.target = tgt;
        assign->data.assignment.value  = make_number(5, 2);
        ctx = transpiler_ctx_create();
        emit_statement(let_node, ctx);
        emit_statement(assign, ctx);
        EXPECT(strstr(ctx->out->data, "pgy_write_Int(&") != NULL);
        transpiler_ctx_destroy(ctx);
    }

    TEST("explicit Release prevents double release");
    {
        /* let a: Slot<Int> = ClaimSlot<Int>(); Write(a,10); Release(a); */
        ASTNode *args0[0];
        ASTNode *init = make_call("ClaimSlot", args0, 0, 1);
        ASTNode *let_node = make_let("a", make_type_node("Slot<Int>"), init, 1);

        ASTNode *a_id = calloc(1, sizeof(ASTNode));
        a_id->type = AST_IDENTIFIER; a_id->data.identifier.name = pergyra_strdup("a");
        ASTNode *w_args[] = { a_id, make_number(10, 2) };
        ASTNode *write_call = make_call("Write", w_args, 2, 2);

        ASTNode *a_id2 = calloc(1, sizeof(ASTNode));
        a_id2->type = AST_IDENTIFIER; a_id2->data.identifier.name = pergyra_strdup("a");
        ASTNode *r_args[] = { a_id2 };
        ASTNode *rel_call = make_call("Release", r_args, 1, 3);
        ctx = transpiler_ctx_create();
        emit_statement(let_node, ctx);
        emit_statement(write_call, ctx);
        emit_statement(rel_call, ctx);

        int count = 0;
        const char *p = ctx->out->data;
        while ((p = strstr(p, "pgy_release_Int")) != NULL) { count++; p++; }
        EXPECT(count == 1);
        transpiler_ctx_destroy(ctx);
    }

    TEST("pin block emits typed runtime pin wrapper with cleanup");
    {
        ASTNode *let_node = make_let("x", make_type_node("Slot<Int>"),
                                      make_number(42, 1), 1);
        ASTNode *pin_block = calloc(1, sizeof(ASTNode));
        pin_block->type = AST_BLOCK;
        pin_block->line = 2;
        pin_block->data.block.statements = NULL;
        pin_block->data.block.count = 0;
        pin_block->data.block.is_pin_block = true;
        pin_block->data.block.pin_view_is_write = true;
        pin_block->data.block.pin_source_name = pergyra_strdup("x");
        pin_block->data.block.pin_view_name = pergyra_strdup("view");

        ctx = transpiler_ctx_create();
        emit_statement(let_node, ctx);
        emit_block(pin_block, ctx);

        EXPECT_STR_CONTAINS(ctx->out->data, "PgyPinnedSlotView_Int");
        EXPECT_STR_CONTAINS(ctx->out->data, "cleanup(pgy_unpin_cleanup_Int)");
        EXPECT_STR_CONTAINS(ctx->out->data, "pgy_pin_write_Int(&x)");

        ast_destroy(pin_block);
        transpiler_ctx_destroy(ctx);
    }
}

static void
test_stdlib_and_enum_emit(void)
{
    printf("\n[stdlib_enum]\n");

    TranspilerCtx *ctx;

    TEST("array literal let emits PgyArray_Int builder");
    {
        ASTNode *arr = calloc(1, sizeof(ASTNode));
        arr->type = AST_ARRAY_LITERAL;
        arr->line = 1;
        arr->data.array_literal.count = 3;
        arr->data.array_literal.elements = calloc(3, sizeof(ASTNode *));
        arr->data.array_literal.elements[0] = make_number(1, 1);
        arr->data.array_literal.elements[1] = make_number(2, 1);
        arr->data.array_literal.elements[2] = make_number(3, 1);

        ASTNode *node = make_let("values", make_generic_type("Array", "Int"), arr, 1);
        const char *out = emit_stmt_to_str(node, &ctx);
        EXPECT_STR_CONTAINS(out, "PgyArray_Int values = ({");
        EXPECT_STR_CONTAINS(out, "pgy_array_push_Int");
        transpiler_ctx_destroy(ctx);
    }

    TEST("empty array literal uses explicit Array<T> annotation");
    {
        ASTNode *arr = calloc(1, sizeof(ASTNode));
        arr->type = AST_ARRAY_LITERAL;
        arr->line = 1;
        arr->data.array_literal.count = 0;
        arr->data.array_literal.elements = NULL;

        ASTNode *node = make_let("values", make_generic_type("Array", "String"), arr, 1);
        const char *out = emit_stmt_to_str(node, &ctx);
        EXPECT(ctx->backend_error == NULL);
        EXPECT_STR_CONTAINS(out, "PgyArray_String values = ({");
        EXPECT_STR_CONTAINS(out, "pgy_array_new_String(0)");
        transpiler_ctx_destroy(ctx);
    }

    TEST("empty array literal without annotation is rejected by C backend");
    {
        ASTNode *arr = calloc(1, sizeof(ASTNode));
        arr->type = AST_ARRAY_LITERAL;
        arr->line = 1;
        arr->data.array_literal.count = 0;
        arr->data.array_literal.elements = NULL;

        ASTNode *node = make_let("values", NULL, arr, 1);
        (void)emit_stmt_to_str(node, &ctx);
        EXPECT(ctx->backend_error != NULL);
        EXPECT_STR_CONTAINS(ctx->backend_error, "requires an explicit Array<T> annotation");
        transpiler_ctx_destroy(ctx);
    }

    TEST("String built-ins map to runtime helpers");
    {
        ASTNode *args[2] = { make_string_lit("a", 1), make_string_lit("b", 1) };
        ASTNode *call = make_call("Concat", args, 2, 1);
        ASTNode *node = make_let("s", make_type_node("String"), call, 1);
        const char *out = emit_stmt_to_str(node, &ctx);
        EXPECT_STR_CONTAINS(out, "StringConcat(\"a\", \"b\")");
        transpiler_ctx_destroy(ctx);
    }

    TEST("Set built-ins map to runtime helpers");
    {
        const char *source =
            "func Main() -> Void {\n"
            "    let seen: Set<Int> = SetNew();\n"
            "    SetAdd(seen, 7);\n"
            "    Log(SetHas(seen, 7));\n"
            "    Log(SetSize(seen));\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        HIRProgram *hir = NULL;
        RIRProgram *rir = NULL;
        MIRProgram *mir = lower_program_to_mir(program, &hir, &rir);
        ctx = transpiler_ctx_create();

        emit_program(ctx);

        EXPECT_STR_CONTAINS(ctx->out->data, "pgy_set_new_int()");
        EXPECT_STR_CONTAINS(ctx->out->data, "pgy_set_add_int");
        EXPECT_STR_CONTAINS(ctx->out->data, "pgy_set_has_int");
        EXPECT_STR_CONTAINS(ctx->out->data, "pgy_set_size_int");

        transpiler_ctx_destroy(ctx);
        mir_destroy(mir);
        rir_destroy(rir);
        hir_destroy(hir);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("string equality lowers through runtime helper");
    {
        const char *source =
            "func Match(name: String) -> Bool {\n"
            "    return name == \"audit\";\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        HIRProgram *hir = NULL;
        RIRProgram *rir = NULL;
        MIRProgram *mir = lower_program_to_mir(program, &hir, &rir);
        ctx = transpiler_ctx_create();

        emit_program(ctx);

        EXPECT_STR_CONTAINS(ctx->out->data, "pgy_string_equals(name, \"audit\")");

        transpiler_ctx_destroy(ctx);
        mir_destroy(mir);
        rir_destroy(rir);
        hir_destroy(hir);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("string concat let inference emits char* local");
    {
        const char *source =
            "struct UnitDraft {\n"
            "    roleTitle: String;\n"
            "    originTitle: String;\n"
            "}\n"
            "func FinalizeUnitSpec(draft: UnitDraft) -> String {\n"
            "    let title = draft.roleTitle + \" of the \" + draft.originTitle;\n"
            "    return title;\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        HIRProgram *hir = NULL;
        RIRProgram *rir = NULL;
        MIRProgram *mir = lower_program_to_mir(program, &hir, &rir);
        ctx = transpiler_ctx_create();

        emit_program(ctx);

        EXPECT_STR_CONTAINS(ctx->out->data, "StringConcat(");
        EXPECT_STR_CONTAINS(ctx->out->data, "draft.roleTitle");
        EXPECT_STR_CONTAINS(ctx->out->data, "draft.originTitle");

        transpiler_ctx_destroy(ctx);
        mir_destroy(mir);
        rir_destroy(rir);
        hir_destroy(hir);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("function type params lower to callable declarators and typed lambdas");
    {
        const char *source =
            "struct StrategyContext {\n"
            "    morale: Int;\n"
            "}\n"
            "func Apply(base: Int, ctx: StrategyContext, policy: func(Int, StrategyContext) -> Int) -> Int {\n"
            "    return policy(base, ctx);\n"
            "}\n"
            "func Run() -> Int {\n"
            "    let ctx = StrategyContext(3);\n"
            "    return Apply(2, ctx, (base: Int, ctx: StrategyContext) => base + ctx.morale);\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        HIRProgram *hir = NULL;
        RIRProgram *rir = NULL;
        MIRProgram *mir = lower_program_to_mir(program, &hir, &rir);
        ctx = transpiler_ctx_create();

        emit_program(ctx);

        EXPECT_STR_CONTAINS(ctx->out->data,
            "int32_t Apply(int32_t base, StrategyContext ctx, int32_t (*policy)(int32_t, StrategyContext))");
        EXPECT(strstr(ctx->decls->data, "pgy_lambda_") != NULL
            || strstr(ctx->helpers->data, "pgy_lambda_") != NULL);
        EXPECT(strstr(ctx->decls->data, "int32_t base, StrategyContext ctx") != NULL
            || strstr(ctx->helpers->data, "int32_t base, StrategyContext ctx") != NULL);

        transpiler_ctx_destroy(ctx);
        mir_destroy(mir);
        rir_destroy(rir);
        hir_destroy(hir);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("function typed locals and returns lower as function pointers");
    {
        const char *source =
            "func AddOne(x: Int) -> Int {\n"
            "    return x + 1;\n"
            "}\n"
            "func MakeAdder() -> func(Int) -> Int {\n"
            "    return AddOne;\n"
            "}\n"
            "func Main() -> Void {\n"
            "    let f: func(Int) -> Int = AddOne;\n"
            "    let g = MakeAdder();\n"
            "    Log(f(4));\n"
            "    Log(g(9));\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        HIRProgram *hir = NULL;
        RIRProgram *rir = NULL;
        MIRProgram *mir = lower_program_to_mir(program, &hir, &rir);
        ctx = transpiler_ctx_create();

        emit_program(ctx);

        EXPECT_STR_CONTAINS(ctx->decls->data, "MakeAdder");
        EXPECT_STR_CONTAINS(ctx->out->data, "MakeAdder");
        EXPECT_STR_CONTAINS(ctx->out->data, "AddOne");

        transpiler_ctx_destroy(ctx);
        mir_destroy(mir);
        rir_destroy(rir);
        hir_destroy(hir);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("MIR locals keep function-pointer declarators for inferred and annotated callables");
    {
        const char *source =
            "func Compact(route: String, ok: Bool, handle: Int) -> String {\n"
            "    return route;\n"
            "}\n"
            "func Pick(mode: String) -> func(String, Bool, Int) -> String {\n"
            "    return Compact;\n"
            "}\n"
            "func Main() -> Void {\n"
            "    let checkoutFormatter = Pick(\"verbose\");\n"
            "    let refundFormatter: func(String, Bool, Int) -> String = Compact;\n"
            "    Log(checkoutFormatter(\"/checkout\", true, 4101));\n"
            "    Log(refundFormatter(\"/refund\", false, 8831));\n"
            "}\n";
        ASTNode *program = NULL;
        HIRProgram *hir = NULL;
        RIRProgram *rir = NULL;
        MIRProgram *mir = NULL;
        bool ok = lower_pipeline_from_source(source, &program, &hir, &rir, &mir);
        ctx = transpiler_ctx_create();
        ctx->mir = mir;

        EXPECT(ok);
        emit_program(ctx);

        EXPECT_STR_CONTAINS(ctx->out->data,
            "char* (*_pgy_ssa_checkoutFormatter_");
        EXPECT_STR_CONTAINS(ctx->out->data,
            ")(char*, bool, int32_t) = 0;");
        EXPECT_STR_CONTAINS(ctx->out->data,
            "_pgy_ssa_checkoutFormatter_1 = Pick(\"verbose\");");
        EXPECT_STR_CONTAINS(ctx->out->data,
            "char* (*_pgy_ssa_refundFormatter_");
        EXPECT_STR_CONTAINS(ctx->out->data,
            ")(char*, bool, int32_t) = 0;");
        EXPECT_STR_CONTAINS(ctx->out->data,
            "_pgy_ssa_refundFormatter_1 = Compact;");

        transpiler_ctx_destroy(ctx);
        mir_destroy(mir);
        rir_destroy(rir);
        hir_destroy(hir);
        ast_destroy(program);
    }

    TEST("List<subject> typed let emits specialized helpers");
    {
        const char *source =
            "subject Player {\n"
            "    let hp: Int;\n"
            "}\n"
            "func Main() -> Void {\n"
            "    let roster: List<Player> = ListNew();\n"
            "    let recruit = Player(10);\n"
            "    ListPush(roster, recruit);\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        HIRProgram *hir = NULL;
        RIRProgram *rir = NULL;
        MIRProgram *mir = lower_program_to_mir(program, &hir, &rir);
        ctx = transpiler_ctx_create();

        emit_program(ctx);

        EXPECT_STR_CONTAINS(ctx->out->data, "pgy_list_new_Player()");
        EXPECT_STR_CONTAINS(ctx->out->data, "pgy_list_push_Player");
        EXPECT(strstr(ctx->decls->data, "PGY_LIST_DEFINE(Player, Player)") != NULL
            || strstr(ctx->out->data, "PGY_LIST_DEFINE(Player, Player)") != NULL);

        transpiler_ctx_destroy(ctx);
        mir_destroy(mir);
        rir_destroy(rir);
        hir_destroy(hir);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("for-in over List<subject> lowers through list storage");
    {
        const char *source =
            "subject Event {\n"
            "    let title: String;\n"
            "}\n"
            "func Main() -> Void {\n"
            "    let events: List<Event> = ListNew();\n"
            "    ListPush(events, Event(\"Kickoff\"));\n"
            "    for event in events {\n"
            "        Print(event.title);\n"
            "    }\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        HIRProgram *hir = NULL;
        RIRProgram *rir = NULL;
        MIRProgram *mir = lower_program_to_mir(program, &hir, &rir);
        ctx = transpiler_ctx_create();

        emit_program(ctx);

        EXPECT_STR_CONTAINS(ctx->out->data, "pgy_list_new_Event()");
        EXPECT_STR_CONTAINS(ctx->out->data, "size_t _pgy_idx_");
        EXPECT_STR_CONTAINS(ctx->out->data, ".count");
        EXPECT_STR_CONTAINS(ctx->out->data, ".data[_pgy_idx_");

        transpiler_ctx_destroy(ctx);
        mir_destroy(mir);
        rir_destroy(rir);
        hir_destroy(hir);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("Queue<class> typed let emits specialized helpers");
    {
        const char *source =
            "class Weapon {\n"
            "    let name: String;\n"
            "}\n"
            "func Main() -> Void {\n"
            "    let bow = Weapon(\"Bow\");\n"
            "    let satchel: Queue<Weapon> = QueueNew();\n"
            "    QueuePush(satchel, bow);\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        HIRProgram *hir = NULL;
        RIRProgram *rir = NULL;
        MIRProgram *mir = lower_program_to_mir(program, &hir, &rir);
        ctx = transpiler_ctx_create();

        emit_program(ctx);

        EXPECT_STR_CONTAINS(ctx->out->data, "pgy_queue_new_Weapon()");
        EXPECT_STR_CONTAINS(ctx->out->data, "pgy_queue_push_Weapon");
        EXPECT(strstr(ctx->decls->data, "PGY_QUEUE_DEFINE(Weapon, Weapon)") != NULL
            || strstr(ctx->out->data, "PGY_QUEUE_DEFINE(Weapon, Weapon)") != NULL);

        transpiler_ctx_destroy(ctx);
        mir_destroy(mir);
        rir_destroy(rir);
        hir_destroy(hir);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("Now and Sleep builtins emit runtime calls");
    {
        const char *source =
            "func Main() -> Void {\n"
            "    let t0: Int = Now();\n"
            "    Sleep(5);\n"
            "    let t1: Int = Now();\n"
            "    Log(ToString(t1 - t0));\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        HIRProgram *hir = NULL;
        RIRProgram *rir = NULL;
        MIRProgram *mir = lower_program_to_mir(program, &hir, &rir);
        ctx = transpiler_ctx_create();

        emit_program(ctx);

        EXPECT_STR_CONTAINS(ctx->out->data, "pgy_now_ms()");
        EXPECT_STR_CONTAINS(ctx->out->data, "pgy_sleep_ms(");

        transpiler_ctx_destroy(ctx);
        mir_destroy(mir);
        rir_destroy(rir);
        hir_destroy(hir);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("ReadLine builtin emits runtime input call");
    {
        const char *source =
            "func Main() -> Void {\n"
            "    let line: String = ReadLine();\n"
            "    Print(line);\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        HIRProgram *hir = NULL;
        RIRProgram *rir = NULL;
        MIRProgram *mir = lower_program_to_mir(program, &hir, &rir);
        ctx = transpiler_ctx_create();

        emit_program(ctx);

        EXPECT_STR_CONTAINS(ctx->out->data, "pgy_input(\"\"");
        EXPECT_STR_CONTAINS(ctx->out->data, "pgy_print(");

        transpiler_ctx_destroy(ctx);
        mir_destroy(mir);
        rir_destroy(rir);
        hir_destroy(hir);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("HashMap<String, subject> typed let emits specialized helpers");
    {
        const char *source =
            "subject Player {\n"
            "    let hp: Int;\n"
            "}\n"
            "func Main() -> Void {\n"
            "    let registry: HashMap<String, Player> = MapNew();\n"
            "    let hero = Player(42);\n"
            "    MapSet(registry, \"hero\", hero);\n"
            "    let loaded = MapGet(registry, \"hero\");\n"
            "    Log(ToString(loaded.hp));\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        HIRProgram *hir = NULL;
        RIRProgram *rir = NULL;
        MIRProgram *mir = lower_program_to_mir(program, &hir, &rir);
        ctx = transpiler_ctx_create();

        emit_program(ctx);

        EXPECT_STR_CONTAINS(ctx->out->data, "pgy_map_new_");
        EXPECT_STR_CONTAINS(ctx->out->data, "pgy_map_set_Player");
        EXPECT_STR_CONTAINS(ctx->out->data, "pgy_map_get_Player");
        EXPECT(strstr(ctx->decls->data, "PGY_HASHMAP_DEFINE(Player, Player)") != NULL
            || strstr(ctx->out->data, "PGY_HASHMAP_DEFINE(Player, Player)") != NULL);

        transpiler_ctx_destroy(ctx);
        mir_destroy(mir);
        rir_destroy(rir);
        hir_destroy(hir);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("type aliases lower to typedefs and preserve specialized collection types");
    {
        const char *source =
            "class Player {\n"
            "    let hp: Int;\n"
            "}\n"
            "type UserId = Int;\n"
            "type PartyIndex = HashMap<String, Player>;\n"
            "func Load(id: UserId, registry: PartyIndex) -> Player {\n"
            "    return MapGet(registry, \"hero\");\n"
            "}\n"
            "func Main() -> Void {\n"
            "    let id: UserId = 7;\n"
            "    let registry: PartyIndex = MapNew();\n"
            "    MapSet(registry, \"hero\", Player(42));\n"
            "    let hero = Load(id, registry);\n"
            "    Log(ToString(hero.hp));\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        HIRProgram *hir = NULL;
        RIRProgram *rir = NULL;
        MIRProgram *mir = lower_program_to_mir(program, &hir, &rir);
        ctx = transpiler_ctx_create();

        emit_program(ctx);

        EXPECT(strstr(ctx->out->data, "typedef int32_t UserId;") != NULL
               || strstr(ctx->decls->data, "typedef int32_t UserId;") != NULL);
        EXPECT_STR_CONTAINS(ctx->out->data, "PartyIndex");
        EXPECT(strstr(ctx->decls->data, "PGY_HASHMAP_DEFINE(Player") != NULL);
        EXPECT_STR_CONTAINS(ctx->decls->data, "Load(");
        EXPECT_STR_CONTAINS(ctx->out->data, "pgy_map_new_");

        transpiler_ctx_destroy(ctx);
        mir_destroy(mir);
        rir_destroy(rir);
        hir_destroy(hir);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("nested HashMap<String, List<String>> parses and lowers");
    {
        const char *source =
            "func Main() -> Void {\n"
            "    let events: List<String> = ListNew();\n"
            "    ListPush(events, \"a\");\n"
            "    let buckets: HashMap<String, List<String>> = MapNew();\n"
            "    MapSet(buckets, \"today\", events);\n"
            "    let loaded = MapGet(buckets, \"today\");\n"
            "    Log(ToString(ListSize(loaded)));\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        HIRProgram *hir = NULL;
        RIRProgram *rir = NULL;
        MIRProgram *mir = lower_program_to_mir(program, &hir, &rir);
        ctx = transpiler_ctx_create();

        emit_program(ctx);

        EXPECT_STR_CONTAINS(ctx->out->data, "pgy_list_new_string()");
        EXPECT_STR_CONTAINS(ctx->out->data, "pgy_map_new_List_String()");
        EXPECT_STR_CONTAINS(ctx->out->data, "pgy_map_set_List_String");
        EXPECT_STR_CONTAINS(ctx->out->data, "pgy_map_get_List_String");

        transpiler_ctx_destroy(ctx);
        mir_destroy(mir);
        rir_destroy(rir);
        hir_destroy(hir);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("nested specialized collection signatures emit type defs before prototypes");
    {
        const char *source =
            "func BuildBuckets() -> HashMap<String, List<String>> {\n"
            "    let events: List<String> = ListNew();\n"
            "    let buckets: HashMap<String, List<String>> = MapNew();\n"
            "    MapSet(buckets, \"today\", events);\n"
            "    return buckets;\n"
            "}\n"
            "func RenderBuckets(buckets: HashMap<String, List<String>>) -> String {\n"
            "    let loaded = MapGet(buckets, \"today\");\n"
            "    return ListGet(loaded, 0);\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        HIRProgram *hir = NULL;
        RIRProgram *rir = NULL;
        MIRProgram *mir = lower_program_to_mir(program, &hir, &rir);
        ctx = transpiler_ctx_create();
        char *map_define_pos;
        char *build_decl_pos;

        emit_program(ctx);

        EXPECT_STR_CONTAINS(ctx->decls->data,
            "PGY_HASHMAP_DEFINE(List_String, PgyList_String)");
        EXPECT_STR_CONTAINS(ctx->decls->data,
            "PgyHashMap_List_String BuildBuckets(void);");
        EXPECT_STR_CONTAINS(ctx->decls->data,
            "char* RenderBuckets(PgyHashMap_List_String buckets);");

        map_define_pos = strstr(ctx->decls->data,
            "PGY_HASHMAP_DEFINE(List_String, PgyList_String)");
        build_decl_pos = strstr(ctx->decls->data,
            "PgyHashMap_List_String BuildBuckets(void);");
        EXPECT(map_define_pos != NULL && build_decl_pos != NULL && map_define_pos < build_decl_pos);

        transpiler_ctx_destroy(ctx);
        mir_destroy(mir);
        rir_destroy(rir);
        hir_destroy(hir);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("enum variant identifier emits qualified C enum constant");
    {
        ASTNode *enum_decl = calloc(1, sizeof(ASTNode));
        enum_decl->type = AST_ENUM_DECL;
        enum_decl->data.enum_decl.name = pergyra_strdup("Color");
        enum_decl->data.enum_decl.variant_count = 2;
        enum_decl->data.enum_decl.variants = calloc(2, sizeof(char *));
        enum_decl->data.enum_decl.variants[0] = pergyra_strdup("Red");
        enum_decl->data.enum_decl.variants[1] = pergyra_strdup("Blue");

        ASTNode *fn_body = ast_create_block();
        ast_add_statement(fn_body,
            make_let("c", make_type_node("Color"),
                make_identifier("Red", 2), 2));

        ASTNode *fn = calloc(1, sizeof(ASTNode));
        fn->type = AST_FUNC_DECL;
        fn->data.func_decl.name = "Main";
        fn->data.func_decl.return_type = make_type_node("Void");
        fn->data.func_decl.body = fn_body;
        fn->data.func_decl.param_count = 0;

        ASTNode *stmts[2] = { enum_decl, fn };
        ASTNode *prog = make_program(stmts, 2);
        HIRProgram *hir = NULL;
        RIRProgram *rir = NULL;
        MIRProgram *mir = lower_program_to_mir(prog, &hir, &rir);
        ctx = transpiler_ctx_create();
        emit_program(ctx);

        EXPECT_STR_CONTAINS(ctx->out->data, "Color_Red");
        transpiler_ctx_destroy(ctx);
        mir_destroy(mir);
        rir_destroy(rir);
        hir_destroy(hir);
    }

    TEST("operator overload dispatch uses operator_add_Type");
    {
        FuncParam opa, opb, maina, mainb;
        memset(&opa, 0, sizeof(opa)); opa.name = "a"; opa.type = make_type_node("Vec2");
        memset(&opb, 0, sizeof(opb)); opb.name = "b"; opb.type = make_type_node("Vec2");
        memset(&maina, 0, sizeof(maina)); maina.name = "a"; maina.type = make_type_node("Vec2");
        memset(&mainb, 0, sizeof(mainb)); mainb.name = "b"; mainb.type = make_type_node("Vec2");
        FuncParam *op_params[2] = { &opa, &opb };
        FuncParam *main_params[2] = { &maina, &mainb };

        ASTNode *op_fn = calloc(1, sizeof(ASTNode));
        op_fn->type = AST_FUNC_DECL;
        op_fn->data.func_decl.name = "operator_add_Vec2";
        op_fn->data.func_decl.params = op_params;
        op_fn->data.func_decl.param_count = 2;
        op_fn->data.func_decl.return_type = make_type_node("Vec2");
        ASTNode *op_body = ast_create_block();
        ast_add_statement(op_body, make_return(make_identifier("a", 1), 1));
        op_fn->data.func_decl.body = op_body;

        ASTNode *main_fn = calloc(1, sizeof(ASTNode));
        main_fn->type = AST_FUNC_DECL;
        main_fn->data.func_decl.name = "Main";
        main_fn->data.func_decl.params = main_params;
        main_fn->data.func_decl.param_count = 2;
        main_fn->data.func_decl.return_type = make_type_node("Vec2");
        ASTNode *sum = ast_create_binary(make_identifier("a", 2),
            (Token){ .type = TOKEN_PLUS }, make_identifier("b", 2));
        ASTNode *main_body = ast_create_block();
        ast_add_statement(main_body, make_return(sum, 2));
        main_fn->data.func_decl.body = main_body;

        ASTNode *stmts[2] = { op_fn, main_fn };
        ASTNode *prog = make_program(stmts, 2);
        HIRProgram *hir = NULL;
        RIRProgram *rir = NULL;
        MIRProgram *mir = lower_program_to_mir(prog, &hir, &rir);
        ctx = transpiler_ctx_create();
        emit_program(ctx);

        EXPECT_STR_CONTAINS(ctx->out->data, "operator_add_Vec2(");
        transpiler_ctx_destroy(ctx);
        mir_destroy(mir);
        rir_destroy(rir);
        hir_destroy(hir);
    }

    TEST("operator overload dispatch keeps left-type suffix stable across nested inferred calls");
    {
        FuncParam opa, opb;
        ASTNode *op_fn;
        ASTNode *make_vec_fn;
        ASTNode *make_count_fn;
        ASTNode *main_fn;
        ASTNode *main_body;
        ASTNode *sum;
        ASTNode *left_call;
        ASTNode *right_call;
        ASTNode *stmts[4];
        HIRProgram *hir = NULL;
        RIRProgram *rir = NULL;
        MIRProgram *mir;

        memset(&opa, 0, sizeof(opa));
        memset(&opb, 0, sizeof(opb));
        opa.name = "a";
        opa.type = make_type_node("Vec2");
        opb.name = "b";
        opb.type = make_type_node("Int");

        op_fn = calloc(1, sizeof(ASTNode));
        op_fn->type = AST_FUNC_DECL;
        op_fn->data.func_decl.name = "operator_add_Vec2";
        op_fn->data.func_decl.params = calloc(2, sizeof(FuncParam *));
        op_fn->data.func_decl.params[0] = &opa;
        op_fn->data.func_decl.params[1] = &opb;
        op_fn->data.func_decl.param_count = 2;
        op_fn->data.func_decl.return_type = make_type_node("Vec2");
        op_fn->data.func_decl.body = ast_create_block();
        ast_add_statement(op_fn->data.func_decl.body,
            make_return(make_identifier("a", 1), 1));

        make_vec_fn = calloc(1, sizeof(ASTNode));
        make_vec_fn->type = AST_FUNC_DECL;
        make_vec_fn->data.func_decl.name = "MakeVec";
        make_vec_fn->data.func_decl.return_type = make_type_node("Vec2");
        make_vec_fn->data.func_decl.body = ast_create_block();
        ast_add_statement(make_vec_fn->data.func_decl.body,
            make_return(make_identifier("seedVec", 2), 2));

        make_count_fn = calloc(1, sizeof(ASTNode));
        make_count_fn->type = AST_FUNC_DECL;
        make_count_fn->data.func_decl.name = "MakeCount";
        make_count_fn->data.func_decl.return_type = make_type_node("Int");
        make_count_fn->data.func_decl.body = ast_create_block();
        ast_add_statement(make_count_fn->data.func_decl.body,
            make_return(make_number(7, 3), 3));

        left_call = make_call("MakeVec", NULL, 0, 4);
        right_call = make_call("MakeCount", NULL, 0, 4);
        sum = ast_create_binary(left_call, (Token){ .type = TOKEN_PLUS }, right_call);

        main_fn = calloc(1, sizeof(ASTNode));
        main_fn->type = AST_FUNC_DECL;
        main_fn->data.func_decl.name = "Main";
        main_fn->data.func_decl.return_type = make_type_node("Vec2");
        main_body = ast_create_block();
        ast_add_statement(main_body, make_return(sum, 4));
        main_fn->data.func_decl.body = main_body;

        stmts[0] = op_fn;
        stmts[1] = make_vec_fn;
        stmts[2] = make_count_fn;
        stmts[3] = main_fn;

        mir = lower_program_to_mir(make_program(stmts, 4), &hir, &rir);
        ctx = transpiler_ctx_create();
        emit_program(ctx);

        EXPECT_STR_CONTAINS(ctx->out->data, "operator_add_Vec2(");
        EXPECT_STR_NOT_CONTAINS(ctx->out->data, "operator_add_Int(");

        transpiler_ctx_destroy(ctx);
        mir_destroy(mir);
        rir_destroy(rir);
        hir_destroy(hir);
    }

    TEST("role ability Add emits operator_add_Type alias");
    {
        FuncParam rhs_param, a_param, b_param;
        memset(&rhs_param, 0, sizeof(rhs_param));
        memset(&a_param, 0, sizeof(a_param));
        memset(&b_param, 0, sizeof(b_param));
        rhs_param.name = "other";
        rhs_param.type = make_type_node("Int");
        a_param.name = "a";
        a_param.type = make_type_node("Int");
        b_param.name = "b";
        b_param.type = make_type_node("Int");

        ASTNode *role_method = calloc(1, sizeof(ASTNode));
        role_method->type = AST_FUNC_DECL;
        role_method->data.func_decl.name = "Add";
        role_method->data.func_decl.params = calloc(1, sizeof(FuncParam *));
        role_method->data.func_decl.params[0] = &rhs_param;
        role_method->data.func_decl.param_count = 1;
        role_method->data.func_decl.return_type = make_type_node("Int");
        ASTNode *role_body = ast_create_block();
        ast_add_statement(role_body, make_return(make_number(123, 1), 1));
        role_method->data.func_decl.body = role_body;

        ASTNode *impl = calloc(1, sizeof(ASTNode));
        impl->type = AST_IMPL_ABILITY;
        impl->data.impl_ability.ability_ref = ast_create_type("Arithmetic");
        impl->data.impl_ability.methods = calloc(1, sizeof(ASTNode *));
        impl->data.impl_ability.methods[0] = role_method;
        impl->data.impl_ability.method_count = 1;

        ASTNode *role = calloc(1, sizeof(ASTNode));
        role->type = AST_ROLE_DECL;
        role->data.role_decl.name = "IntMath";
        role->data.role_decl.for_type = make_type_node("Int");
        role->data.role_decl.impl_abilities = calloc(1, sizeof(ASTNode *));
        role->data.role_decl.impl_abilities[0] = impl;
        role->data.role_decl.impl_count = 1;

        ASTNode *main_fn = calloc(1, sizeof(ASTNode));
        main_fn->type = AST_FUNC_DECL;
        main_fn->data.func_decl.name = "Main";
        main_fn->data.func_decl.params = calloc(2, sizeof(FuncParam *));
        main_fn->data.func_decl.params[0] = &a_param;
        main_fn->data.func_decl.params[1] = &b_param;
        main_fn->data.func_decl.param_count = 2;
        main_fn->data.func_decl.return_type = make_type_node("Int");
        ASTNode *sum = ast_create_binary(make_identifier("a", 2),
            (Token){ .type = TOKEN_PLUS }, make_identifier("b", 2));
        ASTNode *main_body = ast_create_block();
        ast_add_statement(main_body, make_return(sum, 2));
        main_fn->data.func_decl.body = main_body;

        ASTNode *stmts[2] = { role, main_fn };
        ASTNode *prog = make_program(stmts, 2);
        HIRProgram *hir = NULL;
        RIRProgram *rir = NULL;
        MIRProgram *mir = lower_program_to_mir(prog, &hir, &rir);
        ctx = transpiler_ctx_create();
        emit_program(ctx);

        EXPECT_STR_CONTAINS(ctx->out->data, "operator_add_Int(int32_t lhs, int32_t other)");
        transpiler_ctx_destroy(ctx);
        mir_destroy(mir);
        rir_destroy(rir);
        hir_destroy(hir);
    }
}
