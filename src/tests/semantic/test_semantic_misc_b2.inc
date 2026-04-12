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

    TEST("generic class instantiation accepts ability style where constraint");
    {
        const char *source =
            "ability Comparable { }\n"
            "subject Player { let hp: Int; }\n"
            "role PlayerComparable for Player {\n"
            "    impl ability Comparable { }\n"
            "}\n"
            "class Crate<T> where T: Comparable {\n"
            "    let value: T;\n"
            "}\n"
            "func Main() -> Void {\n"
            "    let c: Crate<Player> = Crate(Player(1));\n"
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

    TEST("generic class instantiation rejects missing ability style where constraint");
    {
        const char *source =
            "ability Comparable { }\n"
            "subject Merchant { let trust: Int; }\n"
            "class Crate<T> where T: Comparable {\n"
            "    let value: T;\n"
            "}\n"
            "func Main() -> Void {\n"
            "    let c: Crate<Merchant> = Crate(Merchant(1));\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticResult *result = semantic_analyze(program);

        EXPECT(!parser_has_error(parser));
        EXPECT(result != NULL && result->error_count > 0);
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "does not satisfy constraint 'Comparable'"));
        EXPECT(ctx_has_diagnostic_substring_from_result(result, "Reason:"));
        EXPECT(ctx_has_diagnostic_substring_from_result(result, "Fix:"));

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("innate ability implementation is allowed inside declaring module");
    {
        const char *source =
            "innate ability Combatable {\n"
            "    func Attack() -> Int;\n"
            "}\n"
            "subject Fighter { let hp: Int; }\n"
            "role Warrior for Fighter {\n"
            "    impl ability Combatable {\n"
            "        func Attack() -> Int { return 10; }\n"
            "    }\n"
            "}\n"
            "func Main() -> Void {\n"
            "    let fighter: Fighter = Fighter(1);\n"
            "    Log(fighter.hp);\n"
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

    TEST("innate ability rejects implementation outside declaring module");
    {
        const char *ability_path = "test_innate_ability_module_a.pgy";
        const char *main_path = "test_innate_ability_module_b.pgy";
        const char *ability_source =
            "export innate ability Combatable {\n"
            "    func Attack() -> Int;\n"
            "}\n";
        const char *main_source =
            "import \"test_innate_ability_module_a.pgy\";\n"
            "subject Hacker { let hp: Int; }\n"
            "role Rogue for Hacker {\n"
            "    impl ability Combatable {\n"
            "        func Attack() -> Int { return 7; }\n"
            "    }\n"
            "}\n"
            "func Main() -> Void {\n"
            "    let h: Hacker = Hacker(1);\n"
            "    Log(h.hp);\n"
            "}\n";
        FILE *ability_file = fopen(ability_path, "wb");
        FILE *main_file = fopen(main_path, "wb");
        char *error_message = NULL;
        ASTNode *program = NULL;
        SemanticResult *result = NULL;

        EXPECT(ability_file != NULL && main_file != NULL);
        if (ability_file != NULL) {
            fputs(ability_source, ability_file);
            fclose(ability_file);
        }
        if (main_file != NULL) {
            fputs(main_source, main_file);
            fclose(main_file);
        }

        program = import_resolver_load_program(main_path, &error_message);
        EXPECT(program != NULL);
        if (program != NULL)
            result = semantic_analyze(program);
        EXPECT(error_message == NULL);
        EXPECT(result != NULL && result->error_count > 0);
        EXPECT(result != NULL && ctx_has_diagnostic_substring_from_result(result,
            "innate ability 'Combatable' cannot be implemented outside its declaring module"));

        free(error_message);
        semantic_result_destroy(result);
        ast_destroy(program);
        remove(main_path);
        remove(ability_path);
    }

    TEST("explicit export hides non-exported imported declarations");
    {
        const char *module_path = "test_export_visibility_module_a.pgy";
        const char *main_path = "test_export_visibility_module_main_a.pgy";
        const char *module_source =
            "func Hidden() -> Int { return 1; }\n"
            "export func Visible() -> Int { return 2; }\n";
        const char *main_source =
            "import \"test_export_visibility_module_a.pgy\";\n"
            "func Main() -> Void {\n"
            "    Log(Visible());\n"
            "    Log(Hidden());\n"
            "}\n";
        FILE *module_file = fopen(module_path, "wb");
        FILE *main_file = fopen(main_path, "wb");
        char *error_message = NULL;
        ASTNode *program = NULL;
        SemanticResult *result = NULL;

        EXPECT(module_file != NULL && main_file != NULL);
        if (module_file != NULL) {
            fputs(module_source, module_file);
            fclose(module_file);
        }
        if (main_file != NULL) {
            fputs(main_source, main_file);
            fclose(main_file);
        }

        program = import_resolver_load_program(main_path, &error_message);
        EXPECT(program != NULL);
        if (program != NULL)
            result = semantic_analyze(program);
        EXPECT(error_message == NULL);
        EXPECT(result != NULL && result->error_count > 0);
        EXPECT(result != NULL && ctx_has_diagnostic_substring_from_result(result,
            "Undefined function 'Hidden'"));

        free(error_message);
        semantic_result_destroy(result);
        ast_destroy(program);
        remove(main_path);
        remove(module_path);
    }

    TEST("explicit export keeps exported imported declarations visible");
    {
        const char *module_path = "test_export_visibility_module_b.pgy";
        const char *main_path = "test_export_visibility_module_main_b.pgy";
        const char *module_source =
            "func Hidden() -> Int { return 1; }\n"
            "export func Visible() -> Int { return 2; }\n";
        const char *main_source =
            "import \"test_export_visibility_module_b.pgy\";\n"
            "func Main() -> Void {\n"
            "    Log(Visible());\n"
            "}\n";
        FILE *module_file = fopen(module_path, "wb");
        FILE *main_file = fopen(main_path, "wb");
        char *error_message = NULL;
        ASTNode *program = NULL;
        SemanticResult *result = NULL;

        EXPECT(module_file != NULL && main_file != NULL);
        if (module_file != NULL) {
            fputs(module_source, module_file);
            fclose(module_file);
        }
        if (main_file != NULL) {
            fputs(main_source, main_file);
            fclose(main_file);
        }

        program = import_resolver_load_program(main_path, &error_message);
        EXPECT(program != NULL);
        if (program != NULL)
            result = semantic_analyze(program);
        EXPECT(error_message == NULL);
        EXPECT(result != NULL && result->error_count == 0);

        free(error_message);
        semantic_result_destroy(result);
        ast_destroy(program);
        remove(main_path);
        remove(module_path);
    }

    TEST("explicit export hides non-exported imported nominal constructors");
    {
        const char *source =
            "class HiddenBox {\n"
            "    let value: Int;\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticContext *ctx = semantic_context_create();
        ASTNode *box = NULL;
        ASTNode *call = NULL;
        Type *resolved = NULL;

        EXPECT(!parser_has_error(parser));
        EXPECT(ctx != NULL && program != NULL);
        box = program != NULL ? program->data.program.statements[0] : NULL;
        EXPECT(box != NULL);
        box->origin_path = pergyra_strdup("hidden_box_module.pgy");
        ctx->program_root = program;

        type_check_class_decl(box, ctx);
        ctx->current_module_path = "consumer_module.pgy";

        call = make_call("HiddenBox", (ASTNode *[]){ make_number(1, 1) }, 1, 1);
        call->origin_path = pergyra_strdup("consumer_module.pgy");
        resolved = type_check_expression(call, ctx);

        EXPECT(resolved == TYPE_UNKNOWN);
        EXPECT(ctx->has_error);
        EXPECT(ctx_has_diagnostic_substring(ctx,
            "Constructor 'HiddenBox' is not accessible across the current visibility boundary"));

        ast_destroy(call);
        ast_destroy(program);
        semantic_context_destroy(ctx);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("explicit export keeps exported imported nominal constructors visible");
    {
        const char *module_path = "test_export_visibility_module_d.pgy";
        const char *main_path = "test_export_visibility_module_main_d.pgy";
        const char *module_source =
            "export class PublicBox {\n"
            "    let value: Int;\n"
            "}\n";
        const char *main_source =
            "import \"test_export_visibility_module_d.pgy\";\n"
            "func Main() -> Void {\n"
            "    let h: PublicBox = PublicBox(1);\n"
            "    Log(h.value);\n"
            "}\n";
        FILE *module_file = fopen(module_path, "wb");
        FILE *main_file = fopen(main_path, "wb");
        char *error_message = NULL;
        ASTNode *program = NULL;
        SemanticResult *result = NULL;

        EXPECT(module_file != NULL && main_file != NULL);
        if (module_file != NULL) {
            fputs(module_source, module_file);
            fclose(module_file);
        }
        if (main_file != NULL) {
            fputs(main_source, main_file);
            fclose(main_file);
        }

        program = import_resolver_load_program(main_path, &error_message);
        EXPECT(program != NULL);
        if (program != NULL)
            result = semantic_analyze(program);
        EXPECT(error_message == NULL);
        EXPECT(result != NULL && result->error_count == 0);

        free(error_message);
        semantic_result_destroy(result);
        ast_destroy(program);
        remove(main_path);
        remove(module_path);
    }

    TEST("unknown stdlib use is rejected");
    {
        const char *source =
            "use missingmod;\n"
            "func Main() -> Void { return; }\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticResult *result = semantic_analyze(program);

        EXPECT(!parser_has_error(parser));
        EXPECT(result != NULL && result->error_count > 0);
        EXPECT(result != NULL && ctx_has_diagnostic_substring_from_result(
            result, "Unknown stdlib use 'missingmod'"));

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("known stdlib use remains accepted");
    {
        const char *source =
            "use datetime;\n"
            "func Main() -> Void { Log(Now()); }\n";
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

    TEST("new known stdlib use remains accepted");
    {
        const char *source =
            "use money;\n"
            "func Main() -> Void {\n"
            "    let amount: Money = MoneyOf(100, \"KRW\");\n"
            "    Log(RenderMoney(amount));\n"
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

    TEST("layered stdlib and domain kit uses remain accepted");
    {
        const char *main_path = "test_layered_stdlib_domain_kits_use_main.pgy";
        const char *main_source =
            "use datetime;\n"
            "use money;\n"
            "use timer;\n"
            "use versioning;\n"
            "use ledger;\n"
            "use obligation;\n"
            "use device_adapter;\n"
            "func Main() -> Void {\n"
            "    let amount: Money = MoneyOf(100, \"KRW\");\n"
            "    let version: VersionStamp = VersionInitial(\"wallet\");\n"
            "    let key: IdempotencyKey = MakeIdempotencyKey(\"pay\", 1);\n"
            "    let posting: LedgerPosting = BuildTransferPosting(\"cash\", \"sales\", amount, version, key, \"memo\");\n"
            "    let started: Instant = InstantNow();\n"
            "    let delay: Duration = DurationMs(50);\n"
            "    let deadline: Instant = InstantAdd(started, delay);\n"
            "    let schedule: TimerSpec = TimerAfter(\"tick\", started.milliseconds, delay.milliseconds);\n"
            "    let due: Bool = TimerExpired(schedule, deadline.milliseconds);\n"
            "    let rule: Obligation = OpenObligation(\"RULE\", \"merchant\", started.milliseconds, 1000);\n"
            "    let review: ObligationCheck = EvaluateObligation(rule, deadline.milliseconds);\n"
            "    let reg: DeviceRegister = Register(\"temp\", 4096);\n"
            "    let sample: DeviceSample = SampleDevice(reg, 7, deadline.milliseconds);\n"
            "    Log(RenderLedgerPosting(posting));\n"
            "    Log(RenderObligation(rule));\n"
            "    Log(RenderDeviceSample(sample));\n"
            "    Log(ToString(due));\n"
            "    Log(ToString(review.violated));\n"
            "}\n";
        FILE *main_file = fopen(main_path, "wb");
        char *error_message = NULL;
        ASTNode *program = NULL;
        SemanticResult *result = NULL;

        EXPECT(main_file != NULL);
        if (main_file != NULL) {
            fputs(main_source, main_file);
            fclose(main_file);
        }

        program = import_resolver_load_program(main_path, &error_message);
        EXPECT(program != NULL);
        EXPECT(error_message == NULL);
        if (program != NULL) {
            result = semantic_analyze(program);
        }
        EXPECT(result != NULL && result->error_count == 0);

        free(error_message);
        semantic_result_destroy(result);
        ast_destroy(program);
        remove(main_path);
    }

    TEST("intent declaration binds subjects and validates step contracts");
    {
        const char *source =
            "subject Player {\n"
            "    let hp: Int;\n"
            "    action pay(self) -> Void { return; }\n"
            "}\n"
            "subject Merchant {\n"
            "    let trust: Int;\n"
            "}\n"
            "ability Payable { func Pay() -> Void; }\n"
            "role BuyerRole for Player {\n"
            "    impl ability Payable {\n"
            "        func Pay() -> Void { return; }\n"
            "    }\n"
            "}\n"
            "effect PaymentEffect for bearer: Player { }\n"
            "zone PaymentZone {\n"
            "    subject slot buyer: Player\n"
            "}\n"
            "intent Purchase {\n"
            "    involves buyer: Player;\n"
            "    involves seller: Merchant;\n"
            "    step pay {\n"
            "        where: PaymentZone;\n"
            "        who: buyer;\n"
            "        requires: Payable;\n"
            "        authorized by: buyer;\n"
            "        causes: PaymentEffect;\n"
            "        expect: buyer.hp >= 0;\n"
            "    }\n"
            "    success: buyer.hp >= 0;\n"
            "    failure: false;\n"
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

    TEST("intent declaration rejects unknown who participant");
    {
        const char *source =
            "subject Player {\n"
            "    let hp: Int;\n"
            "    action pay(self) -> Void { return; }\n"
            "}\n"
            "zone PaymentZone {\n"
            "    subject slot buyer: Player\n"
            "}\n"
            "intent Purchase {\n"
            "    involves buyer: Player;\n"
            "    step pay {\n"
            "        where: PaymentZone;\n"
            "        who: ghost;\n"
            "        expect: true;\n"
            "    }\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticResult *result = semantic_analyze(program);

        EXPECT(!parser_has_error(parser));
        EXPECT(result != NULL && result->error_count > 0);
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "unknown participant 'ghost'"));

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("intent declaration enforces zone slots and subject abilities");
    {
        const char *source =
            "subject Player {\n"
            "    let hp: Int;\n"
            "    action Fight(self) -> Void { return; }\n"
            "}\n"
            "subject Merchant {\n"
            "    let trust: Int;\n"
            "}\n"
            "ability Striking { func StrikeMark() -> String; }\n"
            "zone BattleZone {\n"
            "    subject slot buyer: Merchant\n"
            "    authority buyer requires Striking\n"
            "}\n"
            "intent Clash {\n"
            "    involves hero: Player;\n"
            "    step Fight {\n"
            "        where: BattleZone;\n"
            "        who: hero;\n"
            "        requires: Striking;\n"
            "        authorized by: hero;\n"
            "        expect: true;\n"
            "    }\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticResult *result = semantic_analyze(program);

        EXPECT(!parser_has_error(parser));
        EXPECT(result != NULL && result->error_count > 0);
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "has no matching subject slot"));
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "does not implement it"));

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("intent declaration supports params and bool step contracts");
    {
        const char *source =
            "subject Driver {\n"
            "    let started: Bool;\n"
            "    action Ignite(self) -> Void { self.started = true; }\n"
            "}\n"
            "zone CockpitZone {\n"
            "    subject slot driver: Driver\n"
            "}\n"
            "intent DriveCar(cockpit: CockpitZone, driver: Driver) {\n"
            "    exclusive;\n"
            "    priority: 3;\n"
            "    step Ignite {\n"
            "        where: CockpitZone;\n"
            "        using: cockpit;\n"
            "        who: driver;\n"
            "        on: driver.Ignite();\n"
            "        on: driver.Ignite();\n"
            "        pre: true;\n"
            "        guard: driver.started;\n"
            "        post: driver.started;\n"
            "        invariant: driver.started;\n"
            "        expect: driver.started;\n"
            "    }\n"
            "    success: driver.started;\n"
            "    failure: false;\n"
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

    TEST("intent step inherits action requires within causes and authorized by");
    {
        const char *source =
            "ability Payable { func Pay() -> Void; }\n"
            "subject Player {\n"
            "    let hp: Int;\n"
            "    action Pay(self) -> Void\n"
            "        requires Payable\n"
            "        within PaymentZone\n"
            "        causes PaymentEffect\n"
            "        authorized by self {\n"
            "        return;\n"
            "    }\n"
            "}\n"
            "role BuyerRole for Player {\n"
            "    impl ability Payable {\n"
            "        func Pay() -> Void { return; }\n"
            "    }\n"
            "}\n"
            "effect PaymentEffect for bearer: Player { }\n"
            "zone PaymentZone {\n"
            "    subject slot buyer: Player\n"
            "    effect slot payment: PaymentEffect\n"
            "    authority buyer requires Payable\n"
            "}\n"
            "intent Purchase {\n"
            "    involves buyer: Player;\n"
            "    step Pay {\n"
            "        who: buyer;\n"
            "        expect: true;\n"
            "    }\n"
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

    TEST("intent authority diagnostic mentions inherited action zone");
    {
        const char *source =
            "/// @effects secure\n"
            "func Gate() -> Bool { return true; }\n"
            "subject Driver {\n"
            "    let started: Bool;\n"
            "    action Ignite(self) -> Void\n"
            "        within CockpitZone {\n"
            "        self.started = true;\n"
            "    }\n"
            "}\n"
            "zone CockpitZone {\n"
            "    subject slot driver: Driver\n"
            "    authority driver\n"
            "}\n"
            "intent DriveCar {\n"
            "    involves driver: Driver;\n"
            "    step Ignite {\n"
            "        who: driver;\n"
            "        on: Gate();\n"
            "        expect: true;\n"
            "    }\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticResult *result = semantic_analyze(program);

        EXPECT(!parser_has_error(parser));
        EXPECT(result != NULL && result->error_count > 0);
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "inherited from matching action contract"));
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "inherited zone from matching action contract: CockpitZone"));

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("intent authority diagnostic suggests single who alias for authorized by");
    {
        const char *source =
            "subject Driver {\n"
            "    let started: Bool;\n"
            "}\n"
            "zone CockpitZone {\n"
            "    subject slot driver: Driver\n"
            "    authority driver\n"
            "}\n"
            "intent DriveCar(cockpit: CockpitZone, driver: Driver) {\n"
            "    step Ignite {\n"
            "        where: CockpitZone;\n"
            "        using: cockpit;\n"
            "        who: driver;\n"
            "        on: true;\n"
            "        expect: true;\n"
            "    }\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticResult *result = semantic_analyze(program);

        EXPECT(!parser_has_error(parser));
        EXPECT(result != NULL && result->error_count > 0);
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "add 'authorized by: driver;' to the step"));

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("intent authorized participant mismatch diagnostic includes reason and fix");
    {
        const char *source =
            "subject Driver {\n"
            "    let started: Bool;\n"
            "}\n"
            "subject Passenger {\n"
            "    let seated: Bool;\n"
            "}\n"
            "zone CockpitZone {\n"
            "    subject slot driver: Driver\n"
            "    authority driver\n"
            "}\n"
            "intent DriveCar {\n"
            "    involves passenger: Passenger;\n"
            "    step Ignite {\n"
            "        where: CockpitZone;\n"
            "        who: passenger;\n"
            "        authorized by: passenger;\n"
            "        on: true;\n"
            "        expect: true;\n"
            "    }\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticResult *result = semantic_analyze(program);

        EXPECT(!parser_has_error(parser));
        EXPECT(result != NULL && result->error_count > 0);
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "has no matching subject slot"));
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "Reason:"));
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "Fix:"));

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("subject return diagnostic includes reason and fix");
    {
        const char *source =
            "subject Courier {\n"
            "    let level: Int;\n"
            "}\n"
            "func MakeCourier() -> Courier {\n"
            "    return Courier();\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticResult *result = semantic_analyze(program);

        EXPECT(!parser_has_error(parser));
        EXPECT(result != NULL && result->error_count > 0);
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "Returning subjects by value is not supported yet"));
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "subject values are zone/world anchored handles"));
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "return a struct/class/object/tobject projection instead"));

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("intent transfer mismatch diagnostic includes reason and fix");
    {
        const char *source =
            "subject Courier { let level: Int; }\n"
            "zone LoadingZone {\n"
            "    subject slot courier: Courier\n"
            "}\n"
            "zone DeliveryZone {\n"
            "    subject slot courier: Courier\n"
            "}\n"
            "intent MoveCargo(load: LoadingZone, deliver: DeliveryZone, courier: Courier) {\n"
            "    step Deliver {\n"
            "        who: courier;\n"
            "        where: LoadingZone;\n"
            "        using: load;\n"
            "        transfer: load -> deliver;\n"
            "        expect: true;\n"
            "    }\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticResult *result = semantic_analyze(program);

        EXPECT(!parser_has_error(parser));
        EXPECT(result != NULL && result->error_count > 0);
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "does not match the current zone contract"));
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "Reason:"));
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "Fix:"));
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "using binding does not match the transfer target"));
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "transfer target derivation would choose using 'deliver'"));
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "transfer target derivation would choose zone 'DeliveryZone'"));
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "locally declared zone on step: LoadingZone"));
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "locally declared using on step: load"));

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("intent authority diagnostic mentions locally declared step zone");
    {
        const char *source =
            "/// @effects secure\n"
            "func Gate() -> Bool { return true; }\n"
            "subject Driver {\n"
            "    let started: Bool;\n"
            "    action Ignite(self) -> Void {\n"
            "        self.started = true;\n"
            "    }\n"
            "}\n"
            "zone CockpitZone {\n"
            "    subject slot driver: Driver\n"
            "    authority driver\n"
            "}\n"
            "intent DriveCar(cockpit: CockpitZone, driver: Driver) {\n"
            "    step Ignite {\n"
            "        where: CockpitZone;\n"
            "        using: cockpit;\n"
            "        who: driver;\n"
            "        on: Gate();\n"
            "        expect: true;\n"
            "    }\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticResult *result = semantic_analyze(program);

        EXPECT(!parser_has_error(parser));
        EXPECT(result != NULL && result->error_count > 0);
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "locally declared zone on step: CockpitZone"));
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "locally declared using on step: cockpit"));
        EXPECT(!ctx_has_diagnostic_substring_from_result(result,
            "inherited zone from matching action"));

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("duplicate stdlib use emits warning but stays accepted");
    {
        const char *source =
            "use datetime;\n"
            "use datetime;\n"
            "func Main() -> Void {\n"
            "    Log(Now());\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticResult *result = semantic_analyze(program);

        EXPECT(!parser_has_error(parser));
        EXPECT(result != NULL && result->error_count == 0);
        EXPECT(result != NULL && result->warning_count > 0);
        EXPECT(result != NULL && ctx_has_diagnostic_substring_from_result(
            result, "Duplicate stdlib use 'datetime'"));

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("intent authority diagnostic mentions derived transfer zone and using");
    {
        const char *source =
            "subject Courier { let level: Int; }\n"
            "zone LoadingZone {\n"
            "    subject slot courier: Courier\n"
            "}\n"
            "zone DeliveryZone {\n"
            "    subject slot courier: Courier\n"
            "    authority courier\n"
            "}\n"
            "intent MoveCargo(load: LoadingZone, deliver: DeliveryZone, courier: Courier) {\n"
            "    step Deliver {\n"
            "        transfer: load -> deliver;\n"
            "        who: courier;\n"
            "        expect: true;\n"
            "    }\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticResult *result = semantic_analyze(program);

        EXPECT(!parser_has_error(parser));
        EXPECT(result != NULL && result->error_count > 0);
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "derived zone from transfer target: DeliveryZone"));
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "derived using from transfer target: deliver"));

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("zone projection diagnostic includes reason and fix");
    {
        const char *source =
            "subject Player { let hp: Int; }\n"
            "object PlayerView { hp: Int; }\n"
            "zone BattleZone {\n"
            "    subject slot player: Player\n"
            "    subject slot wrong: Player\n"
            "    bind wrong from player\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticResult *result = semantic_analyze(program);

        EXPECT(!parser_has_error(parser));
        EXPECT(result != NULL && result->error_count > 0);
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "is not a projection slot"));
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "Reason:"));
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "ownership anchors, not projection sinks"));
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "Fix:"));
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "choose an object slot for local projection sync"));

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("intent authority-bearing zone rejects secure helper call without authorized by");
    {
        const char *source =
            "/// @effects secure\n"
            "func Gate() -> Bool { return true; }\n"
            "subject Driver {\n"
            "    let started: Bool;\n"
            "    action Ignite(self) -> Void { self.started = true; }\n"
            "}\n"
            "zone CockpitZone {\n"
            "    subject slot driver: Driver\n"
            "    authority driver\n"
            "}\n"
            "intent DriveCar(cockpit: CockpitZone, driver: Driver) {\n"
            "    step Ignite {\n"
            "        where: CockpitZone;\n"
            "        using: cockpit;\n"
            "        who: driver;\n"
            "        on: Gate();\n"
            "        expect: true;\n"
            "    }\n"
            "    success: true;\n"
            "    failure: false;\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticResult *result = semantic_analyze(program);

        EXPECT(!parser_has_error(parser));
        EXPECT(result != NULL && result->error_count > 0);
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "invokes authority-sensitive helpers"));

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("intent authority-bearing zone rejects zone-action helper call without authorized by");
    {
        const char *source =
            "subject Driver {\n"
            "    let started: Bool;\n"
            "    action Ignite(self) -> Void within CockpitZone causes Started {\n"
            "        self.started = true;\n"
            "    }\n"
            "}\n"
            "effect Started for bearer: Driver {\n"
            "    subject slot bearer: Driver\n"
            "}\n"
            "zone CockpitZone {\n"
            "    subject slot driver: Driver\n"
            "    effect slot started: Started\n"
            "    authority driver\n"
            "}\n"
            "intent DriveCar(cockpit: CockpitZone, driver: Driver) {\n"
            "    step Ignite {\n"
            "        where: CockpitZone;\n"
            "        using: cockpit;\n"
            "        who: driver;\n"
            "        on: driver.Ignite();\n"
            "        expect: true;\n"
            "    }\n"
            "    success: true;\n"
            "    failure: false;\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticResult *result = semantic_analyze(program);

        EXPECT(!parser_has_error(parser));
        EXPECT(result != NULL && result->error_count > 0);
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "invokes authority-sensitive helpers"));

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("intent declaration rejects non-bool guard and invariant");
    {
        const char *source =
            "subject Driver {\n"
            "    let hp: Int;\n"
            "    action Ignite(self) -> Void { hp = hp + 1; }\n"
            "}\n"
            "zone CockpitZone {\n"
            "    subject slot driver: Driver\n"
            "}\n"
            "intent DriveCar(driver: Driver) {\n"
            "    step Ignite {\n"
            "        where: CockpitZone;\n"
            "        who: driver;\n"
            "        on: driver.Ignite();\n"
            "        guard: driver.hp;\n"
            "        invariant: driver.hp;\n"
            "        expect: true;\n"
            "    }\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticResult *result = semantic_analyze(program);

        EXPECT(!parser_has_error(parser));
        EXPECT(result != NULL && result->error_count > 0);
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "Intent guard expects a Bool value"));
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "Intent invariant expects a Bool value"));

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("intent declaration supports compensate and trace builtins");
    {
        const char *source =
            "subject Driver {\n"
            "    let started: Bool;\n"
            "    action Ignite(self) -> Void { self.started = true; }\n"
            "    action RollbackIgnite(self) -> Void { self.started = false; }\n"
            "}\n"
            "zone CockpitZone {\n"
            "    subject slot driver: Driver\n"
            "}\n"
            "intent DriveCar(driver: Driver) {\n"
            "    rollback: current;\n"
            "    step Ignite {\n"
            "        where: CockpitZone;\n"
            "        who: driver;\n"
            "        on: driver.Ignite();\n"
            "        compensate: driver.RollbackIgnite();\n"
            "        pre: true;\n"
            "        guard: false;\n"
            "        post: driver.started;\n"
            "        expect: true;\n"
            "    }\n"
            "    success: true;\n"
            "    failure: false;\n"
            "}\n"
            "func Main() -> Void {\n"
            "    Log(IntentLastTrace());\n"
            "    Log(IntentLastFailure());\n"
            "    Log(IntentLastName());\n"
            "    Log(ToString(IntentLastHandle()));\n"
            "    Log(ToString(IntentLastTraceId()));\n"
            "    Log(ToString(IntentLastStepCount()));\n"
            "    Log(ToString(IntentLastFailed()));\n"
            "    Log(ToString(IntentHistoryCount()));\n"
            "    Log(IntentHistoryStepName(0));\n"
            "    Log(IntentHistoryStepZone(0));\n"
            "    Log(IntentHistoryStepPhase(0));\n"
            "    Log(IntentHistoryStepParticipant(0));\n"
            "    Log(IntentHistoryStepSlot(0));\n"
            "    Log(IntentHistoryStepFromZone(0));\n"
            "    Log(IntentHistoryStepFromSlot(0));\n"
            "    Log(IntentHistoryStepToZone(0));\n"
            "    Log(IntentHistoryStepToSlot(0));\n"
            "    Log(ToString(IntentHistoryStepOk(0)));\n"
            "    Log(IntentHistoryStepFailure(0));\n"
            "    Log(ToString(IntentActiveCount()));\n"
            "    Log(IntentActiveName(0));\n"
            "    Log(ToString(IntentActiveHandle(0)));\n"
            "    Log(ToString(IntentActiveParentHandle(0)));\n"
            "    Log(ToString(IntentActiveTraceId(0)));\n"
            "    Log(ToString(IntentActivePriority(0)));\n"
            "    Log(ToString(IntentActiveSubjectCount(0)));\n"
            "    Log(ToString(IntentActiveStepCount(0)));\n"
            "    Log(ToString(IntentActiveConcurrent(0)));\n"
            "    Log(ToString(IntentActiveFailed(0)));\n"
            "    Log(IntentActiveFailure(0));\n"
            "    Log(IntentActiveTrace(0));\n"
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

    TEST("intent declaration validates cross-world transfer bindings");
    {
        const char *source =
            "subject Buyer {\n"
            "    let hp: Int;\n"
            "    action Promote(self) -> Void { hp = hp + 1; }\n"
            "}\n"
            "zone CartZone {\n"
            "    subject slot buyer: Buyer\n"
            "}\n"
            "zone PaymentZone {\n"
            "    subject slot buyer: Buyer\n"
            "}\n"
            "intent Checkout(cart: CartZone, payment: PaymentZone, buyer: Buyer) {\n"
            "    step Promote {\n"
            "        where: PaymentZone;\n"
            "        using: payment;\n"
            "        transfer: cart -> payment;\n"
            "        who: buyer;\n"
            "        on: buyer.Promote();\n"
            "        expect: true;\n"
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

    TEST("intent step infers using and where from transfer target");
    {
        const char *source =
            "subject Buyer {\n"
            "    let hp: Int;\n"
            "    action Promote(self) -> Void { hp = hp + 1; }\n"
            "}\n"
            "zone CartZone {\n"
            "    subject slot buyer: Buyer\n"
            "}\n"
            "zone PaymentZone {\n"
            "    subject slot buyer: Buyer\n"
            "}\n"
            "intent Checkout(cart: CartZone, payment: PaymentZone, buyer: Buyer) {\n"
            "    step Promote {\n"
            "        transfer: cart -> payment;\n"
            "        who: buyer;\n"
            "        on: buyer.Promote();\n"
            "        expect: true;\n"
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

    TEST("intent step move shorthand infers using and where from transfer target");
    {
        const char *source =
            "subject Buyer {\n"
            "    let hp: Int;\n"
            "    action Promote(self) -> Void { hp = hp + 1; }\n"
            "}\n"
            "zone CartZone {\n"
            "    subject slot buyer: Buyer\n"
            "}\n"
            "zone PaymentZone {\n"
            "    subject slot buyer: Buyer\n"
            "}\n"
            "intent Checkout(cart: CartZone, payment: PaymentZone, buyer: Buyer) {\n"
            "    step Promote {\n"
            "        move cart to payment;\n"
            "        who: buyer;\n"
            "        on: buyer.Promote();\n"
            "        expect: true;\n"
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

    TEST("intent step warns when matching action contract is restated locally");
    {
        const char *source =
            "ability DriverCap { func Ping() -> Void; }\n"
            "subject Driver {\n"
            "    let started: Bool;\n"
            "    action Ignite(self) -> Void\n"
            "        requires DriverCap\n"
            "        within CockpitZone\n"
            "        causes Started\n"
            "        authorized by self {\n"
            "        self.started = true;\n"
            "    }\n"
            "}\n"
            "role DriverCapRole for Driver {\n"
            "    impl ability DriverCap {\n"
            "        func Ping() -> Void { return; }\n"
            "    }\n"
            "}\n"
            "effect Started for bearer: Driver {\n"
            "    subject slot bearer: Driver\n"
            "}\n"
            "zone CockpitZone {\n"
            "    subject slot driver: Driver\n"
            "    authority driver requires DriverCap\n"
            "}\n"
            "intent DriveCar(cockpit: CockpitZone, driver: Driver) {\n"
            "    step Ignite {\n"
            "        who: driver;\n"
            "        where: CockpitZone;\n"
            "        using: cockpit;\n"
            "        requires: DriverCap;\n"
            "        causes: Started;\n"
            "        authorized by: driver;\n"
            "        on: true;\n"
            "        expect: true;\n"
            "    }\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticResult *result = semantic_analyze(program);

        EXPECT(!parser_has_error(parser));
        EXPECT(result != NULL && result->error_count == 0);
        EXPECT(result != NULL && result->warning_count >= 1);
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "restates contract clauses already provided by the matching action contract"));
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "Matching action contract: Driver.Ignite"));
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "Redundant clauses:"));
        EXPECT(ctx_has_diagnostic_substring_from_result(result, "- who"));
        EXPECT(ctx_has_diagnostic_substring_from_result(result, "- where"));
        EXPECT(ctx_has_diagnostic_substring_from_result(result, "- requires"));
        EXPECT(ctx_has_diagnostic_substring_from_result(result, "- causes"));
        EXPECT(ctx_has_diagnostic_substring_from_result(result, "- authorized by"));

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("intent step inherits action where requires causes and authorized by");
    {
        const char *source =
            "ability Prepared {\n"
            "    func Ready() -> Bool;\n"
            "}\n"
            "subject Hero {\n"
            "    let hp: Int;\n"
            "    action Guard(self) -> Void\n"
            "        requires Prepared\n"
            "        within BattleZone\n"
            "        authorized by self\n"
            "        causes Guarded {\n"
            "        self.hp = self.hp + 1;\n"
            "    }\n"
            "}\n"
            "role HeroPrepared for Hero {\n"
            "    impl ability Prepared {\n"
            "        func Ready() -> Bool { return true; }\n"
            "    }\n"
            "}\n"
            "effect Guarded for bearer: Hero { }\n"
            "zone BattleZone {\n"
            "    subject slot hero: Hero\n"
            "    effect slot guarded: Guarded\n"
            "    authority hero requires Prepared\n"
            "}\n"
            "intent Patrol(battle: BattleZone, hero: Hero) {\n"
            "    step Guard {\n"
            "        who: hero;\n"
            "        on: hero.Guard();\n"
            "        expect: true;\n"
            "    }\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticResult *result = semantic_analyze(program);
        ASTNode *intent = NULL;
        ASTNode *step = NULL;

        if (program != NULL && program->type == AST_PROGRAM) {
            for (size_t i = 0; i < program->data.program.count; i++) {
                ASTNode *stmt = program->data.program.statements[i];
                if (stmt != NULL && stmt->type == AST_INTENT_DECL) {
                    intent = stmt;
                    break;
                }
            }
        }
        if (intent != NULL && intent->data.intent_decl.step_count > 0)
            step = intent->data.intent_decl.steps[0];

        EXPECT(!parser_has_error(parser));
        EXPECT(result != NULL && result->error_count == 0);
        EXPECT(step != NULL);
        EXPECT(step != NULL && step->data.intent_step.inherited_where_from_action);
        EXPECT(step != NULL && step->data.intent_step.inherited_requires_from_action);
        EXPECT(step != NULL && step->data.intent_step.inherited_causes_from_action);
        EXPECT(step != NULL && step->data.intent_step.inherited_authorized_by_from_action);
        EXPECT(step != NULL
            && step->data.intent_step.where_type != NULL
            && step->data.intent_step.where_type->type == AST_TYPE
            && strcmp(step->data.intent_step.where_type->data.type.name, "BattleZone") == 0);
        EXPECT(step != NULL
            && step->data.intent_step.required_ability_count == 1
            && step->data.intent_step.required_abilities[0] != NULL
            && step->data.intent_step.required_abilities[0]->type == AST_TYPE
            && strcmp(step->data.intent_step.required_abilities[0]->data.type.name, "Prepared") == 0);
        EXPECT(step != NULL
            && step->data.intent_step.causes_effect != NULL
            && strcmp(step->data.intent_step.causes_effect, "Guarded") == 0);
        EXPECT(step != NULL
            && step->data.intent_step.authorized_by_count == 1
            && strcmp(step->data.intent_step.authorized_by[0], "hero") == 0);
        EXPECT(step != NULL
            && step->data.intent_step.using_expr != NULL
            && step->data.intent_step.using_expr->type == AST_IDENTIFIER
            && strcmp(step->data.intent_step.using_expr->data.identifier.name, "battle") == 0);

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("intent step infers who from unique matching subject action");
    {
        const char *source =
            "subject Hero {\n"
            "    let hp: Int;\n"
            "    action Guard(self) -> Void\n"
            "        within BattleZone\n"
            "        authorized by self {\n"
            "        self.hp = self.hp + 1;\n"
            "    }\n"
            "}\n"
            "zone BattleZone {\n"
            "    subject slot hero: Hero\n"
            "    authority hero\n"
            "}\n"
            "intent Patrol(battle: BattleZone, hero: Hero) {\n"
            "    step Guard {\n"
            "        using: battle;\n"
            "        on: hero.Guard();\n"
            "        expect: true;\n"
            "    }\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticResult *result = semantic_analyze(program);
        ASTNode *intent = NULL;
        ASTNode *step = NULL;

        if (program != NULL && program->type == AST_PROGRAM) {
            for (size_t i = 0; i < program->data.program.count; i++) {
                ASTNode *stmt = program->data.program.statements[i];
                if (stmt != NULL && stmt->type == AST_INTENT_DECL) {
                    intent = stmt;
                    break;
                }
            }
        }
        if (intent != NULL && intent->data.intent_decl.step_count > 0)
            step = intent->data.intent_decl.steps[0];

        EXPECT(!parser_has_error(parser));
        EXPECT(result != NULL && result->error_count == 0);
        EXPECT(step != NULL && step->data.intent_step.inherited_who_from_action);
        EXPECT(step != NULL && step->data.intent_step.who_count == 1);
        EXPECT(step != NULL && strcmp(step->data.intent_step.who_names[0], "hero") == 0);
        EXPECT(step != NULL && step->data.intent_step.authorized_by_count == 1);
        EXPECT(step != NULL && strcmp(step->data.intent_step.authorized_by[0], "hero") == 0);
        EXPECT(step != NULL
            && step->data.intent_step.using_expr != NULL
            && step->data.intent_step.using_expr->type == AST_IDENTIFIER
            && strcmp(step->data.intent_step.using_expr->data.identifier.name, "battle") == 0);

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("intent step infers where from explicit using zone binding");
    {
        const char *source =
            "subject Hero {\n"
            "    let hp: Int;\n"
            "    action Guard(self) -> Void {\n"
            "        self.hp = self.hp + 1;\n"
            "    }\n"
            "}\n"
            "zone BattleZone {\n"
            "    subject slot hero: Hero\n"
            "    authority hero\n"
            "}\n"
            "intent Patrol(battle: BattleZone, hero: Hero) {\n"
            "    step Guard {\n"
            "        using: battle;\n"
            "        who: hero;\n"
            "        on: hero.Guard();\n"
            "        expect: true;\n"
            "    }\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticResult *result = semantic_analyze(program);
        ASTNode *intent = NULL;
        ASTNode *step = NULL;

        if (program != NULL && program->type == AST_PROGRAM) {
            for (size_t i = 0; i < program->data.program.count; i++) {
                ASTNode *stmt = program->data.program.statements[i];
                if (stmt != NULL && stmt->type == AST_INTENT_DECL) {
                    intent = stmt;
                    break;
                }
            }
        }
        if (intent != NULL && intent->data.intent_decl.step_count > 0)
            step = intent->data.intent_decl.steps[0];

        EXPECT(!parser_has_error(parser));
        EXPECT(result != NULL && result->error_count == 0);
        EXPECT(step != NULL
            && step->data.intent_step.where_type != NULL
            && step->data.intent_step.where_type->type == AST_TYPE
            && strcmp(step->data.intent_step.where_type->data.type.name, "BattleZone") == 0);

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("intent step move shorthand accepts unique target zone type");
    {
        const char *source =
            "subject Buyer {\n"
            "    let hp: Int;\n"
            "    action Promote(self) -> Void { hp = hp + 1; }\n"
            "}\n"
            "zone CartZone {\n"
            "    subject slot buyer: Buyer\n"
            "}\n"
            "zone PaymentZone {\n"
            "    subject slot buyer: Buyer\n"
            "}\n"
            "intent Checkout(cart: CartZone, payment: PaymentZone, buyer: Buyer) {\n"
            "    step Promote {\n"
            "        move cart to PaymentZone;\n"
            "        who: buyer;\n"
            "        on: buyer.Promote();\n"
            "        expect: true;\n"
            "    }\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticResult *result = semantic_analyze(program);
        ASTNode *intent = NULL;
        ASTNode *step = NULL;

        if (program != NULL && program->type == AST_PROGRAM) {
            for (size_t i = 0; i < program->data.program.count; i++) {
                ASTNode *stmt = program->data.program.statements[i];
                if (stmt != NULL && stmt->type == AST_INTENT_DECL) {
                    intent = stmt;
                    break;
                }
            }
        }
        if (intent != NULL && intent->data.intent_decl.step_count > 0)
            step = intent->data.intent_decl.steps[0];

        EXPECT(!parser_has_error(parser));
        EXPECT(result != NULL && result->error_count == 0);
        EXPECT(step != NULL
            && step->data.intent_step.transfer_to_alias != NULL
            && strcmp(step->data.intent_step.transfer_to_alias, "payment") == 0);
        EXPECT(step != NULL
            && step->data.intent_step.using_expr != NULL
            && step->data.intent_step.using_expr->type == AST_IDENTIFIER
            && strcmp(step->data.intent_step.using_expr->data.identifier.name, "payment") == 0);
        EXPECT(step != NULL
            && step->data.intent_step.where_type != NULL
            && step->data.intent_step.where_type->type == AST_TYPE
            && strcmp(step->data.intent_step.where_type->data.type.name, "PaymentZone") == 0);

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("intent step may orchestrate a subintent without local where/who");
    {
        const char *source =
            "subject Buyer {\n"
            "    let hp: Int;\n"
            "}\n"
            "func Touch() -> Void { return; }\n"
            "zone CheckoutZone {\n"
            "    subject slot buyer: Buyer\n"
            "}\n"
            "intent Charge(checkout: CheckoutZone, buyer: Buyer) {\n"
            "    step verify {\n"
            "        where: CheckoutZone;\n"
            "        using: checkout;\n"
            "        who: buyer;\n"
            "        expect: true;\n"
            "    }\n"
            "    success: true;\n"
            "    failure: false;\n"
            "}\n"
            "intent Checkout(checkout: CheckoutZone, buyer: Buyer) {\n"
            "    step pay {\n"
            "        intent: Charge(checkout, buyer);\n"
            "        expect: true;\n"
            "    }\n"
            "    success: true;\n"
            "    failure: false;\n"
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

    TEST("using alias statement lowers to local binding");
    {
        const char *source =
            "subject Player {\n"
            "    let hp: Int;\n"
            "    func ReadHP(self) -> Int {\n"
            "        using self.hp as hp;\n"
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

    TEST("intent declaration rejects spawn inside step clauses");
    {
        const char *source =
            "subject Buyer {\n"
            "    let hp: Int;\n"
            "}\n"
            "zone CheckoutZone {\n"
            "    subject slot buyer: Buyer\n"
            "}\n"
            "intent Checkout(checkout: CheckoutZone, buyer: Buyer) {\n"
            "    step pay {\n"
            "        where: CheckoutZone;\n"
            "        using: checkout;\n"
            "        who: buyer;\n"
            "        on: spawn Touch();\n"
            "        expect: true;\n"
            "    }\n"
            "    success: true;\n"
            "    failure: false;\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticResult *result = semantic_analyze(program);

        EXPECT(!parser_has_error(parser));
        EXPECT(result != NULL && result->error_count > 0);
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "on clause cannot contain 'spawn'"));

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("Now and Sleep builtins type-check");
    {
        const char *source =
            "func Main() -> Void {\n"
            "    let t0: Int = Now();\n"
            "    Sleep(1);\n"
            "    let t1: Int = Now();\n"
            "    Log(ToString(t1 - t0));\n"
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

    TEST("ReadLine builtin type-checks");
    {
        const char *source =
            "func Main() -> Void {\n"
            "    let line: String = ReadLine();\n"
            "    Print(line);\n"
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

    TEST("intent step intent clause must target another intent");
    {
        const char *source =
            "subject Buyer {\n"
            "    let hp: Int;\n"
            "    func Pay(self) -> Bool { return true; }\n"
            "}\n"
            "func Charge(buyer: Buyer) -> Bool { return true; }\n"
            "intent Checkout(buyer: Buyer) {\n"
            "    step pay {\n"
            "        intent: Charge(buyer);\n"
            "        expect: true;\n"
            "    }\n"
            "    success: true;\n"
            "    failure: false;\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticResult *result = semantic_analyze(program);

        EXPECT(!parser_has_error(parser));
        EXPECT(result != NULL && result->error_count > 0);
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "is not a declared intent"));

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("ability fields cannot reference non-exported foreign type");
    {
        const char *source =
            "subject Hidden { let hp: Int; }\n"
            "ability NeedsHidden {\n"
            "    fields h: Hidden\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticResult *result;

        EXPECT(!parser_has_error(parser));
        program->data.program.statements[0]->origin_path = pergyra_strdup("hidden_module.pgy");
        program->data.program.statements[1]->origin_path = pergyra_strdup("ability_module.pgy");

        result = semantic_analyze(program);
        EXPECT(result != NULL && result->error_count > 0);
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "cannot declare field 'h' in fields with non-exported type 'Hidden'"));

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("role impl cannot reference non-exported foreign ability");
    {
        const char *module_path = "test_role_impl_hidden_ability_module_a.pgy";
        const char *main_path = "test_role_impl_hidden_ability_module_main_a.pgy";
        const char *module_source =
            "private ability HiddenAbility {\n"
            "    func Ping() -> Void;\n"
            "}\n";
        const char *main_source =
            "import \"test_role_impl_hidden_ability_module_a.pgy\";\n"
            "subject Hero { let hp: Int; }\n"
            "role HeroRole for Hero {\n"
            "    impl ability HiddenAbility {\n"
            "        func Ping() -> Void { return; }\n"
            "    }\n"
            "}\n";
        FILE *module_file = fopen(module_path, "wb");
        FILE *main_file = fopen(main_path, "wb");
        char *error_message = NULL;
        ASTNode *program = NULL;
        SemanticResult *result = NULL;

        EXPECT(module_file != NULL && main_file != NULL);
        if (module_file != NULL) {
            fputs(module_source, module_file);
            fclose(module_file);
        }
        if (main_file != NULL) {
            fputs(main_source, main_file);
            fclose(main_file);
        }

        program = import_resolver_load_program(main_path, &error_message);
        EXPECT(program != NULL);
        if (program != NULL)
            result = semantic_analyze(program);
        EXPECT(error_message == NULL);
        EXPECT(result != NULL && result->error_count > 0);
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "cannot implement non-exported ability 'HiddenAbility'"));

        free(error_message);
        semantic_result_destroy(result);
        ast_destroy(program);
        remove(main_path);
        remove(module_path);
    }

    TEST("action requires cannot reference non-exported foreign ability");
    {
        const char *module_path = "test_action_requires_hidden_ability_module_a.pgy";
        const char *main_path = "test_action_requires_hidden_ability_module_main_a.pgy";
        const char *module_source =
            "private ability HiddenAbility {\n"
            "    func Ping() -> Void;\n"
            "}\n";
        const char *main_source =
            "import \"test_action_requires_hidden_ability_module_a.pgy\";\n"
            "subject Hero {\n"
            "    let hp: Int;\n"
            "    action Do(self) -> Void requires HiddenAbility {\n"
            "        return;\n"
            "    }\n"
            "}\n";
        FILE *module_file = fopen(module_path, "wb");
        FILE *main_file = fopen(main_path, "wb");
        char *error_message = NULL;
        ASTNode *program = NULL;
        SemanticResult *result = NULL;

        EXPECT(module_file != NULL && main_file != NULL);
        if (module_file != NULL) {
            fputs(module_source, module_file);
            fclose(module_file);
        }
        if (main_file != NULL) {
            fputs(main_source, main_file);
            fclose(main_file);
        }

        program = import_resolver_load_program(main_path, &error_message);
        EXPECT(program != NULL);
        if (program != NULL)
            result = semantic_analyze(program);
        EXPECT(error_message == NULL);
        EXPECT(result != NULL && result->error_count > 0);
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "cannot require non-exported ability 'HiddenAbility'"));

        free(error_message);
        semantic_result_destroy(result);
        ast_destroy(program);
        remove(main_path);
        remove(module_path);
    }

    TEST("action requires accepts generic ability type arguments when ability is generic");
    {
        const char *source =
            "ability Combatable<T> { func Ping() -> Void; }\n"
            "subject Hero {\n"
            "    let hp: Int;\n"
            "    action Do(self) -> Void requires Combatable<Int> {\n"
            "        return;\n"
            "    }\n"
            "}\n"
            "role HeroRole for Hero {\n"
            "    impl ability Combatable<Int> { func Ping() -> Void { return; } }\n"
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

    TEST("action requires rejects generic ability type arguments when ability is non-generic");
    {
        const char *source =
            "ability Combatable { func Ping() -> Void; }\n"
            "subject Hero {\n"
            "    action Do(self) -> Void requires Combatable<Int> {\n"
            "        return;\n"
            "    }\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticResult *result = semantic_analyze(program);

        EXPECT(!parser_has_error(parser));
        EXPECT(result != NULL && result->error_count > 0);
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "does not accept generic type arguments"));

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("zone authority requires accepts generic ability type arguments when ability is generic");
    {
        const char *source =
            "ability Combatable<T> { func Ping() -> Void; }\n"
            "subject Hero { let hp: Int; }\n"
            "role HeroRole for Hero {\n"
            "    impl ability Combatable<Int> { func Ping() -> Void { return; } }\n"
            "}\n"
            "zone BattleZone {\n"
            "    subject slot hero: Hero\n"
            "    authority hero requires Combatable<Int>\n"
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

    TEST("zone authority requires rejects wrong generic ability arity");
    {
        const char *source =
            "ability Combatable<T> { func Ping() -> Void; }\n"
            "subject Hero { let hp: Int; }\n"
            "role HeroRole for Hero {\n"
            "    impl ability Combatable { func Ping() -> Void { return; } }\n"
            "}\n"
            "zone BattleZone {\n"
            "    subject slot hero: Hero\n"
            "    authority hero requires Combatable<Int, String>\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticResult *result = semantic_analyze(program);

        EXPECT(!parser_has_error(parser));
        EXPECT(result != NULL && result->error_count > 0);
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "requires 1 generic argument(s)"));

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("action requires rejects mismatched generic ability impl");
    {
        const char *source =
            "ability Combatable<T> { func Ping() -> Void; }\n"
            "subject Hero {\n"
            "    action Do(self) -> Void requires Combatable<Int> {\n"
            "        return;\n"
            "    }\n"
            "}\n"
            "role HeroRole for Hero {\n"
            "    impl ability Combatable<String> { func Ping() -> Void { return; } }\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticResult *result = semantic_analyze(program);

        EXPECT(!parser_has_error(parser));
        EXPECT(result != NULL && result->error_count > 0);
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "requires ability 'Combatable<Int>'"));
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "implements 'Combatable<String>' instead"));
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "Reason:"));
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "Fix:"));

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("zone authority reports required and actual generic ability mismatch");
    {
        const char *source =
            "ability Combatable<T> { func Ping() -> Void; }\n"
            "subject Hero { let hp: Int; }\n"
            "role HeroRole for Hero {\n"
            "    impl ability Combatable<String> { func Ping() -> Void { return; } }\n"
            "}\n"
            "zone BattleZone {\n"
            "    subject slot hero: Hero\n"
            "    authority hero requires Combatable<Int>\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticResult *result = semantic_analyze(program);

        EXPECT(!parser_has_error(parser));
        EXPECT(result != NULL && result->error_count > 0);
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "requires ability 'Combatable<Int>'"));
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "implements 'Combatable<String>' instead"));
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "Reason:"));
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "Fix:"));

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("party role slot reports required and actual generic ability mismatch");
    {
        const char *source =
            "ability Combatable<T> { func Ping() -> Void; }\n"
            "subject Hero { let hp: Int; }\n"
            "role HeroRole for Hero {\n"
            "    impl ability Combatable<String> { func Ping() -> Void { return; } }\n"
            "}\n"
            "party RaidTeam {\n"
            "    role slot fighter: Combatable<Int>\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticResult *result = semantic_analyze(program);

        EXPECT(!parser_has_error(parser));
        EXPECT(result != NULL && result->error_count > 0);
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "requires ability 'Combatable<Int>'"));
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "implement 'Combatable<String>' instead"));
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "Reason:"));
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "Fix:"));

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("role impl rejects generic type arguments when ability is non-generic");
    {
        const char *source =
            "ability Combatable { func Ping() -> Void; }\n"
            "subject Hero { let hp: Int; }\n"
            "role HeroRole for Hero {\n"
            "    impl ability Combatable<Int> { func Ping() -> Void { return; } }\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticResult *result = semantic_analyze(program);

        EXPECT(!parser_has_error(parser));
        EXPECT(result != NULL && result->error_count > 0);
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "does not accept generic type arguments in impl clauses"));
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "ability declaration 'Combatable' is non-generic"));

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("role impl rejects wrong generic ability arity");
    {
        const char *source =
            "ability Combatable<T> { func Ping() -> Void; }\n"
            "subject Hero { let hp: Int; }\n"
            "role HeroRole for Hero {\n"
            "    impl ability Combatable<Int, String> { func Ping() -> Void { return; } }\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticResult *result = semantic_analyze(program);

        EXPECT(!parser_has_error(parser));
        EXPECT(result != NULL && result->error_count > 0);
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "requires 1 generic argument(s) in impl clauses, got 2"));
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "impl ability uses 'Combatable<Int, String>'"));

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("generic ability declaration accepts generic fields entries and method signatures");
    {
        const char *source =
            "ability Bufferable<T> {\n"
            "    fields items: List<T>;\n"
            "    func Put(value: T) -> Void;\n"
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

    TEST("generic ability declaration accepts where clause bounds");
    {
        const char *source =
            "ability Bufferable<T> where T: String {\n"
            "    fields items: List<T>;\n"
            "    func Put(value: T) -> Void;\n"
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

    TEST("generic function declaration rejects default type argument for now");
    {
        const char *source =
            "func Identity<T = Int>(value: T) -> T {\n"
            "    return value;\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticResult *result = semantic_analyze(program);

        EXPECT(!parser_has_error(parser));
        EXPECT(result != NULL && result->error_count > 0);
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "Default generic type arguments are not supported yet in function declarations"));

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("generic class declaration rejects default type argument for now");
    {
        const char *source =
            "class Box<T = Int> {\n"
            "    let value: T;\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticResult *result = semantic_analyze(program);

        EXPECT(!parser_has_error(parser));
        EXPECT(result != NULL && result->error_count > 0);
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "Default generic type arguments are not supported yet in class declarations"));

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("generic ability declaration rejects default type argument for now");
    {
        const char *source =
            "ability Bufferable<T = Int> {\n"
            "    func Put(value: T) -> Void;\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticResult *result = semantic_analyze(program);

        EXPECT(!parser_has_error(parser));
        EXPECT(result != NULL && result->error_count > 0);
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "Default generic type arguments are not supported yet in ability declarations"));

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("generic party declaration rejects default type argument for now");
    {
        const char *source =
            "party RaidTeam<T = Int> {\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticResult *result = semantic_analyze(program);

        EXPECT(!parser_has_error(parser));
        EXPECT(result != NULL && result->error_count > 0);
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "Default generic type arguments are not supported yet in party declarations"));

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("generic roster declaration rejects default type argument for now");
    {
        const char *source =
            "roster RaidRoster<T = Int> {\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticResult *result = semantic_analyze(program);

        EXPECT(!parser_has_error(parser));
        EXPECT(result != NULL && result->error_count > 0);
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "Default generic type arguments are not supported yet in roster declarations"));

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("generic ability declaration rejects unknown where bound");
    {
        const char *source =
            "ability Bufferable<T> where T: MissingBound {\n"
            "    func Put(value: T) -> Void;\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticResult *result = semantic_analyze(program);

        EXPECT(!parser_has_error(parser));
        EXPECT(result != NULL && result->error_count > 0);
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "Unknown constraint type 'MissingBound'"));

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("action requires rejects generic ability reference that violates ability where bound");
    {
        const char *source =
            "ability Bufferable<T> where T: Int {\n"
            "    func Put(value: T) -> Void;\n"
            "}\n"
            "subject Hero {\n"
            "    action Do(self) -> Void requires Bufferable<String> {\n"
            "        return;\n"
            "    }\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticResult *result = semantic_analyze(program);

        EXPECT(!parser_has_error(parser));
        EXPECT(result != NULL && result->error_count > 0);
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "uses ability 'Bufferable' with generic argument 'String'"));
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "does not satisfy bound 'Int'"));

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("role impl rejects generic ability reference that violates ability where bound");
    {
        const char *source =
            "ability Bufferable<T> where T: Int {\n"
            "    func Put(value: T) -> Void;\n"
            "}\n"
            "subject Hero { let hp: Int; }\n"
            "role HeroRole for Hero {\n"
            "    impl ability Bufferable<String> {\n"
            "        func Put(value: String) -> Void { return; }\n"
            "    }\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticResult *result = semantic_analyze(program);

        EXPECT(!parser_has_error(parser));
        EXPECT(result != NULL && result->error_count > 0);
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "role 'HeroRole' uses ability 'Bufferable' with generic argument 'String'"));
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "does not satisfy bound 'Int'"));

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("party role slot rejects generic ability reference that violates ability where bound");
    {
        const char *source =
            "ability Bufferable<T> where T: Int {\n"
            "    func Put(value: T) -> Void;\n"
            "}\n"
            "subject Hero { let hp: Int; }\n"
            "party RaidTeam {\n"
            "    role slot fighter: Bufferable<String>\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticResult *result = semantic_analyze(program);

        EXPECT(!parser_has_error(parser));
        EXPECT(result != NULL && result->error_count > 0);
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "party role slot 'fighter' uses ability 'Bufferable' with generic argument 'String'"));
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "does not satisfy bound 'Int'"));

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("foreign ability is visible by default without explicit export");
    {
        const char *module_path = "test_default_export_ability_module_a.pgy";
        const char *main_path = "test_default_export_ability_module_main_a.pgy";
        const char *module_source =
            "ability SharedAbility {\n"
            "    func Ping() -> Void;\n"
            "}\n";
        const char *main_source =
            "import \"test_default_export_ability_module_a.pgy\";\n"
            "subject Hero { let hp: Int; }\n"
            "role HeroRole for Hero {\n"
            "    impl ability SharedAbility {\n"
            "        func Ping() -> Void { return; }\n"
            "    }\n"
            "}\n";
        FILE *module_file = fopen(module_path, "wb");
        FILE *main_file = fopen(main_path, "wb");
        char *error_message = NULL;
        ASTNode *program = NULL;
        SemanticResult *result = NULL;

        EXPECT(module_file != NULL && main_file != NULL);
        if (module_file != NULL) {
            fputs(module_source, module_file);
            fclose(module_file);
        }
        if (main_file != NULL) {
            fputs(main_source, main_file);
            fclose(main_file);
        }

        program = import_resolver_load_program(main_path, &error_message);
        EXPECT(program != NULL);
        if (program != NULL)
            result = semantic_analyze(program);
        EXPECT(error_message == NULL);
        EXPECT(result != NULL && result->error_count == 0);

        free(error_message);
        semantic_result_destroy(result);
        ast_destroy(program);
        remove(main_path);
        remove(module_path);
    }

    TEST("foreign generic ability is visible by default without explicit export");
    {
        const char *module_path = "test_default_export_generic_ability_module_a.pgy";
        const char *main_path = "test_default_export_generic_ability_module_main_a.pgy";
        const char *module_source =
            "ability SharedAbility<T> {\n"
            "    func Ping() -> Void;\n"
            "}\n";
        const char *main_source =
            "import \"test_default_export_generic_ability_module_a.pgy\";\n"
            "subject Hero { let hp: Int; }\n"
            "role HeroRole for Hero {\n"
            "    impl ability SharedAbility<Int> {\n"
            "        func Ping() -> Void { return; }\n"
            "    }\n"
            "}\n"
            "subject Boss {\n"
            "    action Do(self) -> Void requires SharedAbility<Int> { return; }\n"
            "}\n"
            "role BossRole for Boss {\n"
            "    impl ability SharedAbility<Int> {\n"
            "        func Ping() -> Void { return; }\n"
            "    }\n"
            "}\n";
        FILE *module_file = fopen(module_path, "wb");
        FILE *main_file = fopen(main_path, "wb");
        char *error_message = NULL;
        ASTNode *program = NULL;
        SemanticResult *result = NULL;

        EXPECT(module_file != NULL && main_file != NULL);
        if (module_file != NULL) {
            fputs(module_source, module_file);
            fclose(module_file);
        }
        if (main_file != NULL) {
            fputs(main_source, main_file);
            fclose(main_file);
        }

        program = import_resolver_load_program(main_path, &error_message);
        EXPECT(program != NULL);
        if (program != NULL)
            result = semantic_analyze(program);
        EXPECT(error_message == NULL);
        EXPECT(result != NULL && result->error_count == 0);

        free(error_message);
        semantic_result_destroy(result);
        ast_destroy(program);
        remove(main_path);
        remove(module_path);
    }

    TEST("private generic ability stays hidden across modules");
    {
        const char *module_path = "test_private_generic_ability_module_a.pgy";
        const char *main_path = "test_private_generic_ability_module_main_a.pgy";
        const char *module_source =
            "private ability HiddenAbility<T> {\n"
            "    func Ping() -> Void;\n"
            "}\n";
        const char *main_source =
            "import \"test_private_generic_ability_module_a.pgy\";\n"
            "subject Hero { let hp: Int; }\n"
            "role HeroRole for Hero {\n"
            "    impl ability HiddenAbility<Int> {\n"
            "        func Ping() -> Void { return; }\n"
            "    }\n"
            "}\n"
            "subject Boss {\n"
            "    action Do(self) -> Void requires HiddenAbility<Int> { return; }\n"
            "}\n";
        FILE *module_file = fopen(module_path, "wb");
        FILE *main_file = fopen(main_path, "wb");
        char *error_message = NULL;
        ASTNode *program = NULL;
        SemanticResult *result = NULL;

        EXPECT(module_file != NULL && main_file != NULL);
        if (module_file != NULL) {
            fputs(module_source, module_file);
            fclose(module_file);
        }
        if (main_file != NULL) {
            fputs(main_source, main_file);
            fclose(main_file);
        }

        program = import_resolver_load_program(main_path, &error_message);
        EXPECT(program != NULL);
        if (program != NULL)
            result = semantic_analyze(program);
        EXPECT(error_message == NULL);
        EXPECT(result != NULL && result->error_count > 0);
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "non-exported ability 'HiddenAbility'"));

        free(error_message);
        semantic_result_destroy(result);
        ast_destroy(program);
        remove(main_path);
        remove(module_path);
    }

    TEST("zone bind group expands into multiple inferred projection sync entries");
    {
        const char *source =
            "subject Player { let hp: Int; let name: String; }\n"
            "object PlayerView { hp: Int; }\n"
            "tobject PlayerDto { hp: Int; name: String; }\n"
            "zone BattleZone {\n"
            "    subject slot player: Player\n"
            "    object slot playerView: PlayerView\n"
            "    tobject slot snapshot: PlayerDto\n"
            "    bind [playerView, snapshot] from player\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticResult *result = semantic_analyze(program);
        ASTNode *zone = NULL;

        EXPECT(!parser_has_error(parser));
        EXPECT(result != NULL && result->error_count == 0);
        if (program != NULL) {
            for (size_t i = 0; i < program->data.program.count; i++) {
                ASTNode *stmt = program->data.program.statements[i];
                if (stmt != NULL && stmt->type == AST_ZONE_DECL
                    && stmt->data.zone_decl.name != NULL
                    && strcmp(stmt->data.zone_decl.name, "BattleZone") == 0) {
                    zone = stmt;
                    break;
                }
            }
        }
        EXPECT(zone != NULL);
        EXPECT(zone != NULL && zone->data.zone_decl.refresh_count == 2);
        if (zone != NULL && zone->data.zone_decl.refresh_count == 2) {
            ASTNode *first = zone->data.zone_decl.refreshes[0];
            ASTNode *second = zone->data.zone_decl.refreshes[1];
            EXPECT(first != NULL
                && first->data.zone_refresh.infer_target_kind
                && strcmp(first->data.zone_refresh.object_slot_name, "playerView") == 0
                && strcmp(first->data.zone_refresh.source_slot_name, "player") == 0);
            EXPECT(second != NULL
                && second->data.zone_refresh.infer_target_kind
                && strcmp(second->data.zone_refresh.object_slot_name, "snapshot") == 0
                && strcmp(second->data.zone_refresh.source_slot_name, "player") == 0);
        }

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("zone refresh and publish groups expand with participant and target kind");
    {
        const char *source =
            "subject Player { let hp: Int; let name: String; }\n"
            "object PlayerView { hp: Int; }\n"
            "object PlayerCard { hp: Int; }\n"
            "tobject PlayerDto { hp: Int; name: String; }\n"
            "tobject PlayerPacket { hp: Int; name: String; }\n"
            "zone BattleZone {\n"
            "    subject slot player: Player\n"
            "    object slot playerView: PlayerView\n"
            "    object slot playerCard: PlayerCard\n"
            "    tobject slot snapshot: PlayerDto\n"
            "    tobject slot packet: PlayerPacket\n"
            "    refresh [playerView, playerCard] from player by player\n"
            "    publish [snapshot, packet] from player by player\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticResult *result = semantic_analyze(program);
        ASTNode *zone = NULL;

        EXPECT(!parser_has_error(parser));
        EXPECT(result != NULL && result->error_count == 0);
        if (program != NULL) {
            for (size_t i = 0; i < program->data.program.count; i++) {
                ASTNode *stmt = program->data.program.statements[i];
                if (stmt != NULL && stmt->type == AST_ZONE_DECL
                    && stmt->data.zone_decl.name != NULL
                    && strcmp(stmt->data.zone_decl.name, "BattleZone") == 0) {
                    zone = stmt;
                    break;
                }
            }
        }
        EXPECT(zone != NULL);
        EXPECT(zone != NULL && zone->data.zone_decl.refresh_count == 4);
        if (zone != NULL && zone->data.zone_decl.refresh_count == 4) {
            ASTNode *refresh_a = zone->data.zone_decl.refreshes[0];
            ASTNode *refresh_b = zone->data.zone_decl.refreshes[1];
            ASTNode *publish_a = zone->data.zone_decl.refreshes[2];
            ASTNode *publish_b = zone->data.zone_decl.refreshes[3];

            EXPECT(refresh_a != NULL
                && !refresh_a->data.zone_refresh.requires_dto
                && !refresh_a->data.zone_refresh.infer_target_kind
                && strcmp(refresh_a->data.zone_refresh.object_slot_name, "playerView") == 0
                && strcmp(refresh_a->data.zone_refresh.participant_slot_name, "player") == 0);
            EXPECT(refresh_b != NULL
                && !refresh_b->data.zone_refresh.requires_dto
                && !refresh_b->data.zone_refresh.infer_target_kind
                && strcmp(refresh_b->data.zone_refresh.object_slot_name, "playerCard") == 0
                && strcmp(refresh_b->data.zone_refresh.participant_slot_name, "player") == 0);
            EXPECT(publish_a != NULL
                && publish_a->data.zone_refresh.requires_dto
                && !publish_a->data.zone_refresh.infer_target_kind
                && strcmp(publish_a->data.zone_refresh.object_slot_name, "snapshot") == 0
                && strcmp(publish_a->data.zone_refresh.participant_slot_name, "player") == 0);
            EXPECT(publish_b != NULL
                && publish_b->data.zone_refresh.requires_dto
                && !publish_b->data.zone_refresh.infer_target_kind
                && strcmp(publish_b->data.zone_refresh.object_slot_name, "packet") == 0
                && strcmp(publish_b->data.zone_refresh.participant_slot_name, "player") == 0);
        }

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("zone refresh map records explicit target-to-source field pairs");
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
        ASTNode *zone = NULL;

        EXPECT(!parser_has_error(parser));
        EXPECT(result != NULL && result->error_count == 0);
        if (program != NULL) {
            for (size_t i = 0; i < program->data.program.count; i++) {
                ASTNode *stmt = program->data.program.statements[i];
                if (stmt != NULL && stmt->type == AST_ZONE_DECL
                    && stmt->data.zone_decl.name != NULL
                    && strcmp(stmt->data.zone_decl.name, "BattleZone") == 0) {
                    zone = stmt;
                    break;
                }
            }
        }
        EXPECT(zone != NULL);
        EXPECT(zone != NULL && zone->data.zone_decl.refresh_count == 1);
        if (zone != NULL && zone->data.zone_decl.refresh_count == 1) {
            ASTNode *refresh = zone->data.zone_decl.refreshes[0];
            EXPECT(refresh != NULL
                && refresh->data.zone_refresh.field_map_count == 2);
            EXPECT(refresh != NULL
                && strcmp(refresh->data.zone_refresh.mapped_target_fields[0], "totalHp") == 0
                && strcmp(refresh->data.zone_refresh.mapped_source_fields[0], "hp") == 0);
            EXPECT(refresh != NULL
                && strcmp(refresh->data.zone_refresh.mapped_target_fields[1], "label") == 0
                && strcmp(refresh->data.zone_refresh.mapped_source_fields[1], "name") == 0);
        }

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }
}

/* -----------------------------------------------------------------
 * Main
 * ----------------------------------------------------------------- */
