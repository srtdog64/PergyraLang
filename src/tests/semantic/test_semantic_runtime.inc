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

    TEST("zone/world references warn when relation/effect/zone types are missing");
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
        EXPECT(result != NULL && result->error_count == 0);
        EXPECT(result != NULL && result->warning_count >= 3);

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

    TEST("zone warns when authority exists but mutable rules omit by subject alias");
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
        EXPECT(result != NULL && result->error_count == 0);
        EXPECT(result != NULL && result->warning_count >= 1);

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
}

/* -----------------------------------------------------------------
 * Parallel execution semantic tests
 * ----------------------------------------------------------------- */

static void
test_parallel_execution_semantics(void)
{
    printf("\n[parallel_execution_semantics]\n");

    TEST("subject declaration registers SYMBOL_CLASS");
    {
        SemanticContext *ctx = semantic_context_create();
        scope_enter(&ctx->scope, SCOPE_GLOBAL);

        ASTNode *subject_decl = ast_create_subject("Counter");
        subject_decl->line = 1; subject_decl->column = 1;
        type_check_class_decl(subject_decl, ctx);
        EXPECT(!ctx->has_error);

        /* Verify symbol was registered */
        Symbol *sym = scope_lookup(ctx->scope, "Counter");
        EXPECT(sym != NULL && sym->kind == SYMBOL_CLASS);

        semantic_context_destroy(ctx);
        ast_destroy(subject_decl);
    }

    TEST("duplicate subject declaration triggers error");
    {
        SemanticContext *ctx = semantic_context_create();
        scope_enter(&ctx->scope, SCOPE_GLOBAL);

        ASTNode *a1 = ast_create_subject("Counter");
        a1->line = 1; a1->column = 1;
        type_check_class_decl(a1, ctx);

        ASTNode *a2 = ast_create_subject("Counter");
        a2->line = 3; a2->column = 1;
        type_check_class_decl(a2, ctx);
        EXPECT(ctx->has_error);

        semantic_context_destroy(ctx);
        ast_destroy(a1);
        ast_destroy(a2);
    }

    TEST("role can bind to subject host");
    {
        const char *source =
            "subject Counter {\n"
            "    let count: Int;\n"
            "}\n"
            "ability Tickable { func Tick() -> Void; }\n"
            "role CounterRole for Counter {\n"
            "    impl ability Tickable { func Tick() -> Void { Log(1); } }\n"
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

    TEST("await rejects Future<Slot<T>> anchored handle payload");
    {
        SemanticContext *ctx = semantic_context_create();
        scope_enter(&ctx->scope, SCOPE_GLOBAL);
        ctx->in_async_func = true;

        Type *slot_type = type_create_slot(TYPE_INT, false);
        Type *args[1] = { slot_type };
        Type *future_type = type_create_constructed(TYPE_FUTURE, args, 1);
        scope_declare(ctx->scope,
            symbol_create_variable("pending_slot", future_type, 1, 1));

        ASTNode *await = ast_create_await_expression(make_identifier("pending_slot", 1));
        await->line = 1; await->column = 1;

        type_check_expression(await, ctx);
        EXPECT(ctx->has_error
            && ctx_has_diagnostic_substring(ctx, "anchored resource handles"));

        semantic_context_destroy(ctx);
        ast_destroy(await);
    }

    TEST("await on RemoteFuture<QubitSlot> yields Result<QubitSlot>");
    {
        SemanticContext *ctx = semantic_context_create();
        scope_enter(&ctx->scope, SCOPE_GLOBAL);
        ctx->in_async_func = true;

        Type *args[1] = { TYPE_QUBIT };
        Type *future_type = type_create_constructed(TYPE_REMOTE_FUTURE, args, 1);
        scope_declare(ctx->scope,
            symbol_create_variable("remote_qubit", future_type, 1, 1));

        ASTNode *await_expr =
            ast_create_await_expression(make_identifier("remote_qubit", 1));
        Type *t = type_check_expression(await_expr, ctx);
        EXPECT(!ctx->has_error);
        EXPECT(t != NULL
            && t->kind == TYPE_KIND_CONSTRUCTED
            && type_equals(t->data.constructed.constructor, TYPE_RESULT));
        EXPECT(t != NULL
            && t->kind == TYPE_KIND_CONSTRUCTED
            && t->data.constructed.arg_count >= 1
            && t->data.constructed.args[0] == TYPE_QUBIT);

        semantic_context_destroy(ctx);
        ast_destroy(await_expr);
    }

    TEST("await may initialize a movable resource binding");
    {
        SemanticContext *ctx = semantic_context_create();
        scope_enter(&ctx->scope, SCOPE_GLOBAL);
        ctx->in_async_func = true;

        Type *args[1] = { TYPE_QUBIT };
        Type *future_type = type_create_constructed(TYPE_FUTURE, args, 1);
        scope_declare(ctx->scope,
            symbol_create_variable("pending_qubit", future_type, 1, 1));

        ASTNode *decl = ast_create_let_declaration("q");
        decl->data.let_decl.type = ast_create_type("QubitSlot");
        decl->data.let_decl.initializer =
            ast_create_await_expression(make_identifier("pending_qubit", 1));
        type_check_let_decl(decl, ctx);
        EXPECT(!ctx->has_error);

        ASTNode *state_args[1] = { make_identifier("q", 2) };
        ASTNode *state = make_call("QubitState", state_args, 1, 2);
        type_check_expression(state, ctx);
        EXPECT(!ctx->has_error);

        semantic_context_destroy(ctx);
        ast_destroy(decl);
        ast_destroy(state);
    }

    TEST("await movable resource rejects inline use without binding");
    {
        SemanticContext *ctx = semantic_context_create();
        scope_enter(&ctx->scope, SCOPE_GLOBAL);
        ctx->in_async_func = true;

        Type *args[1] = { TYPE_QUBIT };
        Type *future_type = type_create_constructed(TYPE_FUTURE, args, 1);
        scope_declare(ctx->scope,
            symbol_create_variable("pending_qubit", future_type, 1, 1));

        ASTNode *await = ast_create_await_expression(make_identifier("pending_qubit", 1));
        await->line = 1; await->column = 1;
        ASTNode *state_args[1] = { await };
        ASTNode *state = make_call("QubitState", state_args, 1, 1);

        type_check_expression(state, ctx);
        EXPECT(ctx->has_error
            && ctx_has_diagnostic_substring(ctx, "bound to a named variable"));

        semantic_context_destroy(ctx);
        ast_destroy(state);
    }

    TEST("parallel-family: spawn returns Future<T>");
    {
        SemanticContext *ctx = semantic_context_create();
        scope_enter(&ctx->scope, SCOPE_GLOBAL);

        ASTNode *num = make_number(42, 1);
        ASTNode *spawn = ast_create_spawn_expression(num);
        spawn->line = 1; spawn->column = 1;

        Type *t = type_check_spawn_expr(spawn, ctx);
        EXPECT(t != NULL);
        EXPECT(t->kind == TYPE_KIND_CONSTRUCTED);
        EXPECT(type_equals(t->data.constructed.constructor, TYPE_FUTURE));
        EXPECT(type_equals(t->data.constructed.args[0], TYPE_INT));
        EXPECT(!ctx->has_error);

        semantic_context_destroy(ctx);
        ast_destroy(spawn);
    }

    TEST("Cancel(Future<Int>) returns Bool");
    {
        SemanticContext *ctx = semantic_context_create();
        scope_enter(&ctx->scope, SCOPE_GLOBAL);

        Type *args[1] = { TYPE_INT };
        Type *future_type = type_create_constructed(TYPE_FUTURE, args, 1);
        scope_declare(ctx->scope,
            symbol_create_variable("pending", future_type, 1, 1));

        ASTNode *call_args[1] = { make_identifier("pending", 1) };
        ASTNode *call = make_call("Cancel", call_args, 1, 1);
        Type *t = type_check_expression(call, ctx);
        EXPECT(!ctx->has_error && type_equals(t, TYPE_BOOL));

        semantic_context_destroy(ctx);
        ast_destroy(call);
    }

    TEST("Cancel rejects non-future values");
    {
        SemanticContext *ctx = semantic_context_create();
        scope_enter(&ctx->scope, SCOPE_GLOBAL);

        ASTNode *call_args[1] = { make_number(42, 1) };
        ASTNode *call = make_call("Cancel", call_args, 1, 1);
        type_check_expression(call, ctx);
        EXPECT(ctx->has_error
            && ctx_has_diagnostic_substring(ctx,
                "Cancel requires Future<T> or RemoteFuture<T>"));

        semantic_context_destroy(ctx);
        ast_destroy(call);
    }

    TEST("IsCancelled returns Bool");
    {
        SemanticContext *ctx = semantic_context_create();
        scope_enter(&ctx->scope, SCOPE_GLOBAL);

        ASTNode *call = make_call("IsCancelled", NULL, 0, 1);
        Type *t = type_check_expression(call, ctx);
        EXPECT(!ctx->has_error && type_equals(t, TYPE_BOOL));

        semantic_context_destroy(ctx);
        ast_destroy(call);
    }

    TEST("channel send accepts plain value payload");
    {
        SemanticContext *ctx = semantic_context_create();
        scope_enter(&ctx->scope, SCOPE_GLOBAL);

        Type *args[1] = { TYPE_INT };
        Type *channel_type = type_create_constructed(TYPE_CHANNEL, args, 1);
        scope_declare(ctx->scope,
            symbol_create_variable("ch", channel_type, 1, 1));

        ASTNode *send = ast_create_channel_send(
            make_identifier("ch", 1), make_number(42, 1));
        send->line = 1; send->column = 1;

        Type *t = type_check_expression(send, ctx);
        EXPECT(!ctx->has_error && type_equals(t, TYPE_VOID));

        semantic_context_destroy(ctx);
        ast_destroy(send);
    }

    TEST("TryRecv(Channel<Int>) returns Option<Int>");
    {
        SemanticContext *ctx = semantic_context_create();
        scope_enter(&ctx->scope, SCOPE_GLOBAL);

        Type *args[1] = { TYPE_INT };
        Type *channel_type = type_create_constructed(TYPE_CHANNEL, args, 1);
        scope_declare(ctx->scope,
            symbol_create_variable("ch", channel_type, 1, 1));

        ASTNode *call_args[1] = { make_identifier("ch", 1) };
        ASTNode *call = make_call("TryRecv", call_args, 1, 1);
        Type *t = type_check_expression(call, ctx);
        EXPECT(!ctx->has_error);
        EXPECT(t != NULL
            && t->kind == TYPE_KIND_CONSTRUCTED
            && type_equals(t->data.constructed.constructor, TYPE_OPTION));
        EXPECT(t != NULL
            && t->data.constructed.arg_count >= 1
            && t->data.constructed.args[0] == TYPE_INT);

        semantic_context_destroy(ctx);
        ast_destroy(call);
    }

    TEST("RecvTimeout(Channel<Int>, Int) returns Option<Int>");
    {
        SemanticContext *ctx = semantic_context_create();
        scope_enter(&ctx->scope, SCOPE_GLOBAL);

        Type *args[1] = { TYPE_INT };
        Type *channel_type = type_create_constructed(TYPE_CHANNEL, args, 1);
        scope_declare(ctx->scope,
            symbol_create_variable("ch", channel_type, 1, 1));

        ASTNode *call_args[2] = { make_identifier("ch", 1), make_number(1000, 1) };
        ASTNode *call = make_call("RecvTimeout", call_args, 2, 1);
        Type *t = type_check_expression(call, ctx);
        EXPECT(!ctx->has_error);
        EXPECT(t != NULL
            && t->kind == TYPE_KIND_CONSTRUCTED
            && type_equals(t->data.constructed.constructor, TYPE_OPTION));
        EXPECT(t != NULL
            && t->data.constructed.arg_count >= 1
            && t->data.constructed.args[0] == TYPE_INT);

        semantic_context_destroy(ctx);
        ast_destroy(call);
    }

    TEST("ChannelLength(Channel<Int>) returns Int");
    {
        SemanticContext *ctx = semantic_context_create();
        scope_enter(&ctx->scope, SCOPE_GLOBAL);

        Type *args[1] = { TYPE_INT };
        Type *channel_type = type_create_constructed(TYPE_CHANNEL, args, 1);
        scope_declare(ctx->scope,
            symbol_create_variable("ch", channel_type, 1, 1));

        ASTNode *call_args[1] = { make_identifier("ch", 1) };
        ASTNode *call = make_call("ChannelLength", call_args, 1, 1);
        Type *t = type_check_expression(call, ctx);
        EXPECT(!ctx->has_error && type_equals(t, TYPE_INT));

        semantic_context_destroy(ctx);
        ast_destroy(call);
    }

    TEST("ChannelCapacity(Channel<Int>) returns Int");
    {
        SemanticContext *ctx = semantic_context_create();
        scope_enter(&ctx->scope, SCOPE_GLOBAL);

        Type *args[1] = { TYPE_INT };
        Type *channel_type = type_create_constructed(TYPE_CHANNEL, args, 1);
        scope_declare(ctx->scope,
            symbol_create_variable("ch", channel_type, 1, 1));

        ASTNode *call_args[1] = { make_identifier("ch", 1) };
        ASTNode *call = make_call("ChannelCapacity", call_args, 1, 1);
        Type *t = type_check_expression(call, ctx);
        EXPECT(!ctx->has_error && type_equals(t, TYPE_INT));

        semantic_context_destroy(ctx);
        ast_destroy(call);
    }

    TEST("ChannelSpace(Channel<Int>) returns Int");
    {
        SemanticContext *ctx = semantic_context_create();
        scope_enter(&ctx->scope, SCOPE_GLOBAL);

        Type *args[1] = { TYPE_INT };
        Type *channel_type = type_create_constructed(TYPE_CHANNEL, args, 1);
        scope_declare(ctx->scope,
            symbol_create_variable("ch", channel_type, 1, 1));

        ASTNode *call_args[1] = { make_identifier("ch", 1) };
        ASTNode *call = make_call("ChannelSpace", call_args, 1, 1);
        Type *t = type_check_expression(call, ctx);
        EXPECT(!ctx->has_error && type_equals(t, TYPE_INT));

        semantic_context_destroy(ctx);
        ast_destroy(call);
    }

    TEST("ChannelFull(Channel<Int>) returns Bool");
    {
        SemanticContext *ctx = semantic_context_create();
        scope_enter(&ctx->scope, SCOPE_GLOBAL);

        Type *args[1] = { TYPE_INT };
        Type *channel_type = type_create_constructed(TYPE_CHANNEL, args, 1);
        scope_declare(ctx->scope,
            symbol_create_variable("ch", channel_type, 1, 1));

        ASTNode *call_args[1] = { make_identifier("ch", 1) };
        ASTNode *call = make_call("ChannelFull", call_args, 1, 1);
        Type *t = type_check_expression(call, ctx);
        EXPECT(!ctx->has_error && type_equals(t, TYPE_BOOL));

        semantic_context_destroy(ctx);
        ast_destroy(call);
    }

    TEST("ChannelClosed(Channel<Int>) returns Bool");
    {
        SemanticContext *ctx = semantic_context_create();
        scope_enter(&ctx->scope, SCOPE_GLOBAL);

        Type *args[1] = { TYPE_INT };
        Type *channel_type = type_create_constructed(TYPE_CHANNEL, args, 1);
        scope_declare(ctx->scope,
            symbol_create_variable("ch", channel_type, 1, 1));

        ASTNode *call_args[1] = { make_identifier("ch", 1) };
        ASTNode *call = make_call("ChannelClosed", call_args, 1, 1);
        Type *t = type_check_expression(call, ctx);
        EXPECT(!ctx->has_error && type_equals(t, TYPE_BOOL));

        semantic_context_destroy(ctx);
        ast_destroy(call);
    }

    TEST("TrySendStatus(Channel<Int>, Int) returns Option<Bool>");
    {
        SemanticContext *ctx = semantic_context_create();
        scope_enter(&ctx->scope, SCOPE_GLOBAL);

        Type *args[1] = { TYPE_INT };
        Type *channel_type = type_create_constructed(TYPE_CHANNEL, args, 1);
        scope_declare(ctx->scope,
            symbol_create_variable("ch", channel_type, 1, 1));

        ASTNode *call_args[2] = { make_identifier("ch", 1), make_number(7, 1) };
        ASTNode *call = make_call("TrySendStatus", call_args, 2, 1);
        Type *t = type_check_expression(call, ctx);
        EXPECT(!ctx->has_error && t != NULL
               && strcmp(t->name, "Option<Bool>") == 0);

        semantic_context_destroy(ctx);
        ast_destroy(call);
    }

    TEST("SendTimeoutStatus(Channel<Int>, Int, Int) returns Option<Bool>");
    {
        SemanticContext *ctx = semantic_context_create();
        scope_enter(&ctx->scope, SCOPE_GLOBAL);

        Type *args[1] = { TYPE_INT };
        Type *channel_type = type_create_constructed(TYPE_CHANNEL, args, 1);
        scope_declare(ctx->scope,
            symbol_create_variable("ch", channel_type, 1, 1));

        ASTNode *call_args[3] = {
            make_identifier("ch", 1),
            make_number(7, 1),
            make_number(1000, 1)
        };
        ASTNode *call = make_call("SendTimeoutStatus", call_args, 3, 1);
        Type *t = type_check_expression(call, ctx);
        EXPECT(!ctx->has_error && t != NULL
               && strcmp(t->name, "Option<Bool>") == 0);

        semantic_context_destroy(ctx);
        ast_destroy(call);
    }

    TEST("ChannelLength rejects non-channel values");
    {
        SemanticContext *ctx = semantic_context_create();
        scope_enter(&ctx->scope, SCOPE_GLOBAL);

        ASTNode *call_args[1] = { make_number(42, 1) };
        ASTNode *call = make_call("ChannelLength", call_args, 1, 1);
        type_check_expression(call, ctx);
        EXPECT(ctx->has_error);

        semantic_context_destroy(ctx);
        ast_destroy(call);
    }

    TEST("TrySend(Channel<Int>, Int) returns Bool");
    {
        SemanticContext *ctx = semantic_context_create();
        scope_enter(&ctx->scope, SCOPE_GLOBAL);

        Type *args[1] = { TYPE_INT };
        Type *channel_type = type_create_constructed(TYPE_CHANNEL, args, 1);
        scope_declare(ctx->scope,
            symbol_create_variable("ch", channel_type, 1, 1));

        ASTNode *call_args[2] = { make_identifier("ch", 1), make_number(42, 1) };
        ASTNode *call = make_call("TrySend", call_args, 2, 1);
        Type *t = type_check_expression(call, ctx);
        EXPECT(!ctx->has_error && type_equals(t, TYPE_BOOL));

        semantic_context_destroy(ctx);
        ast_destroy(call);
    }

    TEST("TrySend rejects movable resource channel payloads");
    {
        SemanticContext *ctx = semantic_context_create();
        scope_enter(&ctx->scope, SCOPE_GLOBAL);

        Type *args[1] = { TYPE_QUBIT };
        Type *channel_type = type_create_constructed(TYPE_CHANNEL, args, 1);
        scope_declare(ctx->scope,
            symbol_create_variable("ch", channel_type, 1, 1));

        ASTNode *decl = ast_create_let_declaration("q");
        decl->data.let_decl.type = ast_create_type("QubitSlot");
        decl->data.let_decl.initializer = make_call("ClaimQubit", NULL, 0, 1);
        type_check_let_decl(decl, ctx);

        ASTNode *call_args[2] = { make_identifier("ch", 2), make_identifier("q", 2) };
        ASTNode *call = make_call("TrySend", call_args, 2, 2);
        type_check_expression(call, ctx);
        EXPECT(ctx->has_error
            && ctx_has_diagnostic_substring(ctx,
                "TrySend does not support movable resource sends yet"));

        semantic_context_destroy(ctx);
        ast_destroy(decl);
        ast_destroy(call);
    }

    TEST("channel send rejects anchored Slot handle payload");
    {
        SemanticContext *ctx = semantic_context_create();
        scope_enter(&ctx->scope, SCOPE_GLOBAL);

        Type *slot_type = type_create_slot(TYPE_INT, false);
        Type *args[1] = { slot_type };
        Type *channel_type = type_create_constructed(TYPE_CHANNEL, args, 1);
        scope_declare(ctx->scope,
            symbol_create_variable("ch", channel_type, 1, 1));
        scope_declare(ctx->scope,
            symbol_create_slot("slot", slot_type, false, NULL, 1, 1));

        ASTNode *send = ast_create_channel_send(
            make_identifier("ch", 1), make_identifier("slot", 1));
        send->line = 1; send->column = 1;

        type_check_expression(send, ctx);
        EXPECT(ctx->has_error
            && ctx_has_diagnostic_substring(ctx, "anchored resource handles"));

        semantic_context_destroy(ctx);
        ast_destroy(send);
    }

    TEST("channel send moves QubitSlot from named binding");
    {
        SemanticContext *ctx = semantic_context_create();
        scope_enter(&ctx->scope, SCOPE_GLOBAL);

        Type *args[1] = { TYPE_QUBIT };
        Type *channel_type = type_create_constructed(TYPE_CHANNEL, args, 1);
        scope_declare(ctx->scope,
            symbol_create_variable("ch", channel_type, 1, 1));

        ASTNode *decl = ast_create_let_declaration("q");
        decl->data.let_decl.type = ast_create_type("QubitSlot");
        decl->data.let_decl.initializer = make_call("ClaimQubit", NULL, 0, 1);
        type_check_let_decl(decl, ctx);

        ASTNode *send = ast_create_channel_send(
            make_identifier("ch", 2), make_identifier("q", 2));
        send->line = 2; send->column = 1;
        type_check_expression(send, ctx);
        EXPECT(!ctx->has_error);

        ASTNode *state_args[1] = { make_identifier("q", 3) };
        ASTNode *state = make_call("QubitState", state_args, 1, 3);
        type_check_expression(state, ctx);
        EXPECT(ctx->has_error);

        semantic_context_destroy(ctx);
        ast_destroy(decl);
        ast_destroy(send);
        ast_destroy(state);
    }

    TEST("channel send rejects anonymous movable resource payload");
    {
        SemanticContext *ctx = semantic_context_create();
        scope_enter(&ctx->scope, SCOPE_GLOBAL);

        Type *args[1] = { TYPE_QUBIT };
        Type *channel_type = type_create_constructed(TYPE_CHANNEL, args, 1);
        scope_declare(ctx->scope,
            symbol_create_variable("ch", channel_type, 1, 1));

        ASTNode *send = ast_create_channel_send(
            make_identifier("ch", 1), make_call("ClaimQubit", NULL, 0, 1));
        send->line = 1; send->column = 1;

        type_check_expression(send, ctx);
        EXPECT(ctx->has_error
            && ctx_has_diagnostic_substring(ctx, "bind the value first"));

        semantic_context_destroy(ctx);
        ast_destroy(send);
    }

    TEST("channel recv may initialize a movable resource binding");
    {
        SemanticContext *ctx = semantic_context_create();
        scope_enter(&ctx->scope, SCOPE_GLOBAL);

        Type *args[1] = { TYPE_QUBIT };
        Type *channel_type = type_create_constructed(TYPE_CHANNEL, args, 1);
        scope_declare(ctx->scope,
            symbol_create_variable("ch", channel_type, 1, 1));

        ASTNode *decl = ast_create_let_declaration("q");
        decl->data.let_decl.type = ast_create_type("QubitSlot");
        decl->data.let_decl.initializer =
            ast_create_channel_recv(make_identifier("ch", 1));
        type_check_let_decl(decl, ctx);
        EXPECT(!ctx->has_error);

        ASTNode *state_args[1] = { make_identifier("q", 2) };
        ASTNode *state = make_call("QubitState", state_args, 1, 2);
        type_check_expression(state, ctx);
        EXPECT(!ctx->has_error);

        semantic_context_destroy(ctx);
        ast_destroy(decl);
        ast_destroy(state);
    }
}

static void
test_effect_inference(void)
{
    printf("\n[effect_inference]\n");

    TEST("Measure infers nondeterministic + collapse on function");
    {
        SemanticContext *ctx = semantic_context_create();

        ASTNode *func = ast_create_function("Observe");
        func->data.func_decl.return_type = ast_create_type("Int");
        func->data.func_decl.body = ast_create_block();

        ASTNode *decl = ast_create_let_declaration("q");
        decl->data.let_decl.type = ast_create_type("QubitSlot");
        decl->data.let_decl.initializer = make_call("ClaimQubit", NULL, 0, 2);
        ast_add_statement(func->data.func_decl.body, decl);

        ASTNode *ret = ast_create_return_statement();
        ASTNode *measure_args[1] = { make_identifier("q", 3) };
        ret->data.return_stmt.value = make_call("Measure", measure_args, 1, 3);
        ast_add_statement(func->data.func_decl.body, ret);

        type_check_func_decl(func, ctx);

        Symbol *sym = scope_lookup(ctx->scope, "Observe");
        uint32_t effects = sym != NULL && sym->type != NULL
            ? type_function_effects(sym->type) : EFFECT_NONE;
        EXPECT(!ctx->has_error
            && type_effect_mask_has(effects, EFFECT_NONDETERMINISTIC)
            && type_effect_mask_has(effects, EFFECT_COLLAPSE));

        semantic_context_destroy(ctx);
        ast_destroy(func);
    }

    TEST("effectful calls propagate function effects to callers");
    {
        SemanticContext *ctx = semantic_context_create();

        ASTNode *observe = ast_create_function("Observe");
        observe->data.func_decl.return_type = ast_create_type("Int");
        observe->data.func_decl.body = ast_create_block();

        ASTNode *decl = ast_create_let_declaration("q");
        decl->data.let_decl.type = ast_create_type("QubitSlot");
        decl->data.let_decl.initializer = make_call("ClaimQubit", NULL, 0, 2);
        ast_add_statement(observe->data.func_decl.body, decl);

        ASTNode *measure_ret = ast_create_return_statement();
        ASTNode *measure_args[1] = { make_identifier("q", 3) };
        measure_ret->data.return_stmt.value =
            make_call("Measure", measure_args, 1, 3);
        ast_add_statement(observe->data.func_decl.body, measure_ret);
        type_check_func_decl(observe, ctx);

        ASTNode *wrapper = ast_create_function("WrapObserve");
        wrapper->data.func_decl.return_type = ast_create_type("Int");
        wrapper->data.func_decl.body = ast_create_block();

        ASTNode *ret = ast_create_return_statement();
        ret->data.return_stmt.value = make_call("Observe", NULL, 0, 6);
        ast_add_statement(wrapper->data.func_decl.body, ret);
        type_check_func_decl(wrapper, ctx);

        Symbol *sym = scope_lookup(ctx->scope, "WrapObserve");
        uint32_t effects = sym != NULL && sym->type != NULL
            ? type_function_effects(sym->type) : EFFECT_NONE;
        EXPECT(!ctx->has_error
            && type_effect_mask_has(effects, EFFECT_NONDETERMINISTIC)
            && type_effect_mask_has(effects, EFFECT_COLLAPSE));

        semantic_context_destroy(ctx);
        ast_destroy(observe);
        ast_destroy(wrapper);
    }

    TEST("SecureSlot declarations infer secure effect on function");
    {
        SemanticContext *ctx = semantic_context_create();

        ASTNode *func = ast_create_function("TouchSecure");
        func->data.func_decl.return_type = ast_create_type("Int");
        func->data.func_decl.body = ast_create_block();

        ASTNode *decl = ast_create_let_declaration("secret");
        decl->data.let_decl.type = make_generic_type("SecureSlot", "Int");
        decl->data.let_decl.initializer = make_number(7, 2);
        ast_add_statement(func->data.func_decl.body, decl);

        ASTNode *ret = ast_create_return_statement();
        ret->data.return_stmt.value = make_number(1, 3);
        ast_add_statement(func->data.func_decl.body, ret);

        type_check_func_decl(func, ctx);

        Symbol *sym = scope_lookup(ctx->scope, "TouchSecure");
        uint32_t effects = sym != NULL && sym->type != NULL
            ? type_function_effects(sym->type) : EFFECT_NONE;
        EXPECT(!ctx->has_error
            && type_effect_mask_has(effects, EFFECT_SECURE));

        semantic_context_destroy(ctx);
        ast_destroy(func);
    }

    TEST("DeviceSlot parameter types are rejected as anchored handles");
    {
        SemanticContext *ctx = semantic_context_create();

        ASTNode *func = ast_create_function("UseDevice");
        func->data.func_decl.return_type = ast_create_type("Void");
        func->data.func_decl.body = ast_create_block();
        func->data.func_decl.param_count = 1;
        func->data.func_decl.params = calloc(1, sizeof(FuncParam *));
        func->data.func_decl.params[0] =
            make_func_param("dev", make_generic_type("DeviceSlot", "Int"));

        type_check_func_decl(func, ctx);
        EXPECT(ctx->has_error
            && ctx_has_diagnostic_substring(ctx, "Anchored handle parameters are currently closed"));

        semantic_context_destroy(ctx);
        ast_destroy(func);
    }

    TEST("DeviceSlot copy into new binding is rejected");
    {
        SemanticContext *ctx = semantic_context_create();
        scope_enter(&ctx->scope, SCOPE_GLOBAL);

        Type *dev_args[1] = { TYPE_INT };
        Type *device_type = type_create_constructed(TYPE_DEVICE_SLOT, dev_args, 1);
        scope_declare(ctx->scope,
            symbol_create_variable("dev", device_type, 1, 1));

        ASTNode *decl = ast_create_let_declaration("copy");
        decl->data.let_decl.type = make_generic_type("DeviceSlot", "Int");
        decl->data.let_decl.initializer = make_identifier("dev", 2);

        type_check_let_decl(decl, ctx);
        EXPECT(ctx->has_error
            && ctx_has_diagnostic_substring(ctx, "Anchored resource handles"));

        semantic_context_destroy(ctx);
        ast_destroy(decl);
    }

    TEST("DeviceSlot use-after-release is rejected");
    {
        SemanticContext *ctx = semantic_context_create();
        scope_enter(&ctx->scope, SCOPE_GLOBAL);

        ASTNode *decl = ast_create_let_declaration("dev");
        decl->data.let_decl.type = make_generic_type("DeviceSlot", "Int");
        decl->data.let_decl.initializer = make_call("ClaimDeviceSlot", NULL, 0, 1);
        EXPECT(type_check_let_decl(decl, ctx));

        ASTNode *release_args[1] = { make_identifier("dev", 2) };
        ASTNode *read_args[1] = { make_identifier("dev", 3) };
        ASTNode *release = make_call("ReleaseDeviceSlot", release_args, 1, 2);
        ASTNode *read = make_call("DeviceRead", read_args, 1, 3);

        type_check_expression(release, ctx);
        ctx->has_error = false;
        type_check_expression(read, ctx);

        EXPECT(ctx->has_error
            && ctx_has_diagnostic_substring(ctx, "released DeviceSlot"));

        semantic_context_destroy(ctx);
        ast_destroy(decl);
        ast_destroy(release);
        ast_destroy(read);
    }

    TEST("DeviceSlot double release is rejected");
    {
        SemanticContext *ctx = semantic_context_create();
        scope_enter(&ctx->scope, SCOPE_GLOBAL);

        ASTNode *decl = ast_create_let_declaration("dev");
        decl->data.let_decl.type = make_generic_type("DeviceSlot", "Int");
        decl->data.let_decl.initializer = make_call("ClaimDeviceSlot", NULL, 0, 1);
        EXPECT(type_check_let_decl(decl, ctx));

        ASTNode *release_a_args[1] = { make_identifier("dev", 2) };
        ASTNode *release_b_args[1] = { make_identifier("dev", 3) };
        ASTNode *release_a = make_call("ReleaseDeviceSlot", release_a_args, 1, 2);
        ASTNode *release_b = make_call("ReleaseDeviceSlot", release_b_args, 1, 3);

        type_check_expression(release_a, ctx);
        ctx->has_error = false;
        type_check_expression(release_b, ctx);

        EXPECT(ctx->has_error
            && ctx_has_diagnostic_substring(ctx, "already been released"));

        semantic_context_destroy(ctx);
        ast_destroy(decl);
        ast_destroy(release_a);
        ast_destroy(release_b);
    }

    TEST("subject copy into new binding is rejected");
    {
        const char *source =
            "subject Vec2 {\n"
            "    let x: Int;\n"
            "}\n"
            "func Main() -> Void {\n"
            "    let a: Vec2 = Vec2();\n"
            "    let b: Vec2 = a;\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticResult *result = semantic_analyze(program);

        EXPECT(!parser_has_error(parser));
        EXPECT(result != NULL && result->error_count > 0);
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "Subjects cannot be copied into a new binding"));

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("struct value copy into new binding is allowed");
    {
        const char *source =
            "struct Vec2 {\n"
            "    x: Int;\n"
            "}\n"
            "func Main() -> Void {\n"
            "    let a: Vec2 = Vec2();\n"
            "    let b: Vec2 = a;\n"
            "    b.x = 1;\n"
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

    TEST("class value copy into new binding is allowed");
    {
        const char *source =
            "class Vec2 {\n"
            "    let x: Int;\n"
            "}\n"
            "func Main() -> Void {\n"
            "    let a: Vec2 = Vec2();\n"
            "    let b: Vec2 = a;\n"
            "    b.x = 1;\n"
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

    TEST("subject parameter is accepted (reference semantics)");
    {
        const char *source =
            "subject Vec2 {\n"
            "    let x: Int;\n"
            "}\n"
            "func Use(v: Vec2) -> Void {\n"
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

    TEST("class return by value is allowed");
    {
        const char *source =
            "class Vec2 {\n"
            "    let x: Int;\n"
            "}\n"
            "func Make() -> Vec2 {\n"
            "    return Vec2();\n"
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

    TEST("subject return by value is rejected");
    {
        const char *source =
            "subject Vec2 {\n"
            "    let x: Int;\n"
            "}\n"
            "func Make() -> Vec2 {\n"
            "    return Vec2();\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticResult *result = semantic_analyze(program);

        EXPECT(!parser_has_error(parser));
        EXPECT(result != NULL && result->error_count > 0);
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "Returning subjects by value is not supported yet"));

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("subject copy into new binding is rejected");
    {
        const char *source =
            "subject Counter {\n"
            "    let count: Int;\n"
            "}\n"
            "func Main() -> Void {\n"
            "    let a: Counter = Counter();\n"
            "    let b: Counter = a;\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticResult *result = semantic_analyze(program);

        EXPECT(!parser_has_error(parser));
        EXPECT(result != NULL && result->error_count > 0);
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "Subjects cannot be copied into a new binding"));

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("subject declaration emits no deprecation warning");
    {
        const char *source =
            "subject Counter {\n"
            "    let count: Int;\n"
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

    TEST("subject syntax is rejected by subject copy rules");
    {
        const char *source =
            "subject Counter {\n"
            "    let count: Int;\n"
            "}\n"
            "func Main() -> Void {\n"
            "    let a: Counter = Counter();\n"
            "    let b: Counter = a;\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticResult *result = semantic_analyze(program);

        EXPECT(!parser_has_error(parser));
        EXPECT(result != NULL && result->error_count > 0);
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "Subjects cannot be copied into a new binding"));

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("class method bare field names resolve in class scope");
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
        SemanticResult *result = semantic_analyze(program);

        EXPECT(!parser_has_error(parser));
        EXPECT(result != NULL && result->error_count == 0);

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("zone publish to tobject target requires explicit by subject alias when authority is declared");
    {
        const char *source =
            "subject Player { let hp: Int; }\n"
            "tobject PlayerPacket { hp: Int; }\n"
            "zone BattleZone {\n"
            "    subject slot player: Player\n"
            "    tobject slot packet: PlayerPacket\n"
            "    authority player\n"
            "    publish packet from player\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticResult *result = semantic_analyze(program);

        EXPECT(!parser_has_error(parser));
        EXPECT(result != NULL && result->error_count == 1);
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "must specify 'by <subjectSlot>'"));

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("zone refresh map remaps target fields from differently named source fields");
    {
        const char *source =
            "subject Player { let hp: Int; let name: String; }\n"
            "object PlayerView { totalHp: Int; label: String; }\n"
            "zone BattleZone {\n"
            "    subject slot player: Player\n"
            "    object slot playerView: PlayerView\n"
            "    refresh playerView from player map {\n"
            "        totalHp <- hp;\n"
            "        label <- name;\n"
            "    }\n"
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

    TEST("authority-bearing zone publish requires explicit by authority");
    {
        const char *source =
            "subject Player { let hp: Int; }\n"
            "tobject PlayerPacket { hp: Int; }\n"
            "zone Arena {\n"
            "    subject slot player: Player\n"
            "    tobject slot packet: PlayerPacket\n"
            "    authority player\n"
            "    publish packet from player\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticResult *result = semantic_analyze(program);

        EXPECT(!parser_has_error(parser));
        EXPECT(result != NULL && result->error_count > 0);
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "must specify 'by <subjectSlot>'"));

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("zone publish to tobject target passes with explicit by subject alias");
    {
        const char *source =
            "subject Player { let hp: Int; }\n"
            "tobject PlayerPacket { hp: Int; }\n"
            "zone BattleZone {\n"
            "    subject slot player: Player\n"
            "    tobject slot packet: PlayerPacket\n"
            "    authority player\n"
            "    publish packet from player by player\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticResult *result = semantic_analyze(program);        EXPECT(!parser_has_error(parser));
        EXPECT(result != NULL && result->error_count == 0);

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("nested vessel-backed projection resolves through subject fields");
    {
        const char *source =
            "vessel Cycle { age: Int; fatigue: Int; }\n"
            "vessel Traits { metabolism: Int; }\n"
            "subject Creature {\n"
            "    let name: String;\n"
            "    vessel cycle: Cycle;\n"
            "    vessel traits: Traits;\n"
            "}\n"
            "object CreatureView { age: Int; fatigue: Int; metabolism: Int; }\n"
            "tobject CreaturePacket { age: Int; metabolism: Int; }\n"
            "zone MeadowZone {\n"
            "    subject slot grazer: Creature\n"
            "    object slot grazerView: CreatureView = ToObject(CreatureView, grazer)\n"
            "    tobject slot grazerPacket: CreaturePacket = ToTObject(CreaturePacket, grazer)\n"
            "    refresh grazerView from grazer\n"
            "    publish grazerPacket from grazer\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticResult *result = semantic_analyze(program);        EXPECT(!parser_has_error(parser));
        EXPECT(result != NULL && result->error_count == 0);

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("zone methods resolve bare shared fields and helper funcs in zone scope");
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
        SemanticResult *result = semantic_analyze(program);        EXPECT(!parser_has_error(parser));
        EXPECT(result != NULL && result->error_count == 0);

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("world methods resolve bare shared fields zone slots and helper funcs in world scope");
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
        SemanticResult *result = semantic_analyze(program);

        EXPECT(!parser_has_error(parser));
        EXPECT(result != NULL && result->error_count == 0);

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("class constructor positional arguments are type-checked");
    {
        const char *source =
            "class Vec2 {\n"
            "    let x: Int;\n"
            "    let y: Int;\n"
            "}\n"
            "func Main() -> Void {\n"
            "    let v: Vec2 = Vec2(3, \"bad\");\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticResult *result = semantic_analyze(program);

        EXPECT(!parser_has_error(parser));
        EXPECT(result != NULL && result->error_count > 0);
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "argument 2 initializes field 'y'"));

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("class constructor rejects too many positional arguments");
    {
        const char *source =
            "class Vec2 {\n"
            "    let x: Int;\n"
            "    let y: Int;\n"
            "}\n"
            "func Main() -> Void {\n"
            "    let v: Vec2 = Vec2(1, 2, 3);\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticResult *result = semantic_analyze(program);

        EXPECT(!parser_has_error(parser));
        EXPECT(result != NULL && result->error_count > 0);
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "accepts at most 2 positional field argument"));

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("Slot<subject> is accepted as a local object-cell anchor");
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
        SemanticResult *result = semantic_analyze(program);

        EXPECT(!parser_has_error(parser));
        EXPECT(result != NULL && result->error_count == 0);

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("Slot<subject> is accepted as a local object-cell anchor");
    {
        const char *source =
            "subject Bot {\n"
            "    let hp: Int;\n"
            "}\n"
            "func Main() -> Void {\n"
            "    let s: Slot<Bot> = Bot(7);\n"
            "    Write(s, Bot(9));\n"
            "    Release(s);\n"
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

    TEST("SecureSlot<subject> is accepted as a secure object-cell anchor");
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
        SemanticResult *result = semantic_analyze(program);

        EXPECT(!parser_has_error(parser));
        EXPECT(result != NULL && result->error_count == 0);

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("SecureSlot<subject> is accepted as a secure object-cell anchor");
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
        SemanticResult *result = semantic_analyze(program);

        EXPECT(!parser_has_error(parser));
        EXPECT(result != NULL && result->error_count == 0);

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("spawn and channel send infer remote effect on function");
    {
        SemanticContext *ctx = semantic_context_create();

        ASTNode *func = ast_create_function("Dispatch");
        func->data.func_decl.return_type = ast_create_type("Int");
        func->data.func_decl.body = ast_create_block();
        func->data.func_decl.param_count = 1;
        func->data.func_decl.params = calloc(1, sizeof(FuncParam *));
        func->data.func_decl.params[0] =
            make_func_param("ch", make_generic_type("Channel", "Int"));

        ASTNode *pending = ast_create_let_declaration("pending");
        pending->data.let_decl.initializer =
            ast_create_spawn_expression(make_number(42, 2));
        ast_add_statement(func->data.func_decl.body, pending);

        ASTNode *send = ast_create_channel_send(make_identifier("ch", 3),
                                                make_number(7, 3));
        ast_add_statement(func->data.func_decl.body, send);

        ASTNode *ret = ast_create_return_statement();
        ret->data.return_stmt.value = make_number(1, 4);
        ast_add_statement(func->data.func_decl.body, ret);

        type_check_func_decl(func, ctx);

        Symbol *sym = scope_lookup(ctx->scope, "Dispatch");
        uint32_t effects = sym != NULL && sym->type != NULL
            ? type_function_effects(sym->type) : EFFECT_NONE;
        EXPECT(!ctx->has_error
            && type_effect_mask_has(effects, EFFECT_REMOTE));

        semantic_context_destroy(ctx);
        ast_destroy(func);
    }

    TEST("structured comment @effects merges declared effect into function");
    {
        SemanticContext *ctx = semantic_context_create();

        ASTNode *func = ast_create_function("PlanRemote");
        func->data.func_decl.return_type = ast_create_type("Int");
        func->data.func_decl.body = ast_create_block();
        func->data.func_decl.doc_comment =
            make_effect_doc_comment("remote secure");

        ASTNode *ret = ast_create_return_statement();
        ret->data.return_stmt.value = make_number(1, 2);
        ast_add_statement(func->data.func_decl.body, ret);

        type_check_func_decl(func, ctx);

        Symbol *sym = scope_lookup(ctx->scope, "PlanRemote");
        uint32_t effects = sym != NULL && sym->type != NULL
            ? type_function_effects(sym->type) : EFFECT_NONE;
        EXPECT(!ctx->has_error
            && type_effect_mask_has(effects, EFFECT_REMOTE)
            && type_effect_mask_has(effects, EFFECT_SECURE));

        semantic_context_destroy(ctx);
        ast_destroy(func);
    }

    TEST("source-level /// @effects flows from parser into semantic effects");
    {
        const char *source =
            "/// @effects remote secure\n"
            "func PlanRemote() -> Int {\n"
            "    return 1;\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticContext *ctx = semantic_context_create();

        EXPECT(!parser_has_error(parser));
        type_check_program(program, ctx);

        Symbol *sym = scope_lookup(ctx->scope, "PlanRemote");
        uint32_t effects = sym != NULL && sym->type != NULL
            ? type_function_effects(sym->type) : EFFECT_NONE;
        EXPECT(!ctx->has_error
            && type_effect_mask_has(effects, EFFECT_REMOTE)
            && type_effect_mask_has(effects, EFFECT_SECURE));

        semantic_context_destroy(ctx);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("source-level with effects clause flows from parser into semantic effects");
    {
        const char *source =
            "func PlanRemote() -> Int with effects remote, secure {\n"
            "    return 1;\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticContext *ctx = semantic_context_create();

        EXPECT(!parser_has_error(parser));
        type_check_program(program, ctx);

        Symbol *sym = scope_lookup(ctx->scope, "PlanRemote");
        uint32_t effects = sym != NULL && sym->type != NULL
            ? type_function_effects(sym->type) : EFFECT_NONE;
        EXPECT(!ctx->has_error
            && type_effect_mask_has(effects, EFFECT_REMOTE)
            && type_effect_mask_has(effects, EFFECT_SECURE));

        semantic_context_destroy(ctx);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("declared effects must cover inferred body effects");
    {
        const char *source =
            "/// @effects local\n"
            "func Dispatch() -> Int {\n"
            "    let pending = spawn 42;\n"
            "    return 1;\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticResult *result = semantic_analyze(program);

        EXPECT(!parser_has_error(parser));
        EXPECT(result != NULL && result->error_count > 0);
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "missing declared effects: remote"));

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("signature effects must cover inferred body effects");
    {
        const char *source =
            "func Dispatch() -> Int with effects local {\n"
            "    let pending = spawn 42;\n"
            "    return 1;\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticResult *result = semantic_analyze(program);

        EXPECT(!parser_has_error(parser));
        EXPECT(result != NULL && result->error_count > 0);
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "missing declared effects: remote"));

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("signature effects may exactly match inferred body effects");
    {
        const char *source =
            "func Dispatch() -> Int with effects remote {\n"
            "    let pending = spawn 42;\n"
            "    return 1;\n"
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

    TEST("declared effects may exactly match inferred body effects");
    {
        const char *source =
            "/// @effects remote\n"
            "func Dispatch() -> Int {\n"
            "    let pending = spawn 42;\n"
            "    return 1;\n"
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

static void
test_misc_grammar_edges(void)
{
    printf("\n[misc_grammar]\n");

    TEST("unsafe block type-checks its body");
    {
        SemanticContext *ctx = semantic_context_create();
        scope_enter(&ctx->scope, SCOPE_GLOBAL);

        ASTNode *body = ast_create_block();
        ASTNode *args[1] = { make_number(1, 1) };
        ast_add_statement(body, make_call("Log", args, 1, 1));
        ASTNode *unsafe_block = ast_create_unsafe_block(body);
        unsafe_block->line = 1; unsafe_block->column = 1;

        type_check_statement(unsafe_block, ctx);
        EXPECT(!ctx->has_error);

        semantic_context_destroy(ctx);
        ast_destroy(unsafe_block);
    }

    TEST("defer statement type-checks its body");
    {
        SemanticContext *ctx = semantic_context_create();
        scope_enter(&ctx->scope, SCOPE_GLOBAL);

        ASTNode *body = ast_create_block();
        ASTNode *args[1] = { make_number(1, 1) };
        ast_add_statement(body, make_call("Log", args, 1, 1));
        ASTNode *defer_stmt = ast_create_defer_statement(body);
        defer_stmt->line = 1; defer_stmt->column = 1;

        type_check_statement(defer_stmt, ctx);
        EXPECT(!ctx->has_error);

        semantic_context_destroy(ctx);
        ast_destroy(defer_stmt);
    }

    TEST("bind statement is accepted by semantic pass");
    {
        SemanticContext *ctx = semantic_context_create();
        scope_enter(&ctx->scope, SCOPE_GLOBAL);

        ASTNode *bind = ast_create_bind_statement("team", "fighter", "Warrior");
        bind->line = 1; bind->column = 1;

        type_check_statement(bind, ctx);
        EXPECT(!ctx->has_error);

        semantic_context_destroy(ctx);
        ast_destroy(bind);
    }

    TEST("else if chain type-checks nested branch structure");
    {
        SemanticContext *ctx = semantic_context_create();
        scope_enter(&ctx->scope, SCOPE_GLOBAL);

        ASTNode *then_block = ast_create_block();
        ASTNode *then_args[1] = { make_number(1, 1) };
        ast_add_statement(then_block, make_call("Log", then_args, 1, 1));

        ASTNode *else_then = ast_create_block();
        ASTNode *else_then_args[1] = { make_number(2, 1) };
        ast_add_statement(else_then, make_call("Log", else_then_args, 1, 1));

        ASTNode *else_final = ast_create_block();
        ASTNode *else_final_args[1] = { make_number(3, 1) };
        ast_add_statement(else_final, make_call("Log", else_final_args, 1, 1));

        ASTNode *nested_if = ast_create_if_statement();
        nested_if->data.if_stmt.condition = ast_create_boolean(false);
        nested_if->data.if_stmt.then_branch = else_then;
        nested_if->data.if_stmt.else_branch = else_final;

        ASTNode *outer_if = ast_create_if_statement();
        outer_if->data.if_stmt.condition = ast_create_boolean(true);
        outer_if->data.if_stmt.then_branch = then_block;
        outer_if->data.if_stmt.else_branch = nested_if;
        outer_if->line = 1; outer_if->column = 1;

        type_check_statement(outer_if, ctx);
        EXPECT(!ctx->has_error);

        semantic_context_destroy(ctx);
        ast_destroy(outer_if);
    }

    /* ---- RemoteFuture design: await returns Result, Claim/Read/Write/Release rejected ---- */

    TEST("await on local Future<Int> returns Int (not Result)");
    {
        SemanticContext *ctx = semantic_context_create();
        scope_enter(&ctx->scope, SCOPE_GLOBAL);
        ctx->in_async_func = true;

        Type *args[1] = { TYPE_INT };
        Type *future_type = type_create_constructed(TYPE_FUTURE, args, 1);
        scope_declare(ctx->scope,
            symbol_create_variable("local_future", future_type, 1, 1));

        ASTNode *await_expr =
            ast_create_await_expression(make_identifier("local_future", 1));
        Type *t = type_check_expression(await_expr, ctx);
        EXPECT(!ctx->has_error);
        EXPECT(t == TYPE_INT);

        semantic_context_destroy(ctx);
        ast_destroy(await_expr);
    }

    TEST("await on RemoteFuture<Int> returns Result<Int>");
    {
        SemanticContext *ctx = semantic_context_create();
        scope_enter(&ctx->scope, SCOPE_GLOBAL);
        ctx->in_async_func = true;

        Type *args[1] = { TYPE_INT };
        Type *future_type = type_create_constructed(TYPE_REMOTE_FUTURE, args, 1);
        scope_declare(ctx->scope,
            symbol_create_variable("remote_future", future_type, 1, 1));

        ASTNode *await_expr =
            ast_create_await_expression(make_identifier("remote_future", 1));
        Type *t = type_check_expression(await_expr, ctx);
        EXPECT(!ctx->has_error);
        EXPECT(t != NULL
            && t->kind == TYPE_KIND_CONSTRUCTED
            && type_equals(t->data.constructed.constructor, TYPE_RESULT));
        EXPECT(t != NULL
            && t->data.constructed.arg_count >= 1
            && t->data.constructed.args[0] == TYPE_INT);

        semantic_context_destroy(ctx);
        ast_destroy(await_expr);
    }

    TEST("Read on RemoteFuture is rejected with helpful error");
    {
        SemanticContext *ctx = semantic_context_create();
        scope_enter(&ctx->scope, SCOPE_GLOBAL);

        Type *args[1] = { TYPE_INT };
        Type *future_type = type_create_constructed(TYPE_REMOTE_FUTURE, args, 1);
        scope_declare(ctx->scope,
            symbol_create_variable("rf", future_type, 1, 1));

        ASTNode *read_args[1] = { make_identifier("rf", 1) };
        ASTNode *read_call = make_call("Read", read_args, 1, 1);
        type_check_expression(read_call, ctx);
        EXPECT(ctx->has_error
            && ctx_has_diagnostic_substring(ctx, "RemoteFuture does not support Read"));

        semantic_context_destroy(ctx);
        ast_destroy(read_call);
    }

    TEST("Write on RemoteFuture is rejected with helpful error");
    {
        SemanticContext *ctx = semantic_context_create();
        scope_enter(&ctx->scope, SCOPE_GLOBAL);

        Type *args[1] = { TYPE_INT };
        Type *future_type = type_create_constructed(TYPE_REMOTE_FUTURE, args, 1);
        scope_declare(ctx->scope,
            symbol_create_variable("rf", future_type, 1, 1));

        ASTNode *write_args[2] = { make_identifier("rf", 1), make_number(42, 1) };
        ASTNode *write_call = make_call("Write", write_args, 2, 1);
        type_check_expression(write_call, ctx);
        EXPECT(ctx->has_error
            && ctx_has_diagnostic_substring(ctx, "RemoteFuture does not support Write"));

        semantic_context_destroy(ctx);
        ast_destroy(write_call);
    }

    TEST("Release on RemoteFuture is rejected with helpful error");
    {
        SemanticContext *ctx = semantic_context_create();
        scope_enter(&ctx->scope, SCOPE_GLOBAL);

        Type *args[1] = { TYPE_INT };
        Type *future_type = type_create_constructed(TYPE_REMOTE_FUTURE, args, 1);
        scope_declare(ctx->scope,
            symbol_create_variable("rf", future_type, 1, 1));

        ASTNode *release_args[1] = { make_identifier("rf", 1) };
        ASTNode *release_call = make_call("Release", release_args, 1, 1);
        type_check_expression(release_call, ctx);
        EXPECT(ctx->has_error
            && ctx_has_diagnostic_substring(ctx, "RemoteFuture does not support Release"));

        semantic_context_destroy(ctx);
        ast_destroy(release_call);
    }

    TEST("Option<Int> annotation resolves to constructed type");
    {
        SemanticContext *ctx = semantic_context_create();
        scope_enter(&ctx->scope, SCOPE_GLOBAL);
        ASTNode *opt = make_generic_type("Option", "Int");
        Type *t = resolve_type_node(opt, ctx);
        EXPECT(!ctx->has_error);
        EXPECT(t != NULL
            && t->kind == TYPE_KIND_CONSTRUCTED
            && type_equals(t->data.constructed.constructor, TYPE_OPTION));
        EXPECT(t != NULL
            && t->data.constructed.arg_count >= 1
            && t->data.constructed.args[0] == TYPE_INT);
        semantic_context_destroy(ctx);
        ast_destroy(opt);
    }

    TEST("Some(42) returns Option<Int>");
    {
        SemanticContext *ctx = semantic_context_create();
        scope_enter(&ctx->scope, SCOPE_GLOBAL);
        ASTNode *args[1] = { make_number(42, 1) };
        ASTNode *call = make_call("Some", args, 1, 1);
        Type *t = type_check_expression(call, ctx);
        EXPECT(!ctx->has_error);
        EXPECT(t != NULL
            && t->kind == TYPE_KIND_CONSTRUCTED
            && type_equals(t->data.constructed.constructor, TYPE_OPTION));
        EXPECT(t != NULL
            && t->data.constructed.arg_count >= 1
            && t->data.constructed.args[0] == TYPE_INT);
        semantic_context_destroy(ctx);
        ast_destroy(call);
    }

    TEST("IsSome requires Option<T>");
    {
        SemanticContext *ctx = semantic_context_create();
        scope_enter(&ctx->scope, SCOPE_GLOBAL);
        ASTNode *args[1] = { make_number(42, 1) };
        ASTNode *call = make_call("IsSome", args, 1, 1);
        type_check_expression(call, ctx);
        EXPECT(ctx->has_error
            && ctx_has_diagnostic_substring(ctx, "IsSome requires Option<T>"));
        semantic_context_destroy(ctx);
        ast_destroy(call);
    }

    TEST("ToTObject returns tobject projection type but warns as boundary projection outside domain context");
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
        SemanticResult *result = semantic_analyze(program);

        EXPECT(!parser_has_error(parser));
        EXPECT(result != NULL && result->error_count == 0);
        EXPECT(result != NULL && result->warning_count == 1);
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "direct boundary projection outside relation/effect/zone/world context"));

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("ToTObject accepts subject source");
    {
        const char *source =
            "tobject BotDto { hp: Int; }\n"
            "subject Bot {\n"
            "    let hp: Int;\n"
            "}\n"
            "func Main() -> Void {\n"
            "    let bot: Bot = Bot();\n"
            "    let snapshot: BotDto = ToTObject(BotDto, bot);\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticResult *result = semantic_analyze(program);

        EXPECT(!parser_has_error(parser));
        EXPECT(result != NULL && result->error_count == 0);
        EXPECT(result != NULL && result->warning_count == 1);
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "direct boundary projection outside relation/effect/zone/world context"));

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("ToTObject rejects missing fields, non-tobject targets, and unnamed sources");
    {
        const char *source =
            "tobject PlayerDto { hp: Int; name: String; }\n"
            "object PlayerView { hp: Int; name: String; }\n"
            "subject Player { let hp: Int; }\n"
            "class PassivePlayer { let hp: Int; let name: String; }\n"
            "func Main() -> Void {\n"
            "    let player: Player = Player();\n"
            "    let passive: PassivePlayer = PassivePlayer();\n"
            "    let missing: PlayerDto = ToTObject(PlayerDto, player);\n"
            "    let wrong = ToTObject(Player, player);\n"
            "    let wrongView = ToTObject(PlayerView, player);\n"
            "    let anon = ToTObject(PlayerDto, Player());\n"
            "    let legacy = ToTObject(PlayerDto, passive);\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticResult *result = semantic_analyze(program);

        EXPECT(!parser_has_error(parser));
        EXPECT(result != NULL && result->error_count == 5);

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("ToObject returns object projection type but warns as internal projection outside domain context");
    {
        const char *source =
            "object PlayerView { hp: Int; name: String; }\n"
            "subject Player { let hp: Int; let name: String; }\n"
            "func Main() -> Void {\n"
            "    let player: Player = Player();\n"
            "    let view: PlayerView = ToObject(PlayerView, player);\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticResult *result = semantic_analyze(program);

        EXPECT(!parser_has_error(parser));
        EXPECT(result != NULL && result->error_count == 0);
        EXPECT(result != NULL && result->warning_count == 1);
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "direct internal projection outside relation/effect/zone/world context"));

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("object and tobject declarations may carry passive helper funcs");
    {
        const char *source =
            "object PlayerView {\n"
            "    hp: Int;\n"
            "    func Label(self) -> Int {\n"
            "        return hp;\n"
            "    }\n"
            "}\n"
            "tobject PlayerDto {\n"
            "    hp: Int;\n"
            "    func Export(self) -> Int {\n"
            "        return hp;\n"
            "    }\n"
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

    TEST("vessel declarations and subject vessel fields are accepted");
    {
        const char *source =
            "vessel HealthState {\n"
            "    current: Int;\n"
            "    func IsDead(self) -> Bool {\n"
            "        return current <= 0;\n"
            "    }\n"
            "}\n"
            "subject Player {\n"
            "    vessel health: HealthState;\n"
            "    action TakeDamage(self, amount: Int) -> Void {\n"
            "        Log(amount);\n"
            "    }\n"
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

    TEST("subject vessel fields reject non-vessel target types");
    {
        const char *source =
            "object PlayerView { hp: Int; }\n"
            "subject Player {\n"
            "    vessel view: PlayerView;\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticResult *result = semantic_analyze(program);

        EXPECT(!parser_has_error(parser));
        EXPECT(result != NULL && result->error_count > 0);
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "must reference a vessel type"));

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("subject action validates requires within causes and authorized by clauses");
    {
        const char *source =
            "vessel CombatStats { power: Int; }\n"
            "subject Player {\n"
            "    vessel combat: CombatStats;\n"
            "    action Attack(self, amount: Int) -> Void\n"
            "        requires Combatable\n"
            "        within BattleZone\n"
            "        causes DamageEffect\n"
            "        authorized by self {\n"
            "        Log(1);\n"
            "    }\n"
            "}\n"
            "ability Combatable { func Strike() -> Void; }\n"
            "role Warrior for Player {\n"
            "    impl ability Combatable { func Strike() -> Void { Log(1); } }\n"
            "}\n"
            "effect DamageEffect for target: Player { }\n"
            "zone BattleZone {\n"
            "    subject slot player: Player\n"
            "    effect slot damage: DamageEffect\n"
            "    authority player requires Combatable\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticResult *result = semantic_analyze(program);        EXPECT(!parser_has_error(parser));
        EXPECT(result != NULL && result->error_count == 0);

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("subject action authorized by requires subject-host parameters and matching zone authority");
    {
        const char *source =
            "subject Player {\n"
            "    action Trade(self, amount: Int) -> Void\n"
            "        within TradeZone\n"
            "        authorized by amount {\n"
            "        Log(1);\n"
            "    }\n"
            "}\n"
            "zone TradeZone {\n"
            "    subject slot buyer: Player\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticResult *result = semantic_analyze(program);

        EXPECT(!parser_has_error(parser));
        EXPECT(result != NULL && result->error_count > 0);
        EXPECT(ctx_has_diagnostic_substring_from_result(
            result, "must be a subject host"));

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("subject action causes effect and within zone enforce target compatibility");
    {
        const char *source =
            "subject Player {\n"
            "    action Attack(self) -> Void\n"
            "        within BattleZone\n"
            "        causes DamageEffect\n"
            "        authorized by self {\n"
            "        Log(1);\n"
            "    }\n"
            "}\n"
            "effect DamageEffect for bearer: Player { }\n"
            "zone BattleZone {\n"
            "    subject slot player: Player\n"
            "    authority player\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticResult *result = semantic_analyze(program);

        EXPECT(!parser_has_error(parser));
        EXPECT(result != NULL && result->error_count == 0);
        EXPECT(result != NULL && result->warning_count >= 1);
        EXPECT(ctx_has_diagnostic_substring_from_result(
            result, "has no matching effect slot"));

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("subject declarations allow both func and action");
    {
        const char *source =
            "subject Player {\n"
            "    let hp: Int;\n"
            "    func CalculatePower(self) -> Int {\n"
            "        return hp * 2;\n"
            "    }\n"
            "    action Tick(self) -> Void {\n"
            "        Log(1);\n"
            "    }\n"
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

    TEST("ToObject rejects non-object targets and non-subject sources");
    {
        const char *source =
            "object PlayerView { hp: Int; }\n"
            "tobject PlayerDto { hp: Int; }\n"
            "subject Player { let hp: Int; }\n"
            "class PassivePlayer { let hp: Int; }\n"
            "func Main() -> Void {\n"
            "    let player: Player = Player();\n"
            "    let passive: PassivePlayer = PassivePlayer();\n"
            "    let wrongDto = ToObject(PlayerDto, player);\n"
            "    let wrongSubject = ToObject(Player, player);\n"
            "    let legacy = ToObject(PlayerView, passive);\n"
            "    let anon = ToObject(PlayerView, Player());\n"
            "}\n";
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

    TEST("HasState works inside zone methods for declared states");
    {
        const char *source =
            "subject Player { let hp: Int; }\n"
            "relation TrustedLink for source: Player, target: Player { }\n"
            "effect Poisoned for bearer: Player { }\n"
            "zone BattleZone {\n"
            "    subject slot player: Player\n"
            "    subject slot enemy: Player\n"
            "    relation slot trust: TrustedLink\n"
            "    effect slot poison: Poisoned\n"
            "    state poisoned: effect poison on player\n"
            "    state allied: relation trust between player, enemy\n"
            "    func Tick() -> Void {\n"
            "        if HasState(poisoned) || HasState(\"allied\") || HasState(poisoned, player) || HasState(allied, player, enemy) {\n"
            "            Log(1);\n"
            "        }\n"
            "    }\n"
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

    TEST("HasState validates state slot arity and endpoint matching");
    {
        const char *source =
            "subject Player { let hp: Int; }\n"
            "zone BattleZone {\n"
            "    subject slot player: Player\n"
            "    subject slot enemy: Player\n"
            "    effect slot poison: Poisoned\n"
            "    relation slot trust: TrustedLink\n"
            "    state poisoned: effect poison on player\n"
            "    state allied: relation trust between player, enemy\n"
            "    func Tick() -> Void {\n"
            "        let a = HasState(poisoned, enemy);\n"
            "        let b = HasState(allied, player);\n"
            "        let c = HasState(allied, enemy, player);\n"
            "        Log(a || b || c);\n"
            "    }\n"
            "}\n"
            "effect Poisoned for bearer: Player { }\n"
            "relation TrustedLink for source: Player, target: Player { }\n";
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

    TEST("HasState rejects unknown states and use outside zone");
    {
        const char *source =
            "subject Player { let hp: Int; }\n"
            "zone BattleZone {\n"
            "    subject slot player: Player\n"
            "    state poisoned: effect poison on player\n"
            "    effect slot poison: Poisoned\n"
            "    func Tick() -> Void {\n"
            "        if HasState(missing) {\n"
            "            Log(1);\n"
            "        }\n"
            "    }\n"
            "}\n"
            "effect Poisoned for bearer: Player { }\n"
            "func Main() -> Void {\n"
            "    let active = HasState(\"poisoned\");\n"
            "    Log(active);\n"
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

    TEST("HasLayer works inside zone methods for declared relation and effect slots");
    {
        const char *source =
            "subject Player { let hp: Int; }\n"
            "relation TrustedLink for source: Player, target: Player { }\n"
            "effect Poisoned for bearer: Player { }\n"
            "zone BattleZone {\n"
            "    subject slot player: Player\n"
            "    subject slot enemy: Player\n"
            "    relation slot trust: TrustedLink\n"
            "    effect slot poison: Poisoned\n"
            "    func Tick() -> Void {\n"
            "        if HasLayer(trust) || HasLayer(\"poison\") {\n"
            "            Log(1);\n"
            "        }\n"
            "    }\n"
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

    TEST("HasLayer rejects unknown names and use outside zone");
    {
        const char *source =
            "subject Player { let hp: Int; }\n"
            "effect Poisoned for bearer: Player { }\n"
            "zone BattleZone {\n"
            "    subject slot player: Player\n"
            "    effect slot poison: Poisoned\n"
            "    func Tick() -> Void {\n"
            "        if HasLayer(missing) {\n"
            "            Log(1);\n"
            "        }\n"
            "    }\n"
            "}\n"
            "func Main() -> Void {\n"
            "    let active = HasLayer(\"poison\");\n"
            "    Log(active);\n"
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

    TEST("HasProjection works inside relation/effect/zone methods for declared object and tobject slots");
    {
        const char *source =
            "subject Player { let hp: Int; let name: String; }\n"
            "object PlayerView { hp: Int; }\n"
            "tobject PlayerDto { hp: Int; name: String; }\n"
            "relation TrustedLink for source: Player, target: Player {\n"
            "    subject slot left: Player\n"
            "    subject slot right: Player\n"
            "    object slot playerView: PlayerView\n"
            "    refresh playerView from left\n"
            "    func Inspect() -> Void {\n"
            "        if HasProjection(playerView) {\n"
            "            Log(1);\n"
            "        }\n"
            "    }\n"
            "}\n"
            "effect Poisoned for bearer: Player {\n"
            "    subject slot player: Player\n"
            "    tobject slot snapshot: PlayerDto\n"
            "    publish snapshot from player\n"
            "    func Tick() -> Void {\n"
            "        if HasProjection(\"snapshot\") {\n"
            "            Log(1);\n"
            "        }\n"
            "    }\n"
            "}\n"
            "zone BattleZone {\n"
            "    subject slot player: Player\n"
            "    object slot playerView: PlayerView\n"
            "    tobject slot snapshot: PlayerDto\n"
            "    refresh playerView from player\n"
            "    publish snapshot from player\n"
            "    func Update() -> Void {\n"
            "        if HasProjection(playerView) || HasProjection(\"snapshot\") {\n"
            "            Log(1);\n"
            "        }\n"
            "    }\n"
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

    TEST("HasProjection rejects subject slots, unknown names, and use outside domain context");
    {
        const char *source =
            "subject Player { let hp: Int; let name: String; }\n"
            "object PlayerView { hp: Int; }\n"
            "zone BattleZone {\n"
            "    subject slot player: Player\n"
            "    object slot snapshot: PlayerView\n"
            "    func Show() -> Void {\n"
            "        let a = HasProjection(player);\n"
            "        let b = HasProjection(missing);\n"
            "        Log(a || b);\n"
            "    }\n"
            "}\n"
            "func Main() -> Void {\n"
            "    let active = HasProjection(\"snapshot\");\n"
            "    Log(active);\n"
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

    TEST("HasZone works inside world methods for declared zone states");
    {
        const char *source =
            "zone BattleZone { }\n"
            "world GameWorld {\n"
            "    zone battle: BattleZone\n"
            "    state liveBattle: zone battle\n"
            "    activate liveBattle\n"
            "    maintain battle\n"
            "    func Tick() -> Void {\n"
            "        if HasZone(liveBattle) || HasZone(\"battle\") {\n"
            "            Log(1);\n"
            "        }\n"
            "    }\n"
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

    TEST("HasZone rejects unknown names and use outside world");
    {
        const char *source =
            "zone BattleZone { }\n"
            "world GameWorld {\n"
            "    zone battle: BattleZone\n"
            "    func Tick() -> Void {\n"
            "        let active = HasZone(missing);\n"
            "        Log(active);\n"
            "    }\n"
            "}\n"
            "func Main() -> Void {\n"
            "    let active = HasZone(\"battle\");\n"
            "    Log(active);\n"
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

    TEST("HasZoneProjection/HasZoneLayer/HasZoneState work inside world methods");
    {
        const char *source =
            "subject Player { let hp: Int; }\n"
            "object PlayerView { hp: Int; }\n"
            "effect Poisoned for bearer: Player { }\n"
            "zone BattleZone {\n"
            "    subject slot player: Player\n"
            "    object slot playerView: PlayerView\n"
            "    effect slot poison: Poisoned\n"
            "    state poisoned: effect poison on player\n"
            "    refresh playerView from player\n"
            "    maintain poisoned\n"
            "}\n"
            "world GameWorld {\n"
            "    zone battle: BattleZone\n"
            "    func Tick() -> Void {\n"
            "        if HasZoneProjection(battle, playerView) || HasZoneLayer(battle, poison) || HasZoneState(battle, poisoned) {\n"
            "            Log(1);\n"
            "        }\n"
            "    }\n"
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

    TEST("HasZoneProjection/HasZoneLayer/HasZoneState reject unknown names and use outside world");
    {
        const char *source =
            "subject Player { let hp: Int; }\n"
            "object PlayerView { hp: Int; }\n"
            "effect Poisoned for bearer: Player { }\n"
            "zone BattleZone {\n"
            "    subject slot player: Player\n"
            "    object slot playerView: PlayerView\n"
            "    effect slot poison: Poisoned\n"
            "    state poisoned: effect poison on player\n"
            "}\n"
            "world GameWorld {\n"
            "    zone battle: BattleZone\n"
            "    func Tick() -> Void {\n"
            "        let a = HasZoneProjection(battle, missing);\n"
            "        let b = HasZoneLayer(battle, missing);\n"
            "        let c = HasZoneState(battle, missing);\n"
            "        Log(a); Log(b); Log(c);\n"
            "    }\n"
            "}\n"
            "func Main() -> Void {\n"
            "    let a = HasZoneProjection(\"battle\", \"playerView\");\n"
            "    let b = HasZoneLayer(\"battle\", \"poison\");\n"
            "    let c = HasZoneState(\"battle\", \"poisoned\");\n"
            "    Log(a); Log(b); Log(c);\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticResult *result = semantic_analyze(program);

        EXPECT(!parser_has_error(parser));
        EXPECT(result != NULL && result->error_count == 6);

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("world derived states validate zone projection/layer/state contracts");
    {
        const char *source =
            "subject Player { let hp: Int; }\n"
            "object PlayerView { hp: Int; }\n"
            "effect Poisoned for bearer: Player { }\n"
            "zone BattleZone {\n"
            "    subject slot player: Player\n"
            "    object slot playerView: PlayerView\n"
            "    effect slot poison: Poisoned\n"
            "    state poisoned: effect poison on player\n"
            "    refresh playerView from player\n"
            "    maintain poisoned\n"
            "}\n"
            "world GameWorld {\n"
            "    zone battle: BattleZone\n"
            "    state battleProjected: zone battle projection playerView\n"
            "    state battleLayered: zone battle layer poison\n"
            "    state battlePoisoned: zone battle state poisoned\n"
            "    activate battle\n"
            "    func Tick() -> Void {\n"
            "        if HasZone(battleProjected) || HasZone(battleLayered) || HasZone(battlePoisoned) {\n"
            "            Log(1);\n"
            "        }\n"
            "    }\n"
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

    TEST("world derived states reject unknown details and lifecycle actions");
    {
        const char *source =
            "subject Player { let hp: Int; }\n"
            "object PlayerView { hp: Int; }\n"
            "effect Poisoned for bearer: Player { }\n"
            "zone BattleZone {\n"
            "    subject slot player: Player\n"
            "    object slot playerView: PlayerView\n"
            "    effect slot poison: Poisoned\n"
            "    state poisoned: effect poison on player\n"
            "}\n"
            "world GameWorld {\n"
            "    zone battle: BattleZone\n"
            "    state badProjection: zone battle projection missing\n"
            "    state badLayer: zone battle layer missing\n"
            "    state badState: zone battle state missing\n"
            "    state derivedPoisoned: zone battle state poisoned\n"
            "    activate derivedPoisoned\n"
            "}\n";
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

    TEST("world composed states validate ordered zone/state composition");
    {
        const char *source =
            "zone BattleZone { }\n"
            "world GameWorld {\n"
            "    zone battle: BattleZone\n"
            "    zone camp: BattleZone\n"
            "    state battleLive: zone battle\n"
            "    state campLive: zone camp\n"
            "    state allLive: all battleLive, campLive\n"
            "    state anyLive: any allLive, campLive\n"
            "    func Tick() -> Void {\n"
            "        if HasZone(allLive) || HasZone(anyLive) {\n"
            "            Log(1);\n"
            "        }\n"
            "    }\n"
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

    TEST("world composed states reject self-reference and forward references");
    {
        const char *source =
            "zone BattleZone { }\n"
            "world GameWorld {\n"
            "    zone battle: BattleZone\n"
            "    state looped: any looped, battle\n"
            "    state later: any definedLater, battle\n"
            "    state definedLater: zone battle\n"
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

    TEST("world composed states warn on duplicate and redundant plain zone inputs");
    {
        const char *source =
            "subject Player { let hp: Int; }\n"
            "zone BattleZone {\n"
            "    subject slot player: Player\n"
            "}\n"
            "world GameWorld {\n"
            "    zone battle: BattleZone\n"
            "    state battleLive: zone battle\n"
            "    state noisy: all battle, battleLive, battle\n"
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

    TEST("world lifecycle warns on duplicate and conflicting zone directions");
    {
        const char *source =
            "subject Player { let hp: Int; }\n"
            "zone BattleZone {\n"
            "    subject slot player: Player\n"
            "}\n"
            "world GameWorld {\n"
            "    zone battle: BattleZone\n"
            "    state liveBattle: zone battle\n"
            "    activate battle\n"
            "    activate liveBattle\n"
            "    deactivate battle\n"
            "    deactivate liveBattle\n"
            "    maintain battle\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticResult *result = semantic_analyze(program);

        EXPECT(!parser_has_error(parser));
        EXPECT(result != NULL && result->error_count == 0);
        EXPECT(result != NULL && result->warning_count >= 6);

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("world composed states warn when raw zone slots are used as derived inputs");
    {
        const char *source =
            "subject Player { let hp: Int; }\n"
            "zone BattleZone {\n"
            "    subject slot player: Player\n"
            "}\n"
            "world GameWorld {\n"
            "    zone battle: BattleZone\n"
            "    zone camp: BattleZone\n"
            "    state campLive: zone camp\n"
            "    state mixed: any battle, campLive\n"
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

    TEST("Zones embedded into a world cannot be mutated through the old binding");
    {
        const char *source =
            "subject Player { let hp: Int; }\n"
            "zone BattleZone {\n"
            "    subject slot player: Player\n"
            "    shared hp: Int = 0\n"
            "    func Hurt() -> Void { hp = hp + 1; }\n"
            "}\n"
            "world GameWorld {\n"
            "    zone battle: BattleZone\n"
            "}\n"
            "func Main() -> Void {\n"
            "    let player = Player(10);\n"
            "    let battle = BattleZone(player);\n"
            "    let world = GameWorld(battle);\n"
            "    battle.hp = 3;\n"
            "    battle.Hurt();\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticResult *result = semantic_analyze(program);

        EXPECT(!parser_has_error(parser));
        EXPECT(result != NULL && result->error_count == 2);
        EXPECT(result != NULL && result->warning_count >= 1);
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "implicitly copies zone binding 'battle'"));

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("Clone makes world zone copy explicit and keeps old binding mutable");
    {
        const char *source =
            "subject Player { let hp: Int; }\n"
            "zone BattleZone {\n"
            "    subject slot player: Player\n"
            "    shared hp: Int = 0\n"
            "    func Hurt() -> Void { hp = hp + 1; }\n"
            "}\n"
            "world GameWorld {\n"
            "    zone battle: BattleZone\n"
            "}\n"
            "func Main() -> Void {\n"
            "    let battle = BattleZone(Player(10));\n"
            "    let world = GameWorld(Clone(battle));\n"
            "    battle.hp = 3;\n"
            "    battle.Hurt();\n"
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

    /* ---- Generic class semantic tests ---- */

    TEST("generic class declaration passes semantic check");
    {
        const char *source =
            "class Box<T> {\n"
            "    let value: T;\n"
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

    TEST("generic class with method passes semantic check");
    {
        const char *source =
            "class Wrapper<T> {\n"
            "    let data: T;\n"
            "    func GetData(self) -> T {\n"
            "        return self.data;\n"
            "    }\n"
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

    TEST("generic class with concrete usage passes semantic check");
    {
        const char *source =
            "class Pair<T> {\n"
            "    let first: T;\n"
            "    let second: T;\n"
            "}\n"
            "func Main() -> Void {\n"
            "    let p: Pair<Int> = Pair(3, 7);\n"
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
}

/* -----------------------------------------------------------------
 * Main
 * ----------------------------------------------------------------- */
