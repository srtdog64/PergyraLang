static void
test_program_emit_tail(void)
{
    TEST("relation methods lower bare sibling calls through MIR host inventory");
    {
        const char *source =
            "subject Player {\n"
            "    let hp: Int;\n"
            "}\n"
            "relation CombatLink {\n"
            "    subject slot left: Player\n"
            "    func Score(self) -> Int {\n"
            "        return 7;\n"
            "    }\n"
            "    func Total(self) -> Int {\n"
            "        return Score();\n"
            "    }\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        HIRProgram *hir = NULL;
        RIRProgram *rir = NULL;
        MIRProgram *mir = lower_program_to_mir(program, &hir, &rir);
        TranspilerCtx *ctx = transpiler_ctx_create();

        emit_program(ctx);

        EXPECT_STR_CONTAINS(ctx->out->data, "CombatLink_Total(CombatLink *self)");
        EXPECT_STR_CONTAINS(ctx->out->data, "return CombatLink_Score(self);");

        transpiler_ctx_destroy(ctx);
        mir_destroy(mir);
        rir_destroy(rir);
        hir_destroy(hir);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("vessel methods lower like passive pointer-self receivers");
    {
        const char *source =
            "vessel HealthState {\n"
            "    current: Int;\n"
            "    func IsDead(self) -> Bool {\n"
            "        return current <= 0;\n"
            "    }\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        HIRProgram *hir = NULL;
        RIRProgram *rir = NULL;
        MIRProgram *mir = lower_program_to_mir(program, &hir, &rir);
        TranspilerCtx *ctx = transpiler_ctx_create();

        emit_program(ctx);

        EXPECT(strstr(ctx->out->data, "HealthState_IsDead(HealthState *self)") != NULL
            || strstr(ctx->helpers->data, "HealthState_IsDead(HealthState *self)") != NULL);
        EXPECT(strstr(ctx->out->data, "return (self->current <= 0);") != NULL
            || strstr(ctx->helpers->data, "return (self->current <= 0);") != NULL);

        transpiler_ctx_destroy(ctx);
        mir_destroy(mir);
        rir_destroy(rir);
        hir_destroy(hir);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("class constructor positional arguments lower to field initialization");
    {
        const char *source =
            "class Vec2 {\n"
            "    let x: Int;\n"
            "    let y: Int;\n"
            "}\n"
            "func Main() -> Void {\n"
            "    let v: Vec2 = Vec2(3, 7);\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        HIRProgram *hir = NULL;
        RIRProgram *rir = NULL;
        MIRProgram *mir = lower_program_to_mir(program, &hir, &rir);
        TranspilerCtx *ctx = transpiler_ctx_create();

        emit_program(ctx);

        EXPECT_STR_CONTAINS(ctx->out->data, "Vec2");
        EXPECT_STR_CONTAINS(ctx->out->data, ".x = 3");
        EXPECT_STR_CONTAINS(ctx->out->data, ".y = 7");

        transpiler_ctx_destroy(ctx);
        mir_destroy(mir);
        rir_destroy(rir);
        hir_destroy(hir);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("Slot<subject> lowers through generated object-cell slot helpers");
    {
        const char *source =
            "subject Vec2 {\n"
            "    let x: Int;\n"
            "    let y: Int;\n"
            "}\n"
            "func Main() -> Void {\n"
            "    let s: Slot<Vec2> = Vec2(3, 7);\n"
            "    Write(s, Vec2(1, 2));\n"
            "    Release(s);\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        HIRProgram *hir = NULL;
        RIRProgram *rir = NULL;
        MIRProgram *mir = lower_program_to_mir(program, &hir, &rir);
        TranspilerCtx *ctx = transpiler_ctx_create();

        emit_program(ctx);

        EXPECT_STR_CONTAINS(ctx->out->data, "PGY_SLOT_DEFINE(Vec2, Vec2)");
        EXPECT_STR_CONTAINS(ctx->out->data, "PgySlot_Vec2 s = pgy_claim_Vec2();");
        EXPECT_STR_CONTAINS(ctx->out->data, "pgy_write_Vec2(&s, (Vec2){ .x = 3, .y = 7 });");
        EXPECT_STR_CONTAINS(ctx->out->data, "pgy_write_Vec2(&s, (Vec2){ .x = 1, .y = 2 });");
        EXPECT_STR_CONTAINS(ctx->out->data, "pgy_release_Vec2(&s);");

        transpiler_ctx_destroy(ctx);
        mir_destroy(mir);
        rir_destroy(rir);
        hir_destroy(hir);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("SecureSlot<subject> lowers through generated secure object-cell slot helpers");
    {
        const char *source =
            "subject Vec2 {\n"
            "    let x: Int;\n"
            "    let y: Int;\n"
            "}\n"
            "func Main() -> Void {\n"
            "    let s: SecureSlot<Vec2> = Vec2(3, 7);\n"
            "    Write(s, Vec2(1, 2), s_token);\n"
            "    Release(s, s_token);\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        HIRProgram *hir = NULL;
        RIRProgram *rir = NULL;
        MIRProgram *mir = lower_program_to_mir(program, &hir, &rir);
        TranspilerCtx *ctx = transpiler_ctx_create();

        emit_program(ctx);

        EXPECT_STR_CONTAINS(ctx->out->data, "PGY_SECURE_SLOT_DEFINE(Vec2, Vec2)");
        EXPECT_STR_CONTAINS(ctx->out->data, "PgyToken_Vec2");
        EXPECT_STR_CONTAINS(ctx->out->data, "pgy_claim_secure_Vec2");
        EXPECT_STR_CONTAINS(ctx->out->data, "pgy_secure_write_Vec2");
        EXPECT_STR_CONTAINS(ctx->out->data, "pgy_secure_release_Vec2");

        transpiler_ctx_destroy(ctx);
        mir_destroy(mir);
        rir_destroy(rir);
        hir_destroy(hir);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("SecureSlot<subject> lowers through generated secure object-cell slot helpers");
    {
        const char *source =
            "subject Bot {\n"
            "    let hp: Int;\n"
            "}\n"
            "func Main() -> Void {\n"
            "    let s: SecureSlot<Bot> = Bot(7);\n"
            "    Write(s, Bot(9), s_token);\n"
            "    Release(s, s_token);\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        HIRProgram *hir = NULL;
        RIRProgram *rir = NULL;
        MIRProgram *mir = lower_program_to_mir(program, &hir, &rir);
        TranspilerCtx *ctx = transpiler_ctx_create();

        emit_program(ctx);

        EXPECT_STR_CONTAINS(ctx->out->data, "PGY_SECURE_SLOT_DEFINE(Bot, Bot)");
        EXPECT_STR_CONTAINS(ctx->out->data, "PgyToken_Bot");
        EXPECT_STR_CONTAINS(ctx->out->data, "pgy_claim_secure_Bot");
        EXPECT_STR_CONTAINS(ctx->out->data, "pgy_secure_write_Bot");
        EXPECT_STR_CONTAINS(ctx->out->data, "pgy_secure_release_Bot");

        transpiler_ctx_destroy(ctx);
        mir_destroy(mir);
        rir_destroy(rir);
        hir_destroy(hir);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("ref Slot<subject> parameter lowers as slot pointer boundary");
    {
        const char *source =
            "subject Vec2 {\n"
            "    let x: Int;\n"
            "    let y: Int;\n"
            "}\n"
            "func Touch(ref s: Slot<Vec2>) -> Void {\n"
            "    Write(s, Vec2(1, 2));\n"
            "}\n"
            "func Main() -> Void {\n"
            "    let s: Slot<Vec2> = Vec2(3, 7);\n"
            "    Touch(s);\n"
            "    Release(s);\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        HIRProgram *hir = NULL;
        RIRProgram *rir = NULL;
        MIRProgram *mir = lower_program_to_mir(program, &hir, &rir);
        TranspilerCtx *ctx = transpiler_ctx_create();

        emit_program(ctx);

        EXPECT_STR_CONTAINS(ctx->out->data, "void Touch(PgySlot_Vec2 *s)");
        EXPECT_STR_CONTAINS(ctx->out->data, "pgy_write_Vec2(s, (Vec2){ .x = 1, .y = 2 });");
        EXPECT_STR_CONTAINS(ctx->out->data, "Touch(&s);");

        transpiler_ctx_destroy(ctx);
        mir_destroy(mir);
        rir_destroy(rir);
        hir_destroy(hir);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("own SecureSlot<subject> parameter lowers as secure slot pointer boundary");
    {
        const char *source =
            "subject Vec2 {\n"
            "    let x: Int;\n"
            "    let y: Int;\n"
            "}\n"
            "func Consume(own s: SecureSlot<Vec2>) -> Void {\n"
            "    Write(s, Vec2(1, 2), s_token);\n"
            "    Release(s, s_token);\n"
            "}\n"
            "func Main() -> Void {\n"
            "    let s: SecureSlot<Vec2> = Vec2(3, 7);\n"
            "    Consume(s);\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        HIRProgram *hir = NULL;
        RIRProgram *rir = NULL;
        MIRProgram *mir = lower_program_to_mir(program, &hir, &rir);
        TranspilerCtx *ctx = transpiler_ctx_create();

        emit_program(ctx);

        EXPECT_STR_CONTAINS(ctx->out->data, "Consume(PgySecureSlot_Vec2");
        EXPECT_STR_CONTAINS(ctx->out->data, "pgy_secure_write_Vec2");
        EXPECT_STR_CONTAINS(ctx->out->data, "pgy_secure_release_Vec2");
        EXPECT_STR_CONTAINS(ctx->out->data, "Consume(");

        transpiler_ctx_destroy(ctx);
        mir_destroy(mir);
        rir_destroy(rir);
        hir_destroy(hir);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("secure boundary forwarding preserves paired token through helper call");
    {
        const char *source =
            "subject Vec2 {\n"
            "    let x: Int;\n"
            "    let y: Int;\n"
            "}\n"
            "func ConsumeInner(own s: SecureSlot<Vec2>) -> Void {\n"
            "    Write(s, Vec2(1, 2), s_token);\n"
            "    Release(s, s_token);\n"
            "}\n"
            "func ConsumeOuter(own s: SecureSlot<Vec2>) -> Void {\n"
            "    ConsumeInner(s);\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        HIRProgram *hir = NULL;
        RIRProgram *rir = NULL;
        MIRProgram *mir = lower_program_to_mir(program, &hir, &rir);
        TranspilerCtx *ctx = transpiler_ctx_create();

        emit_program(ctx);

        EXPECT_STR_CONTAINS(ctx->out->data, "void ConsumeOuter(PgySecureSlot_Vec2 *s, PgyToken_Vec2 s_token)");
        EXPECT_STR_CONTAINS(ctx->out->data, "ConsumeInner(s, s_token);");

        transpiler_ctx_destroy(ctx);
        mir_destroy(mir);
        rir_destroy(rir);
        hir_destroy(hir);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("transitive secure boundary forwarding preserves paired token through helper chain");
    {
        const char *source =
            "subject Vec2 {\n"
            "    let x: Int;\n"
            "    let y: Int;\n"
            "}\n"
            "func ConsumeInner(own s: SecureSlot<Vec2>) -> Void {\n"
            "    Write(s, Vec2(1, 2), s_token);\n"
            "    Release(s, s_token);\n"
            "}\n"
            "func ConsumeMiddle(own s: SecureSlot<Vec2>) -> Void {\n"
            "    ConsumeInner(s);\n"
            "}\n"
            "func ConsumeOuter(own s: SecureSlot<Vec2>) -> Void {\n"
            "    ConsumeMiddle(s);\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        HIRProgram *hir = NULL;
        RIRProgram *rir = NULL;
        MIRProgram *mir = lower_program_to_mir(program, &hir, &rir);
        TranspilerCtx *ctx = transpiler_ctx_create();

        emit_program(ctx);

        EXPECT_STR_CONTAINS(ctx->out->data, "void ConsumeOuter(PgySecureSlot_Vec2 *s, PgyToken_Vec2 s_token)");
        EXPECT_STR_CONTAINS(ctx->out->data, "void ConsumeMiddle(PgySecureSlot_Vec2 *s, PgyToken_Vec2 s_token)");
        EXPECT_STR_CONTAINS(ctx->out->data, "ConsumeMiddle(s, s_token);");
        EXPECT_STR_CONTAINS(ctx->out->data, "ConsumeInner(s, s_token);");

        transpiler_ctx_destroy(ctx);
        mir_destroy(mir);
        rir_destroy(rir);
        hir_destroy(hir);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("extern block emits C prototypes");
    {
        ASTNode ext; memset(&ext, 0, sizeof(ext));
        ext.type = AST_EXTERN_BLOCK;
        ext.data.extern_block.abi = "C";

        FuncParam p; memset(&p, 0, sizeof(p));
        p.name = "flags";
        p.type = make_type_node("Int");
        FuncParam *params[1] = { &p };

        ASTNode fn1; memset(&fn1, 0, sizeof(fn1));
        fn1.type = AST_FUNC_DECL;
        fn1.data.func_decl.name = "SDL_Init";
        fn1.data.func_decl.params = params;
        fn1.data.func_decl.param_count = 1;
        fn1.data.func_decl.return_type = make_type_node("Int");
        fn1.data.func_decl.body = NULL;

        ASTNode fn2; memset(&fn2, 0, sizeof(fn2));
        fn2.type = AST_FUNC_DECL;
        fn2.data.func_decl.name = "SDL_Quit";
        fn2.data.func_decl.params = NULL;
        fn2.data.func_decl.param_count = 0;
        fn2.data.func_decl.return_type = make_type_node("Void");
        fn2.data.func_decl.body = NULL;

        ASTNode *decls[2] = { &fn1, &fn2 };
        ext.data.extern_block.declarations = decls;
        ext.data.extern_block.count = 2;

        ASTNode *stmts[1] = { &ext };
        ASTNode *prog = make_program(stmts, 1);
        HIRProgram *hir = NULL;
        RIRProgram *rir = NULL;
        MIRProgram *mir = lower_program_to_mir(prog, &hir, &rir);

        TranspilerCtx *ctx = transpiler_ctx_create();
        emit_program(ctx);

        EXPECT_STR_CONTAINS(ctx->out->data, "extern \"C\"");
        EXPECT_STR_CONTAINS(ctx->out->data, "int32_t SDL_Init(int32_t flags);");
        EXPECT_STR_CONTAINS(ctx->out->data, "void SDL_Quit();");

        transpiler_ctx_destroy(ctx);
        mir_destroy(mir);
        rir_destroy(rir);
        hir_destroy(hir);
    }

    TEST("extern Bool return uses bool stringification");
    {
        const char *source =
            "extern \"C\" {\n"
            "    func pgy_zone_authority_validate_flags_export(hasZone: Bool, hasParticipant: Bool, zone: String, participant: String) -> Bool;\n"
            "    func pgy_zone_authority_last_ok_rt_export() -> Bool;\n"
            "}\n"
            "func Main() -> Void {\n"
            "    Log(ToString(pgy_zone_authority_validate_flags_export(true, true, \"BattleZone\", \"owner\")));\n"
            "    Log(ToString(pgy_zone_authority_last_ok_rt_export()));\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = NULL;
        HIRProgram *hir = NULL;
        RIRProgram *rir = NULL;
        MIRProgram *mir = NULL;
        TranspilerCtx *ctx = NULL;

        program = parser_parse_program(parser);
        EXPECT(program != NULL);
        mir = lower_program_to_mir(program, &hir, &rir);
        ctx = transpiler_ctx_create();
        emit_program(ctx);

        EXPECT_STR_CONTAINS(ctx->out->data,
            "pgy_bool_to_string(pgy_zone_authority_validate_flags_export(true, true, \"BattleZone\", \"owner\"))");
        EXPECT_STR_CONTAINS(ctx->out->data,
            "pgy_bool_to_string(pgy_zone_authority_last_ok_rt_export())");
        EXPECT_STR_NOT_CONTAINS(ctx->out->data,
            "pgy_int_to_string(pgy_zone_authority_validate_flags_export");
        EXPECT_STR_NOT_CONTAINS(ctx->out->data,
            "pgy_int_to_string(pgy_zone_authority_last_ok_rt_export()");

        transpiler_ctx_destroy(ctx);
        mir_destroy(mir);
        rir_destroy(rir);
        hir_destroy(hir);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("event declaration stays at file scope");
    {
        ASTNode event_node;
        ASTNode param_node;
        ASTNode *params[1] = { &param_node };
        ASTNode *stmts[2];
        ASTNode *prog;
        HIRProgram *hir;
        RIRProgram *rir;
        MIRProgram *mir;
        TranspilerCtx *ctx;
        const char *event_pos;

        memset(&event_node, 0, sizeof(event_node));
        memset(&param_node, 0, sizeof(param_node));

        event_node.type = AST_EVENT_DECL;
        event_node.data.event_decl.name = "OnHit";
        event_node.data.event_decl.params = params;
        event_node.data.event_decl.param_count = 1;
        event_node.data.event_decl.return_type = make_type_node("Void");

        param_node.type = AST_LET_DECL;
        param_node.data.let_decl.name = "damage";
        param_node.data.let_decl.type = make_type_node("Int");

        stmts[0] = &event_node;
        stmts[1] = make_let("boot", make_type_node("Int"), make_number(1, 1), 1);
        prog = make_program(stmts, 2);
        rir = NULL;
        mir = lower_program_to_mir(prog, &hir, &rir);
        ctx = transpiler_ctx_create();
        emit_program(ctx);

        event_pos = strstr(ctx->out->data, "typedef void (*OnHit_Handler)");
        EXPECT(event_pos != NULL);

        transpiler_ctx_destroy(ctx);
        mir_destroy(mir);
        rir_destroy(rir);
        hir_destroy(hir);
    }
}

/* -----------------------------------------------------------------
 * Ability / Role codegen
 * ----------------------------------------------------------------- */


static void
test_program_emit(void)
{
    test_program_emit_head();
    test_program_emit_tail();
}
