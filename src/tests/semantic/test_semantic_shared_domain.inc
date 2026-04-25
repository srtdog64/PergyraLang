static void
test_shared_memory_features(void)
{
    printf("\n[shared_memory]\n");

    TEST("Rc<Int> annotation resolves to constructed type");
    {
        SemanticContext *ctx = semantic_context_create();
        ASTNode *rc_type = make_generic_type("Rc", "Int");
        Type *resolved = resolve_type_node(rc_type, ctx);

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

    TEST("zone refresh rejects wrong slot kinds");
    {
        const char *source =
            "subject Player { let hp: Int; }\n"
            "object PlayerView { hp: Int; }\n"
            "tobject PlayerDto { hp: Int; }\n"
            "zone BrokenZone {\n"
            "    subject slot player: Player\n"
            "    object slot playerView: PlayerView\n"
            "    tobject slot packet: PlayerDto\n"
            "    refresh player from player\n"
            "    refresh playerView from packet\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticResult *result = semantic_analyze(program);

        EXPECT(!parser_has_error(parser));
        EXPECT(result != NULL && result->error_count == 2);

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("zone bind rejects non object/tobject targets and tobject sources");
    {
        const char *source =
            "subject Player { let hp: Int; }\n"
            "object PlayerView { hp: Int; }\n"
            "tobject PlayerDto { hp: Int; }\n"
            "zone BrokenZone {\n"
            "    subject slot player: Player\n"
            "    object slot playerView: PlayerView\n"
            "    tobject slot packet: PlayerDto\n"
            "    bind player from player\n"
            "    bind playerView from packet\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticResult *result = semantic_analyze(program);

        EXPECT(!parser_has_error(parser));
        EXPECT(result != NULL && result->error_count == 2);

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("zone slot initializer enforces declared slot type");
    {
        const char *source =
            "subject Player { let hp: Int; }\n"
            "object PlayerView { hp: Int; }\n"
            "zone BrokenZone {\n"
            "    subject slot player: Player\n"
            "    object slot bad: Int = ToObject(PlayerView, player)\n"
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

    TEST("zone maintain validates relation/effect contracts");
    {
        const char *source =
            "subject Player { let hp: Int; }\n"
            "zone BattleZone {\n"
            "    subject slot player: Player\n"
            "    subject slot enemy: Player\n"
            "    relation slot trust: TrustedLink\n"
            "    effect slot poison: Poisoned\n"
            "    authority player\n"
            "    maintain poison on player by player\n"
            "    maintain trust between player, enemy by player\n"
            "    state poisoned: effect poison on player\n"
            "    state allied: relation trust between player, enemy\n"
            "}\n"
            "relation TrustedLink for source: Player, target: Player { }\n"
            "effect Poisoned for bearer: Player { }\n";
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

    TEST("zone lifecycle statements can reference declared state aliases");
    {
        const char *source =
            "subject Player { let hp: Int; }\n"
            "zone BattleZone {\n"
            "    subject slot player: Player\n"
            "    subject slot enemy: Player\n"
            "    relation slot trust: TrustedLink\n"
            "    effect slot poison: Poisoned\n"
            "    authority player\n"
            "    state poisoned: effect poison on player\n"
            "    state allied: relation trust between player, enemy\n"
            "    apply poisoned by player\n"
            "    link allied by player\n"
            "    detach poisoned by player\n"
            "    unlink allied by player\n"
            "    maintain poisoned by player\n"
            "    maintain allied by player\n"
            "}\n"
            "relation TrustedLink for source: Player, target: Player { }\n"
            "effect Poisoned for bearer: Player { }\n";
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

    TEST("zone lifecycle state aliases reject wrong verb kind");
    {
        const char *source =
            "subject Player { let hp: Int; }\n"
            "zone BattleZone {\n"
            "    subject slot player: Player\n"
            "    subject slot enemy: Player\n"
            "    relation slot trust: TrustedLink\n"
            "    effect slot poison: Poisoned\n"
            "    authority player\n"
            "    state poisoned: effect poison on player\n"
            "    state allied: relation trust between player, enemy\n"
            "    apply allied by player\n"
            "    link poisoned by player\n"
            "}\n"
            "relation TrustedLink for source: Player, target: Player { }\n"
            "effect Poisoned for bearer: Player { }\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticResult *result = semantic_analyze(program);

        EXPECT(!parser_has_error(parser));
        EXPECT(result != NULL && result->error_count == 2);

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("zone authority requires declared subject slots and explicit by subject alias");
    {
        const char *source =
            "subject Player { let hp: Int; }\n"
            "object PlayerView { hp: Int; }\n"
            "zone BattleZone {\n"
            "    subject slot player: Player\n"
            "    object slot playerView: PlayerView\n"
            "    authority playerView\n"
            "    refresh playerView from player by playerView\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticResult *result = semantic_analyze(program);

        EXPECT(!parser_has_error(parser));
        EXPECT(result != NULL && result->error_count == 2);

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("zone authority can require abilities implemented by subject roles");
    {
        const char *source =
            "subject Player { let hp: Int; }\n"
            "ability Commandable { func Command() -> Void; }\n"
            "ability Damageable { func Hit() -> Void; }\n"
            "role PlayerCommander for Player {\n"
            "    impl ability Commandable { func Command() -> Void { Log(1); } }\n"
            "    impl ability Damageable { func Hit() -> Void { Log(1); } }\n"
            "}\n"
            "zone BattleZone {\n"
            "    subject slot player: Player\n"
            "    authority player requires Commandable, Damageable\n"
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

    TEST("zone authority ability requirements reject missing role impls");
    {
        const char *source =
            "subject Player { let hp: Int; }\n"
            "ability Commandable { func Command() -> Void; }\n"
            "ability Healable { func Heal() -> Void; }\n"
            "role PlayerCommander for Player {\n"
            "    impl ability Commandable { func Command() -> Void { Log(1); } }\n"
            "}\n"
            "zone BattleZone {\n"
            "    subject slot player: Player\n"
            "    authority player requires Commandable, Healable\n"
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

    TEST("zone rejects mutable rules that omit by subject alias when authority exists");
    {
        const char *source =
            "subject Player { let hp: Int; }\n"
            "zone BattleZone {\n"
            "    subject slot player: Player\n"
            "    effect slot poison: Poisoned\n"
            "    authority player\n"
            "    apply poison to player\n"
            "}\n"
            "effect Poisoned for bearer: Player { }\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticResult *result = semantic_analyze(program);

        EXPECT(!parser_has_error(parser));
        EXPECT(result != NULL && result->error_count >= 1);

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("zone maintain warns on duplicate and conflicting lifecycle rules");
    {
        const char *source =
            "subject Player { let hp: Int; }\n"
            "zone BattleZone {\n"
            "    subject slot player: Player\n"
            "    subject slot enemy: Player\n"
            "    relation slot trust: TrustedLink\n"
            "    effect slot poison: Poisoned\n"
            "    authority player\n"
            "    maintain poison on player by player\n"
            "    maintain poison on player by player\n"
            "    detach poison from player by player\n"
            "    maintain trust between player, enemy by player\n"
            "    maintain trust between player, enemy by player\n"
            "    unlink trust between player, enemy by player\n"
            "}\n"
            "relation TrustedLink for source: Player, target: Player { }\n"
            "effect Poisoned for bearer: Player { }\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticResult *result = semantic_analyze(program);

        EXPECT(!parser_has_error(parser));
        EXPECT(result != NULL && result->error_count == 0);
        EXPECT(result != NULL && result->warning_count >= 4);

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("zone apply rejects unknown effect or target slot");
    {
        const char *source =
            "subject Player { let hp: Int; }\n"
            "zone BattleZone {\n"
            "    subject slot player: Player\n"
            "    effect slot poison: Poisoned\n"
            "    apply missing to player\n"
            "    apply poison to missingTarget\n"
            "}\n"
            "effect Poisoned for bearer: Player { }\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticResult *result = semantic_analyze(program);

        EXPECT(!parser_has_error(parser));
        EXPECT(result != NULL && result->error_count == 2);

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("zone link rejects unknown relation or endpoint slot");
    {
        const char *source =
            "subject Player { let hp: Int; }\n"
            "zone BattleZone {\n"
            "    subject slot player: Player\n"
            "    subject slot enemy: Player\n"
            "    relation slot trust: TrustedLink\n"
            "    link missing between player, enemy\n"
            "    link trust between player, missingEnemy\n"
            "}\n"
            "relation TrustedLink for source: Player, target: Player { }\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticResult *result = semantic_analyze(program);

        EXPECT(!parser_has_error(parser));
        EXPECT(result != NULL && result->error_count == 2);

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("zone detach and unlink reject unknown slots");
    {
        const char *source =
            "subject Player { let hp: Int; }\n"
            "zone BattleZone {\n"
            "    subject slot player: Player\n"
            "    subject slot enemy: Player\n"
            "    relation slot trust: TrustedLink\n"
            "    effect slot poison: Poisoned\n"
            "    detach missing from player\n"
            "    detach poison from missingEnemy\n"
            "    unlink missing between player, enemy\n"
            "    unlink trust between player, missingEnemy\n"
            "}\n"
            "relation TrustedLink for source: Player, target: Player { }\n"
            "effect Poisoned for bearer: Player { }\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticResult *result = semantic_analyze(program);

        EXPECT(!parser_has_error(parser));
        EXPECT(result != NULL && result->error_count == 4);

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("zone link enforces relation endpoint types and arity");
    {
        const char *source =
            "subject Player { let hp: Int; }\n"
            "subject Monster { let hp: Int; }\n"
            "zone BattleZone {\n"
            "    subject slot player: Player\n"
            "    subject slot enemy: Monster\n"
            "    relation slot trust: TrustedLink\n"
            "    relation slot crowd: CrowdLink\n"
            "    link trust between player, enemy\n"
            "    link crowd between player, enemy\n"
            "}\n"
            "relation TrustedLink for source: Player, target: Player { }\n"
            "relation CrowdLink for a: Player, b: Player, c: Player { }\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticResult *result = semantic_analyze(program);

        EXPECT(!parser_has_error(parser));
        EXPECT(result != NULL && result->error_count == 2);

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("zone apply enforces effect target type and arity");
    {
        const char *source =
            "subject Player { let hp: Int; }\n"
            "subject Monster { let hp: Int; }\n"
            "zone BattleZone {\n"
            "    subject slot player: Player\n"
            "    effect slot curse: Cursed\n"
            "    effect slot split: SplitMind\n"
            "    apply curse to player\n"
            "    apply split to player\n"
            "}\n"
            "effect Cursed for bearer: Monster { }\n"
            "effect SplitMind for a: Player, b: Player { }\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticResult *result = semantic_analyze(program);

        EXPECT(!parser_has_error(parser));
        EXPECT(result != NULL && result->error_count == 2);

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("zone warns when subject count exceeds recommended shape");
    {
        const char *source =
            "subject Player { let hp: Int; }\n"
            "zone BusyZone {\n"
            "    subject slot a: Player\n"
            "    subject slot b: Player\n"
            "    subject slot c: Player\n"
            "    subject slot d: Player\n"
            "    subject slot e: Player\n"
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

    TEST("relation/effect/zone reject non-subject type in subject slot");
    {
        const char *source =
            "struct PlayerView { hp: Int; }\n"
            "relation BrokenLink {\n"
            "    subject slot source: PlayerView\n"
            "}\n"
            "effect BrokenEffect {\n"
            "    subject slot bearer: PlayerView\n"
            "}\n"
            "zone BrokenZone {\n"
            "    subject slot player: PlayerView\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticResult *result = semantic_analyze(program);

        EXPECT(!parser_has_error(parser));
        EXPECT(result != NULL && result->error_count == 3);

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("zone apply and link accept object slots when effect/relation declare object targets");
    {
        const char *source =
            "object Door { hp: Int; }\n"
            "object Key { id: Int; }\n"
            "object DoorView { hp: Int; }\n"
            "effect Highlighted for object target: Door {\n"
            "    object slot view: DoorView\n"
            "    refresh view from target\n"
            "}\n"
            "relation KeyBinding for object door: Door, object key: Key {\n"
            "}\n"
            "zone LockZone {\n"
            "    object slot door: Door\n"
            "    object slot key: Key\n"
            "    effect slot glow: Highlighted\n"
            "    relation slot binding: KeyBinding\n"
            "    apply glow to door\n"
            "    link binding between door, key\n"
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

    TEST("zone apply and link enforce object endpoint types");
    {
        const char *source =
            "object Door { hp: Int; }\n"
            "object Key { id: Int; }\n"
            "object Coin { value: Int; }\n"
            "effect Highlighted for object target: Door {\n"
            "}\n"
            "relation KeyBinding for object door: Door, object key: Key {\n"
            "}\n"
            "zone LockZone {\n"
            "    object slot door: Door\n"
            "    object slot coin: Coin\n"
            "    effect slot glow: Highlighted\n"
            "    relation slot binding: KeyBinding\n"
            "    apply glow to coin\n"
            "    link binding between coin, door\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticResult *result = semantic_analyze(program);

        EXPECT(!parser_has_error(parser));
        EXPECT(result != NULL && result->error_count == 3);

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("relation/effect can refresh and publish projection slots from subject slots");
    {
        const char *source =
            "subject Player { let hp: Int; let name: String; }\n"
            "object PlayerView { hp: Int; }\n"
            "tobject PlayerDto { hp: Int; name: String; }\n"
            "relation TrustedLink for source: Player, target: Player {\n"
            "    object slot snapshot: PlayerView\n"
            "    tobject slot packet: PlayerDto\n"
            "    refresh snapshot from source\n"
            "    publish packet from target\n"
            "}\n"
            "effect Poisoned for bearer: Player {\n"
            "    object slot view: PlayerView\n"
            "    tobject slot packet: PlayerDto\n"
            "    refresh view from bearer\n"
            "    publish packet from bearer\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticResult *result = semantic_analyze(program);

        EXPECT(!parser_has_error(parser));
        EXPECT(result != NULL && result->error_count == 0);
        EXPECT(result != NULL && result->warning_count == 0);

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("relation/effect bind derives object and tobject projection targets from slot kind");
    {
        const char *source =
            "subject Player { let hp: Int; let name: String; }\n"
            "object PlayerView { hp: Int; }\n"
            "tobject PlayerDto { hp: Int; name: String; }\n"
            "relation TrustedLink for source: Player, target: Player {\n"
            "    object slot snapshot: PlayerView\n"
            "    tobject slot packet: PlayerDto\n"
            "    bind snapshot from source\n"
            "    bind packet from target\n"
            "}\n"
            "effect Poisoned for bearer: Player {\n"
            "    object slot view: PlayerView\n"
            "    tobject slot packet: PlayerDto\n"
            "    bind view from bearer\n"
            "    bind packet from bearer\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticResult *result = semantic_analyze(program);

        EXPECT(!parser_has_error(parser));
        EXPECT(result != NULL && result->error_count == 0);
        EXPECT(result != NULL && result->warning_count == 0);

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("relation/effect constructors are type-checked as nominal overlays");
    {
        const char *source =
            "subject Player { let hp: Int; let name: String; }\n"
            "relation TrustedLink for source: Player, target: Player { }\n"
            "effect Poisoned for bearer: Player { }\n"
            "func Main() -> Void {\n"
            "    let trust = TrustedLink(Player(7, \"src\"), Player(9, \"dst\"));\n"
            "    let poison = Poisoned(Player(5, \"bear\"));\n"
            "    Log(1);\n"
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

    TEST("relation/effect constructors reject mismatched positional field types");
    {
        const char *source =
            "subject Player { let hp: Int; let name: String; }\n"
            "relation TrustedLink for source: Player, target: Player { }\n"
            "effect Poisoned for bearer: Player { }\n"
            "func Main() -> Void {\n"
            "    let trust = TrustedLink(1, Player(9, \"dst\"));\n"
            "    let poison = Poisoned(2);\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticResult *result = semantic_analyze(program);

        EXPECT(!parser_has_error(parser));
        EXPECT(result != NULL && result->error_count >= 2);

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }


    TEST("relation/effect/zone reject legacy class in subject slot");
    {
        const char *source =
            "class PassivePlayer { let hp: Int; }\n"
            "relation BrokenLink {\n"
            "    subject slot source: PassivePlayer\n"
            "}\n"
            "effect BrokenEffect {\n"
            "    subject slot bearer: PassivePlayer\n"
            "}\n"
            "zone BrokenZone {\n"
            "    subject slot player: PassivePlayer\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticResult *result = semantic_analyze(program);

        EXPECT(!parser_has_error(parser));
        EXPECT(result != NULL && result->error_count == 3);

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("relation/effect/zone accept subject types in subject slot");
    {
        const char *source =
            "subject Bot {\n"
            "    let hp: Int;\n"
            "}\n"
            "relation ActiveLink {\n"
            "    subject slot source: Bot\n"
            "}\n"
            "effect ActiveEffect {\n"
            "    subject slot bearer: Bot\n"
            "}\n"
            "zone ActiveZone {\n"
            "    subject slot player: Bot\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticResult *result = semantic_analyze(program);

        EXPECT(!parser_has_error(parser));
        EXPECT(result != NULL && result->error_count == 0);
        EXPECT(result != NULL && result->warning_count == 0);

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }
    TEST("relation between subject, subject passes");
    {
        const char *source =
            "subject Player { let hp: Int; }\n"
            "relation Marriage between subject, subject\n"
            "{\n"
            "    shared years: Int = 0\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticResult *result = semantic_analyze(program);

        EXPECT(!parser_has_error(parser));
        EXPECT(result != NULL && result->error_count == 0);

        /* verify between fields were parsed */
        ASTNode *rel = program->data.program.statements[1];
        EXPECT(rel->data.relation_decl.between_left_kind == RELATION_ENDPOINT_SUBJECT);
        EXPECT(rel->data.relation_decl.between_right_kind == RELATION_ENDPOINT_SUBJECT);
        EXPECT(rel->data.relation_decl.between_left_type == NULL);
        EXPECT(rel->data.relation_decl.between_right_type == NULL);
        EXPECT(!rel->data.relation_decl.between_left_many);
        EXPECT(!rel->data.relation_decl.between_right_many);

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("relation between subject, class[] parses 1:N");
    {
        const char *source =
            "subject Player { let hp: Int; }\n"
            "class Item { let name: String; }\n"
            "relation Ownership between subject, class[]\n"
            "{\n"
            "    shared acquired_at: Int = 0\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticResult *result = semantic_analyze(program);

        EXPECT(!parser_has_error(parser));
        EXPECT(result != NULL && result->error_count == 0);

        ASTNode *rel = program->data.program.statements[2];
        EXPECT(rel->data.relation_decl.between_left_kind == RELATION_ENDPOINT_SUBJECT);
        EXPECT(rel->data.relation_decl.between_right_kind == RELATION_ENDPOINT_CLASS);
        EXPECT(rel->data.relation_decl.between_left_type == NULL);
        EXPECT(rel->data.relation_decl.between_right_type == NULL);
        EXPECT(!rel->data.relation_decl.between_left_many);
        EXPECT(rel->data.relation_decl.between_right_many);

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("relation between object, object rejects (no subject)");
    {
        const char *source =
            "object ViewA { x: Int; }\n"
            "object ViewB { y: Int; }\n"
            "relation BadLink between object, object\n"
            "{\n"
            "    shared weight: Int = 0\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticResult *result = semantic_analyze(program);

        EXPECT(!parser_has_error(parser));
        EXPECT(result != NULL && result->error_count > 0);

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("relation between subject[], subject[] parses N:N");
    {
        const char *source =
            "subject Nation { let name: String; }\n"
            "relation Alliance between subject[], subject[]\n"
            "{\n"
            "    shared trust: Int = 0\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticResult *result = semantic_analyze(program);

        EXPECT(!parser_has_error(parser));
        EXPECT(result != NULL && result->error_count == 0);

        ASTNode *rel = program->data.program.statements[1];
        EXPECT(rel->data.relation_decl.between_left_many);
        EXPECT(rel->data.relation_decl.between_right_many);

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("zone link uses relation between subject and object/class contracts");
    {
        const char *source =
            "subject Player { let hp: Int; }\n"
            "object Villager { let mood: Int; }\n"
            "class Item { let name: String; }\n"
            "relation Marriage between subject, object { shared years: Int = 0 }\n"
            "relation Ownership between subject, class { shared acquired_at: Int = 0 }\n"
            "zone Town {\n"
            "    subject slot player: Player\n"
            "    object slot wife: Villager\n"
            "    object slot wrongItemView: Villager\n"
            "    object slot wrongWifeView: Villager\n"
            "    object slot itemOwnerView: Villager\n"
            "    subject slot backup: Player\n"
            "    relation slot marriage: Marriage\n"
            "    relation slot ownership: Ownership\n"
            "    link marriage between player, wife\n"
            "    link ownership between player, wrongItemView\n"
            "    link marriage between player, backup\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticResult *result = semantic_analyze(program);

        EXPECT(!parser_has_error(parser));
        EXPECT(result != NULL && result->error_count >= 2);

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }
}

/* -----------------------------------------------------------------
 * Async system tests
 * ----------------------------------------------------------------- */

