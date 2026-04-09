static void
test_roster_world_emit(void)
{
    printf("\n[roster_world_emit]\n");

    TEST("roster emits struct with party slot");
    {
        ASTNode sys_node; memset(&sys_node, 0, sizeof(sys_node));
        sys_node.type = AST_ROSTER_DECL;
        sys_node.data.roster_decl.name = "CombatSystem";

        ASTNode ps; memset(&ps, 0, sizeof(ps));
        ps.type = AST_SYSTEMIC_SLOT;
        ps.data.roster_slot.slot_name = "team1";
        ps.data.roster_slot.party_type = "DungeonTeam";

        ASTNode *party_slots[1] = { &ps };
        sys_node.data.roster_decl.party_slots = party_slots;
        sys_node.data.roster_decl.party_count = 1;
        sys_node.data.roster_decl.shared_fields = NULL;
        sys_node.data.roster_decl.shared_count = 0;
        sys_node.data.roster_decl.methods = NULL;
        sys_node.data.roster_decl.method_count = 0;

        TranspilerCtx *ctx = transpiler_ctx_create();
        emit_roster_decl(&sys_node, ctx);

        EXPECT_STR_CONTAINS(ctx->out->data, "typedef struct CombatSystem");
        EXPECT_STR_CONTAINS(ctx->out->data, "DungeonTeam team1");
        EXPECT_STR_CONTAINS(ctx->out->data, "} CombatSystem;");

        transpiler_ctx_destroy(ctx);
    }

    TEST("world emits struct with roster and zone members");
    {
        ASTNode world_node; memset(&world_node, 0, sizeof(world_node));
        world_node.type = AST_WORLD_DECL;
        world_node.data.world_decl.name = "GameWorld";

        ASTNode ws; memset(&ws, 0, sizeof(ws));
        ws.type = AST_WORLD_SYSTEMIC;
        ws.data.world_roster.slot_name = "combat";
        ws.data.world_roster.roster_type = "CombatSystem";

        ASTNode wz; memset(&wz, 0, sizeof(wz));
        wz.type = AST_WORLD_ZONE;
        wz.data.world_zone.slot_name = "battle";
        wz.data.world_zone.zone_type = "BattleZone";

        ASTNode *rosters[1] = { &ws };
        ASTNode *zones[1] = { &wz };
        world_node.data.world_decl.rosters = rosters;
        world_node.data.world_decl.roster_count = 1;
        world_node.data.world_decl.zones = zones;
        world_node.data.world_decl.zone_count = 1;
        world_node.data.world_decl.shared_fields = NULL;
        world_node.data.world_decl.shared_count = 0;
        world_node.data.world_decl.methods = NULL;
        world_node.data.world_decl.method_count = 0;

        TranspilerCtx *ctx = transpiler_ctx_create();
        emit_world_decl(&world_node, ctx);

        EXPECT_STR_CONTAINS(ctx->out->data, "typedef struct GameWorld");
        EXPECT_STR_CONTAINS(ctx->out->data, "CombatSystem combat");
        EXPECT_STR_CONTAINS(ctx->out->data, "BattleZone battle");
        EXPECT_STR_CONTAINS(ctx->out->data, "} GameWorld;");

        transpiler_ctx_destroy(ctx);
    }

    TEST("relation/effect declarations emit struct layers and projection sync helpers");
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
            "    func Tick() -> Void {\n"
            "        Log(1);\n"
            "    }\n"
            "}\n"
            "effect Poisoned for bearer: Player {\n"
            "    object slot view: PlayerView\n"
            "    tobject slot packet: PlayerDto\n"
            "    refresh view from bearer\n"
            "    publish packet from bearer\n"
            "    func Tick() -> Void {\n"
            "        Log(1);\n"
            "    }\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        HIRProgram *hir = lower_program(program);
        TranspilerCtx *ctx = transpiler_ctx_create();
        ctx->hir = hir;
        emit_statement(program->data.program.statements[3], ctx);
        emit_statement(program->data.program.statements[4], ctx);

        EXPECT_STR_CONTAINS(ctx->out->data, "typedef struct TrustedLink");
        EXPECT_STR_CONTAINS(ctx->out->data, "} TrustedLink;");
        EXPECT_STR_CONTAINS(ctx->out->data, "TrustedLink_sync(TrustedLink *self)");
        EXPECT_STR_CONTAINS(ctx->out->data, "bool __projection_dirty_snapshot;");
        EXPECT_STR_CONTAINS(ctx->out->data, "bool __projection_dirty_packet;");
        EXPECT_STR_CONTAINS(ctx->out->data, "if (!(self->__projection_dirty_snapshot || self->__projection_dirty_packet)) return;");
        EXPECT_STR_CONTAINS(ctx->out->data, "self->snapshot = (PlayerView){ .hp = self->source.hp };");
        EXPECT_STR_CONTAINS(ctx->out->data, "self->packet = (PlayerDto){ .hp = self->target.hp, .name = self->target.name };");
        EXPECT_STR_CONTAINS(ctx->out->data, "TrustedLink_sync(self);");
        EXPECT_STR_CONTAINS(ctx->out->data, "typedef struct Poisoned");
        EXPECT_STR_CONTAINS(ctx->out->data, "} Poisoned;");
        EXPECT_STR_CONTAINS(ctx->out->data, "Poisoned_sync(Poisoned *self)");
        EXPECT_STR_CONTAINS(ctx->out->data, "bool __projection_dirty_view;");
        EXPECT_STR_CONTAINS(ctx->out->data, "bool __projection_dirty_packet;");
        EXPECT_STR_CONTAINS(ctx->out->data, "if (!(self->__projection_dirty_view || self->__projection_dirty_packet)) return;");
        EXPECT_STR_CONTAINS(ctx->out->data, "self->view = (PlayerView){ .hp = self->bearer.hp };");
        EXPECT_STR_CONTAINS(ctx->out->data, "self->packet = (PlayerDto){ .hp = self->bearer.hp, .name = self->bearer.name };");
        EXPECT_STR_CONTAINS(ctx->out->data, "Poisoned_sync(self);");

        transpiler_ctx_destroy(ctx);
        hir_destroy(hir);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("relation/effect constructors lower to compound literals and pointer-self method calls");
    {
        const char *source =
            "subject Player { let hp: Int; let name: String; }\n"
            "relation TrustedLink for source: Player, target: Player {\n"
            "    func Show() -> Void { Log(1); }\n"
            "}\n"
            "effect Poisoned for bearer: Player {\n"
            "    func Show() -> Void { Log(2); }\n"
            "}\n"
            "func Main() -> Void {\n"
            "    let trust = TrustedLink(Player(7, \"src\"), Player(9, \"dst\"));\n"
            "    let poison = Poisoned(Player(5, \"bear\"));\n"
            "    trust.Show();\n"
            "    poison.Show();\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        HIRProgram *hir = lower_program(program);
        TranspilerCtx *ctx = transpiler_ctx_create();
        ctx->hir = hir;
        emit_program(hir, ctx);

        EXPECT_STR_CONTAINS(ctx->out->data, "TrustedLink trust = (TrustedLink){ .source = (Player){ .hp = 7, .name = \"src\" }, .target = (Player){ .hp = 9, .name = \"dst\" } };");
        EXPECT_STR_CONTAINS(ctx->out->data, "Poisoned poison = (Poisoned){ .bearer = (Player){ .hp = 5, .name = \"bear\" } };");
        EXPECT_STR_CONTAINS(ctx->out->data, "TrustedLink_Show(&trust);");
        EXPECT_STR_CONTAINS(ctx->out->data, "Poisoned_Show(&poison);");

        transpiler_ctx_destroy(ctx);
        hir_destroy(hir);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("zone/world emit runtime sync state and projection helpers");
    {
        const char *source =
            "subject Player { let hp: Int; let name: String; }\n"
            "object PlayerView { hp: Int; }\n"
            "tobject PlayerDto { hp: Int; name: String; }\n"
            "effect Poisoned for bearer: Player { }\n"
            "zone BattleZone {\n"
            "    subject slot player: Player\n"
            "    object slot playerView: PlayerView\n"
            "    tobject slot snapshot: PlayerDto\n"
            "    effect slot poison: Poisoned\n"
            "    state poisoned: effect poison on player\n"
            "    refresh playerView from player\n"
            "    publish snapshot from player\n"
            "    maintain poisoned\n"
            "    func Tick() -> Bool {\n"
            "        return HasState(poisoned);\n"
            "    }\n"
            "}\n"
            "world GameWorld {\n"
            "    zone battle: BattleZone\n"
            "    state liveBattle: zone battle\n"
            "    activate liveBattle\n"
            "    func IsBattleLive() -> Bool {\n"
            "        return HasZone(liveBattle);\n"
            "    }\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        HIRProgram *hir = lower_program(program);
        TranspilerCtx *ctx = transpiler_ctx_create();

        emit_program(hir, ctx);

        EXPECT_STR_CONTAINS(ctx->out->data, "typedef struct BattleZone");
        EXPECT_STR_CONTAINS(ctx->out->data, "bool __projection_ready_playerView;");
        EXPECT_STR_CONTAINS(ctx->out->data, "bool __projection_ready_snapshot;");
        EXPECT_STR_CONTAINS(ctx->out->data, "bool __projection_dirty_playerView;");
        EXPECT_STR_CONTAINS(ctx->out->data, "bool __projection_dirty_snapshot;");
        EXPECT_STR_CONTAINS(ctx->out->data, "bool __layer_active_poison;");
        EXPECT_STR_CONTAINS(ctx->out->data, "bool __state_poisoned;");
        EXPECT_STR_CONTAINS(ctx->out->data, "BattleZone_sync(BattleZone *self)");
        EXPECT_STR_CONTAINS(ctx->out->data, "self->__projection_ready_playerView = false;");
        EXPECT_STR_CONTAINS(ctx->out->data, "self->__projection_ready_snapshot = false;");
        EXPECT_STR_CONTAINS(ctx->out->data, "self->__layer_active_poison = false;");
        EXPECT_STR_CONTAINS(ctx->out->data, "self->playerView = (PlayerView){ .hp = self->player.hp };");
        EXPECT_STR_CONTAINS(ctx->out->data, "self->snapshot = (PlayerDto){ .hp = self->player.hp, .name = self->player.name };");
        EXPECT_STR_CONTAINS(ctx->out->data, "self->__projection_ready_playerView = true;");
        EXPECT_STR_CONTAINS(ctx->out->data, "self->__projection_ready_snapshot = true;");
        EXPECT_STR_CONTAINS(ctx->out->data, "self->__layer_active_poison = true;");
        EXPECT_STR_CONTAINS(ctx->out->data, "self->__state_poisoned = true;");
        EXPECT_STR_CONTAINS(ctx->out->data, "return self->__state_poisoned;");
        EXPECT_STR_CONTAINS(ctx->out->data, "typedef struct GameWorld");
        EXPECT_STR_CONTAINS(ctx->out->data, "bool __zone_active_battle;");
        EXPECT_STR_CONTAINS(ctx->out->data, "bool __zone_dirty_battle;");
        EXPECT_STR_CONTAINS(ctx->out->data, "bool __zone_state_liveBattle;");
        EXPECT_STR_CONTAINS(ctx->out->data, "bool __world_derived_dirty;");
        EXPECT_STR_CONTAINS(ctx->out->data, "GameWorld_sync(GameWorld *self)");
        EXPECT_STR_CONTAINS(ctx->out->data, "bool _pgy_prev_active_battle = self->__zone_active_battle;");
        EXPECT_STR_CONTAINS(ctx->out->data, "bool _pgy_world_needs_derived = self->__world_derived_dirty;");
        EXPECT_STR_CONTAINS(ctx->out->data, "self->__zone_dirty_battle = true;");
        EXPECT_STR_CONTAINS(ctx->out->data, "if (self->__zone_dirty_battle) {");
        EXPECT_STR_CONTAINS(ctx->out->data, "BattleZone_sync(&self->battle);");
        EXPECT_STR_CONTAINS(ctx->out->data, "self->__zone_dirty_battle = false;");
        EXPECT_STR_CONTAINS(ctx->out->data, "if (_pgy_world_needs_derived) {");
        EXPECT_STR_CONTAINS(ctx->out->data, "self->__zone_active_battle = true;");
        EXPECT_STR_CONTAINS(ctx->out->data, "self->__zone_state_liveBattle = self->__zone_active_battle;");
        EXPECT_STR_CONTAINS(ctx->out->data, "return self->__zone_state_liveBattle;");

        transpiler_ctx_destroy(ctx);
        hir_destroy(hir);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("zone/world constructors lower to designated runtime instances");
    {
        const char *source =
            "subject Player { let hp: Int; let name: String; }\n"
            "effect Poisoned for bearer: Player { }\n"
            "zone BattleZone {\n"
            "    subject slot player: Player\n"
            "    effect slot poison: Poisoned\n"
            "    state poisoned: effect poison on player\n"
            "    apply poisoned\n"
            "    func Show() -> Bool { return HasState(poisoned); }\n"
            "}\n"
            "world GameWorld {\n"
            "    zone battle: BattleZone\n"
            "    state liveBattle: zone battle\n"
            "    activate liveBattle\n"
            "    func Live() -> Bool { return HasZone(liveBattle); }\n"
            "}\n"
            "func Main() -> Void {\n"
            "    let battle = BattleZone(Player(7, \"neo\"));\n"
            "    let world = GameWorld(battle);\n"
            "    Log(world.Live());\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        HIRProgram *hir = lower_program(program);
        TranspilerCtx *ctx = transpiler_ctx_create();
        ctx->hir = hir;

        emit_program(hir, ctx);

        EXPECT_STR_CONTAINS(ctx->out->data, "BattleZone battle = (BattleZone){ .player = (Player){ .hp = 7, .name = \"neo\" } };");
        EXPECT_STR_CONTAINS(ctx->out->data, "GameWorld world = (GameWorld){ .battle = battle, .__zone_dirty_battle = true, .__world_derived_dirty = true };");
        EXPECT_STR_CONTAINS(ctx->out->data, "return self->__state_poisoned;");
        EXPECT_STR_CONTAINS(ctx->out->data, "return self->__zone_state_liveBattle;");

        transpiler_ctx_destroy(ctx);
        hir_destroy(hir);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("world cross-layer queries lower to embedded zone runtime flags");
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
            "    func Show(self) -> Void {\n"
            "        Log(HasZoneProjection(battle, playerView));\n"
            "        Log(HasZoneLayer(battle, poison));\n"
            "        Log(HasZoneState(battle, poisoned));\n"
            "    }\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        HIRProgram *hir = lower_program(program);
        TranspilerCtx *ctx = transpiler_ctx_create();
        ctx->hir = hir;

        emit_program(hir, ctx);

        EXPECT_STR_CONTAINS(ctx->out->data, "pgy_log(self->battle.__projection_ready_playerView);");
        EXPECT_STR_CONTAINS(ctx->out->data, "pgy_log(self->battle.__layer_active_poison);");
        EXPECT_STR_CONTAINS(ctx->out->data, "pgy_log(self->battle.__state_poisoned);");

        transpiler_ctx_destroy(ctx);
        hir_destroy(hir);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("zone action calls activate matching effect layers at runtime");
    {
        const char *source =
            "subject Player {\n"
            "    let hp: Int;\n"
            "    action Attack(self) -> Void\n"
            "        within BattleZone\n"
            "        causes Poisoned\n"
            "        authorized by self {\n"
            "        hp = hp - 1;\n"
            "    }\n"
            "}\n"
            "object PlayerView { hp: Int; }\n"
            "effect Poisoned for bearer: Player {\n"
            "    object slot view: PlayerView\n"
            "    refresh view from bearer\n"
            "}\n"
            "zone BattleZone {\n"
            "    subject slot player: Player\n"
            "    effect slot poison: Poisoned\n"
            "    authority player\n"
            "    func Tick(self) -> Void {\n"
            "        self.player.Attack();\n"
            "        Log(HasLayer(poison));\n"
            "    }\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        HIRProgram *hir = lower_program(program);
        TranspilerCtx *ctx = transpiler_ctx_create();
        ctx->hir = hir;

        emit_program(hir, ctx);

        EXPECT_STR_CONTAINS(ctx->out->data, "Player_Attack(&self->player);");
        EXPECT_STR_CONTAINS(ctx->out->data, "self->__layer_active_poison = true;");
        EXPECT_STR_CONTAINS(ctx->out->data, "self->poison.bearer = self->player;");
        EXPECT_STR_CONTAINS(ctx->out->data, "Poisoned_sync(&self->poison);");
        EXPECT_STR_CONTAINS(ctx->out->data,
            "pgy_log(BattleZone_has_layer_poison(self, __pgy_zone_gen));");

        transpiler_ctx_destroy(ctx);
        hir_destroy(hir);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("world derived states lower to embedded zone contracts");
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
            "    func Show(self) -> Void {\n"
            "        Log(HasZone(battleProjected));\n"
            "        Log(HasZone(battleLayered));\n"
            "        Log(HasZone(battlePoisoned));\n"
            "    }\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        HIRProgram *hir = lower_program(program);
        TranspilerCtx *ctx = transpiler_ctx_create();
        ctx->hir = hir;

        emit_program(hir, ctx);

        EXPECT_STR_CONTAINS(ctx->out->data, "self->__zone_state_battleProjected = (self->__zone_active_battle && self->battle.__projection_ready_playerView);");
        EXPECT_STR_CONTAINS(ctx->out->data, "self->__zone_state_battleLayered = (self->__zone_active_battle && self->battle.__layer_active_poison);");
        EXPECT_STR_CONTAINS(ctx->out->data, "self->__zone_state_battlePoisoned = (self->__zone_active_battle && self->battle.__state_poisoned);");
        EXPECT_STR_CONTAINS(ctx->out->data, "pgy_log(self->__zone_state_battleProjected);");
        EXPECT_STR_CONTAINS(ctx->out->data, "pgy_log(self->__zone_state_battleLayered);");
        EXPECT_STR_CONTAINS(ctx->out->data, "pgy_log(self->__zone_state_battlePoisoned);");

        transpiler_ctx_destroy(ctx);
        hir_destroy(hir);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("world composed states lower to combined zone/world flags");
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
            "    activate battle\n"
            "    maintain camp\n"
            "    func Show(self) -> Void {\n"
            "        Log(HasZone(allLive));\n"
            "        Log(HasZone(anyLive));\n"
            "    }\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        HIRProgram *hir = lower_program(program);
        TranspilerCtx *ctx = transpiler_ctx_create();
        ctx->hir = hir;

        emit_program(hir, ctx);

        EXPECT_STR_CONTAINS(ctx->out->data, "self->__zone_state_allLive = (self->__zone_state_battleLive && self->__zone_state_campLive);");
        EXPECT_STR_CONTAINS(ctx->out->data, "self->__zone_state_anyLive = (self->__zone_state_allLive || self->__zone_state_campLive);");
        EXPECT_STR_CONTAINS(ctx->out->data, "pgy_log(self->__zone_state_allLive);");
        EXPECT_STR_CONTAINS(ctx->out->data, "pgy_log(self->__zone_state_anyLive);");

        transpiler_ctx_destroy(ctx);
        hir_destroy(hir);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("world sync emits command, zone, and derived phases in order");
    {
        const char *source =
            "zone BattleZone { }\n"
            "world GameWorld {\n"
            "    zone battle: BattleZone\n"
            "    state battleLive: zone battle\n"
            "    activate battle\n"
            "}\n";
        const char *reset_pos;
        const char *directives_pos;
        const char *zone_sync_pos;
        const char *derived_pos;
        const char *battle_sync_pos;
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        HIRProgram *hir = lower_program(program);
        TranspilerCtx *ctx = transpiler_ctx_create();
        ctx->hir = hir;

        emit_program(hir, ctx);

        reset_pos = strstr(ctx->out->data, "/* world command pass: reset */");
        directives_pos = strstr(ctx->out->data, "/* world command pass: directives */");
        zone_sync_pos = strstr(ctx->out->data, "/* world zone sync pass */");
        derived_pos = strstr(ctx->out->data, "/* world derived pass */");
        battle_sync_pos = strstr(ctx->out->data, "BattleZone_sync(&self->battle);");
        EXPECT(reset_pos != NULL);
        EXPECT(directives_pos != NULL);
        EXPECT(zone_sync_pos != NULL);
        EXPECT(derived_pos != NULL);
        EXPECT(battle_sync_pos != NULL);
        EXPECT(strstr(ctx->out->data, "bool __zone_dirty_battle;") != NULL);
        EXPECT(strstr(ctx->out->data, "bool __world_derived_dirty;") != NULL);
        EXPECT(reset_pos < directives_pos);
        EXPECT(directives_pos < zone_sync_pos);
        EXPECT(zone_sync_pos < battle_sync_pos);
        EXPECT(battle_sync_pos < derived_pos);

        transpiler_ctx_destroy(ctx);
        hir_destroy(hir);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("world methods conservatively invalidate embedded zones before post-sync");
    {
        const char *source =
            "zone BattleZone { }\n"
            "world GameWorld {\n"
            "    zone battle: BattleZone\n"
            "    func Touch(self) -> Void { Log(1); }\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        HIRProgram *hir = lower_program(program);
        TranspilerCtx *ctx = transpiler_ctx_create();
        ctx->hir = hir;

        emit_program(hir, ctx);

        EXPECT_STR_CONTAINS(ctx->out->data, "void\nGameWorld_Touch(GameWorld *self)\n{");
        EXPECT_STR_CONTAINS(ctx->out->data, "self->__zone_dirty_battle = true;\n    self->__world_derived_dirty = true;\n    GameWorld_sync(self);");

        transpiler_ctx_destroy(ctx);
        hir_destroy(hir);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("zone sync binds typed relation/effect layers before projection reads");
    {
        const char *source =
            "subject Player { let hp: Int; let name: String; }\n"
            "object PlayerView { hp: Int; }\n"
            "tobject PlayerDto { name: String; }\n"
            "effect Poisoned for bearer: Player {\n"
            "    object slot view: PlayerView\n"
            "    refresh view from bearer\n"
            "}\n"
            "relation TrustedLink for source: Player, target: Player {\n"
            "    tobject slot packet: PlayerDto\n"
            "    publish packet from target\n"
            "}\n"
            "zone BattleZone {\n"
            "    subject slot player: Player\n"
            "    subject slot enemy: Player\n"
            "    effect slot poison: Poisoned\n"
            "    relation slot trust: TrustedLink\n"
            "    apply poison to player\n"
            "    link trust between player, enemy\n"
            "    func Show(self) -> Void {\n"
            "        Log(self.poison.view.hp);\n"
            "        Log(self.trust.packet.name);\n"
            "    }\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        HIRProgram *hir = lower_program(program);
        TranspilerCtx *ctx = transpiler_ctx_create();

        emit_program(hir, ctx);

        EXPECT_STR_CONTAINS(ctx->out->data, "Poisoned poison;");
        EXPECT_STR_CONTAINS(ctx->out->data, "TrustedLink trust;");
        EXPECT_STR_CONTAINS(ctx->out->data, "self->poison.bearer = self->player;");
        EXPECT_STR_CONTAINS(ctx->out->data, "Poisoned_sync(&self->poison);");
        EXPECT_STR_CONTAINS(ctx->out->data, "self->trust.source = self->player;");
        EXPECT_STR_CONTAINS(ctx->out->data, "self->trust.target = self->enemy;");
        EXPECT_STR_CONTAINS(ctx->out->data, "TrustedLink_sync(&self->trust);");
        EXPECT_STR_CONTAINS(ctx->out->data, "pgy_log(self->poison.view.hp);");
        EXPECT_STR_CONTAINS(ctx->out->data, "pgy_log(self->trust.packet.name);");

        transpiler_ctx_destroy(ctx);
        hir_destroy(hir);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("zone sync binds object-target relation/effect layers before projection reads");
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
            "    func Show(self) -> Void {\n"
            "        Log(self.glow.view.hp);\n"
            "    }\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        HIRProgram *hir = lower_program(program);
        TranspilerCtx *ctx = transpiler_ctx_create();

        emit_program(hir, ctx);

        EXPECT_STR_CONTAINS(ctx->out->data, "Highlighted glow;");
        EXPECT_STR_CONTAINS(ctx->out->data, "KeyBinding binding;");
        EXPECT_STR_CONTAINS(ctx->out->data, "self->glow.target = self->door;");
        EXPECT_STR_CONTAINS(ctx->out->data, "Highlighted_sync(&self->glow);");
        EXPECT_STR_CONTAINS(ctx->out->data, "self->binding.door = self->door;");
        EXPECT_STR_CONTAINS(ctx->out->data, "self->binding.key = self->key;");
        EXPECT_STR_CONTAINS(ctx->out->data, "KeyBinding_sync(&self->binding);");
        EXPECT_STR_CONTAINS(ctx->out->data, "pgy_log(self->glow.view.hp);");

        transpiler_ctx_destroy(ctx);
        hir_destroy(hir);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }
}

/* -----------------------------------------------------------------
 * Parallel execution emitter tests
 * ----------------------------------------------------------------- */

static void
test_parallel_execution_emit(void)
{
    printf("\n[parallel_execution_emit]\n");

    TEST("subject emits struct typedef");
    {
        ASTNode subject_node; memset(&subject_node, 0, sizeof(subject_node));
        subject_node.type = AST_CLASS_DECL;
        subject_node.data.class_decl.name = "Counter";
        subject_node.data.class_decl.nominal_kind = NOMINAL_DECL_SUBJECT;

        ClassField field; memset(&field, 0, sizeof(field));
        field.name = "count";
        ASTNode field_type; memset(&field_type, 0, sizeof(field_type));
        field_type.type = AST_TYPE;
        field_type.data.type.name = "Int";
        field.type = &field_type;

        ClassField *fields[1] = { &field };
        subject_node.data.class_decl.fields = fields;
        subject_node.data.class_decl.field_count = 1;
        subject_node.data.class_decl.methods = NULL;
        subject_node.data.class_decl.method_count = 0;

        TranspilerCtx *ctx = transpiler_ctx_create();
        emit_class_decl(&subject_node, ctx);

        EXPECT_STR_CONTAINS(ctx->out->data, "typedef struct Counter");
        EXPECT_STR_CONTAINS(ctx->out->data, "int32_t count");
        EXPECT_STR_CONTAINS(ctx->out->data, "} Counter;");
        EXPECT_STR_CONTAINS(ctx->out->data, "PGY_SLOT_DEFINE(Counter, Counter)");
        EXPECT_STR_CONTAINS(ctx->out->data, "PGY_SECURE_SLOT_DEFINE(Counter, Counter)");
        EXPECT_STR_CONTAINS(ctx->out->data, "PGY_BOX_DEFINE(Counter, Counter)");

        transpiler_ctx_destroy(ctx);
    }

    TEST("subject syntax lowers through subject codegen");
    {
        const char *source =
            "subject Counter {\n"
            "    let count: Int;\n"
            "}\n"
            "func Main() -> Void {\n"
            "    let c: Counter = Counter();\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        HIRProgram *hir = lower_program(program);
        TranspilerCtx *ctx = transpiler_ctx_create();

        emit_program(hir, ctx);

        EXPECT_STR_CONTAINS(ctx->out->data, "typedef struct Counter\n{");
        EXPECT_STR_CONTAINS(ctx->out->data, "} Counter;");

        transpiler_ctx_destroy(ctx);
        hir_destroy(hir);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("channel send emits pgy_channel_send");
    {
        ASTNode ch_node; memset(&ch_node, 0, sizeof(ch_node));
        ch_node.type = AST_IDENTIFIER;
        ch_node.data.identifier.name = "myChan";

        ASTNode val_node; memset(&val_node, 0, sizeof(val_node));
        val_node.type = AST_NUMBER;
        val_node.data.number.value = 42;

        ASTNode send_node; memset(&send_node, 0, sizeof(send_node));
        send_node.type = AST_CHANNEL_SEND;
        send_node.data.channel_send.channel = &ch_node;
        send_node.data.channel_send.value = &val_node;

        TranspilerCtx *ctx = transpiler_ctx_create();
        char *result = emit_expression(&send_node, ctx);

        EXPECT(result != NULL);
        EXPECT(strstr(result, "pgy_channel_send") != NULL);
        EXPECT(strstr(result, "myChan") != NULL);

        free(result);
        transpiler_ctx_destroy(ctx);
    }

    TEST("channel recv emits pgy_channel_recv");
    {
        ASTNode ch_node; memset(&ch_node, 0, sizeof(ch_node));
        ch_node.type = AST_IDENTIFIER;
        ch_node.data.identifier.name = "myChan";

        ASTNode recv_node; memset(&recv_node, 0, sizeof(recv_node));
        recv_node.type = AST_CHANNEL_RECV;
        recv_node.data.channel_recv.channel = &ch_node;

        TranspilerCtx *ctx = transpiler_ctx_create();
        char *result = emit_expression(&recv_node, ctx);

        EXPECT(result != NULL);
        EXPECT(strstr(result, "pgy_channel_recv") != NULL);
        EXPECT(strstr(result, "myChan") != NULL);

        free(result);
        transpiler_ctx_destroy(ctx);
    }

    TEST("TryRecv emits Option-based non-blocking receive");
    {
        TranspilerCtx *ctx = transpiler_ctx_create();
        ASTNode *cap_args[1] = { make_number(4, 1) };
        emit_statement(
            make_let("ch", make_type_node("Channel<Int>"),
                     make_call("Channel", cap_args, 1, 1), 1),
            ctx);

        ASTNode *args[1] = { make_identifier("ch", 2) };
        char *result = emit_expression(make_call("TryRecv", args, 1, 2), ctx);

        EXPECT(result != NULL);
        EXPECT(strstr(result, "pgy_channel_try_recv_Int(&ch, &_pgy_recv_tmp)") != NULL);
        EXPECT(strstr(result, "Some_Int(_pgy_recv_tmp)") != NULL);
        EXPECT(strstr(result, "None_Int()") != NULL);

        free(result);
        transpiler_ctx_destroy(ctx);
    }

    TEST("RecvTimeout emits Option-based timed receive");
    {
        TranspilerCtx *ctx = transpiler_ctx_create();
        ASTNode *cap_args[1] = { make_number(4, 1) };
        emit_statement(
            make_let("ch", make_type_node("Channel<Int>"),
                     make_call("Channel", cap_args, 1, 1), 1),
            ctx);

        ASTNode *args[2] = { make_identifier("ch", 2), make_number(1000, 2) };
        char *result = emit_expression(make_call("RecvTimeout", args, 2, 2), ctx);

        EXPECT(result != NULL);
        EXPECT(strstr(result, "pgy_channel_recv_timeout_Int(&ch, &_pgy_recv_tmp, (uint64_t)(1000))") != NULL);
        EXPECT(strstr(result, "Some_Int(_pgy_recv_tmp)") != NULL);
        EXPECT(strstr(result, "None_Int()") != NULL);

        free(result);
        transpiler_ctx_destroy(ctx);
    }

    TEST("TrySend emits pgy_channel_try_send");
    {
        TranspilerCtx *ctx = transpiler_ctx_create();
        ASTNode *cap_args[1] = { make_number(4, 1) };
        emit_statement(
            make_let("ch", make_type_node("Channel<Int>"),
                     make_call("Channel", cap_args, 1, 1), 1),
            ctx);

        ASTNode *args[2] = { make_identifier("ch", 2), make_number(42, 2) };
        char *result = emit_expression(make_call("TrySend", args, 2, 2), ctx);

        EXPECT(result != NULL);
        EXPECT(strstr(result, "pgy_channel_try_send_Int(&ch, 42)") != NULL);

        free(result);
        transpiler_ctx_destroy(ctx);
    }

    TEST("TrySendStatus emits Option<Bool> channel status helper");
    {
        TranspilerCtx *ctx = transpiler_ctx_create();
        ASTNode *cap_args[1] = { make_number(4, 1) };
        emit_statement(
            make_let("ch", make_type_node("Channel<Int>"),
                     make_call("Channel", cap_args, 1, 1), 1),
            ctx);

        ASTNode *args[2] = { make_identifier("ch", 2), make_number(42, 2) };
        char *result = emit_expression(make_call("TrySendStatus", args, 2, 2), ctx);

        EXPECT(result != NULL);
        EXPECT(strstr(result, "pgy_channel_try_send_status_Int(&ch, 42)") != NULL);

        free(result);
        transpiler_ctx_destroy(ctx);
    }

    TEST("Cancel emits pgy_task_cancel");
    {
        TranspilerCtx *ctx = transpiler_ctx_create();
        ASTNode *spawn = calloc(1, sizeof(ASTNode));
        spawn->type = AST_SPAWN_EXPR;
        spawn->data.spawn_expr.function = make_identifier("Work", 1);
        spawn->data.spawn_expr.arguments = NULL;
        spawn->data.spawn_expr.arg_count = 0;
        emit_statement(make_let("pending", NULL, spawn, 1), ctx);

        ASTNode *args[1] = { make_identifier("pending", 2) };
        char *result = emit_expression(make_call("Cancel", args, 1, 2), ctx);

        EXPECT(result != NULL);
        EXPECT(strstr(result, "pgy_task_cancel(pending)") != NULL);

        free(result);
        transpiler_ctx_destroy(ctx);
    }

    TEST("IsCancelled emits pgy_task_is_cancelled");
    {
        TranspilerCtx *ctx = transpiler_ctx_create();
        char *result = emit_expression(make_call("IsCancelled", NULL, 0, 1), ctx);

        EXPECT(result != NULL);
        EXPECT(strstr(result, "pgy_task_is_cancelled()") != NULL);

        free(result);
        transpiler_ctx_destroy(ctx);
    }

    TEST("event invoke emits generated invoke helper call");
    {
        ASTNode event_node; memset(&event_node, 0, sizeof(event_node));
        event_node.type = AST_IDENTIFIER;
        event_node.data.identifier.name = "OnHit";

        ASTNode arg_node; memset(&arg_node, 0, sizeof(arg_node));
        arg_node.type = AST_NUMBER;
        arg_node.data.number.value = 7;

        ASTNode *args[1] = { &arg_node };
        ASTNode invoke_node; memset(&invoke_node, 0, sizeof(invoke_node));
        invoke_node.type = AST_EVENT_INVOKE;
        invoke_node.data.event_invoke.event = &event_node;
        invoke_node.data.event_invoke.arguments = args;
        invoke_node.data.event_invoke.arg_count = 1;

        TranspilerCtx *ctx = transpiler_ctx_create();
        char *result = emit_expression(&invoke_node, ctx);

        EXPECT_STR_CONTAINS(result, "OnHit_INVOKE(&OnHit, 7)");

        free(result);
        transpiler_ctx_destroy(ctx);
    }

    TEST("context access emits self role slot access");
    {
        ASTNode context; memset(&context, 0, sizeof(context));
        context.type = AST_CONTEXT_ACCESS;
        context.data.context_access.method_name = "GetRole";
        context.data.context_access.role_slot_name = "tank";

        TranspilerCtx *ctx = transpiler_ctx_create();
        char *result = emit_expression(&context, ctx);

        EXPECT_STR_CONTAINS(result, "self->tank");

        free(result);
        transpiler_ctx_destroy(ctx);
    }

    TEST("ChannelLength emits pgy_channel_length");
    {
        TranspilerCtx *ctx = transpiler_ctx_create();
        ASTNode *cap_args[1] = { make_number(4, 1) };
        emit_statement(
            make_let("ch", make_type_node("Channel<Int>"),
                     make_call("Channel", cap_args, 1, 1), 1),
            ctx);
        ASTNode *args[1] = { make_identifier("ch", 2) };
        char *result = emit_expression(make_call("ChannelLength", args, 1, 2), ctx);

        EXPECT(strstr(result, "pgy_channel_length_Int(&ch)") != NULL);

        free(result);
        transpiler_ctx_destroy(ctx);
    }

    TEST("ChannelCapacity emits pgy_channel_capacity");
    {
        TranspilerCtx *ctx = transpiler_ctx_create();
        ASTNode *cap_args[1] = { make_number(4, 1) };
        emit_statement(
            make_let("ch", make_type_node("Channel<Int>"),
                     make_call("Channel", cap_args, 1, 1), 1),
            ctx);
        ASTNode *args[1] = { make_identifier("ch", 2) };
        char *result = emit_expression(make_call("ChannelCapacity", args, 1, 2), ctx);

        EXPECT(strstr(result, "pgy_channel_capacity_Int(&ch)") != NULL);

        free(result);
        transpiler_ctx_destroy(ctx);
    }

    TEST("ChannelSpace emits pgy_channel_space");
    {
        TranspilerCtx *ctx = transpiler_ctx_create();
        ASTNode *cap_args[1] = { make_number(4, 1) };
        emit_statement(
            make_let("ch", make_type_node("Channel<Int>"),
                     make_call("Channel", cap_args, 1, 1), 1),
            ctx);
        ASTNode *args[1] = { make_identifier("ch", 2) };
        char *result = emit_expression(make_call("ChannelSpace", args, 1, 2), ctx);

        EXPECT(strstr(result, "pgy_channel_space_Int(&ch)") != NULL);

        free(result);
        transpiler_ctx_destroy(ctx);
    }

    TEST("ChannelFull emits pgy_channel_full");
    {
        TranspilerCtx *ctx = transpiler_ctx_create();
        ASTNode *cap_args[1] = { make_number(4, 1) };
        emit_statement(
            make_let("ch", make_type_node("Channel<Int>"),
                     make_call("Channel", cap_args, 1, 1), 1),
            ctx);
        ASTNode *args[1] = { make_identifier("ch", 2) };
        char *result = emit_expression(make_call("ChannelFull", args, 1, 2), ctx);

        EXPECT(strstr(result, "pgy_channel_full_Int(&ch)") != NULL);

        free(result);
        transpiler_ctx_destroy(ctx);
    }

    TEST("ChannelClosed emits pgy_channel_closed");
    {
        TranspilerCtx *ctx = transpiler_ctx_create();
        ASTNode *cap_args[1] = { make_number(4, 1) };
        emit_statement(
            make_let("ch", make_type_node("Channel<Int>"),
                     make_call("Channel", cap_args, 1, 1), 1),
            ctx);
        ASTNode *args[1] = { make_identifier("ch", 2) };
        char *result = emit_expression(make_call("ChannelClosed", args, 1, 2), ctx);

        EXPECT(strstr(result, "pgy_channel_closed_Int(&ch)") != NULL);

        free(result);
        transpiler_ctx_destroy(ctx);
    }

    TEST("SendTimeoutStatus emits Option<Bool> timed channel status helper");
    {
        TranspilerCtx *ctx = transpiler_ctx_create();
        ASTNode *cap_args[1] = { make_number(4, 1) };
        emit_statement(
            make_let("ch", make_type_node("Channel<Int>"),
                     make_call("Channel", cap_args, 1, 1), 1),
            ctx);
        ASTNode *args[3] = {
            make_identifier("ch", 2),
            make_number(42, 2),
            make_number(1000, 2)
        };
        char *result = emit_expression(make_call("SendTimeoutStatus", args, 3, 2), ctx);

        EXPECT(strstr(result, "pgy_channel_send_timeout_status_Int(&ch, 42, (uint64_t)(1000))") != NULL);

        free(result);
        transpiler_ctx_destroy(ctx);
    }

    TEST("lambda expression emits helper prototype and definition");
    {
        ASTNode param; memset(&param, 0, sizeof(param));
        param.type = AST_IDENTIFIER;
        param.data.identifier.name = "x";

        ASTNode *body_expr = make_identifier("x", 1);
        ASTNode *body_stmts[1] = { make_return(body_expr, 1) };
        ASTNode *body = make_block(body_stmts, 1);

        ASTNode *params[1] = { &param };
        ASTNode lambda; memset(&lambda, 0, sizeof(lambda));
        lambda.type = AST_LAMBDA_EXPR;
        lambda.data.lambda_expr.params = params;
        lambda.data.lambda_expr.param_count = 1;
        lambda.data.lambda_expr.body = body;
        lambda.data.lambda_expr.return_type = make_type_node("Int");

        TranspilerCtx *ctx = transpiler_ctx_create();
        char *result = emit_expression(&lambda, ctx);

        EXPECT_STR_CONTAINS(result, "pgy_lambda_");
        EXPECT_STR_CONTAINS(ctx->decls->data, "static int32_t pgy_lambda_");
        EXPECT_STR_CONTAINS(ctx->helpers->data, "return x;");

        free(result);
        ast_destroy(body);
        ast_destroy(lambda.data.lambda_expr.return_type);
        transpiler_ctx_destroy(ctx);
    }

}

/* -----------------------------------------------------------------
 * Slot sugar tests
 * ----------------------------------------------------------------- */
