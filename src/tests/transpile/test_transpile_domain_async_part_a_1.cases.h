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

        MIRProgram *mir = mir_program_from_decl_for_test(&sys_node);
        g_last_mir = mir;
        TranspilerCtx *ctx = transpiler_ctx_create();
        emit_roster_decl(&sys_node, ctx);

        EXPECT_STR_CONTAINS(ctx->out->data, "typedef struct CombatSystem");
        EXPECT_STR_CONTAINS(ctx->out->data, "DungeonTeam team1");
        EXPECT_STR_CONTAINS(ctx->out->data, "} CombatSystem;");

        transpiler_ctx_destroy(ctx);
        mir_destroy(mir);
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

        MIRProgram *mir = mir_program_from_decl_for_test(&world_node);
        g_last_mir = mir;
        TranspilerCtx *ctx = transpiler_ctx_create();
        emit_world_decl(&world_node, ctx);

        EXPECT_STR_CONTAINS(ctx->out->data, "typedef struct GameWorld");
        EXPECT_STR_CONTAINS(ctx->out->data, "CombatSystem combat");
        EXPECT_STR_CONTAINS(ctx->out->data, "BattleZone battle");
        EXPECT_STR_CONTAINS(ctx->out->data, "} GameWorld;");

        transpiler_ctx_destroy(ctx);
        mir_destroy(mir);
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
        HIRProgram *hir = NULL;
        RIRProgram *rir = NULL;
        MIRProgram *mir = lower_program_to_mir_strict(program, &hir, &rir);
        TranspilerCtx *ctx = transpiler_ctx_create();
        ctx->mir = mir;
        emit_program(ctx);

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
        mir_destroy(mir);
        rir_destroy(rir);
        hir_destroy(hir);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("relation/effect constructors lower to compound literals and pointer-self method calls");
    {
        const char *source =
            "subject Player { let hp: Int; }\n"
            "relation TrustedLink for source: Player, target: Player {\n"
            "    func Show() -> Void { Log(1); }\n"
            "}\n"
            "effect Poisoned for bearer: Player {\n"
            "    func Show() -> Void { Log(2); }\n"
            "}\n"
            "func Main() -> Void {\n"
            "    let trust = TrustedLink(Player(7), Player(9));\n"
            "    let poison = Poisoned(Player(5));\n"
            "    trust.Show();\n"
            "    poison.Show();\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        HIRProgram *hir = NULL;
        RIRProgram *rir = NULL;
        MIRProgram *mir = lower_program_to_mir_strict(program, &hir, &rir);
        TranspilerCtx *ctx = transpiler_ctx_create();
        ctx->mir = mir;
        emit_program(ctx);

        EXPECT_STR_CONTAINS(ctx->out->data, "TrustedLink");
        EXPECT_STR_CONTAINS(ctx->out->data, "Poisoned");
        EXPECT_STR_CONTAINS(ctx->out->data, "TrustedLink_Show");
        EXPECT_STR_CONTAINS(ctx->out->data, "Poisoned_Show");

        transpiler_ctx_destroy(ctx);
        mir_destroy(mir);
        rir_destroy(rir);
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
        HIRProgram *hir = NULL;
        RIRProgram *rir = NULL;
        MIRProgram *mir = lower_program_to_mir_strict(program, &hir, &rir);
        TranspilerCtx *ctx = transpiler_ctx_create();
        ctx->mir = mir;

        emit_program(ctx);

        EXPECT_STR_CONTAINS(ctx->out->data, "typedef struct BattleZone");
        EXPECT_STR_CONTAINS(ctx->out->data, "bool __projection_ready_playerView;");
        EXPECT_STR_CONTAINS(ctx->out->data, "bool __projection_ready_snapshot;");
        EXPECT_STR_CONTAINS(ctx->out->data, "bool __projection_dirty_playerView;");
        EXPECT_STR_CONTAINS(ctx->out->data, "bool __projection_dirty_snapshot;");
        EXPECT_STR_CONTAINS(ctx->out->data, "uint32_t __projection_epoch_playerView;");
        EXPECT_STR_CONTAINS(ctx->out->data, "int __projection_cause_playerView;");
        EXPECT_STR_CONTAINS(ctx->out->data, "uint32_t __projection_epoch_snapshot;");
        EXPECT_STR_CONTAINS(ctx->out->data, "int __projection_cause_snapshot;");
        EXPECT_STR_CONTAINS(ctx->out->data, "bool __layer_active_poison;");
        EXPECT_STR_CONTAINS(ctx->out->data, "uint32_t __layer_epoch_poison;");
        EXPECT_STR_CONTAINS(ctx->out->data, "int __layer_cause_poison;");
        EXPECT_STR_CONTAINS(ctx->out->data, "bool __state_poisoned;");
        EXPECT_STR_CONTAINS(ctx->out->data, "uint32_t __state_epoch_poisoned;");
        EXPECT_STR_CONTAINS(ctx->out->data, "int __state_cause_poisoned;");
        EXPECT_STR_CONTAINS(ctx->out->data, "BattleZone_sync(BattleZone *self)");
        EXPECT_STR_CONTAINS(ctx->out->data, "self->__projection_ready_playerView = false;");
        EXPECT_STR_CONTAINS(ctx->out->data, "self->__projection_ready_snapshot = false;");
        EXPECT_STR_CONTAINS(ctx->out->data, "self->__layer_active_poison = false;");
        EXPECT_STR_CONTAINS(ctx->out->data, "self->playerView = (PlayerView){ .hp = self->player.hp };");
        EXPECT_STR_CONTAINS(ctx->out->data, "self->snapshot = (PlayerDto){ .hp = self->player.hp, .name = self->player.name };");
        EXPECT_STR_CONTAINS(ctx->out->data, "self->__projection_ready_playerView = true;");
        EXPECT_STR_CONTAINS(ctx->out->data, "self->__projection_epoch_playerView++;");
        EXPECT_STR_CONTAINS(ctx->out->data, "self->__projection_cause_playerView = 1;");
        EXPECT_STR_CONTAINS(ctx->out->data, "self->__projection_ready_snapshot = true;");
        EXPECT_STR_CONTAINS(ctx->out->data, "self->__projection_epoch_snapshot++;");
        EXPECT_STR_CONTAINS(ctx->out->data, "self->__projection_cause_snapshot = 1;");
        EXPECT_STR_CONTAINS(ctx->out->data, "self->__layer_active_poison = true;");
        EXPECT_STR_CONTAINS(ctx->out->data, "self->__layer_epoch_poison++;");
        EXPECT_STR_CONTAINS(ctx->out->data, "self->__layer_cause_poison = 3;");
        EXPECT_STR_CONTAINS(ctx->out->data, "self->__state_poisoned = true;");
        EXPECT_STR_CONTAINS(ctx->out->data, "self->__state_epoch_poisoned++;");
        EXPECT_STR_CONTAINS(ctx->out->data, "self->__state_cause_poisoned = 3;");
        EXPECT_STR_CONTAINS(ctx->out->data, "return self->__state_poisoned;");
        EXPECT_STR_CONTAINS(ctx->out->data, "typedef struct GameWorld");
        EXPECT_STR_CONTAINS(ctx->out->data, "bool __zone_active_battle;");
        EXPECT_STR_CONTAINS(ctx->out->data, "bool __zone_dirty_battle;");
        EXPECT_STR_CONTAINS(ctx->out->data, "uint32_t __zone_epoch_battle;");
        EXPECT_STR_CONTAINS(ctx->out->data, "int __zone_cause_battle;");
        EXPECT_STR_CONTAINS(ctx->out->data, "bool __zone_state_liveBattle;");
        EXPECT_STR_CONTAINS(ctx->out->data, "uint32_t __zone_state_epoch_liveBattle;");
        EXPECT_STR_CONTAINS(ctx->out->data, "int __zone_state_cause_liveBattle;");
        EXPECT_STR_CONTAINS(ctx->out->data, "bool __world_derived_dirty;");
        EXPECT_STR_CONTAINS(ctx->out->data, "GameWorld_sync(GameWorld *self)");
        EXPECT_STR_CONTAINS(ctx->out->data, "bool _pgy_prev_active_battle = self->__zone_active_battle;");
        EXPECT_STR_CONTAINS(ctx->out->data, "size_t _pgy_world_frontier_pass = 0;");
        EXPECT_STR_CONTAINS(ctx->out->data, "size_t _pgy_world_frontier_pass_limit = 5;");
        EXPECT_STR_CONTAINS(ctx->out->data, "bool _pgy_world_frontier_continue = true;");
        EXPECT_STR_CONTAINS(ctx->out->data, "while (_pgy_world_frontier_continue && _pgy_world_frontier_pass < _pgy_world_frontier_pass_limit) {");
        EXPECT_STR_CONTAINS(ctx->out->data, "bool _pgy_world_needs_derived = self->__world_derived_dirty;");
        EXPECT_STR_CONTAINS(ctx->out->data, "self->__zone_dirty_battle = true;");
        EXPECT_STR_CONTAINS(ctx->out->data, "if (self->__zone_dirty_battle) {");
        EXPECT_STR_CONTAINS(ctx->out->data, "BattleZone_sync(&self->battle);");
        EXPECT_STR_CONTAINS(ctx->out->data, "self->__zone_dirty_battle = false;");
        EXPECT_STR_CONTAINS(ctx->out->data, "if (_pgy_world_needs_derived) {");
        EXPECT_STR_CONTAINS(ctx->out->data, "size_t _pgy_world_pass = 0;");
        EXPECT_STR_CONTAINS(ctx->out->data, "bool _pgy_world_continue = true;");
        EXPECT_STR_CONTAINS(ctx->out->data, "while (_pgy_world_continue && _pgy_world_pass < _pgy_world_pass_limit) {");
        EXPECT_STR_CONTAINS(ctx->out->data, "_pgy_world_continue = false;");
        EXPECT_STR_CONTAINS(ctx->out->data, "_pgy_world_pass++;");
        EXPECT_STR_CONTAINS(ctx->out->data, "PGY_PANIC(\"world derived recompute exceeded bounded pass limit\");");
        EXPECT_STR_CONTAINS(ctx->out->data, "bool _pgy_prev_zone_state_liveBattle = self->__zone_state_liveBattle;");
        EXPECT_STR_CONTAINS(ctx->out->data, "self->__zone_active_battle = true;");
        EXPECT_STR_CONTAINS(ctx->out->data, "self->__zone_epoch_battle++;");
        EXPECT_STR_CONTAINS(ctx->out->data, "self->__zone_cause_battle = 7;");
        EXPECT_STR_CONTAINS(ctx->out->data, "self->__zone_state_liveBattle = self->__zone_active_battle;");
        EXPECT_STR_CONTAINS(ctx->out->data, "if (self->__zone_state_liveBattle != _pgy_prev_zone_state_liveBattle) {");
        EXPECT_STR_CONTAINS(ctx->out->data, "self->__zone_state_epoch_liveBattle++;");
        EXPECT_STR_CONTAINS(ctx->out->data, "self->__zone_state_cause_liveBattle = 10;");
        EXPECT_STR_CONTAINS(ctx->out->data, "return self->__zone_state_liveBattle;");

        transpiler_ctx_destroy(ctx);
        mir_destroy(mir);
        rir_destroy(rir);
        hir_destroy(hir);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("zone/world constructors lower to designated runtime instances");
    {
        const char *source =
            "subject Player { let hp: Int; }\n"
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
            "    let battle = BattleZone(Player(7));\n"
            "    let world = GameWorld(Clone(battle));\n"
            "    Log(world.Live());\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        HIRProgram *hir = NULL;
        RIRProgram *rir = NULL;
        MIRProgram *mir = lower_program_to_mir_strict(program, &hir, &rir);
        TranspilerCtx *ctx = transpiler_ctx_create();
        ctx->mir = mir;

        emit_program(ctx);

        EXPECT_STR_CONTAINS(ctx->out->data, "BattleZone");
        EXPECT_STR_CONTAINS(ctx->out->data, "GameWorld");
        EXPECT_STR_CONTAINS(ctx->out->data, "__zone_dirty_battle");
        EXPECT_STR_CONTAINS(ctx->out->data, "__world_derived_dirty");
        EXPECT_STR_CONTAINS(ctx->out->data, "__state_poisoned");
        EXPECT_STR_CONTAINS(ctx->out->data, "__zone_state_liveBattle");

        transpiler_ctx_destroy(ctx);
        mir_destroy(mir);
        rir_destroy(rir);
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
        HIRProgram *hir = NULL;
        RIRProgram *rir = NULL;
        MIRProgram *mir = lower_program_to_mir_strict(program, &hir, &rir);
        TranspilerCtx *ctx = transpiler_ctx_create();
        ctx->mir = mir;

        emit_program(ctx);

        EXPECT_STR_CONTAINS(ctx->out->data, "pgy_log_bool((bool)(self->battle.__projection_ready_playerView));");
        EXPECT_STR_CONTAINS(ctx->out->data, "pgy_log_bool((bool)(self->battle.__layer_active_poison));");
        EXPECT_STR_CONTAINS(ctx->out->data, "pgy_log_bool((bool)(self->battle.__state_poisoned));");

        transpiler_ctx_destroy(ctx);
        mir_destroy(mir);
        rir_destroy(rir);
        hir_destroy(hir);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("zone refresh map lowers renamed source fields into projection literal");
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
        HIRProgram *hir = NULL;
        RIRProgram *rir = NULL;
        MIRProgram *mir = lower_program_to_mir_strict(program, &hir, &rir);
        TranspilerCtx *ctx = transpiler_ctx_create();
        ctx->mir = mir;

        emit_program(ctx);

        EXPECT_STR_CONTAINS(ctx->out->data, ".totalHp = self->player.hp");
        EXPECT_STR_CONTAINS(ctx->out->data, ".label = self->player.name");

        transpiler_ctx_destroy(ctx);
        mir_destroy(mir);
        rir_destroy(rir);
        hir_destroy(hir);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }
