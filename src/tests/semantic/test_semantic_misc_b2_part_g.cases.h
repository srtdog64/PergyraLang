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
                && !refresh_a->data.zone_refresh.derive_target_kind
                && strcmp(refresh_a->data.zone_refresh.object_slot_name, "playerView") == 0
                && strcmp(refresh_a->data.zone_refresh.participant_slot_name, "player") == 0);
            EXPECT(refresh_b != NULL
                && !refresh_b->data.zone_refresh.requires_dto
                && !refresh_b->data.zone_refresh.derive_target_kind
                && strcmp(refresh_b->data.zone_refresh.object_slot_name, "playerCard") == 0
                && strcmp(refresh_b->data.zone_refresh.participant_slot_name, "player") == 0);
            EXPECT(publish_a != NULL
                && publish_a->data.zone_refresh.requires_dto
                && !publish_a->data.zone_refresh.derive_target_kind
                && strcmp(publish_a->data.zone_refresh.object_slot_name, "snapshot") == 0
                && strcmp(publish_a->data.zone_refresh.participant_slot_name, "player") == 0);
            EXPECT(publish_b != NULL
                && publish_b->data.zone_refresh.requires_dto
                && !publish_b->data.zone_refresh.derive_target_kind
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

    TEST("zone projection map duplicate target field includes reason and fix");
    {
        const char *source =
            "subject Player { let hp: Int; let mana: Int; }\n"
            "object PlayerView { total: Int; }\n"
            "zone BattleZone {\n"
            "    subject slot player: Player\n"
            "    object slot playerView: PlayerView\n"
            "    refresh playerView from player map {\n"
            "        total <- hp;\n"
            "        total <- mana;\n"
            "    }\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticResult *result = semantic_analyze(program);

        EXPECT(!parser_has_error(parser));
        EXPECT(result != NULL && result->error_count > 0);
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "projection map duplicates target field 'total'"));
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "Reason:"));
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "each projection target field may be filled from exactly one source field"));
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "Fix:"));
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "keep a single mapping for 'total'"));

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("intent-level where default failure reports provenance");
    {
        const char *source =
            "subject Hero { let hp: Int; }\n"
            "intent Patrol(hero: Hero) {\n"
            "    who: hero;\n"
            "    where: MissingZone;\n"
            "    step Guard { expect: true; }\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticResult *result = semantic_analyze(program);

        EXPECT(!parser_has_error(parser));
        EXPECT(result != NULL && result->error_count > 0);
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "this where value came from the intent-level where default"));
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "declare zone 'MissingZone'"));

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("intent using mismatch reports compressed derivation reason");
    {
        const char *source =
            "subject Hero { let hp: Int; }\n"
            "zone BattleZone {\n"
            "    subject slot hero: Hero\n"
            "}\n"
            "intent Patrol(battle: BattleZone, hero: Hero) {\n"
            "    who: hero;\n"
            "    where: BattleZone;\n"
            "    step Guard { using: hero; expect: true; }\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticResult *result = semantic_analyze(program);

        EXPECT(!parser_has_error(parser));
        EXPECT(result != NULL && result->error_count > 0);
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "using binding must match zone type 'BattleZone', got 'Hero'"));
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "using binding points to a different zone than the current where contract"));
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "change using to a binding of type 'BattleZone'"));

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

}

/* -----------------------------------------------------------------
 * Main
 * ----------------------------------------------------------------- */
