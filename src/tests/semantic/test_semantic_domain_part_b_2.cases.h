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
