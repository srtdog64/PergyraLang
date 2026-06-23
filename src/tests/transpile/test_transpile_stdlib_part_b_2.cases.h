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

        ctx->mir = mir;
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

        ctx->mir = mir;
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

        ctx->mir = mir;
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

        ctx->mir = mir;
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

        ctx->mir = mir;
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

        ctx->mir = mir;
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

        ctx->mir = mir;
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

        ctx->mir = mir;
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

        ctx->mir = mir;
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
        ctx->mir = mir;
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
        ctx->mir = mir;
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
        const char *source =
            "struct Vec2 { x: Int; y: Int; }\n"
            "func operator_add_Vec2(a: Vec2, b: Int) -> Vec2 { return a; }\n"
            "func MakeVec() -> Vec2 { return Vec2(1, 2); }\n"
            "func MakeCount() -> Int { return 7; }\n"
            "func Main() -> Vec2 { return MakeVec() + MakeCount(); }\n";
        ASTNode *prog = NULL;
        HIRProgram *hir = NULL;
        RIRProgram *rir = NULL;
        MIRProgram *mir = NULL;
        bool ok = lower_pipeline_from_source(source, &prog, &hir, &rir, &mir);

        EXPECT(ok);
        ctx = transpiler_ctx_create();
        ctx->mir = mir;
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
