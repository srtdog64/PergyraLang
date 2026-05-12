static void
test_codebuf(void)
{
    printf("\n[codebuf]\n");

    TEST("write simple string");
    {
        CodeBuf *b = codebuf_create();
        codebuf_write(b, "hello");
        EXPECT(strcmp(b->data, "hello") == 0 && b->len == 5);
        codebuf_destroy(b);
    }

    TEST("write formatted string");
    {
        CodeBuf *b = codebuf_create();
        codebuf_write(b, "int x = %d;", 42);
        EXPECT(strcmp(b->data, "int x = 42;") == 0);
        codebuf_destroy(b);
    }

    TEST("write triggers growth beyond initial capacity");
    {
        CodeBuf *b = codebuf_create();
        for (int i = 0; i < 1000; i++)
            codebuf_write(b, "a");
        EXPECT(b->len == 1000);
        codebuf_destroy(b);
    }

    TEST("multiple writes concatenate correctly");
    {
        CodeBuf *b = codebuf_create();
        codebuf_write(b, "foo");
        codebuf_write(b, "bar");
        codebuf_write(b, "baz");
        EXPECT(strcmp(b->data, "foobarbaz") == 0);
        codebuf_destroy(b);
    }
}

/* -----------------------------------------------------------------
 * Tests: type mapping
 * ----------------------------------------------------------------- */

static void
test_type_mapping(void)
{
    printf("\n[type_mapping]\n");

    TEST("Int -> int32_t");
    EXPECT(strcmp(pergyra_type_to_c("Int"), "int32_t") == 0);

    TEST("Long -> int64_t");
    EXPECT(strcmp(pergyra_type_to_c("Long"), "int64_t") == 0);

    TEST("Float -> float");
    EXPECT(strcmp(pergyra_type_to_c("Float"), "float") == 0);

    TEST("Bool -> bool");
    EXPECT(strcmp(pergyra_type_to_c("Bool"), "bool") == 0);

    TEST("String -> char*");
    EXPECT(strcmp(pergyra_type_to_c("String"), "char*") == 0);

    TEST("Void -> void");
    EXPECT(strcmp(pergyra_type_to_c("Void"), "void") == 0);

    TEST("Slot<Int> -> PgySlot_Int");
    EXPECT(strcmp(pergyra_type_to_c("Slot<Int>"), "PgySlot_Int") == 0);

    TEST("Slot<String> -> PgySlot_String");
    EXPECT(strcmp(pergyra_type_to_c("Slot<String>"), "PgySlot_String") == 0);

    TEST("Slot<Vec2> -> PgySlot_Vec2");
    EXPECT(strcmp(pergyra_type_to_c("Slot<Vec2>"), "PgySlot_Vec2") == 0);

    TEST("SecureSlot<Int> -> PgySecureSlot_Int");
    EXPECT(strcmp(pergyra_type_to_c("SecureSlot<Int>"), "PgySecureSlot_Int") == 0);

    TEST("SecureSlot<Vec2> -> PgySecureSlot_Vec2");
    EXPECT(strcmp(pergyra_type_to_c("SecureSlot<Vec2>"), "PgySecureSlot_Vec2") == 0);

    TEST("Array<Vertex> -> PgyArray_Vertex");
    EXPECT(strcmp(pergyra_type_to_c("Array<Vertex>"), "PgyArray_Vertex") == 0);

    TEST("Slice<Vertex> -> PgySlice_Vertex");
    EXPECT(strcmp(pergyra_type_to_c("Slice<Vertex>"), "PgySlice_Vertex") == 0);

    TEST("List<Vertex> -> PgyList_Vertex");
    EXPECT(strcmp(pergyra_type_to_c("List<Vertex>"), "PgyList_Vertex") == 0);

    TEST("Queue<Vertex> -> PgyQueue_Vertex");
    EXPECT(strcmp(pergyra_type_to_c("Queue<Vertex>"), "PgyQueue_Vertex") == 0);

    TEST("Rc<Int> -> PgyRc_Int");
    EXPECT(strcmp(pergyra_type_to_c("Rc<Int>"), "PgyRc_Int") == 0);

    TEST("Weak<Int> -> PgyWeak_Int");
    EXPECT(strcmp(pergyra_type_to_c("Weak<Int>"), "PgyWeak_Int") == 0);

    TEST("Allocator -> PgyAllocator");
    EXPECT(strcmp(pergyra_type_to_c("Allocator"), "PgyAllocator") == 0);

    TEST("Box<Array<Int>> -> PgyBoxArray_Int");
    EXPECT(strcmp(pergyra_type_to_c("Box<Array<Int>>"), "PgyBoxArray_Int") == 0);

    TEST("Array<Unknown> keeps Unknown sentinel");
    EXPECT(strcmp(pergyra_type_to_c("Array<Unknown>"), "Unknown") == 0);

    TEST("Array<Unknown > keeps Unknown sentinel");
    EXPECT(strcmp(pergyra_type_to_c("Array<Unknown >"), "Unknown") == 0);

    TEST("HashMap<String, Unknown> keeps Unknown sentinel");
    EXPECT(strcmp(pergyra_type_to_c("HashMap<String, Unknown>"), "Unknown") == 0);

    TEST("Box<Array<Unknown>> keeps Unknown sentinel");
    EXPECT(strcmp(pergyra_type_to_c("Box<Array<Unknown>>"), "Unknown") == 0);

    TEST("Box<Array<UnknownError>> keeps user type name");
    EXPECT(strcmp(pergyra_type_to_c("Box<Array<UnknownError>>"), "PgyBoxArray_UnknownError") == 0);

    TEST("slot_inner_type_name(Slot<Float>) -> Float");
    EXPECT(strcmp(slot_inner_type_name("Slot<Float>"), "Float") == 0);

    TEST("slot_inner_type_name(SecureSlot<Long>) -> Long");
    EXPECT(strcmp(slot_inner_type_name("SecureSlot<Long>"), "Long") == 0);
}

