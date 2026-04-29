    TEST("zone warns when subject-heavy shape should be decomposed");
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
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "passive business data"));
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "object/vessel support state"));
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "zone-first shape"));
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "objects/vessels"));

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("zone-first passive object shape is accepted without subject pressure");
    {
        const char *source =
            "object Order { id: Int; }\n"
            "object Customer { id: Int; }\n"
            "object AuditView { version: Int; }\n"
            "zone OrderReadZone {\n"
            "    object slot order: Order\n"
            "    object slot customer: Customer\n"
            "    object slot audit: AuditView\n"
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
