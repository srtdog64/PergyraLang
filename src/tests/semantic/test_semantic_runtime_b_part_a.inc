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
