static void
test_ability_decl(void)
{
    printf("\n[ability_decl]\n");

    TEST("valid ability with fields entry passes");
    {
        SemanticContext *ctx = semantic_context_create();
        scope_enter(&ctx->scope, SCOPE_GLOBAL);

        ASTNode *ability = ast_create_ability_declaration("Damageable");
        ability->line = 1; ability->column = 1;

        ASTNode *req = ast_create_require_field("health");
        req->data.require_field.type = ast_create_type("Int");
        req->line = 2; req->column = 1;
        ability->data.ability_decl.require_count = 1;
        ability->data.ability_decl.require_fields = malloc(sizeof(ASTNode*));
        ability->data.ability_decl.require_fields[0] = req;

        type_check_ability_decl(ability, ctx);
        EXPECT(!ctx->has_error);

        semantic_context_destroy(ctx);
        ast_destroy(ability);
    }

    TEST("duplicate ability declaration triggers error");
    {
        SemanticContext *ctx = semantic_context_create();
        scope_enter(&ctx->scope, SCOPE_GLOBAL);

        ASTNode *ability1 = ast_create_ability_declaration("Foo");
        ability1->line = 1; ability1->column = 1;
        type_check_ability_decl(ability1, ctx);

        ASTNode *ability2 = ast_create_ability_declaration("Foo");
        ability2->line = 2; ability2->column = 1;
        type_check_ability_decl(ability2, ctx);
        EXPECT(ctx->has_error);

        semantic_context_destroy(ctx);
        ast_destroy(ability1);
        ast_destroy(ability2);
    }

    TEST("duplicate ability fields entry triggers error");
    {
        SemanticContext *ctx = semantic_context_create();
        scope_enter(&ctx->scope, SCOPE_GLOBAL);

        ASTNode *ability = ast_create_ability_declaration("Damageable");
        ability->line = 1; ability->column = 1;

        ASTNode *req_a = ast_create_require_field("health");
        req_a->data.require_field.type = ast_create_type("Int");
        req_a->line = 2; req_a->column = 1;

        ASTNode *req_b = ast_create_require_field("health");
        req_b->data.require_field.type = ast_create_type("Int");
        req_b->line = 3; req_b->column = 1;

        ability->data.ability_decl.require_count = 2;
        ability->data.ability_decl.require_fields = malloc(2 * sizeof(ASTNode *));
        ability->data.ability_decl.require_fields[0] = req_a;
        ability->data.ability_decl.require_fields[1] = req_b;

        type_check_ability_decl(ability, ctx);
        EXPECT(ctx->has_error);
        EXPECT(ctx_has_diagnostic_substring(ctx, "duplicate field 'health' in fields"));

        semantic_context_destroy(ctx);
        ast_destroy(ability);
    }
}
static void
test_role_decl(void)
{
    printf("\n[role_decl]\n");

    TEST("valid role with impl ability passes");
    {
        SemanticContext *ctx = semantic_context_create();
        scope_enter(&ctx->scope, SCOPE_GLOBAL);

        ASTNode *ability = ast_create_ability_declaration("Healable");
        ability->line = 1; ability->column = 1;
        type_check_ability_decl(ability, ctx);

        ASTNode *role = ast_create_role_declaration("HealerRole");
        role->line = 3; role->column = 1;

        ASTNode *impl = ast_create_impl_ability(ast_create_type("Healable"));
        impl->line = 4; impl->column = 1;
        role->data.role_decl.impl_count = 1;
        role->data.role_decl.impl_abilities = malloc(sizeof(ASTNode*));
        role->data.role_decl.impl_abilities[0] = impl;

        type_check_role_decl(role, ctx);
        EXPECT(!ctx->has_error);

        semantic_context_destroy(ctx);
        ast_destroy(ability);
        ast_destroy(role);
    }

    TEST("role ability fields must exist on bound subject host");
    {
        const char *source =
            "ability Combatable {\n"
            "    fields hp: Int;\n"
            "}\n"
            "subject Bot {\n"
            "    let hp: Int;\n"
            "}\n"
            "role Fighter for Bot {\n"
            "    impl ability Combatable {\n"
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

    TEST("role ability fields reject missing bound subject field");
    {
        const char *source =
            "ability Combatable {\n"
            "    fields hp: Int;\n"
            "}\n"
            "subject Bot {\n"
            "    let mp: Int;\n"
            "}\n"
            "role Fighter for Bot {\n"
            "    impl ability Combatable {\n"
            "    }\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticResult *result = semantic_analyze(program);

        EXPECT(!parser_has_error(parser));
        EXPECT(result != NULL && result->error_count > 0);
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "is missing required field 'hp'"));

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("role with unknown ability produces error");
    {
        SemanticContext *ctx = semantic_context_create();
        scope_enter(&ctx->scope, SCOPE_GLOBAL);

        ASTNode *role = ast_create_role_declaration("BadRole");
        role->line = 1; role->column = 1;

        ASTNode *impl = ast_create_impl_ability(ast_create_type("NonExistent"));
        impl->line = 2; impl->column = 1;
        role->data.role_decl.impl_count = 1;
        role->data.role_decl.impl_abilities = malloc(sizeof(ASTNode*));
        role->data.role_decl.impl_abilities[0] = impl;

        type_check_role_decl(role, ctx);
        EXPECT(ctx->has_error);
        EXPECT(ctx_has_diagnostic_substring(ctx, "implements unknown ability"));

        semantic_context_destroy(ctx);
        ast_destroy(role);
    }

    TEST("role bound to non-subject declaration is rejected");
    {
        const char *source =
            "struct Vec2 {\n"
            "    x: Int;\n"
            "}\n"
            "role ValueRole for Vec2 {\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticResult *result = semantic_analyze(program);

        EXPECT(!parser_has_error(parser));
        EXPECT(result != NULL && result->error_count == 1);
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "must be bound to a subject or primitive domain"));

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("role ability Add enables operator overload on target type");
    {
        const char *source =
            "ability Arithmetic {\n"
            "    func Add(other: Int) -> Int;\n"
            "}\n"
            "role IntMath for Int {\n"
            "    impl ability Arithmetic {\n"
            "        func Add(other: Int) -> Int { return 123; }\n"
            "    }\n"
            "}\n"
            "func Combine(a: Int, b: Int) -> Int {\n"
            "    return a + b;\n"
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

/* -----------------------------------------------------------------
 * Party declarations
 * ----------------------------------------------------------------- */

static void
test_party_decl(void)
{
    printf("\n[party_decl]\n");

    TEST("valid party with subject-backed role slot and shared field passes");
    {
        const char *source =
            "subject Player { let hp: Int; }\n"
            "ability Damageable { func Hit() -> Void; }\n"
            "role Tank for Player {\n"
            "    impl ability Damageable { func Hit() -> Void { Log(1); } }\n"
            "}\n"
            "party DungeonTeam {\n"
            "    role slot tank: Damageable\n"
            "    shared round: Int = 1\n"
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

    TEST("party shared fields are visible through party instances");
    {
        const char *source =
            "ability Speakable { func Greet() -> String; }\n"
            "subject Host { let name: String; }\n"
            "role Greeter for Host {\n"
            "    impl ability Speakable {\n"
            "        func Greet() -> String { return \"hello\"; }\n"
            "    }\n"
            "}\n"
            "party Speaker {\n"
            "    role slot speaker: Speakable\n"
            "    shared round: Int = 1\n"
            "}\n"
            "func Main() -> Void {\n"
            "    let s: Speaker = Speaker();\n"
            "    Log(s.round);\n"
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

    TEST("party constructor validates shared field arity");
    {
        const char *source =
            "party Speaker {\n"
            "    shared round: Int = 1\n"
            "}\n"
            "func Main() -> Void {\n"
            "    let s: Speaker = Speaker(1, 2);\n"
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

    TEST("party self member access is checked by semantic host context");
    {
        const char *source =
            "party Speaker {\n"
            "    shared round: Int = 1\n"
            "    func Report(self) -> Int {\n"
            "        return self.missing + 2;\n"
            "    }\n"
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

    TEST("roster self member access is checked by semantic host context");
    {
        const char *source =
            "party Speaker { shared round: Int = 1 }\n"
            "roster Board {\n"
            "    party slot speaker: Speaker\n"
            "    shared tick: Int = 4\n"
            "    func Report(self) -> Int {\n"
            "        return self.missing + 3;\n"
            "    }\n"
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

    TEST("duplicate party declaration triggers error");
    {
        SemanticContext *ctx = semantic_context_create();
        scope_enter(&ctx->scope, SCOPE_GLOBAL);

        ASTNode *p1 = ast_create_party_declaration("Team");
        p1->line = 1; p1->column = 1;
        type_check_party_decl(p1, ctx);

        ASTNode *p2 = ast_create_party_declaration("Team");
        p2->line = 2; p2->column = 1;
        type_check_party_decl(p2, ctx);
        EXPECT(ctx->has_error);

        semantic_context_destroy(ctx);
        ast_destroy(p1);
        ast_destroy(p2);
    }

    TEST("party role slot rejects abilities without subject-bound role impl");
    {
        const char *source =
            "ability Damageable { func Hit() -> Void; }\n"
            "party DungeonTeam {\n"
            "    role slot tank: Damageable\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticResult *result = semantic_analyze(program);

        EXPECT(!parser_has_error(parser));
        EXPECT(result != NULL && result->error_count == 1);
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "no subject-bound role implements it"));
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "consumer path is party role slot 'tank'"));

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("party role slot reports generic ability mismatch provenance");
    {
        const char *source =
            "ability Damageable<T> { func Hit(self, amount: T) -> Void; }\n"
            "subject Warrior { let hp: Int; }\n"
            "role WarriorDamage for Warrior {\n"
            "    impl ability Damageable<Int> {\n"
            "        func Hit(self, amount: Int) -> Void { return; }\n"
            "    }\n"
            "}\n"
            "party DungeonTeam {\n"
            "    role slot tank: Damageable<String>\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticResult *result = semantic_analyze(program);

        EXPECT(!parser_has_error(parser));
        EXPECT(result != NULL && result->error_count > 0);
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "consumer path is party role slot 'tank'"));
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "expected type args are 'Damageable<String>'"));
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "actual type args are 'Damageable<Int>'"));

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }
}

/* -----------------------------------------------------------------
 * Roster / World declarations
 * ----------------------------------------------------------------- */

static void
test_roster_world_decl(void)
{
    printf("\n[roster_world_decl]\n");

    TEST("valid roster declaration passes");
    {
        SemanticContext *ctx = semantic_context_create();
        scope_enter(&ctx->scope, SCOPE_GLOBAL);

        ASTNode *sys = ast_create_roster_declaration("CombatSystem");
        sys->line = 1; sys->column = 1;
        type_check_roster_decl(sys, ctx);
        EXPECT(!ctx->has_error);

        semantic_context_destroy(ctx);
        ast_destroy(sys);
    }

    TEST("valid world with roster ref passes");
    {
        SemanticContext *ctx = semantic_context_create();
        scope_enter(&ctx->scope, SCOPE_GLOBAL);

        /* Register roster first */
        ASTNode *sys = ast_create_roster_declaration("Combat");
        sys->line = 1; sys->column = 1;
        type_check_roster_decl(sys, ctx);

        /* Create world referencing it */
        ASTNode *world = ast_create_world_declaration("GameWorld");
        world->line = 3; world->column = 1;
        ASTNode *ws = ast_create_world_roster("combat", "Combat");
        ws->line = 4; ws->column = 1;
        world->data.world_decl.roster_count = 1;
        world->data.world_decl.rosters = malloc(sizeof(ASTNode*));
        world->data.world_decl.rosters[0] = ws;

        type_check_world_decl(world, ctx);
        EXPECT(!ctx->has_error);

        semantic_context_destroy(ctx);
        ast_destroy(sys);
        ast_destroy(world);
    }
}

static void
test_extern_block(void)
{
    printf("\n[extern_block]\n");

    TEST("extern C function is visible to later call");
    {
        SemanticContext *ctx = semantic_context_create();

        ASTNode *ext = ast_create_extern_block("C");
        ext->line = 1; ext->column = 1;

        ASTNode *fn = ast_create_function("SDL_Init");
        fn->line = 2; fn->column = 5;
        fn->data.func_decl.return_type = ast_create_type("Int");
        fn->data.func_decl.param_count = 1;
        fn->data.func_decl.params = calloc(1, sizeof(FuncParam*));

        FuncParam *param = calloc(1, sizeof(FuncParam));
        param->name = pergyra_strdup("flags");
        param->type = ast_create_type("Int");
        fn->data.func_decl.params[0] = param;

        ast_add_statement(ext, fn);

        ASTNode **call_args = calloc(1, sizeof(ASTNode*));
        call_args[0] = make_number(0, 4);
        ASTNode *call = make_call("SDL_Init", call_args, 1, 4);
        ASTNode *decl = ast_create_let_declaration("result");
        decl->line = 4; decl->column = 1;
        decl->data.let_decl.initializer = call;

        ASTNode *stmts[2] = { ext, decl };
        ASTNode *program = make_program(stmts, 2);

        type_check_program(program, ctx);
        EXPECT(!ctx->has_error);

        semantic_context_destroy(ctx);
        ast_destroy(ext);
        ast_destroy(decl);
        free(program);
    }
}

static void
test_engine_collections(void)
{
    printf("\n[engine_collections]\n");

    TEST("Array<Int> annotation resolves to constructed type");
    {
        SemanticContext *ctx = semantic_context_create();
        ASTNode *array_type = make_generic_type("Array", "Int");
        Type *resolved = semantic_type_resolution_lookup_metadata_type_ref(
            ctx, array_type);

        EXPECT(resolved->kind == TYPE_KIND_CONSTRUCTED
               && strcmp(resolved->name, "Array<Int>") == 0);

        semantic_context_destroy(ctx);
        ast_destroy(array_type);
    }

    TEST("Slice<Float>.Length resolves to Int");
    {
        SemanticContext *ctx = semantic_context_create();
        Type *args[1] = { TYPE_FLOAT };
        Type *slice_type = type_create_constructed(TYPE_SLICE, args, 1);
        scope_declare(ctx->scope,
            symbol_create_variable("view", slice_type, 1, 1));

        ASTNode *length = ast_create_member_access(
            make_identifier("view", 2), "Length");
        Type *resolved = type_check_expression(length, ctx);

        EXPECT(resolved == TYPE_INT && !ctx->has_error);

        semantic_context_destroy(ctx);
        ast_destroy(length);
    }

    TEST("Array<Int> indexing returns element type");
    {
        SemanticContext *ctx = semantic_context_create();
        Type *args[1] = { TYPE_INT };
        Type *array_type = type_create_constructed(TYPE_ARRAY, args, 1);
        scope_declare(ctx->scope,
            symbol_create_variable("values", array_type, 1, 1));

        ASTNode *access = ast_create_array_access(
            make_identifier("values", 2), make_number(0, 2));
        Type *resolved = type_check_expression(access, ctx);

        EXPECT(resolved == TYPE_INT && !ctx->has_error);

        semantic_context_destroy(ctx);
        ast_destroy(access);
    }

    TEST("List/Map/Set builtins accept matching generic element types");
    {
        const char *source =
            "func Main() -> Void {\n"
            "    let items: List<Int> = ListNew();\n"
            "    ListPush(items, 1);\n"
            "    let first: Int = ListGet(items, 0);\n"
            "    let seen: Set<Int> = SetNew();\n"
            "    SetAdd(seen, first);\n"
            "    let table: HashMap<String, Int> = MapNew();\n"
            "    MapSet(table, \"hp\", first);\n"
            "    let hp: Int = MapGet(table, \"hp\");\n"
            "    Log(first + hp);\n"
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

    TEST("HashMap<Int, Int> annotation resolves and builtins accept Int keys");
    {
        const char *source =
            "func Main() -> Void {\n"
            "    let table: HashMap<Int, Int> = MapNew();\n"
            "    MapSet(table, 7, 42);\n"
            "    let hp: Int = MapGet(table, 7);\n"
            "    Log(hp);\n"
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

    TEST("MapKeys returns Array<String> for HashMap<String, T>");
    {
        const char *source =
            "func Main() -> Void {\n"
            "    let table: HashMap<String, Int> = MapNew();\n"
            "    MapSet(table, \"hp\", 7);\n"
            "    let keys: Array<String> = MapKeys(table);\n"
            "    Log(ArrayLength(keys));\n"
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

    TEST("MapKeys returns Array<Int> for HashMap<Int, T>");
    {
        const char *source =
            "func Main() -> Void {\n"
            "    let table: HashMap<Int, Int> = MapNew();\n"
            "    MapSet(table, 7, 42);\n"
            "    let keys: Array<Int> = MapKeys(table);\n"
            "    let first: Int = keys[0];\n"
            "    Log(first);\n"
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

    TEST("HashMap<Long, Long> annotation resolves and builtins accept Long keys");
    {
        const char *source =
            "func Main() -> Void {\n"
            "    let table: HashMap<Long, Long> = MapNew();\n"
            "    MapSet(table, 7L, 42L);\n"
            "    let hp: Long = MapGet(table, 7L);\n"
            "    let keys: Array<Long> = MapKeys(table);\n"
            "    let first: Long = keys[0];\n"
            "    Log(hp);\n"
            "    Log(first);\n"
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

    TEST("HashMap<Bool, Int> annotation resolves and builtins accept Bool keys");
    {
        const char *source =
            "func Main() -> Void {\n"
            "    let table: HashMap<Bool, Int> = MapNew();\n"
            "    MapSet(table, true, 1);\n"
            "    MapSet(table, false, 2);\n"
            "    let hit: Int = MapGet(table, true);\n"
            "    let keys: Array<Bool> = MapKeys(table);\n"
            "    let first: Bool = keys[0];\n"
            "    Log(hit);\n"
            "    Log(first);\n"
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

    TEST("ListPush rejects wrong element type");
    {
        const char *source =
            "func Main() -> Void {\n"
            "    let items: List<Int> = ListNew();\n"
            "    ListPush(items, \"oops\");\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticResult *result = semantic_analyze(program);

        EXPECT(!parser_has_error(parser));
        EXPECT(result != NULL
            && result->error_count > 0
            && ctx_has_diagnostic_substring_from_result(result, "cannot assign"));

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("MapSet rejects wrong key type");
    {
        const char *source =
            "func Main() -> Void {\n"
            "    let table: HashMap<String, Int> = MapNew();\n"
            "    MapSet(table, 1, 7);\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticResult *result = semantic_analyze(program);

        EXPECT(!parser_has_error(parser));
        EXPECT(result != NULL
            && result->error_count > 0
            && ctx_has_diagnostic_substring_from_result(result, "cannot assign"));

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("MapGet rejects unsupported key kind outside stable subset");
    {
        const char *source =
            "func Main() -> Void {\n"
            "    let table: HashMap<Float, Int> = MapNew();\n"
            "    let hp: Int = MapGet(table, 7.0);\n"
            "    Log(hp);\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticResult *result = semantic_analyze(program);

        EXPECT(!parser_has_error(parser));
        EXPECT(result != NULL
            && result->error_count > 0
            && ctx_has_diagnostic_substring_from_result(
                result, "HashMap currently supports only String, Int, Long, or Bool keys"));

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("HashMap annotation rejects unsupported key kind before MapKeys");
    {
        const char *source =
            "func Main() -> Void {\n"
            "    let table: HashMap<Float, Int> = MapNew();\n"
            "    let keys: Array<Float> = MapKeys(table);\n"
            "    Log(ArrayLength(keys));\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticResult *result = semantic_analyze(program);

        EXPECT(!parser_has_error(parser));
        EXPECT(result != NULL
            && result->error_count > 0
            && ctx_has_diagnostic_substring_from_result(
                result, "HashMap currently supports only String, Int, Long, or Bool keys"));

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("ListGet rejects non-Int index");
    {
        const char *source =
            "func Main() -> Void {\n"
            "    let items: List<Int> = ListNew();\n"
            "    let first: Int = ListGet(items, \"0\");\n"
            "    Log(first);\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticResult *result = semantic_analyze(program);

        EXPECT(!parser_has_error(parser));
        EXPECT(result != NULL
            && result->error_count > 0
            && ctx_has_diagnostic_substring_from_result(result, "cannot assign"));

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("SetAdd rejects wrong element type");
    {
        const char *source =
            "func Main() -> Void {\n"
            "    let seen: Set<Int> = SetNew();\n"
            "    SetAdd(seen, \"oops\");\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticResult *result = semantic_analyze(program);

        EXPECT(!parser_has_error(parser));
        EXPECT(result != NULL
            && result->error_count > 0
            && ctx_has_diagnostic_substring_from_result(result, "cannot assign"));

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("ListSize rejects non-list values");
    {
        const char *source =
            "func Main() -> Void {\n"
            "    let size: Int = ListSize(1);\n"
            "    Log(size);\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticResult *result = semantic_analyze(program);

        EXPECT(!parser_has_error(parser));
        EXPECT(result != NULL
            && result->error_count > 0
            && ctx_has_diagnostic_substring_from_result(result, "ListSize expects List<T>"));

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("ListNew without annotation is rejected");
    {
        const char *source =
            "func Main() -> Void {\n"
            "    let items = ListNew();\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticResult *result = semantic_analyze(program);

        EXPECT(!parser_has_error(parser));
        EXPECT(result != NULL
            && result->error_count > 0
            && ctx_has_diagnostic_substring_from_result(result, "Cannot infer collection type"));

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("MapNew rejects unexpected constructor arguments");
    {
        const char *source =
            "func Main() -> Void {\n"
            "    let table: HashMap<String, Int> = MapNew(1);\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticResult *result = semantic_analyze(program);

        EXPECT(!parser_has_error(parser));
        EXPECT(result != NULL
            && result->error_count > 0
            && ctx_has_diagnostic_substring_from_result(result, "'MapNew' expects 0 argument(s)"));

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }
}
