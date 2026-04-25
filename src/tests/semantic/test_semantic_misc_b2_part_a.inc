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
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "projection slots only exist on relation/effect/zone surfaces"));
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "only accepts object/tobject slots declared on the current zone"));

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
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "Unknown world zone/state 'missing' in HasZone(...)"));
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "Contract source:"));
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "current world: GameWorld"));
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "world observability path is HasZone(missing)"));
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "HasZone(...) is only available inside world declarations and world methods."));

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
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "Unknown zone projection slot 'missing'"));
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "Contract source:"));
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "current world: GameWorld"));
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "world embedding query path is HasZoneProjection(battle, missing)"));
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "embedded zone slot 'battle' resolves to zone 'BattleZone'"));
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "HasZoneProjection(...) is only available inside world declarations and world methods."));

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
        EXPECT(result != NULL && result->error_count == 3);
        EXPECT(result != NULL && result->warning_count == 0);
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "implicitly copies zone binding 'battle'"));
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "embedding handoff edge is 'battle' -> world 'GameWorld' slot 'battle'"));
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "cannot be mutated via assignment after it was embedded into world 'GameWorld' slot 'battle'"));
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "cannot be mutated via hosted func/action call after it was embedded into world 'GameWorld' slot 'battle'"));
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "Reason:"));
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "derived embedding provenance points to world 'GameWorld' slot 'battle'"));
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "Fix:"));
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "mutate it through world 'GameWorld' slot 'battle' after embedding"));

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

    TEST("generic function where clause accepts matching exact bound");
    {
        const char *source =
            "func OnlyInt<T>(value: T) -> T where T: Int {\n"
            "    return value;\n"
            "}\n"
            "func Main() -> Void {\n"
            "    let x: Int = OnlyInt(3);\n"
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

    TEST("generic function where clause rejects non-matching exact bound");
    {
        const char *source =
            "func OnlyInt<T>(value: T) -> T where T: Int {\n"
            "    return value;\n"
            "}\n"
            "func Main() -> Void {\n"
            "    let y = OnlyInt(\"nope\");\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticResult *result = semantic_analyze(program);

        EXPECT(!parser_has_error(parser));
        EXPECT(result != NULL && result->error_count > 0);
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "does not satisfy constraint 'Int'"));
        EXPECT(ctx_has_diagnostic_substring_from_result(result, "Reason:"));
        EXPECT(ctx_has_diagnostic_substring_from_result(result, "Fix:"));

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("generic class where clause rejects unknown bound type");
    {
        const char *source =
            "class Box<T> where T: MissingConstraint {\n"
            "    let value: T;\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticResult *result = semantic_analyze(program);

        EXPECT(!parser_has_error(parser));
        EXPECT(result != NULL && result->error_count > 0);
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "Unknown constraint type 'MissingConstraint' in where clause"));

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("role where clause rejects unknown bound type");
    {
        const char *source =
            "subject Player { let hp: Int; }\n"
            "role Support<T> for T where T: MissingAbility {\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticResult *result = semantic_analyze(program);

        EXPECT(!parser_has_error(parser));
        EXPECT(result != NULL && result->error_count > 0);
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "Unknown constraint type 'MissingAbility' in where clause"));

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("generic function where clause accepts ability-style role constraint");
    {
        const char *source =
            "ability Comparable { }\n"
            "subject Player { let hp: Int; }\n"
            "role PlayerComparable for Player {\n"
            "    impl ability Comparable { }\n"
            "}\n"
            "func Keep<T>(value: T) -> T where T: Comparable {\n"
            "    return value;\n"
            "}\n"
            "func Main() -> Void {\n"
            "    let p: Player = Player(3);\n"
            "    let q: Player = Keep(p);\n"
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

    TEST("generic function where clause rejects missing ability-style role constraint");
    {
        const char *source =
            "ability Comparable { }\n"
            "subject Merchant { let trust: Int; }\n"
            "func Keep<T>(value: T) -> T where T: Comparable {\n"
            "    return value;\n"
            "}\n"
            "func Main() -> Void {\n"
            "    let m: Merchant = Merchant(1);\n"
            "    let n: Merchant = Keep(m);\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticResult *result = semantic_analyze(program);

        EXPECT(!parser_has_error(parser));
        EXPECT(result != NULL && result->error_count > 0);
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "does not satisfy constraint 'Comparable'"));
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "actual type args are"));
        EXPECT(ctx_has_diagnostic_substring_from_result(result, "Reason:"));
        EXPECT(ctx_has_diagnostic_substring_from_result(result, "Fix:"));

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("generic function where clause accepts multiple ability-style bounds");
    {
        const char *source =
            "ability Comparable { }\n"
            "ability Cloneable { }\n"
            "subject Player { let hp: Int; }\n"
            "role PlayerCapabilities for Player {\n"
            "    impl ability Comparable { }\n"
            "    impl ability Cloneable { }\n"
            "}\n"
            "func Keep<T>(value: T) -> T where T: Comparable + Cloneable {\n"
            "    return value;\n"
            "}\n"
            "func Main() -> Void {\n"
            "    let p: Player = Player(3);\n"
            "    let q: Player = Keep(p);\n"
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

    TEST("generic function where clause rejects missing one of multiple ability-style bounds");
    {
        const char *source =
            "ability Comparable { }\n"
            "ability Cloneable { }\n"
            "subject Player { let hp: Int; }\n"
            "role PlayerComparable for Player {\n"
            "    impl ability Comparable { }\n"
            "}\n"
            "func Keep<T>(value: T) -> T where T: Comparable + Cloneable {\n"
            "    return value;\n"
            "}\n"
            "func Main() -> Void {\n"
            "    let p: Player = Player(3);\n"
            "    let q: Player = Keep(p);\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticResult *result = semantic_analyze(program);

        EXPECT(!parser_has_error(parser));
        EXPECT(result != NULL && result->error_count > 0);
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "does not satisfy constraint 'Cloneable'"));
        EXPECT(ctx_has_diagnostic_substring_from_result(result, "Reason:"));
        EXPECT(ctx_has_diagnostic_substring_from_result(result, "Fix:"));

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("generic class instantiation enforces exact where constraint");
    {
        const char *source =
            "class Box<T> where T: Int {\n"
            "    let value: T;\n"
            "}\n"
            "func Main() -> Void {\n"
            "    let ok: Box<Int> = Box(3);\n"
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

    TEST("generic class instantiation rejects non matching where constraint");
    {
        const char *source =
            "class Box<T> where T: Int {\n"
            "    let value: T;\n"
            "}\n"
            "func Main() -> Void {\n"
            "    let bad: Box<String> = Box(\"oops\");\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticResult *result = semantic_analyze(program);

        EXPECT(!parser_has_error(parser));
        EXPECT(result != NULL && result->error_count > 0);
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "does not satisfy constraint 'Int'"));
        EXPECT(ctx_has_diagnostic_substring_from_result(result, "Reason:"));
        EXPECT(ctx_has_diagnostic_substring_from_result(result, "Fix:"));

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }
