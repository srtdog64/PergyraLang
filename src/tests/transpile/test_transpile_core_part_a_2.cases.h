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

    TEST("AllocatorScratch() -> pgy_allocator_scratch()");
    {
        ctx = transpiler_ctx_create();
        result = emit_expression(make_call("AllocatorScratch", NULL, 0, 1), ctx);
        EXPECT(strcmp(result, "pgy_allocator_scratch()") == 0);
        free(result);
        transpiler_ctx_destroy(ctx);
    }

    TEST("AllocatorPool missing capacity fails closed");
    {
        ASTNode *call = make_call("AllocatorPool", NULL, 0, 1);
        ctx = transpiler_ctx_create();
        result = emit_expression(call, ctx);
        EXPECT(result == NULL);
        EXPECT(ctx->backend_error != NULL);
        EXPECT_STR_CONTAINS(ctx->backend_error,
            "AllocatorPool requires exactly one capacity argument");
        ast_destroy(call);
        transpiler_ctx_destroy(ctx);
    }

    TEST("ToTObject missing arguments fails closed");
    {
        ctx = transpiler_ctx_create();
        result = emit_expression(make_call("ToTObject", NULL, 0, 1), ctx);
        EXPECT(result == NULL);
        EXPECT(ctx->backend_error != NULL);
        EXPECT_STR_CONTAINS(ctx->backend_error,
            "ToTObject requires target type and source subject");
        transpiler_ctx_destroy(ctx);
    }

    TEST("HasState(poisoned) -> zone semantic placeholder outside zone context");
    {
        ctx = transpiler_ctx_create();
        ASTNode *args[1] = { make_identifier("poisoned", 1) };
        result = emit_expression(make_call("HasState", args, 1, 1), ctx);
        EXPECT(result == NULL);
        EXPECT(ctx->backend_error != NULL);
        EXPECT_STR_CONTAINS(ctx->backend_error,
            "C backend: HasState requires active zone context");
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
        EXPECT(result == NULL);
        EXPECT(ctx->backend_error != NULL);
        EXPECT_STR_CONTAINS(ctx->backend_error,
            "C backend: HasState requires active zone context");
        transpiler_ctx_destroy(ctx);
    }

    TEST("HasZone(battle) -> world semantic placeholder outside world context");
    {
        ctx = transpiler_ctx_create();
        ASTNode *args[1] = { make_identifier("battle", 1) };
        result = emit_expression(make_call("HasZone", args, 1, 1), ctx);
        EXPECT(result == NULL);
        EXPECT(ctx->backend_error != NULL);
        EXPECT_STR_CONTAINS(ctx->backend_error,
            "C backend: HasZone requires active world context");
        transpiler_ctx_destroy(ctx);
    }

    TEST("HasLayer(poison) -> zone semantic placeholder outside zone context");
    {
        ctx = transpiler_ctx_create();
        ASTNode *args[1] = { make_identifier("poison", 1) };
        result = emit_expression(make_call("HasLayer", args, 1, 1), ctx);
        EXPECT(result == NULL);
        EXPECT(ctx->backend_error != NULL);
        EXPECT_STR_CONTAINS(ctx->backend_error,
            "C backend: HasLayer requires active zone context");
        transpiler_ctx_destroy(ctx);
    }

    TEST("HasProjection(snapshot) -> domain semantic placeholder outside domain context");
    {
        ctx = transpiler_ctx_create();
        ASTNode *args[1] = { make_identifier("snapshot", 1) };
        result = emit_expression(make_call("HasProjection", args, 1, 1), ctx);
        EXPECT(result == NULL);
        EXPECT(ctx->backend_error != NULL);
        EXPECT_STR_CONTAINS(ctx->backend_error,
            "C backend: HasProjection requires active relation/effect/zone projection context");
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
