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

    TEST("StringIndexOf maps to runtime helper");
    {
        ASTNode *args[2] = { make_string_lit("hello", 1), make_string_lit("ell", 1) };
        ASTNode *call = make_call("StringIndexOf", args, 2, 1);
        ASTNode *node = make_let("idx", make_type_node("Int"), call, 1);
        const char *out = emit_stmt_to_str(node, &ctx);
        EXPECT_STR_CONTAINS(out, "StringIndexOf(\"hello\", \"ell\")");
        transpiler_ctx_destroy(ctx);
    }

    TEST("FileExists maps to runtime helper");
    {
        ASTNode *args[1] = { make_string_lit("io.txt", 1) };
        ASTNode *call = make_call("FileExists", args, 1, 1);
        ASTNode *node = make_let("ok", make_type_node("Bool"), call, 1);
        const char *out = emit_stmt_to_str(node, &ctx);
        EXPECT_STR_CONTAINS(out, "pgy_file_exists(\"io.txt\")");
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
        MIRProgram *mir = lower_program_to_mir_strict(program, &hir, &rir);
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

    TEST("List builtin rejects non-addressable storage expression");
    {
        ASTNode *list_call = make_call("MakeList", NULL, 0, 1);
        ASTNode *args[2] = { list_call, make_number(7, 1) };
        ASTNode *call = make_call("ListPush", args, 2, 1);
        ctx = transpiler_ctx_create();
        char *result = emit_expression(call, ctx);

        EXPECT(result != NULL);
        EXPECT_STR_NOT_CONTAINS(result, "&MakeList");
        EXPECT(ctx->backend_error != NULL);
        EXPECT_STR_CONTAINS(ctx->backend_error,
            "requires addressable List storage");

        free(result);
        ast_destroy(call);
        transpiler_ctx_destroy(ctx);
    }

    TEST("Array mutator rejects non-addressable storage expression");
    {
        ASTNode *array_call = make_call("MakeArray", NULL, 0, 1);
        ASTNode *args[2] = { array_call, make_number(7, 1) };
        ASTNode *call = make_call("ArrayPush", args, 2, 1);
        ctx = transpiler_ctx_create();
        char *result = emit_expression(call, ctx);

        EXPECT(result != NULL);
        EXPECT_STR_NOT_CONTAINS(result, "&MakeArray");
        EXPECT(ctx->backend_error != NULL);
        EXPECT_STR_CONTAINS(ctx->backend_error,
            "requires addressable Array storage");

        free(result);
        ast_destroy(call);
        transpiler_ctx_destroy(ctx);
    }

    TEST("StringJoin rejects non-addressable array expression");
    {
        ASTNode *array_call = make_call("StringSplit", NULL, 0, 1);
        ASTNode *args[2] = { array_call, make_string_lit(",", 1) };
        ASTNode *call = make_call("StringJoin", args, 2, 1);
        ctx = transpiler_ctx_create();
        char *result = emit_expression(call, ctx);

        EXPECT(result != NULL);
        EXPECT_STR_NOT_CONTAINS(result, "&StringSplit");
        EXPECT(ctx->backend_error != NULL);
        EXPECT_STR_CONTAINS(ctx->backend_error,
            "requires addressable Array storage");

        free(result);
        ast_destroy(call);
        transpiler_ctx_destroy(ctx);
    }

    TEST("Map builtin rejects non-addressable storage expression");
    {
        ASTNode *map_call = make_call("MakeMap", NULL, 0, 1);
        ASTNode *args[2] = { map_call, make_string_lit("hero", 1) };
        ASTNode *call = make_call("MapGet", args, 2, 1);
        ctx = transpiler_ctx_create();
        char *result = emit_expression(call, ctx);

        EXPECT(result != NULL);
        EXPECT_STR_NOT_CONTAINS(result, "&MakeMap");
        EXPECT(ctx->backend_error != NULL);
        EXPECT_STR_CONTAINS(ctx->backend_error,
            "requires addressable HashMap storage");

        free(result);
        ast_destroy(call);
        transpiler_ctx_destroy(ctx);
    }

    TEST("FSM builtin rejects non-addressable storage expression");
    {
        ASTNode *fsm_call = make_call("MakeFsm", NULL, 0, 1);
        ASTNode *args[1] = { fsm_call };
        ASTNode *call = make_call("FsmCurrent", args, 1, 1);
        ctx = transpiler_ctx_create();
        char *result = emit_expression(call, ctx);

        EXPECT(result != NULL);
        EXPECT_STR_NOT_CONTAINS(result, "&MakeFsm");
        EXPECT(ctx->backend_error != NULL);
        EXPECT_STR_CONTAINS(ctx->backend_error,
            "requires addressable FSM storage");

        free(result);
        ast_destroy(call);
        transpiler_ctx_destroy(ctx);
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
        MIRProgram *mir = lower_program_to_mir_strict(program, &hir, &rir);
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
        MIRProgram *mir = lower_program_to_mir_strict(program, &hir, &rir);
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

    TEST("string interpolation lowers through ToString and StringConcat");
    {
        const char *source =
            "subject Event {\n"
            "    let title: String;\n"
            "    let day: Int;\n"
            "}\n"
            "func Render(event: Event) -> String {\n"
            "    return \"Day ${event.day}: ${event.title}\";\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = lexer != NULL ? parser_create(lexer) : NULL;
        ASTNode *program = parser != NULL ? parser_parse_program(parser) : NULL;
        HIRProgram *hir = NULL;
        RIRProgram *rir = NULL;
        MIRProgram *mir = lower_program_to_mir_strict(program, &hir, &rir);
        ctx = transpiler_ctx_create();

        emit_program(ctx);

        EXPECT_STR_CONTAINS(ctx->out->data, "StringConcat(");
        EXPECT_STR_CONTAINS(ctx->out->data, "pgy_int_to_string(event->day)");
        EXPECT_STR_CONTAINS(ctx->out->data, "event->title");

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
        MIRProgram *mir = lower_program_to_mir_strict(program, &hir, &rir);
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

    TEST("generic function pointer declarators use concrete ctx bindings");
    {
        ASTNode *handler_type = ast_create_event_handler_type();
        char *ctx_decl = NULL;
        char *legacy_decl = NULL;
        ctx = transpiler_ctx_create();

        handler_type->data.event_handler_type.param_types =
            calloc(1, sizeof(ASTNode *));
        handler_type->data.event_handler_type.param_count = 1;
        handler_type->data.event_handler_type.param_capacity = 1;
        handler_type->data.event_handler_type.param_types[0] =
            make_type_node("T");
        handler_type->data.event_handler_type.return_type =
            make_type_node("T");

        strcpy(ctx->generic_bindings[0].name, "T");
        strcpy(ctx->generic_bindings[0].concrete_type, "Long");
        ctx->generic_binding_count = 1;

        ctx_decl = pergyra_ast_typed_declarator_in_ctx(ctx, handler_type, "f");
        legacy_decl = pergyra_ast_typed_declarator(handler_type, "f");

        EXPECT(strcmp(ctx_decl, "int64_t (*f)(int64_t)") == 0);
        EXPECT(strstr(ctx_decl, "T") == NULL);
        EXPECT(strcmp(ctx_decl, legacy_decl) != 0);

        free(ctx_decl);
        free(legacy_decl);
        transpiler_ctx_destroy(ctx);
        ast_destroy(handler_type);
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
        MIRProgram *mir = lower_program_to_mir_strict(program, &hir, &rir);
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
        MIRProgram *mir = lower_program_to_mir_strict(program, &hir, &rir);
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
        MIRProgram *mir = lower_program_to_mir_strict(program, &hir, &rir);
        ctx = transpiler_ctx_create();

        emit_program(ctx);

        EXPECT_STR_CONTAINS(ctx->out->data, "pgy_list_new_Event()");
        EXPECT_STR_CONTAINS(ctx->out->data, "size_t _pgy_idx_");
        EXPECT_STR_CONTAINS(ctx->out->data, "Event _pgy_for_event");
        EXPECT_STR_CONTAINS(ctx->out->data, ".data[_pgy_idx_event");
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
        MIRProgram *mir = lower_program_to_mir_strict(program, &hir, &rir);
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
        MIRProgram *mir = lower_program_to_mir_strict(program, &hir, &rir);
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
        MIRProgram *mir = lower_program_to_mir_strict(program, &hir, &rir);
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
        MIRProgram *mir = lower_program_to_mir_strict(program, &hir, &rir);
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
        MIRProgram *mir = lower_program_to_mir_strict(program, &hir, &rir);
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
        MIRProgram *mir = lower_program_to_mir_strict(program, &hir, &rir);
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
        MIRProgram *mir = lower_program_to_mir_strict(program, &hir, &rir);
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
        FuncParam **op_params = calloc(2, sizeof(FuncParam *));
        FuncParam **main_params = calloc(2, sizeof(FuncParam *));
        op_params[0] = make_func_param("a", "Vec2");
        op_params[1] = make_func_param("b", "Vec2");
        main_params[0] = make_func_param("a", "Vec2");
        main_params[1] = make_func_param("b", "Vec2");

        ASTNode *op_fn = calloc(1, sizeof(ASTNode));
        op_fn->type = AST_FUNC_DECL;
        op_fn->data.func_decl.name = pergyra_strdup("operator_add_Vec2");
        op_fn->data.func_decl.params = op_params;
        op_fn->data.func_decl.param_count = 2;
        op_fn->data.func_decl.return_type = make_type_node("Vec2");
        ASTNode *op_body = ast_create_block();
        ast_add_statement(op_body, make_return(make_identifier("a", 1), 1));
        op_fn->data.func_decl.body = op_body;

        ASTNode *main_fn = calloc(1, sizeof(ASTNode));
        main_fn->type = AST_FUNC_DECL;
        main_fn->data.func_decl.name = pergyra_strdup("Main");
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
        ast_destroy(prog);
    }

    TEST("operator overload dispatch keeps left-type suffix stable across nested inferred calls");
    {
        ASTNode *op_fn;
        ASTNode *make_vec_fn;
        ASTNode *make_count_fn;
        ASTNode *main_fn;
        ASTNode *main_body;
        ASTNode *sum;
        ASTNode *left_call;
        ASTNode *right_call;
        ASTNode *stmts[4];
        ASTNode *prog;
        HIRProgram *hir = NULL;
        RIRProgram *rir = NULL;
        MIRProgram *mir;

        op_fn = calloc(1, sizeof(ASTNode));
        op_fn->type = AST_FUNC_DECL;
        op_fn->data.func_decl.name = pergyra_strdup("operator_add_Vec2");
        op_fn->data.func_decl.params = calloc(2, sizeof(FuncParam *));
        op_fn->data.func_decl.params[0] = make_func_param("a", "Vec2");
        op_fn->data.func_decl.params[1] = make_func_param("b", "Int");
        op_fn->data.func_decl.param_count = 2;
        op_fn->data.func_decl.return_type = make_type_node("Vec2");
        op_fn->data.func_decl.body = ast_create_block();
        ast_add_statement(op_fn->data.func_decl.body,
            make_return(make_identifier("a", 1), 1));

        make_vec_fn = calloc(1, sizeof(ASTNode));
        make_vec_fn->type = AST_FUNC_DECL;
        make_vec_fn->data.func_decl.name = pergyra_strdup("MakeVec");
        make_vec_fn->data.func_decl.return_type = make_type_node("Vec2");
        make_vec_fn->data.func_decl.body = ast_create_block();
        ast_add_statement(make_vec_fn->data.func_decl.body,
            make_return(make_identifier("seedVec", 2), 2));

        make_count_fn = calloc(1, sizeof(ASTNode));
        make_count_fn->type = AST_FUNC_DECL;
        make_count_fn->data.func_decl.name = pergyra_strdup("MakeCount");
        make_count_fn->data.func_decl.return_type = make_type_node("Int");
        make_count_fn->data.func_decl.body = ast_create_block();
        ast_add_statement(make_count_fn->data.func_decl.body,
            make_return(make_number(7, 3), 3));

        left_call = make_call("MakeVec", NULL, 0, 4);
        right_call = make_call("MakeCount", NULL, 0, 4);
        sum = ast_create_binary(left_call, (Token){ .type = TOKEN_PLUS }, right_call);

        main_fn = calloc(1, sizeof(ASTNode));
        main_fn->type = AST_FUNC_DECL;
        main_fn->data.func_decl.name = pergyra_strdup("Main");
        main_fn->data.func_decl.return_type = make_type_node("Vec2");
        main_body = ast_create_block();
        ast_add_statement(main_body, make_return(sum, 4));
        main_fn->data.func_decl.body = main_body;

        stmts[0] = op_fn;
        stmts[1] = make_vec_fn;
        stmts[2] = make_count_fn;
        stmts[3] = main_fn;

        prog = make_program(stmts, 4);
        mir = lower_program_to_mir(prog, &hir, &rir);
        ctx = transpiler_ctx_create();
        emit_program(ctx);

        EXPECT_STR_CONTAINS(ctx->out->data, "operator_add_Vec2(");
        EXPECT_STR_NOT_CONTAINS(ctx->out->data, "operator_add_Int(");

        transpiler_ctx_destroy(ctx);
        mir_destroy(mir);
        rir_destroy(rir);
        hir_destroy(hir);
        ast_destroy(prog);
    }

    TEST("role ability Add emits operator_add_Type alias");
    {
        const char *source =
            "ability Arithmetic { func Add(other: Int) -> Int; }\n"
            "role IntMath for Int {\n"
            "    impl ability Arithmetic {\n"
            "        func Add(other: Int) -> Int { return 123; }\n"
            "    }\n"
            "}\n"
            "func Main() -> Int { return 1 + 2; }\n";
        ASTNode *program = NULL;
        HIRProgram *hir = NULL;
        RIRProgram *rir = NULL;
        MIRProgram *mir = NULL;
        bool ok = lower_pipeline_from_source(source, &program, &hir, &rir, &mir);
        ctx = transpiler_ctx_create();

        EXPECT(ok);
        ctx->mir = mir;
        emit_program(ctx);

        EXPECT_STR_CONTAINS(ctx->out->data, "operator_add_Int");
        EXPECT_STR_CONTAINS(ctx->out->data, "int32_t lhs, int32_t other");
        transpiler_ctx_destroy(ctx);
        mir_destroy(mir);
        rir_destroy(rir);
        hir_destroy(hir);
        ast_destroy(program);
    }
}
