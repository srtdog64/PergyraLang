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
        emit_program(ctx);
        EXPECT_STR_CONTAINS(ctx->out->data, "#include \"pgy_runtime.h\"");
        transpiler_ctx_destroy(ctx);
        mir_destroy(mir);
        rir_destroy(rir);
        hir_destroy(hir);
    }

    TEST("function emitted at top level with correct signature");
    {
        /* func Add(a: Int, b: Int) -> Int { return a + b } */
        FuncParam pa, pb;
        memset(&pa, 0, sizeof(pa)); pa.name = "a"; pa.type = make_type_node("Int");
        memset(&pb, 0, sizeof(pb)); pb.name = "b"; pb.type = make_type_node("Int");
        FuncParam *params[2] = { &pa, &pb };

        ASTNode *fn = calloc(1, sizeof(ASTNode));
        fn->type = AST_FUNC_DECL;
        fn->data.func_decl.name        = "Add";
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
        emit_program(ctx);

        EXPECT_STR_CONTAINS(ctx->out->data, "int32_t Add(int32_t a, int32_t b)");
        transpiler_ctx_destroy(ctx);
        mir_destroy(mir);
        rir_destroy(rir);
        hir_destroy(hir);
    }

    TEST("generic function call emits concrete specialization in C");
    {
        FuncParam px;
        memset(&px, 0, sizeof(px));
        px.name = "x";
        px.type = make_type_node("T");
        FuncParam *identity_params[1] = { &px };

        ASTNode *identity_return_stmts[1] = { make_return(make_identifier("x", 1), 1) };
        ASTNode *identity = calloc(1, sizeof(ASTNode));
        identity->type = AST_FUNC_DECL;
        identity->data.func_decl.name = "Identity";
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
        main->data.func_decl.name = "Main";
        main->data.func_decl.return_type = make_type_node("Void");
        main->data.func_decl.body = make_block(main_stmts, 3);

        ASTNode *stmts[2] = { identity, main };
        ASTNode *prog = make_program(stmts, 2);
        HIRProgram *hir = NULL;
        RIRProgram *rir = NULL;
        MIRProgram *mir = lower_program_to_mir(prog, &hir, &rir);

        TranspilerCtx *ctx = transpiler_ctx_create();
        emit_program(ctx);

        EXPECT_STR_CONTAINS(ctx->out->data, "int32_t Identity_Int(int32_t x)");
        EXPECT_STR_CONTAINS(ctx->out->data, "Identity_Int(");

        transpiler_ctx_destroy(ctx);
        mir_destroy(mir);
        rir_destroy(rir);
        hir_destroy(hir);
    }

    TEST("spawn emits wrapper-based task launch");
    {
        FuncParam px;
        memset(&px, 0, sizeof(px));
        px.name = "x";
        px.type = make_type_node("Int");
        FuncParam *identity_params[1] = { &px };

        ASTNode *identity_return_stmts[1] = { make_return(make_identifier("x", 1), 1) };
        ASTNode *identity = calloc(1, sizeof(ASTNode));
        identity->type = AST_FUNC_DECL;
        identity->data.func_decl.name = "IdentityInt";
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
        ast_add_statement(main_body, spawn);
        ASTNode *main = calloc(1, sizeof(ASTNode));
        main->type = AST_FUNC_DECL;
        main->data.func_decl.name = "Main";
        main->data.func_decl.return_type = make_type_node("Void");
        main->data.func_decl.body = main_body;

        ASTNode *stmts[2] = { identity, main };
        ASTNode *prog = make_program(stmts, 2);
        HIRProgram *hir = NULL;
        RIRProgram *rir = NULL;
        MIRProgram *mir = lower_program_to_mir(prog, &hir, &rir);

        TranspilerCtx *ctx = transpiler_ctx_create();
        emit_program(ctx);

        EXPECT_STR_CONTAINS(ctx->out->data, "IdentityInt");
        EXPECT_STR_CONTAINS(ctx->out->data, "pgy_async_spawn");

        transpiler_ctx_destroy(ctx);
        mir_destroy(mir);
        rir_destroy(rir);
        hir_destroy(hir);
    }

    TEST("parallel block emits pgy_spawn / pgy_await per task");
    {
        ASTNode *tasks[2] = {
            make_call("A", NULL, 0, 1),
            make_call("B", NULL, 0, 1)
        };
        ASTNode *par = calloc(1, sizeof(ASTNode));
        par->type = AST_PARALLEL_BLOCK;
        par->data.parallel.tasks      = tasks;
        par->data.parallel.task_count = 2;

        ASTNode *fnA = calloc(1, sizeof(ASTNode));
        fnA->type = AST_FUNC_DECL;
        fnA->data.func_decl.name = "A";
        fnA->data.func_decl.return_type = make_type_node("Void");
        fnA->data.func_decl.body = ast_create_block();
        ASTNode *fnB = calloc(1, sizeof(ASTNode));
        fnB->type = AST_FUNC_DECL;
        fnB->data.func_decl.name = "B";
        fnB->data.func_decl.return_type = make_type_node("Void");
        fnB->data.func_decl.body = ast_create_block();

        ASTNode *main_body = ast_create_block();
        ast_add_statement(main_body, par);
        ASTNode *main = calloc(1, sizeof(ASTNode));
        main->type = AST_FUNC_DECL;
        main->data.func_decl.name = "Main";
        main->data.func_decl.return_type = make_type_node("Void");
        main->data.func_decl.body = main_body;
        main->data.func_decl.param_count = 0;

        ASTNode *stmts[3] = { fnA, fnB, main };
        ASTNode *prog     = make_program(stmts, 3);
        HIRProgram *hir = NULL;
        RIRProgram *rir = NULL;
        MIRProgram *mir = lower_program_to_mir(prog, &hir, &rir);

        TranspilerCtx *ctx = transpiler_ctx_create();
        emit_program(ctx);

        EXPECT_STR_CONTAINS(ctx->out->data, "pgy_spawn");
        EXPECT_STR_CONTAINS(ctx->out->data, "pgy_await");
        EXPECT_STR_CONTAINS(ctx->out->data, "_pgy_par_");
        transpiler_ctx_destroy(ctx);
        mir_destroy(mir);
        rir_destroy(rir);
        hir_destroy(hir);
    }

    TEST("struct emits typedef struct and method function");
    {
        ASTNode *st = calloc(1, sizeof(ASTNode));
        st->type = AST_CLASS_DECL;
        st->data.class_decl.name = "Vec3";
        st->data.class_decl.is_struct = true;

        ClassField fx, fy, fz;
        memset(&fx, 0, sizeof(fx));
        memset(&fy, 0, sizeof(fy));
        memset(&fz, 0, sizeof(fz));
        fx.name = "x"; fx.type = make_type_node("Float");
        fy.name = "y"; fy.type = make_type_node("Float");
        fz.name = "z"; fz.type = make_type_node("Float");
        ClassField *fields[3] = { &fx, &fy, &fz };
        st->data.class_decl.fields = fields;
        st->data.class_decl.field_count = 3;

        ASTNode method; memset(&method, 0, sizeof(method));
        method.type = AST_FUNC_DECL;
        method.data.func_decl.name = "Length";
        method.data.func_decl.params = NULL;
        method.data.func_decl.param_count = 0;
        method.data.func_decl.return_type = make_type_node("Float");
        method.data.func_decl.body = NULL;

        ASTNode *methods[1] = { &method };
        st->data.class_decl.methods = methods;
        st->data.class_decl.method_count = 1;

        ASTNode *stmts[1] = { st };
        ASTNode *prog = make_program(stmts, 1);
        HIRProgram *hir = NULL;
        RIRProgram *rir = NULL;
        MIRProgram *mir = lower_program_to_mir(prog, &hir, &rir);

        TranspilerCtx *ctx = transpiler_ctx_create();
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
            "    let hp: Int;\n"
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