/* -----------------------------------------------------------------
 * Tests: expression emitters
 * ----------------------------------------------------------------- */

static void
test_expression_emit(void)
{
    printf("\n[expression_emit]\n");

    TranspilerCtx *ctx;
    char *result;

    TEST("integer literal -> correct C literal");
    {
        ctx    = transpiler_ctx_create();
        result = emit_expression(make_number(42, 1), ctx);
        EXPECT(strcmp(result, "42") == 0);
        free(result);
        transpiler_ctx_destroy(ctx);
    }

    TEST("string literal -> quoted C string");
    {
        ctx    = transpiler_ctx_create();
        result = emit_expression(make_string_lit("hello", 1), ctx);
        EXPECT(strcmp(result, "\"hello\"") == 0);
        free(result);
        transpiler_ctx_destroy(ctx);
    }

    TEST("Some(42) -> Some_Int(42)");
    {
        ctx = transpiler_ctx_create();
        ASTNode *args[1] = { make_number(42, 1) };
        result = emit_expression(make_call("Some", args, 1, 1), ctx);
        EXPECT(strcmp(result, "Some_Int(42)") == 0);
        free(result);
        transpiler_ctx_destroy(ctx);
    }

    TEST("Some(value) without concrete payload type emits diagnostic recovery");
    {
        ctx = transpiler_ctx_create();
        ASTNode *args[1] = { make_identifier("value", 1) };
        result = emit_expression(make_call("Some", args, 1, 1), ctx);
        EXPECT(strcmp(result, "0") == 0);
        EXPECT(ctx->backend_error != NULL);
        EXPECT_STR_CONTAINS(ctx->backend_error,
                            "Some requires concrete payload type");
        free(result);
        transpiler_ctx_destroy(ctx);
    }

    TEST("None() without contextual Option<T> emits diagnostic recovery");
    {
        ctx = transpiler_ctx_create();
        result = emit_expression(make_call("None", NULL, 0, 1), ctx);
        EXPECT(strcmp(result, "0") == 0);
        EXPECT(ctx->backend_error != NULL);
        free(result);
        transpiler_ctx_destroy(ctx);
    }

    TEST("IsSome(None()) without concrete Option<T> emits diagnostic recovery");
    {
        ctx = transpiler_ctx_create();
        ASTNode *args[1] = { make_call("None", NULL, 0, 1) };
        result = emit_expression(make_call("IsSome", args, 1, 1), ctx);
        EXPECT(strcmp(result, "false") == 0);
        EXPECT(ctx->backend_error != NULL);
        EXPECT_STR_CONTAINS(ctx->backend_error,
                            "IsSome requires concrete Option<T>");
        free(result);
        transpiler_ctx_destroy(ctx);
    }

    TEST("Ok(value) with unknown Result payload emits diagnostic recovery");
    {
        ctx = transpiler_ctx_create();
        ctx->expected_type = "Result<Unknown, NetError>";
        ASTNode *args[1] = { make_number(42, 1) };
        result = emit_expression(make_call("Ok", args, 1, 1), ctx);
        EXPECT(strcmp(result, "0") == 0);
        EXPECT(ctx->backend_error != NULL);
        EXPECT_STR_CONTAINS(ctx->backend_error,
                            "cannot derive Result<T, E> specialization");
        free(result);
        transpiler_ctx_destroy(ctx);
    }

    TEST("Result suffix keeps user type names containing Unknown");
    {
        ctx = transpiler_ctx_create();
        ctx->expected_type = "Result<MyUnknownType, NetError>";
        ASTNode *args[1] = { make_number(42, 1) };
        result = emit_expression(make_call("Ok", args, 1, 1), ctx);
        EXPECT(strcmp(result, "Ok_MyUnknownType_NetError(42)") == 0);
        EXPECT(ctx->backend_error == NULL);
        free(result);
        transpiler_ctx_destroy(ctx);
    }

    TEST("identifier -> same name");
    {
        ctx    = transpiler_ctx_create();
        result = emit_expression(make_identifier("myVar", 1), ctx);
        EXPECT(strcmp(result, "myVar") == 0);
        free(result);
        transpiler_ctx_destroy(ctx);
    }

    TEST("user function call -> funcName(arg0, arg1)");
    {
        ctx = transpiler_ctx_create();
        ASTNode *args[2] = { make_number(1, 1), make_number(2, 1) };
        result = emit_expression(make_call("Add", args, 2, 1), ctx);
        EXPECT(strcmp(result, "Add(1, 2)") == 0);
        free(result);
        transpiler_ctx_destroy(ctx);
    }

    TEST("Log(42) -> pgy_log(42)");
    {
        ctx = transpiler_ctx_create();
        ASTNode *args[1] = { make_number(42, 1) };
        result = emit_expression(make_call("Log", args, 1, 1), ctx);
        EXPECT(strcmp(result, "pgy_log(42)") == 0);
        free(result);
        transpiler_ctx_destroy(ctx);
    }

    TEST("Log(\"\\n + indentation\") normalizes multiline as banner");
    {
        ctx = transpiler_ctx_create();
        ASTNode *args[1] = {
            make_string_lit("\n  first\n  second", 1),
        };
        result = emit_expression(make_call("Log", args, 1, 1), ctx);
        EXPECT(strcmp(result, "pgy_log_banner(\"first\\nsecond\")") == 0);
        free(result);
        transpiler_ctx_destroy(ctx);
    }

    TEST("LogBlock(\"\\n + indentation\") normalizes multiline block text");
    {
        ctx = transpiler_ctx_create();
        ASTNode *args[1] = {
            make_string_lit("\n  first\n  second\n", 1),
        };
        result = emit_expression(make_call("LogBlock", args, 1, 1), ctx);
        EXPECT(strcmp(result, "pgy_log_banner(\"first\\nsecond\")") == 0);
        free(result);
        transpiler_ctx_destroy(ctx);
    }

    TEST("LogBanner(\"\\n + indentation\") normalizes multiline banner text");
    {
        ctx = transpiler_ctx_create();
        ASTNode *args[1] = {
            make_string_lit("\n  first\n  second", 1),
        };
        result = emit_expression(make_call("LogBanner", args, 1, 1), ctx);
        EXPECT(strcmp(result, "pgy_log_banner(\"first\\nsecond\")") == 0);
        free(result);
        transpiler_ctx_destroy(ctx);
    }

    TEST("LogRaw preserves raw newline and leading spaces");
    {
        ctx = transpiler_ctx_create();
        ASTNode *args[1] = {
            make_string_lit("\n  first\n  second", 1),
        };
        result = emit_expression(make_call("LogRaw", args, 1, 1), ctx);
        EXPECT(strcmp(result, "pgy_log(\"\\n  first\\n  second\")") == 0);
        free(result);
        transpiler_ctx_destroy(ctx);
    }

    TEST("Read(s) -> pgy_read_Int(&s)");
    {
        ctx = transpiler_ctx_create();
        register_slot_var(ctx, "s", "Int", false, false);
        ASTNode *args[1] = { make_identifier("s", 1) };
        result = emit_expression(make_call("Read", args, 1, 1), ctx);
        EXPECT(strcmp(result, "pgy_read_Int(&s)") == 0);
        free(result);
        transpiler_ctx_destroy(ctx);
    }

    TEST("Release(s) -> pgy_release_Int(&s)");
    {
        ctx = transpiler_ctx_create();
        register_slot_var(ctx, "s", "Int", false, false);
        ASTNode *args[1] = { make_identifier("s", 1) };
        result = emit_expression(make_call("Release", args, 1, 1), ctx);
        EXPECT(strcmp(result, "pgy_release_Int(&s)") == 0);
        free(result);
        transpiler_ctx_destroy(ctx);
    }

    TEST("ViewRead(slot) -> slot");
    {
        ctx = transpiler_ctx_create();
        ASTNode *args[1] = { make_identifier("slot", 1) };
        result = emit_expression(make_call("ViewRead", args, 1, 1), ctx);
        EXPECT(strcmp(result, "slot") == 0);
        free(result);
        transpiler_ctx_destroy(ctx);
    }

    TEST("ViewWrite(slot) -> slot");
    {
        ctx = transpiler_ctx_create();
        ASTNode *args[1] = { make_identifier("slot", 1) };
        result = emit_expression(make_call("ViewWrite", args, 1, 1), ctx);
        EXPECT(strcmp(result, "slot") == 0);
        free(result);
        transpiler_ctx_destroy(ctx);
    }

    TEST("Move(slot) -> slot");
    {
        ctx = transpiler_ctx_create();
        ASTNode *args[1] = { make_identifier("slot", 1) };
        result = emit_expression(make_call("Move", args, 1, 1), ctx);
        EXPECT(strcmp(result, "slot") == 0);
        free(result);
        transpiler_ctx_destroy(ctx);
    }

    TEST("array access -> values[0]");
    {
        ctx = transpiler_ctx_create();
        result = emit_expression(
            ast_create_array_access(make_identifier("values", 1),
                                    make_number(0, 1)),
            ctx);
        EXPECT(strcmp(result, "values[0]") == 0);
        free(result);
        transpiler_ctx_destroy(ctx);
    }

    TEST("RcClone(shared) -> pgy_rc_clone_Int(shared)");
    {
        ctx = transpiler_ctx_create();
        ASTNode *init_args[1] = { make_number(1, 1) };
        emit_statement(
            make_let("shared",
                     make_generic_type("Rc", "Int"),
                     make_call("RcNew", init_args, 1, 1), 1),
            ctx);
        ASTNode *args[1] = { make_identifier("shared", 1) };
        result = emit_expression(make_call("RcClone", args, 1, 1), ctx);
        EXPECT(strcmp(result, "pgy_rc_clone_Int(shared)") == 0);
        free(result);
        transpiler_ctx_destroy(ctx);
    }

    TEST("BoxGet(boxed) -> pgy_box_get_Int(boxed)");
    {
        ctx = transpiler_ctx_create();
        ASTNode *init_args[1] = { make_number(1, 1) };
        emit_statement(
            make_let("boxed",
                     make_generic_type("Box", "Int"),
                     make_call("Box", init_args, 1, 1), 1),
            ctx);
        ASTNode *args[1] = { make_identifier("boxed", 1) };
        result = emit_expression(make_call("BoxGet", args, 1, 1), ctx);
        EXPECT(strcmp(result, "pgy_box_get_Int(boxed)") == 0);
        free(result);
        transpiler_ctx_destroy(ctx);
    }

    TEST("BoxSet(boxed, 42) -> pgy_box_set_Int(&boxed, 42)");
    {
        ctx = transpiler_ctx_create();
        ASTNode *init_args[1] = { make_number(1, 1) };
        emit_statement(
            make_let("boxed",
                     make_generic_type("Box", "Int"),
                     make_call("Box", init_args, 1, 1), 1),
            ctx);
        ASTNode *args[2] = { make_identifier("boxed", 1), make_number(42, 1) };
        result = emit_expression(make_call("BoxSet", args, 2, 1), ctx);
        EXPECT(strcmp(result, "pgy_box_set_Int(&boxed, 42)") == 0);
        free(result);
        transpiler_ctx_destroy(ctx);
    }

    TEST("ToTObject(PlayerDto, player) -> tobject projection literal");
    {
        const char *source =
            "tobject PlayerDto { hp: Int; name: String; }\n"
            "subject Player { let hp: Int; let name: String; }\n"
            "func Main() -> Void {\n"
            "    let player: Player = Player();\n"
            "    let snapshot: PlayerDto = ToTObject(PlayerDto, player);\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        HIRProgram *hir = NULL;
        RIRProgram *rir = NULL;
        MIRProgram *mir = lower_program_to_mir_strict(program, &hir, &rir);
        TranspilerCtx *ctx = transpiler_ctx_create();

        emit_program(ctx);

        EXPECT_STR_CONTAINS(ctx->out->data, "PlayerDto");
        EXPECT_STR_CONTAINS(ctx->out->data, "= (PlayerDto){ .hp =");
        EXPECT_STR_CONTAINS(ctx->out->data, ".name =");

        transpiler_ctx_destroy(ctx);
        mir_destroy(mir);
        rir_destroy(rir);
        hir_destroy(hir);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("ToObject(PlayerView, player) -> object projection literal");
    {
        const char *source =
            "object PlayerView { hp: Int; name: String; }\n"
            "subject Player { let hp: Int; let name: String; }\n"
            "func Main() -> Void {\n"
            "    let player: Player = Player();\n"
            "    let view: PlayerView = ToObject(PlayerView, player);\n"
            "    Log(view.hp);\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        HIRProgram *hir = NULL;
        RIRProgram *rir = NULL;
        MIRProgram *mir = lower_program_to_mir_strict(program, &hir, &rir);
        TranspilerCtx *ctx = transpiler_ctx_create();

        emit_program(ctx);

        EXPECT_STR_CONTAINS(ctx->out->data, "PlayerView");
        EXPECT_STR_CONTAINS(ctx->out->data, "= (PlayerView){ .hp =");
        EXPECT_STR_CONTAINS(ctx->out->data, ".name =");
        EXPECT_STR_CONTAINS(ctx->out->data, "pgy_log(");

        transpiler_ctx_destroy(ctx);
        mir_destroy(mir);
        rir_destroy(rir);
        hir_destroy(hir);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("nested vessel-backed projection lowers through subject field paths");
    {
        const char *source =
            "vessel Cycle { age: Int; fatigue: Int; }\n"
            "vessel Traits { metabolism: Int; }\n"
            "subject Creature { vessel cycle: Cycle; vessel traits: Traits; }\n"
            "object CreatureView { age: Int; fatigue: Int; metabolism: Int; }\n"
            "tobject CreaturePacket { age: Int; metabolism: Int; }\n"
            "func Main() -> Void {\n"
            "    let creature: Creature = Creature();\n"
            "    let view: CreatureView = ToObject(CreatureView, creature);\n"
            "    let packet: CreaturePacket = ToTObject(CreaturePacket, creature);\n"
            "    Log(view.metabolism);\n"
            "    Log(packet.metabolism);\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        HIRProgram *hir = NULL;
        RIRProgram *rir = NULL;
        MIRProgram *mir = lower_program_to_mir_strict(program, &hir, &rir);
        TranspilerCtx *ctx = transpiler_ctx_create();

        emit_program(ctx);

        EXPECT_STR_CONTAINS(ctx->out->data, "CreatureView");
        EXPECT_STR_CONTAINS(ctx->out->data, "CreaturePacket");
        EXPECT_STR_CONTAINS(ctx->out->data, ".cycle.age");
        EXPECT_STR_CONTAINS(ctx->out->data, ".traits.metabolism");
        EXPECT_STR_CONTAINS(ctx->out->data, "pgy_log(");

        transpiler_ctx_destroy(ctx);
        mir_destroy(mir);
        rir_destroy(rir);
        hir_destroy(hir);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("AllocatorTracing() -> pgy_allocator_tracing()");
    {
        ctx = transpiler_ctx_create();
        result = emit_expression(make_call("AllocatorTracing", NULL, 0, 1), ctx);
        EXPECT(strcmp(result, "pgy_allocator_tracing()") == 0);
        free(result);
        transpiler_ctx_destroy(ctx);
    }

    TEST("HasState(poisoned) -> zone semantic placeholder outside zone context");
    {
        ctx = transpiler_ctx_create();
        ASTNode *args[1] = { make_identifier("poisoned", 1) };
        result = emit_expression(make_call("HasState", args, 1, 1), ctx);
        EXPECT(strcmp(result, "false") == 0);
        EXPECT(ctx->backend_error != NULL);
        EXPECT_STR_CONTAINS(ctx->backend_error,
            "C backend: HasState requires active zone context");
        free(result);
        transpiler_ctx_destroy(ctx);
    }

    TEST("HasState(allied, player, enemy) -> zone semantic placeholder outside zone context");
    {
        ctx = transpiler_ctx_create();
        ASTNode *args[3] = {
            make_identifier("allied", 1),
            make_identifier("player", 1),
            make_identifier("enemy", 1)
        };
        result = emit_expression(make_call("HasState", args, 3, 1), ctx);
        EXPECT(strcmp(result, "false") == 0);
        EXPECT(ctx->backend_error != NULL);
        EXPECT_STR_CONTAINS(ctx->backend_error,
            "C backend: HasState requires active zone context");
        free(result);
        transpiler_ctx_destroy(ctx);
    }

    TEST("HasZone(battle) -> world semantic placeholder outside world context");
    {
        ctx = transpiler_ctx_create();
        ASTNode *args[1] = { make_identifier("battle", 1) };
        result = emit_expression(make_call("HasZone", args, 1, 1), ctx);
        EXPECT(strcmp(result, "false") == 0);
        EXPECT(ctx->backend_error != NULL);
        EXPECT_STR_CONTAINS(ctx->backend_error,
            "C backend: HasZone requires active world context");
        free(result);
        transpiler_ctx_destroy(ctx);
    }

    TEST("HasLayer(poison) -> zone semantic placeholder outside zone context");
    {
        ctx = transpiler_ctx_create();
        ASTNode *args[1] = { make_identifier("poison", 1) };
        result = emit_expression(make_call("HasLayer", args, 1, 1), ctx);
        EXPECT(strcmp(result, "false") == 0);
        EXPECT(ctx->backend_error != NULL);
        EXPECT_STR_CONTAINS(ctx->backend_error,
            "C backend: HasLayer requires active zone context");
        free(result);
        transpiler_ctx_destroy(ctx);
    }

    TEST("HasProjection(snapshot) -> domain semantic placeholder outside domain context");
    {
        ctx = transpiler_ctx_create();
        ASTNode *args[1] = { make_identifier("snapshot", 1) };
        result = emit_expression(make_call("HasProjection", args, 1, 1), ctx);
        EXPECT(strcmp(result, "false") == 0);
        EXPECT(ctx->backend_error != NULL);
        EXPECT_STR_CONTAINS(ctx->backend_error,
            "C backend: HasProjection requires active relation/effect/zone projection context");
        free(result);
        transpiler_ctx_destroy(ctx);
    }

    TEST("HasProjection lowers to relation/effect/zone runtime projection flag inside domain context");
    {
        const char *source =
            "subject Player { let hp: Int; let name: String; }\n"
            "object PlayerView { hp: Int; }\n"
            "tobject PlayerDto { hp: Int; name: String; }\n"
            "relation TrustedLink for source: Player, target: Player {\n"
            "    subject slot left: Player\n"
            "    subject slot right: Player\n"
            "    object slot playerView: PlayerView\n"
            "    bind playerView from left\n"
            "}\n"
            "effect Poisoned for bearer: Player {\n"
            "    subject slot player: Player\n"
            "    tobject slot snapshot: PlayerDto\n"
            "    bind snapshot from player\n"
            "}\n"
            "zone BattleZone {\n"
            "    subject slot player: Player\n"
            "    object slot playerView: PlayerView\n"
            "    tobject slot snapshot: PlayerDto\n"
            "    bind playerView from player\n"
            "    bind snapshot from player\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        HIRProgram *hir = NULL;
        RIRProgram *rir = NULL;
        MIRProgram *mir = lower_program_to_mir_strict(program, &hir, &rir);

        ctx = transpiler_ctx_create();
        ctx->mir = mir;

        ctx->current_host_decl = find_test_decl(mir, AST_RELATION_DECL, "TrustedLink");
        {
            ASTNode *args[1] = { make_identifier("playerView", 1) };
            result = emit_expression(make_call("HasProjection", args, 1, 1), ctx);
            EXPECT(strcmp(result, "self->__projection_ready_playerView") == 0);
            free(result);
        }
        ctx->current_host_decl = NULL;

        ctx->current_host_decl = find_test_decl(mir, AST_EFFECT_DECL, "Poisoned");
        {
            ASTNode *args[1] = { make_identifier("snapshot", 1) };
            result = emit_expression(make_call("HasProjection", args, 1, 1), ctx);
            EXPECT(strcmp(result, "self->__projection_ready_snapshot") == 0);
            free(result);
        }
        ctx->current_host_decl = NULL;

        ctx->current_host_decl = find_test_decl(mir, AST_ZONE_DECL, "BattleZone");
        {
            ASTNode *args[1] = { make_identifier("snapshot", 1) };
            result = emit_expression(make_call("HasProjection", args, 1, 1), ctx);
            EXPECT(strcmp(result, "self->__projection_ready_snapshot") == 0);
            free(result);
        }
        {
            ASTNode *args[1] = { make_identifier("playerView", 1) };
            result = emit_expression(make_call("HasProjection", args, 1, 1), ctx);
            EXPECT(strcmp(result, "self->__projection_ready_playerView") == 0);
            free(result);
        }

        transpiler_ctx_destroy(ctx);
        mir_destroy(mir);
        rir_destroy(rir);
        hir_destroy(hir);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("LLVM domain layouts include projection-ready flags for relation/effect/zone");
    {
        const char *source =
            "subject Player { let hp: Int; let name: String; }\n"
            "object PlayerView { hp: Int; }\n"
            "tobject PlayerDto { hp: Int; name: String; }\n"
            "relation TrustedLink for source: Player, target: Player {\n"
            "    subject slot left: Player\n"
            "    subject slot right: Player\n"
            "    object slot playerView: PlayerView\n"
            "    tobject slot snapshot: PlayerDto\n"
            "    bind playerView from left\n"
            "    bind snapshot from right\n"
            "}\n"
            "effect Poisoned for bearer: Player {\n"
            "    subject slot player: Player\n"
            "    object slot playerView: PlayerView\n"
            "    tobject slot snapshot: PlayerDto\n"
            "    bind playerView from player\n"
            "    bind snapshot from player\n"
            "}\n"
            "zone BattleZone {\n"
            "    subject slot player: Player\n"
            "    object slot playerView: PlayerView\n"
            "    tobject slot snapshot: PlayerDto\n"
            "    bind playerView from player\n"
            "    bind snapshot from player\n"
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

        EXPECT_STR_CONTAINS(ctx->out->data, "bool __projection_ready_playerView;");
        EXPECT_STR_CONTAINS(ctx->out->data, "bool __projection_ready_snapshot;");
        EXPECT_STR_CONTAINS(ctx->out->data, "bool __projection_dirty_playerView;");
        EXPECT_STR_CONTAINS(ctx->out->data, "bool __projection_dirty_snapshot;");

        transpiler_ctx_destroy(ctx);
        mir_destroy(mir);
        rir_destroy(rir);
        hir_destroy(hir);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("HasLayer lowers to zone runtime helper inside zone context");
    {
        const char *source =
            "subject Player { let hp: Int; }\n"
            "effect Poisoned for bearer: Player { }\n"
            "relation TrustedLink for source: Player, target: Player { }\n"
            "zone BattleZone {\n"
            "    subject slot player: Player\n"
            "    subject slot enemy: Player\n"
            "    effect slot poison: Poisoned\n"
            "    relation slot trust: TrustedLink\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        HIRProgram *hir = NULL;
        RIRProgram *rir = NULL;
        MIRProgram *mir = lower_program_to_mir_strict(program, &hir, &rir);

        ctx = transpiler_ctx_create();
        ctx->mir = mir;
        ctx->current_host_decl = find_test_decl(mir, AST_ZONE_DECL, "BattleZone");

        {
            ASTNode *args[1] = { make_identifier("poison", 1) };
            result = emit_expression(make_call("HasLayer", args, 1, 1), ctx);
            EXPECT(strcmp(result, "BattleZone_has_layer_poison(self, __pgy_zone_gen)") == 0);
            free(result);
        }

        {
            ASTNode *args[1] = { make_identifier("trust", 1) };
            result = emit_expression(make_call("HasLayer", args, 1, 1), ctx);
            EXPECT(strcmp(result, "BattleZone_has_layer_trust(self, __pgy_zone_gen)") == 0);
            free(result);
        }

        transpiler_ctx_destroy(ctx);
        mir_destroy(mir);
        rir_destroy(rir);
        hir_destroy(hir);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("zone effect pool emits pooled storage and HasLayer helper scaffolding");
    {
        const char *source =
            "subject Player { let hp: Int; }\n"
            "effect DamageEffect for bearer: Player { }\n"
            "zone BattleZone {\n"
            "    subject slot player: Player\n"
            "    effect pool damage: DamageEffect capacity 8\n"
            "    apply damage to player\n"
            "    func Tick() -> Void {\n"
            "        if HasLayer(damage) {\n"
            "            Log(1);\n"
            "        }\n"
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

        EXPECT_STR_CONTAINS(ctx->out->data, "struct { DamageEffect items[8]; bool active[8]; uint8_t count; uint8_t cap; } damage;");
        EXPECT_STR_CONTAINS(ctx->out->data, "PGY_EFFECT_POOL_INIT(self->damage);");
        EXPECT_STR_CONTAINS(ctx->out->data, "PGY_EFFECT_POOL_APPLY(self->damage, _pgy_damage_instance);");
        EXPECT_STR_CONTAINS(ctx->out->data, "static inline bool\nBattleZone_has_layer_damage(BattleZone *self, uint32_t expected_gen)");
        EXPECT_STR_CONTAINS(ctx->out->data, "PGY_ZONE_RDLOCK(self);");
        EXPECT_STR_CONTAINS(ctx->out->data, "PGY_ZONE_GENERATION_WARN_IF_STALE(self, expected_gen, \"BattleZone.damage\")");
        EXPECT_STR_CONTAINS(ctx->out->data, "__pgy_zone_gen");

        transpiler_ctx_destroy(ctx);
        mir_destroy(mir);
        rir_destroy(rir);
        hir_destroy(hir);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("HasState lowers to zone runtime state field inside zone context");
    {
        const char *source =
            "subject Player { let hp: Int; }\n"
            "effect Poisoned for bearer: Player { }\n"
            "relation TrustedLink for source: Player, target: Player { }\n"
            "zone BattleZone {\n"
            "    subject slot player: Player\n"
            "    subject slot enemy: Player\n"
            "    effect slot poison: Poisoned\n"
            "    relation slot trust: TrustedLink\n"
            "    state poisoned: effect poison on player\n"
            "    state allied: relation trust between player, enemy\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        HIRProgram *hir = NULL;
        RIRProgram *rir = NULL;
        MIRProgram *mir = lower_program_to_mir_strict(program, &hir, &rir);

        ctx = transpiler_ctx_create();
        ctx->mir = mir;
        ctx->current_host_decl = find_test_decl(mir, AST_ZONE_DECL, "BattleZone");

        {
            ASTNode *args[1] = { make_identifier("poisoned", 1) };
            result = emit_expression(make_call("HasState", args, 1, 1), ctx);
            EXPECT(strcmp(result, "self->__state_poisoned") == 0);
            free(result);
        }

        {
            ASTNode *args[3] = {
                make_identifier("allied", 1),
                make_identifier("player", 1),
                make_identifier("enemy", 1)
            };
            result = emit_expression(make_call("HasState", args, 3, 1), ctx);
            EXPECT(strcmp(result, "self->__state_allied") == 0);
            free(result);
        }

        transpiler_ctx_destroy(ctx);
        mir_destroy(mir);
        rir_destroy(rir);
        hir_destroy(hir);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("HasZone lowers to world runtime zone fields inside world context");
    {
        const char *source =
            "zone BattleZone { }\n"
            "world GameWorld {\n"
            "    zone battle: BattleZone\n"
            "    state liveBattle: zone battle\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        HIRProgram *hir = NULL;
        RIRProgram *rir = NULL;
        MIRProgram *mir = lower_program_to_mir_strict(program, &hir, &rir);

        ctx = transpiler_ctx_create();
        ctx->mir = mir;
        ctx->current_host_decl = find_test_decl(mir, AST_WORLD_DECL, "GameWorld");

        {
            ASTNode *args[1] = { make_identifier("liveBattle", 1) };
            result = emit_expression(make_call("HasZone", args, 1, 1), ctx);
            EXPECT(strcmp(result, "self->__zone_state_liveBattle") == 0);
            free(result);
        }

        {
            ASTNode *args[1] = { make_identifier("battle", 1) };
            result = emit_expression(make_call("HasZone", args, 1, 1), ctx);
            EXPECT(strcmp(result, "self->__zone_active_battle") == 0);
            free(result);
        }

        transpiler_ctx_destroy(ctx);
        mir_destroy(mir);
        rir_destroy(rir);
        hir_destroy(hir);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }
}

/* -----------------------------------------------------------------
 * Tests: statement emitters
 * ----------------------------------------------------------------- */
