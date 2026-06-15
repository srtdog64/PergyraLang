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
        HIRProgram *hir = NULL;
        RIRProgram *rir = NULL;
        MIRProgram *mir = lower_program_to_mir_strict(program, &hir, &rir);
        TranspilerCtx *ctx = transpiler_ctx_create();
        ctx->mir = mir;

        emit_program(ctx);

        EXPECT_STR_CONTAINS(ctx->out->data, "Player_Attack(&self->player);");
        EXPECT_STR_CONTAINS(ctx->out->data, "self->poison.__projection_dirty_view = true;");
        EXPECT_STR_CONTAINS(ctx->out->data, "self->poison.__projection_ready_view = false;");
        EXPECT_STR_CONTAINS(ctx->out->data,
            "pgy_log_bool((bool)(BattleZone_has_layer_poison(self, __pgy_zone_gen)));");

        transpiler_ctx_destroy(ctx);
        mir_destroy(mir);
        rir_destroy(rir);
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
        HIRProgram *hir = NULL;
        RIRProgram *rir = NULL;
        MIRProgram *mir = lower_program_to_mir_strict(program, &hir, &rir);
        TranspilerCtx *ctx = transpiler_ctx_create();
        ctx->mir = mir;

        emit_program(ctx);

        EXPECT_STR_CONTAINS(ctx->out->data, "self->__zone_state_battleProjected = (self->__zone_active_battle && self->battle.__projection_ready_playerView);");
        EXPECT_STR_CONTAINS(ctx->out->data, "self->__zone_state_battleLayered = (self->__zone_active_battle && self->battle.__layer_active_poison);");
        EXPECT_STR_CONTAINS(ctx->out->data, "self->__zone_state_battlePoisoned = (self->__zone_active_battle && self->battle.__state_poisoned);");
        EXPECT_STR_CONTAINS(ctx->out->data, "pgy_log_bool((bool)(self->__zone_state_battleProjected));");
        EXPECT_STR_CONTAINS(ctx->out->data, "pgy_log_bool((bool)(self->__zone_state_battleLayered));");
        EXPECT_STR_CONTAINS(ctx->out->data, "pgy_log_bool((bool)(self->__zone_state_battlePoisoned));");

        transpiler_ctx_destroy(ctx);
        mir_destroy(mir);
        rir_destroy(rir);
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
        HIRProgram *hir = NULL;
        RIRProgram *rir = NULL;
        MIRProgram *mir = lower_program_to_mir_strict(program, &hir, &rir);
        TranspilerCtx *ctx = transpiler_ctx_create();
        ctx->mir = mir;

        emit_program(ctx);

        EXPECT_STR_CONTAINS(ctx->out->data, "self->__zone_state_allLive = (self->__zone_state_battleLive && self->__zone_state_campLive);");
        EXPECT_STR_CONTAINS(ctx->out->data, "self->__zone_state_anyLive = (self->__zone_state_allLive || self->__zone_state_campLive);");
        EXPECT_STR_CONTAINS(ctx->out->data, "pgy_log_bool((bool)(self->__zone_state_allLive));");
        EXPECT_STR_CONTAINS(ctx->out->data, "pgy_log_bool((bool)(self->__zone_state_anyLive));");

        transpiler_ctx_destroy(ctx);
        mir_destroy(mir);
        rir_destroy(rir);
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
        HIRProgram *hir = NULL;
        RIRProgram *rir = NULL;
        MIRProgram *mir = lower_program_to_mir_strict(program, &hir, &rir);
        TranspilerCtx *ctx = transpiler_ctx_create();
        ctx->mir = mir;

        emit_program(ctx);

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
        mir_destroy(mir);
        rir_destroy(rir);
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
        HIRProgram *hir = NULL;
        RIRProgram *rir = NULL;
        MIRProgram *mir = lower_program_to_mir_strict(program, &hir, &rir);
        TranspilerCtx *ctx = transpiler_ctx_create();
        ctx->mir = mir;

        emit_program(ctx);

        EXPECT_STR_CONTAINS(ctx->out->data, "GameWorld_Touch");
        EXPECT_STR_CONTAINS(ctx->out->data, "__zone_dirty_battle");
        EXPECT_STR_CONTAINS(ctx->out->data, "__world_derived_dirty");
        EXPECT_STR_CONTAINS(ctx->out->data, "GameWorld_sync");

        transpiler_ctx_destroy(ctx);
        mir_destroy(mir);
        rir_destroy(rir);
        hir_destroy(hir);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("world derived state chains emit bounded recompute loop");
    {
        const char *source =
            "zone BattleZone { }\n"
            "world GameWorld {\n"
            "    zone battle: BattleZone\n"
            "    state inner: zone battle\n"
            "    state outer: any inner\n"
            "    activate battle\n"
            "    func Show(self) -> Void {\n"
            "        Log(HasZone(outer));\n"
            "        Log(HasZone(inner));\n"
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

        EXPECT_STR_CONTAINS(ctx->out->data, "size_t _pgy_world_frontier_pass_limit = 4;");
        EXPECT_STR_CONTAINS(ctx->out->data, "while (_pgy_world_frontier_continue && _pgy_world_frontier_pass < _pgy_world_frontier_pass_limit) {");
        EXPECT_STR_CONTAINS(ctx->out->data, "size_t _pgy_world_pass_limit = 3;");
        EXPECT_STR_CONTAINS(ctx->out->data, "bool _pgy_prev_zone_state_outer = self->__zone_state_outer;");
        EXPECT_STR_CONTAINS(ctx->out->data, "self->__zone_state_outer = (self->__zone_state_inner);");
        EXPECT_STR_CONTAINS(ctx->out->data, "if (self->__zone_state_outer != _pgy_prev_zone_state_outer) {");
        EXPECT_STR_CONTAINS(ctx->out->data, "bool _pgy_prev_zone_state_inner = self->__zone_state_inner;");
        EXPECT_STR_CONTAINS(ctx->out->data, "self->__zone_state_inner = self->__zone_active_battle;");
        EXPECT_STR_CONTAINS(ctx->out->data, "if (self->__zone_state_inner != _pgy_prev_zone_state_inner) {");
        EXPECT_STR_CONTAINS(ctx->out->data, "if (_pgy_world_continue) {");
        EXPECT_STR_CONTAINS(ctx->out->data, "PGY_PANIC(\"world derived recompute exceeded bounded pass limit\");");
        EXPECT_STR_CONTAINS(ctx->out->data,
            "if (_pgy_world_derived_changed_any || self->__world_derived_dirty || self->__zone_dirty_battle) {");
        EXPECT_STR_CONTAINS(ctx->out->data, "_pgy_world_frontier_continue = true;");
        EXPECT_STR_CONTAINS(ctx->out->data, "PGY_PANIC(\"world frontier recompute exceeded bounded pass limit\");");

        transpiler_ctx_destroy(ctx);
        mir_destroy(mir);
        rir_destroy(rir);
        hir_destroy(hir);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("zone projection chains emit bounded recompute loop");
    {
        const char *source =
            "subject Player { let hp: Int; }\n"
            "object PlayerView { hp: Int; }\n"
            "object ScoreBoard { hp: Int; }\n"
            "zone BattleZone {\n"
            "    subject slot player: Player\n"
            "    object slot playerView: PlayerView\n"
            "    object slot board: ScoreBoard\n"
            "    authority player\n"
            "    refresh board from playerView by player\n"
            "    refresh playerView from player by player\n"
            "    func Show(self) -> Void {\n"
            "        Log(board.hp);\n"
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

        EXPECT_STR_CONTAINS(ctx->out->data, "size_t _pgy_zone_projection_pass_limit = 3;");
        EXPECT_STR_CONTAINS(ctx->out->data, "while (_pgy_zone_projection_continue && _pgy_zone_projection_pass < _pgy_zone_projection_pass_limit) {");
        EXPECT_STR_CONTAINS(ctx->out->data, "self->__projection_dirty_board = true;");
        EXPECT_STR_CONTAINS(ctx->out->data, "self->__projection_ready_board = false;");
        EXPECT_STR_CONTAINS(ctx->out->data, "_pgy_zone_projection_continue = true;");
        EXPECT_STR_CONTAINS(ctx->out->data, "PGY_PANIC(\"projection recompute exceeded bounded pass limit\");");

        transpiler_ctx_destroy(ctx);
        mir_destroy(mir);
        rir_destroy(rir);
        hir_destroy(hir);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("zone lifecycle emits bounded frontier loop");
    {
        const char *source =
            "subject Player { let hp: Int; }\n"
            "effect Poisoned for bearer: Player {\n"
            "}\n"
            "zone BattleZone {\n"
            "    subject slot player: Player\n"
            "    effect slot poison: Poisoned\n"
            "    state poisoned: effect poison on player\n"
            "    apply poison to player\n"
            "    func Show(self) -> Void {\n"
            "        Log(HasState(poisoned));\n"
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

        EXPECT_STR_CONTAINS(ctx->out->data, "size_t _pgy_zone_frontier_pass_limit = 3;");
        EXPECT_STR_CONTAINS(ctx->out->data, "while (_pgy_zone_frontier_continue && _pgy_zone_frontier_pass < _pgy_zone_frontier_pass_limit) {");
        EXPECT_STR_CONTAINS(ctx->out->data, "bool _pgy_prev_state_poisoned = self->__state_poisoned;");
        EXPECT_STR_CONTAINS(ctx->out->data, "bool _pgy_prev_layer_poison = self->__layer_active_poison;");
        EXPECT_STR_CONTAINS(ctx->out->data, "if (self->__state_poisoned != _pgy_prev_state_poisoned) {");
        EXPECT_STR_CONTAINS(ctx->out->data, "if (self->__layer_active_poison != _pgy_prev_layer_poison) {");
        EXPECT_STR_CONTAINS(ctx->out->data, "_pgy_zone_frontier_continue = true;");
        EXPECT_STR_CONTAINS(ctx->out->data, "PGY_PANIC(\"zone frontier recompute exceeded bounded pass limit\");");

        transpiler_ctx_destroy(ctx);
        mir_destroy(mir);
        rir_destroy(rir);
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
        HIRProgram *hir = NULL;
        RIRProgram *rir = NULL;
        MIRProgram *mir = lower_program_to_mir_strict(program, &hir, &rir);
        TranspilerCtx *ctx = transpiler_ctx_create();

        emit_program(ctx);

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
        mir_destroy(mir);
        rir_destroy(rir);
        hir_destroy(hir);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }
