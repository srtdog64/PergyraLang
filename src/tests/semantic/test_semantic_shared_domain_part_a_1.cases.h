static void
test_shared_memory_features(void)
{
    printf("\n[shared_memory]\n");

    TEST("Rc<Int> annotation resolves to constructed type");
    {
        SemanticContext *ctx = semantic_context_create();
        ASTNode *rc_type = make_generic_type("Rc", "Int");
        Type *resolved = semantic_type_resolution_lookup_metadata_type_ref(
            ctx, rc_type);

        EXPECT(resolved->kind == TYPE_KIND_CONSTRUCTED
               && strcmp(resolved->name, "Rc<Int>") == 0);

        semantic_context_destroy(ctx);
        ast_destroy(rc_type);
    }

    TEST("RcClone returns same Rc<T> type");
    {
        SemanticContext *ctx = semantic_context_create();
        Type *args[1] = { TYPE_INT };
        Type *rc_type = type_create_constructed(TYPE_RC, args, 1);
        scope_declare(ctx->scope,
            symbol_create_variable("shared", rc_type, 1, 1));

        ASTNode *call = make_call("RcClone", (ASTNode *[]){ make_identifier("shared", 2) }, 1, 2);
        Type *resolved = type_check_expression(call, ctx);

        EXPECT(type_equals(resolved, rc_type) && !ctx->has_error);

        semantic_context_destroy(ctx);
        ast_destroy(call);
    }

    TEST("RcDowngrade returns Weak<T>");
    {
        SemanticContext *ctx = semantic_context_create();
        Type *args[1] = { TYPE_INT };
        Type *rc_type = type_create_constructed(TYPE_RC, args, 1);
        scope_declare(ctx->scope,
            symbol_create_variable("shared", rc_type, 1, 1));

        ASTNode *call = make_call("RcDowngrade", (ASTNode *[]){ make_identifier("shared", 2) }, 1, 2);
        Type *resolved = type_check_expression(call, ctx);

        EXPECT(resolved->kind == TYPE_KIND_CONSTRUCTED
               && strcmp(resolved->name, "Weak<Int>") == 0
               && !ctx->has_error);

        semantic_context_destroy(ctx);
        ast_destroy(call);
    }

    TEST("Rc beta stable subset rejects non-primitive payloads");
    {
        SemanticContext *ctx = semantic_context_create();
        Type *hero_type = type_create_primitive("Hero", 0, false);
        Type *args[1] = { hero_type };
        Type *rc_type = type_create_constructed(TYPE_RC, args, 1);
        scope_declare(ctx->scope,
            symbol_create_variable("shared", rc_type, 1, 1));

        ASTNode *call = make_call("RcGet", (ASTNode *[]){ make_identifier("shared", 2) }, 1, 2);
        Type *resolved = type_check_expression(call, ctx);

        EXPECT(resolved == TYPE_UNKNOWN && ctx->has_error);
        EXPECT(ctx_has_diagnostic_substring(ctx,
            "beta-stable shared ownership supports only Int, Long, Float, Double, Bool, or String payloads"));

        semantic_context_destroy(ctx);
        ast_destroy(call);
    }

    TEST("AllocatorPool returns Allocator type");
    {
        SemanticContext *ctx = semantic_context_create();
        ASTNode *call = make_call("AllocatorPool", (ASTNode *[]){ make_number(1024, 1) }, 1, 1);
        Type *resolved = type_check_expression(call, ctx);

        EXPECT(resolved == TYPE_ALLOCATOR && !ctx->has_error);

        semantic_context_destroy(ctx);
        ast_destroy(call);
    }

    TEST("allocator lane constructors return Allocator type");
    {
        const char *names[] = {
            "AllocatorScratch",
            "AllocatorResult",
            "AllocatorPersistent",
        };

        for (size_t i = 0; i < sizeof(names) / sizeof(names[0]); i++) {
            SemanticContext *ctx = semantic_context_create();
            ASTNode *call = make_call(names[i], NULL, 0, 1);
            Type *resolved = type_check_expression(call, ctx);

            EXPECT(resolved == TYPE_ALLOCATOR && !ctx->has_error);

            semantic_context_destroy(ctx);
            ast_destroy(call);
        }
    }

    TEST("Box<Array<Int>> let declaration accepts BoxArray initializer");
    {
        SemanticContext *ctx = semantic_context_create();
        ASTNode *array_type = make_generic_type("Array", "Int");
        ASTNode *boxed_array = make_generic_type_from_node("Box", array_type);
        ASTNode *call = make_call("BoxArray", (ASTNode *[]){ make_number(64, 1) }, 1, 1);
        ASTNode *decl = ast_create_let_declaration("storage");
        decl->data.let_decl.type = boxed_array;
        decl->data.let_decl.initializer = call;

        type_check_let_decl(decl, ctx);
        EXPECT(!ctx->has_error);

        semantic_context_destroy(ctx);
        ast_destroy(decl);
    }

    TEST("BoxGet returns inner class object type");
    {
        const char *source =
            "class Vec2 {\n"
            "    let x: Int;\n"
            "    let y: Int;\n"
            "}\n"
            "func Main() -> Void {\n"
            "    let handle: Box<Vec2> = Box(Vec2(3, 7));\n"
            "    let value: Vec2 = BoxGet(handle);\n"
            "    Log(value.x);\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticResult *result = semantic_analyze(program);

        EXPECT(!parser_has_error(parser));
        if (result != NULL && result->error_count > 0)
            semantic_result_print(result);
        EXPECT(result != NULL && result->error_count == 0);

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("BoxSet requires assignable inner value");
    {
        SemanticContext *ctx = semantic_context_create();
        Type *args[1] = { TYPE_INT };
        Type *box_type = type_create_constructed(TYPE_BOX, args, 1);
        scope_declare(ctx->scope,
            symbol_create_variable("boxed", box_type, 1, 1));

        ASTNode *call = make_call("BoxSet",
            (ASTNode *[]){ make_identifier("boxed", 2), make_number(42, 2) }, 2, 2);
        Type *resolved = type_check_expression(call, ctx);

        EXPECT(resolved == TYPE_VOID && !ctx->has_error);

        semantic_context_destroy(ctx);
        ast_destroy(call);
    }

    TEST("BoxIsValid returns Bool");
    {
        SemanticContext *ctx = semantic_context_create();
        Type *args[1] = { TYPE_STRING };
        Type *box_type = type_create_constructed(TYPE_BOX, args, 1);
        scope_declare(ctx->scope,
            symbol_create_variable("boxed", box_type, 1, 1));

        ASTNode *call = make_call("BoxIsValid",
            (ASTNode *[]){ make_identifier("boxed", 2) }, 1, 2);
        Type *resolved = type_check_expression(call, ctx);

        EXPECT(resolved == TYPE_BOOL && !ctx->has_error);

        semantic_context_destroy(ctx);
        ast_destroy(call);
    }

    TEST("Box<class> can be returned and passed explicitly");
    {
        const char *source =
            "class Vec2 {\n"
            "    let x: Int;\n"
            "    let y: Int;\n"
            "}\n"
            "func MakeVec() -> Box<Vec2> {\n"
            "    return Box(Vec2(1, 2));\n"
            "}\n"
            "func SumX(cell: Box<Vec2>) -> Int {\n"
            "    return BoxGet(cell).x;\n"
            "}\n"
            "func Main() -> Void {\n"
            "    let handle: Box<Vec2> = MakeVec();\n"
            "    Log(SumX(handle));\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticResult *result = semantic_analyze(program);

        EXPECT(!parser_has_error(parser));
        if (result != NULL && result->error_count > 0)
            semantic_result_print(result);
        EXPECT(result != NULL && result->error_count == 0);

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    printf("\n[domain_layer]\n");
    fflush(stdout);

    TEST("relation/effect/zone declarations pass");
    {
        SemanticContext *ctx = semantic_context_create();
        scope_enter(&ctx->scope, SCOPE_GLOBAL);

        ASTNode *relation = ast_create_relation_declaration("TrustedLink");
        relation->line = 1; relation->column = 1;
        ASTNode *effect = ast_create_effect_declaration("Poisoned");
        effect->line = 2; effect->column = 1;
        ASTNode *zone = ast_create_zone_declaration("DungeonZone");
        zone->line = 3; zone->column = 1;

        type_check_relation_decl(relation, ctx);
        type_check_effect_decl(effect, ctx);
        type_check_zone_decl(zone, ctx);
        EXPECT(!ctx->has_error);

        semantic_context_destroy(ctx);
        ast_destroy(relation);
        ast_destroy(effect);
        ast_destroy(zone);
    }

    TEST("relation/effect/zone/world minimal composition parses and subject slots require subject type");
    {
        const char *source =
            "subject Player { let hp: Int; }\n"
            "object PlayerView { hp: Int; }\n"
            "relation TrustedLink for source: Player, target: Player {\n"
            "    object slot snapshot: PlayerView\n"
            "    shared trust: Int = 100\n"
            "}\n"
            "effect Poisoned for bearer: Player {\n"
            "    object slot view: PlayerView\n"
            "    shared stacks: Int = 1\n"
            "}\n"
            "zone BattleZone {\n"
            "    subject slot player: Player\n"
            "    subject slot enemy: Player\n"
            "    object slot playerView: PlayerView\n"
            "    relation slot trust: TrustedLink\n"
            "    effect slot poison: Poisoned\n"
            "    apply poison to player\n"
            "    link trust between player, enemy\n"
            "    detach poison from enemy\n"
            "    unlink trust between player, enemy\n"
            "    shared round: Int = 1\n"
            "}\n"
            "world GameWorld {\n"
            "    zone battle: BattleZone\n"
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

    TEST("relation/effect warn when no subject endpoint or target is declared");
    {
        const char *source =
            "relation LooseLink {\n"
            "    shared trust: Int = 100\n"
            "}\n"
            "effect AmbientFog {\n"
            "    shared density: Int = 1\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticResult *result = semantic_analyze(program);

        EXPECT(!parser_has_error(parser));
        EXPECT(result != NULL && result->error_count == 0);
        EXPECT(result != NULL && result->warning_count >= 2);

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("zone/world references reject missing relation/effect/zone types");
    {
        const char *source =
            "subject Player { let hp: Int; }\n"
            "object PlayerView { hp: Int; }\n"
            "zone BattleZone {\n"
            "    subject slot player: Player\n"
            "    object slot playerView: PlayerView\n"
            "    relation slot trust: MissingRelation\n"
            "    effect slot poison: MissingEffect\n"
            "}\n"
            "world GameWorld {\n"
            "    zone battle: MissingZone\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticResult *result = semantic_analyze(program);

        EXPECT(!parser_has_error(parser));
        EXPECT(result != NULL && result->error_count >= 3);

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("zone object/tobject slot initializer can project from subject slot without direct-projection warning");
    {
        const char *source =
            "subject Player { let hp: Int; let name: String; }\n"
            "object PlayerView { hp: Int; }\n"
            "tobject PlayerDto { hp: Int; name: String; }\n"
            "zone BattleZone {\n"
            "    subject slot player: Player\n"
            "    object slot playerView: PlayerView = ToObject(PlayerView, player)\n"
            "    tobject slot snapshot: PlayerDto = ToTObject(PlayerDto, player)\n"
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

    TEST("zone refresh projects object slot from subject slot");
    {
        const char *source =
            "subject Player { let hp: Int; let name: String; }\n"
            "object PlayerView { hp: Int; }\n"
            "zone BattleZone {\n"
            "    subject slot player: Player\n"
            "    object slot playerView: PlayerView\n"
            "    refresh playerView from player\n"
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

    TEST("zone tobject slot and publish project tobject from subject slot");
    {
        const char *source =
            "subject Player { let hp: Int; let name: String; }\n"
            "tobject PlayerDto { hp: Int; name: String; }\n"
            "zone BattleZone {\n"
            "    subject slot player: Player\n"
            "    tobject slot snapshot: PlayerDto = ToTObject(PlayerDto, player)\n"
            "    publish snapshot from player\n"
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

    TEST("zone bind derives object and tobject projection targets from slot kind");
    {
        const char *source =
            "subject Player { let hp: Int; let name: String; }\n"
            "object PlayerView { hp: Int; }\n"
            "tobject PlayerDto { hp: Int; name: String; }\n"
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
        SemanticResult *result = semantic_analyze(program);

        EXPECT(!parser_has_error(parser));
        EXPECT(result != NULL && result->error_count == 0);

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("zone publish requires tobject slot target");
    {
        const char *source =
            "subject Player { let hp: Int; }\n"
            "object PlayerView { hp: Int; }\n"
            "zone BattleZone {\n"
            "    subject slot player: Player\n"
            "    object slot view: PlayerView\n"
            "    publish view from player\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticResult *result = semantic_analyze(program);

        EXPECT(!parser_has_error(parser));
        EXPECT(result != NULL && result->error_count == 1);

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }
