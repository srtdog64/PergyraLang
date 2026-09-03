static void
test_program_emit_head(void)
{
    printf("\n[program_emit]\n");

    TEST("program header contains #include \"pgy_runtime.h\"");
    {
        ASTNode *stmts[0];
        ASTNode *prog = make_program(stmts, 0);
        HIRProgram *hir = NULL;
        RIRProgram *rir = NULL;
        MIRProgram *mir = lower_program_to_mir(prog, &hir, &rir);
        TranspilerCtx *ctx = transpiler_ctx_create();
        ctx->mir = mir;
        emit_program(ctx);
        EXPECT_STR_CONTAINS(ctx->out->data, "#include \"pgy_runtime.h\"");
        transpiler_ctx_destroy(ctx);
        mir_destroy(mir);
        rir_destroy(rir);
        hir_destroy(hir);
        ast_destroy(prog);
    }

    TEST("function emitted at top level with correct signature");
    {
        /* func Add(a: Int, b: Int) -> Int { return a + b } */
        FuncParam **params = calloc(2, sizeof(FuncParam *));
        params[0] = make_func_param("a", "Int");
        params[1] = make_func_param("b", "Int");

        ASTNode *fn = calloc(1, sizeof(ASTNode));
        fn->type = AST_FUNC_DECL;
        fn->data.func_decl.name        = pergyra_strdup("Add");
        fn->data.func_decl.params      = params;
        fn->data.func_decl.param_count = 2;
        fn->data.func_decl.return_type = make_type_node("Int");
        fn->data.func_decl.body        = NULL;

        ASTNode *stmts[1] = { fn };
        ASTNode *prog = make_program(stmts, 1);
        HIRProgram *hir = NULL;
        RIRProgram *rir = NULL;
        MIRProgram *mir = lower_program_to_mir(prog, &hir, &rir);

        TranspilerCtx *ctx = transpiler_ctx_create();
        ctx->mir = mir;
        emit_program(ctx);

        EXPECT_STR_CONTAINS(ctx->out->data, "int32_t Add(int32_t a, int32_t b)");
        transpiler_ctx_destroy(ctx);
        mir_destroy(mir);
        rir_destroy(rir);
        hir_destroy(hir);
        ast_destroy(prog);
    }

    TEST("generic function call emits concrete specialization in C");
    {
        FuncParam **identity_params = calloc(1, sizeof(FuncParam *));
        identity_params[0] = make_func_param("x", "T");

        ASTNode *identity_return_stmts[1] = { make_return(make_identifier("x", 1), 1) };
        ASTNode *identity = calloc(1, sizeof(ASTNode));
        identity->type = AST_FUNC_DECL;
        identity->data.func_decl.name = pergyra_strdup("Identity");
        identity->data.func_decl.params = identity_params;
        identity->data.func_decl.param_count = 1;
        identity->data.func_decl.return_type = make_type_node("T");
        identity->data.func_decl.body = make_block(identity_return_stmts, 1);
        identity->data.func_decl.generic_params = make_generic_params1("T");

        ASTNode *sum = make_let("sum", make_type_node("Int"), make_number(7, 1), 1);
        ASTNode *identity_args[1] = { make_identifier("sum", 1) };
        ASTNode *echoed = make_let("echoed", make_type_node("Int"),
                                   make_call("Identity", identity_args, 1, 1), 1);
        ASTNode *log_args[1] = { make_identifier("echoed", 1) };
        ASTNode *main_stmts[3] = { sum, echoed, make_call("Log", log_args, 1, 1) };

        ASTNode *main = calloc(1, sizeof(ASTNode));
        main->type = AST_FUNC_DECL;
        main->data.func_decl.name = pergyra_strdup("Main");
        main->data.func_decl.return_type = make_type_node("Void");
        main->data.func_decl.body = make_block(main_stmts, 3);

        ASTNode *stmts[2] = { identity, main };
        ASTNode *prog = make_program(stmts, 2);
        HIRProgram *hir = NULL;
        RIRProgram *rir = NULL;
        MIRProgram *mir = lower_program_to_mir(prog, &hir, &rir);

        TranspilerCtx *ctx = transpiler_ctx_create();
        ctx->mir = mir;
        emit_program(ctx);

        EXPECT_STR_CONTAINS(ctx->out->data, "int32_t Identity_Int(int32_t x)");
        EXPECT_STR_CONTAINS(ctx->out->data, "Identity_Int(");

        transpiler_ctx_destroy(ctx);
        mir_destroy(mir);
        rir_destroy(rir);
        hir_destroy(hir);
        ast_destroy(prog);
    }

    TEST("spawn emits wrapper-based task launch");
    {
        FuncParam **identity_params = calloc(1, sizeof(FuncParam *));
        identity_params[0] = make_func_param("x", "Int");

        ASTNode *identity_return_stmts[1] = { make_return(make_identifier("x", 1), 1) };
        ASTNode *identity = calloc(1, sizeof(ASTNode));
        identity->type = AST_FUNC_DECL;
        identity->data.func_decl.name = pergyra_strdup("IdentityInt");
        identity->data.func_decl.params = identity_params;
        identity->data.func_decl.param_count = 1;
        identity->data.func_decl.return_type = make_type_node("Int");
        identity->data.func_decl.body = make_block(identity_return_stmts, 1);

        ASTNode *spawn_args[1] = { make_number(42, 1) };
        ASTNode *call = make_call("IdentityInt", spawn_args, 1, 1);
        ASTNode *spawn = calloc(1, sizeof(ASTNode));
        spawn->type = AST_SPAWN_EXPR;
        spawn->data.spawn_expr.function = call;

        ASTNode *main_body = ast_create_block();
        ast_add_statement(main_body, ast_create_await_expression(spawn));
        ASTNode *main = calloc(1, sizeof(ASTNode));
        main->type = AST_FUNC_DECL;
        main->data.func_decl.name = pergyra_strdup("Main");
        main->data.func_decl.return_type = make_type_node("Void");
        main->data.func_decl.body = main_body;
        main->is_async_decl = true;

        ASTNode *stmts[2] = { identity, main };
        ASTNode *prog = make_program(stmts, 2);
        HIRProgram *hir = NULL;
        RIRProgram *rir = NULL;
        MIRProgram *mir = lower_program_to_mir(prog, &hir, &rir);

        TranspilerCtx *ctx = transpiler_ctx_create();
        ctx->mir = mir;
        /* The spawn emitter consumes the AIR-carried lane fact per site. */
        static PgySpawnLaneFactRow spawn_lane_rows[1];
        static PgySpawnLanePlan spawn_lane_plan;
        spawn_lane_rows[0].source_stable_id = ast_node_stable_id(spawn);
        spawn_lane_rows[0].lane = PGY_LANE_WORKER_POOL;
        spawn_lane_plan.revision = PGY_SPAWN_LANE_PLAN_REVISION;
        spawn_lane_plan.rows = spawn_lane_rows;
        spawn_lane_plan.row_count = 1;
        spawn_lane_plan.verified = true;
        ctx->spawn_lane_plan = &spawn_lane_plan;
        emit_program(ctx);

        EXPECT_STR_CONTAINS(ctx->out->data, "IdentityInt");
        EXPECT_STR_CONTAINS(ctx->out->data, "pgy_lane_spawn_dispatch");

        transpiler_ctx_destroy(ctx);
        mir_destroy(mir);
        rir_destroy(rir);
        hir_destroy(hir);
        ast_destroy(prog);
    }

    TEST("parallel block emits lane spawn / lane await per task");
    {
        ASTNode **tasks = calloc(2, sizeof(ASTNode *));
        tasks[0] = make_call("A", NULL, 0, 1);
        tasks[1] = make_call("B", NULL, 0, 1);
        ASTNode *par = calloc(1, sizeof(ASTNode));
        par->type = AST_PARALLEL_BLOCK;
        par->data.parallel.tasks      = tasks;
        par->data.parallel.task_count = 2;
        ASTNode *fnA = calloc(1, sizeof(ASTNode));
        fnA->type = AST_FUNC_DECL;
        fnA->data.func_decl.name = pergyra_strdup("A");
        fnA->data.func_decl.return_type = make_type_node("Void");
        fnA->data.func_decl.body = ast_create_block();
        ASTNode *fnB = calloc(1, sizeof(ASTNode));
        fnB->type = AST_FUNC_DECL;
        fnB->data.func_decl.name = pergyra_strdup("B");
        fnB->data.func_decl.return_type = make_type_node("Void");
        fnB->data.func_decl.body = ast_create_block();

        ASTNode *main_body = ast_create_block();
        ast_add_statement(main_body, par);
        ASTNode *main = calloc(1, sizeof(ASTNode));
        main->type = AST_FUNC_DECL;
        main->data.func_decl.name = pergyra_strdup("Main");
        main->data.func_decl.return_type = make_type_node("Void");
        main->data.func_decl.body = main_body;
        main->data.func_decl.param_count = 0;

        ASTNode *stmts[3] = { fnA, fnB, main };
        ASTNode *prog     = make_program(stmts, 3);
        HIRProgram *hir = NULL;
        RIRProgram *rir = NULL;
        MIRProgram *mir = lower_program_to_mir(prog, &hir, &rir);

        TranspilerCtx *ctx = transpiler_ctx_create();
        ctx->mir = mir;
        emit_program(ctx);

        EXPECT_STR_CONTAINS(ctx->out->data, "pgy_lane_spawn_dispatch");
        EXPECT_STR_CONTAINS(ctx->out->data, "pgy_lane_await");
        EXPECT_STR_CONTAINS(ctx->out->data, "_pgy_par_");
        transpiler_ctx_destroy(ctx);
        mir_destroy(mir);
        rir_destroy(rir);
        hir_destroy(hir);
        ast_destroy(prog);
    }

    TEST("struct emits typedef struct and method function");
    {
        ASTNode *st = calloc(1, sizeof(ASTNode));
        st->type = AST_CLASS_DECL;
        st->data.class_decl.name = pergyra_strdup("Vec3");
        st->data.class_decl.is_struct = true;

        ClassField **fields = calloc(3, sizeof(ClassField *));
        fields[0] = make_class_field("x", "Float");
        fields[1] = make_class_field("y", "Float");
        fields[2] = make_class_field("z", "Float");
        st->data.class_decl.fields = fields;
        st->data.class_decl.field_count = 3;

        ASTNode *method = calloc(1, sizeof(ASTNode));
        method->type = AST_FUNC_DECL;
        method->data.func_decl.name = pergyra_strdup("Length");
        method->data.func_decl.params = NULL;
        method->data.func_decl.param_count = 0;
        method->data.func_decl.return_type = make_type_node("Float");
        method->data.func_decl.body = NULL;

        ASTNode **methods = calloc(1, sizeof(ASTNode *));
        methods[0] = method;
        st->data.class_decl.methods = methods;
        st->data.class_decl.method_count = 1;

        ASTNode *stmts[1] = { st };
        ASTNode *prog = make_program(stmts, 1);
        HIRProgram *hir = NULL;
        RIRProgram *rir = NULL;
        MIRProgram *mir = lower_program_to_mir(prog, &hir, &rir);

        TranspilerCtx *ctx = transpiler_ctx_create();
        ctx->mir = mir;
        emit_program(ctx);

        EXPECT_STR_CONTAINS(ctx->out->data, "typedef struct Vec3");
        EXPECT_STR_CONTAINS(ctx->out->data, "float x;");
        EXPECT_STR_CONTAINS(ctx->out->data, "float y;");
        EXPECT_STR_CONTAINS(ctx->out->data, "float z;");
        EXPECT_STR_CONTAINS(ctx->out->data, "float\nVec3_Length(Vec3 self)");

        transpiler_ctx_destroy(ctx);
        mir_destroy(mir);
        rir_destroy(rir);
        hir_destroy(hir);
        ast_destroy(prog);
    }

    TEST("subject method call lowers to self-cell call");
    {
        const char *source =
            "subject Vec2 {\n"
            "    let x: Int;\n"
            "    func Length(self) -> Int {\n"
            "        return self.x;\n"
            "    }\n"
            "}\n"
            "func Main() -> Void {\n"
            "    let v: Vec2 = Vec2();\n"
            "    Log(v.Length());\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        HIRProgram *hir = NULL;
        RIRProgram *rir = NULL;
        MIRProgram *mir = lower_program_to_mir_strict(program, &hir, &rir);
        TranspilerCtx *ctx = transpiler_ctx_create();

        ctx->mir = mir;
        emit_program(ctx);

        EXPECT_STR_CONTAINS(ctx->out->data, "Vec2_Length(&");
        EXPECT_STR_CONTAINS(ctx->out->data, "Vec2 *self");
        EXPECT_STR_CONTAINS(ctx->out->data, "self->x");
        EXPECT_STR_CONTAINS(ctx->out->data, "PGY_BOX_DEFINE(Vec2, Vec2)");

        transpiler_ctx_destroy(ctx);
        mir_destroy(mir);
        rir_destroy(rir);
        hir_destroy(hir);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("class method call lowers by value");
    {
        const char *source =
            "class Vec2 {\n"
            "    let x: Int;\n"
            "    func Length(self) -> Int {\n"
            "        return self.x;\n"
            "    }\n"
            "}\n"
            "func Main() -> Void {\n"
            "    let v: Vec2 = Vec2();\n"
            "    Log(v.Length());\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        HIRProgram *hir = NULL;
        RIRProgram *rir = NULL;
        MIRProgram *mir = lower_program_to_mir_strict(program, &hir, &rir);
        TranspilerCtx *ctx = transpiler_ctx_create();

        ctx->mir = mir;
        emit_program(ctx);

        EXPECT_STR_CONTAINS(ctx->out->data, "Vec2_Length(");
        EXPECT_STR_CONTAINS(ctx->out->data, "Vec2 self");
        EXPECT_STR_CONTAINS(ctx->out->data, "return self.x;");

        transpiler_ctx_destroy(ctx);
        mir_destroy(mir);
        rir_destroy(rir);
        hir_destroy(hir);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("Box<class> handle lowers through explicit Box helpers");
    {
        const char *source =
            "class Vec2 {\n"
            "    let x: Int;\n"
            "    let y: Int;\n"
            "}\n"
            "func MakeVec() -> Box<Vec2> {\n"
            "    return Box(Vec2(1, 2));\n"
            "}\n"
            "func Main() -> Void {\n"
            "    let handle: Box<Vec2> = MakeVec();\n"
            "    Log(BoxGet(handle).x);\n"
            "    BoxSet(handle, Vec2(3, 4));\n"
            "    BoxDrop(handle);\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        HIRProgram *hir = NULL;
        RIRProgram *rir = NULL;
        MIRProgram *mir = lower_program_to_mir_strict(program, &hir, &rir);
        TranspilerCtx *ctx = transpiler_ctx_create();

        ctx->mir = mir;
        emit_program(ctx);

        EXPECT_STR_CONTAINS(ctx->out->data, "PgyBox_Vec2 MakeVec(");
        EXPECT_STR_CONTAINS(ctx->out->data, "pgy_box_new_Vec2(");
        EXPECT_STR_CONTAINS(ctx->out->data, "pgy_box_get_Vec2(");
        EXPECT_STR_CONTAINS(ctx->out->data, "pgy_box_set_Vec2(");
        EXPECT_STR_CONTAINS(ctx->out->data, "pgy_box_drop_Vec2(");

        transpiler_ctx_destroy(ctx);
        mir_destroy(mir);
        rir_destroy(rir);
        hir_destroy(hir);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("class method bare field access lowers through value self");
    {
        const char *source =
            "class Counter {\n"
            "    let count: Int;\n"
            "    func Tick(self, delta: Int) -> Int {\n"
            "        count = count + delta;\n"
            "        return count;\n"
            "    }\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        HIRProgram *hir = NULL;
        RIRProgram *rir = NULL;
        MIRProgram *mir = lower_program_to_mir_strict(program, &hir, &rir);
        TranspilerCtx *ctx = transpiler_ctx_create();

        ctx->mir = mir;
        emit_program(ctx);

        EXPECT_STR_CONTAINS(ctx->out->data, "self.count = (self.count + delta);");
        EXPECT_STR_CONTAINS(ctx->out->data, "return self.count;");

        transpiler_ctx_destroy(ctx);
        mir_destroy(mir);
        rir_destroy(rir);
        hir_destroy(hir);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("subject action bare field access lowers through self cell");
    {
        const char *source =
            "subject Counter {\n"
            "    let count: Int;\n"
            "    action Tick(self, delta: Int) -> Int {\n"
            "        count = count + delta;\n"
            "        return count;\n"
            "    }\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        HIRProgram *hir = NULL;
        RIRProgram *rir = NULL;
        MIRProgram *mir = lower_program_to_mir_strict(program, &hir, &rir);
        TranspilerCtx *ctx = transpiler_ctx_create();

        ctx->mir = mir;
        emit_program(ctx);

        EXPECT_STR_CONTAINS(ctx->out->data, "self->count = (self->count + delta);");
        EXPECT_STR_CONTAINS(ctx->out->data, "return self->count;");

        transpiler_ctx_destroy(ctx);
        mir_destroy(mir);
        rir_destroy(rir);
        hir_destroy(hir);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("subject may own class values and call class func through self field");
    {
        const char *source =
            "class Item {\n"
            "    let name: String;\n"
            "    let damage: Int;\n"
            "    func Info(self) -> String {\n"
            "        return self.name + \" dmg:\" + ToString(self.damage);\n"
            "    }\n"
            "}\n"
            "subject Player {\n"
            "    let name: String;\n"
            "    let weapon: Item;\n"
            "    let mut hp: Int;\n"
            "    func ShowWeapon(self) -> String {\n"
            "        return name + \" holds \" + weapon.Info();\n"
            "    }\n"
            "    action Strike(self, target: Player) -> Int {\n"
            "        let dmg = weapon.damage;\n"
            "        target.hp = target.hp - dmg;\n"
            "        return dmg;\n"
            "    }\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        HIRProgram *hir = NULL;
        RIRProgram *rir = NULL;
        MIRProgram *mir = lower_program_to_mir_strict(program, &hir, &rir);
        TranspilerCtx *ctx = transpiler_ctx_create();

        ctx->mir = mir;
        emit_program(ctx);

        EXPECT_STR_CONTAINS(ctx->out->data, "Info");
        EXPECT_STR_CONTAINS(ctx->out->data, "weapon");
        EXPECT_STR_CONTAINS(ctx->out->data, "target->hp");

        transpiler_ctx_destroy(ctx);
        mir_destroy(mir);
        rir_destroy(rir);
        hir_destroy(hir);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("subject temporary argument rejects pointer-self boundary");
    {
        const char *source =
            "subject Player { let hp: Int; }\n"
            "func Touch(target: Player) -> Void { return; }\n"
            "func Main() -> Void {\n"
            "    Touch(Player());\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        HIRProgram *hir = NULL;
        RIRProgram *rir = NULL;
        MIRProgram *mir = lower_program_to_mir_strict(program, &hir, &rir);
        TranspilerCtx *ctx = transpiler_ctx_create();

        ctx->mir = mir;
        emit_program(ctx);

        EXPECT(ctx->backend_error != NULL);
        EXPECT_STR_CONTAINS(ctx->backend_error,
            "requires addressable storage");
        EXPECT_STR_NOT_CONTAINS(ctx->out->data, "&Player(");

        transpiler_ctx_destroy(ctx);
        mir_destroy(mir);
        rir_destroy(rir);
        hir_destroy(hir);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("subject method temporary argument rejects pointer-self boundary");
    {
        const char *source =
            "subject Player { let hp: Int; }\n"
            "subject Actor {\n"
            "    action Touch(self, target: Player) -> Void { return; }\n"
            "}\n"
            "func Main() -> Void {\n"
            "    let actor: Actor = Actor();\n"
            "    actor.Touch(Player());\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        HIRProgram *hir = NULL;
        RIRProgram *rir = NULL;
        MIRProgram *mir = lower_program_to_mir_strict(program, &hir, &rir);
        TranspilerCtx *ctx = transpiler_ctx_create();

        ctx->mir = mir;
        emit_program(ctx);

        EXPECT(ctx->backend_error != NULL);
        EXPECT_STR_CONTAINS(ctx->backend_error,
            "requires addressable storage");
        EXPECT_STR_NOT_CONTAINS(ctx->out->data, "&Player(");

        transpiler_ctx_destroy(ctx);
        mir_destroy(mir);
        rir_destroy(rir);
        hir_destroy(hir);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("zone methods lower bare shared fields and helper calls through implicit self");
    {
        const char *source =
            "zone BattleZone {\n"
            "    shared round: Int = 1\n"
            "    func Next(self) -> Int {\n"
            "        round = round + 1;\n"
            "        return round;\n"
            "    }\n"
            "    func Tick(self) -> Int {\n"
            "        return Next() + round;\n"
            "    }\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        HIRProgram *hir = NULL;
        RIRProgram *rir = NULL;
        MIRProgram *mir = lower_program_to_mir_strict(program, &hir, &rir);
        TranspilerCtx *ctx = transpiler_ctx_create();

        ctx->mir = mir;
        emit_program(ctx);

        EXPECT_STR_CONTAINS(ctx->out->data, "self->round = (self->round + 1);");
        EXPECT_STR_CONTAINS(ctx->out->data, "return (BattleZone_Next(self) + self->round);");

        transpiler_ctx_destroy(ctx);
        mir_destroy(mir);
        rir_destroy(rir);
        hir_destroy(hir);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("world methods lower bare shared fields zone fields and helper calls through implicit self");
    {
        const char *source =
            "zone BattleZone {\n"
            "    shared round: Int = 1\n"
            "}\n"
            "world GameWorld {\n"
            "    zone battle: BattleZone\n"
            "    shared storm: Int = 1\n"
            "    func Pulse(self) -> Int {\n"
            "        storm = storm + 1;\n"
            "        return storm + battle.round;\n"
            "    }\n"
            "    func Tick(self) -> Int {\n"
            "        return Pulse() + storm;\n"
            "    }\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        HIRProgram *hir = NULL;
        RIRProgram *rir = NULL;
        MIRProgram *mir = lower_program_to_mir_strict(program, &hir, &rir);
        TranspilerCtx *ctx = transpiler_ctx_create();

        ctx->mir = mir;
        emit_program(ctx);

        EXPECT_STR_CONTAINS(ctx->out->data, "self->storm = (self->storm + 1);");
        EXPECT_STR_CONTAINS(ctx->out->data, "return (self->storm + self->battle.round);");
        EXPECT_STR_CONTAINS(ctx->out->data, "return (GameWorld_Pulse(self) + self->storm);");

        transpiler_ctx_destroy(ctx);
        mir_destroy(mir);
        rir_destroy(rir);
        hir_destroy(hir);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

}
