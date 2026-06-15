    TEST("CFG parallel tasks allow shared ref subject boundary reads");
    {
        const char *source =
            "subject Hero {\n"
            "    let hp: Int;\n"
            "}\n"
            "func Inspect(ref hero: Hero) -> Void {\n"
            "    Log(hero.hp);\n"
            "}\n"
            "func Main() -> Void {\n"
            "    let hero: Hero = Hero(10);\n"
            "    parallel {\n"
            "        Inspect(hero);\n"
            "        Inspect(hero);\n"
            "    }\n"
            "    Log(hero.hp);\n"
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

    TEST("CFG parallel rejects shared collection capture");
    {
        const char *source =
            "func Main() -> Void {\n"
            "    let items: Array<Int> = [1, 2, 3];\n"
            "    parallel {\n"
            "        Log(ArrayLength(items));\n"
            "        Log(1);\n"
            "    }\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticResult *result = semantic_analyze(program);

        EXPECT(!parser_has_error(parser));
        EXPECT(result != NULL && result->error_count > 0);
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "cannot capture mutable collection"));

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("CFG parallel rejects borrowed Slice capture");
    {
        const char *source =
            "func Words() -> Array<Int> {\n"
            "    return [1, 2, 3];\n"
            "}\n"
            "func Main() -> Void {\n"
            "    let view: Slice<Int> = Words().Slice(0, 2);\n"
            "    parallel {\n"
            "        Log(ArrayLength(view));\n"
            "        Log(1);\n"
            "    }\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticResult *result = semantic_analyze(program);

        EXPECT(!parser_has_error(parser));
        EXPECT(result != NULL && result->error_count > 0);
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "cannot capture mutable collection"));

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("CFG parallel rejects HashMap storage capture");
    {
        const char *source =
            "func Main() -> Void {\n"
            "    let items: HashMap<String, Int> = MapNew();\n"
            "    parallel {\n"
            "        Log(MapSize(items));\n"
            "        Log(1);\n"
            "    }\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticResult *result = semantic_analyze(program);

        EXPECT(!parser_has_error(parser));
        EXPECT(result != NULL && result->error_count > 0);
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "cannot capture mutable collection"));

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("CFG parallel allows task-local collection shadowing");
    {
        const char *source =
            "func Main() -> Void {\n"
            "    let items: Array<Int> = [1, 2, 3];\n"
            "    parallel {\n"
            "        let f: func(Int) -> Int = (items: Int) => items + 1;\n"
            "        Log(1);\n"
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

    TEST("CFG spawn rejects borrowed subject boundary crossing");
    {
        const char *source =
            "subject Hero {\n"
            "    let hp: Int;\n"
            "}\n"
            "func Inspect(ref hero: Hero) -> Void {\n"
            "    Log(hero.hp);\n"
            "}\n"
            "func Main() -> Void with effects remote {\n"
            "    let hero: Hero = Hero(10);\n"
            "    let pending: Future<Void> = spawn Inspect(hero);\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticResult *result = semantic_analyze(program);

        EXPECT(!parser_has_error(parser));
        EXPECT(result != NULL && result->error_count > 0);
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "cannot cross spawn boundary"));

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("CFG spawn allows copy ref boundary crossing");
    {
        const char *source =
            "func Inspect(ref value: Int) -> Int {\n"
            "    return value;\n"
            "}\n"
            "func Main() -> Void with effects remote {\n"
            "    let value: Int = 10;\n"
            "    let pending: Future<Int> = spawn Inspect(value);\n"
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

    TEST("CFG spawn rejects authority Token boundary crossing");
    {
        ASTNode *program = ast_create_program();
        ASTNode *worker = ast_create_function("UseToken");
        ASTNode *main_func = ast_create_function("Main");
        ASTNode *call = ast_create_call(ast_create_identifier("UseToken"));
        ASTNode *spawn = ast_create_spawn_expression(call);
        SemanticResult *result;

        worker->data.func_decl.return_type = ast_create_type("Void");
        worker->data.func_decl.body = ast_create_block();
        worker->data.func_decl.param_count = 1;
        worker->data.func_decl.params = calloc(1, sizeof(FuncParam *));
        worker->data.func_decl.params[0] =
            make_func_param("token", make_generic_type("Token", "Int"));
        worker->data.func_decl.params[0]->mode = PARAM_MODE_OWN;

        main_func->data.func_decl.return_type = ast_create_type("Void");
        main_func->data.func_decl.body = ast_create_block();
        main_func->data.func_decl.has_effects_clause = true;
        main_func->data.func_decl.declared_effects = EFFECT_REMOTE;
        ast_add_argument(call, ast_create_identifier("token"));
        ast_add_statement(main_func->data.func_decl.body, spawn);

        ast_add_statement(program, worker);
        ast_add_statement(program, main_func);
        result = semantic_analyze(program);

        EXPECT(result != NULL && result->error_count > 0);
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "Authority-bearing Token parameter cannot cross spawn boundary"));

        semantic_result_destroy(result);
        ast_destroy(program);
    }

    TEST("CFG spawn rejects borrowed Slice boundary crossing");
    {
        const char *source =
            "func Inspect(ref view: Slice<Int>) -> Void {\n"
            "    Log(ArrayLength(view));\n"
            "}\n"
            "func Words() -> Array<Int> {\n"
            "    return [1, 2, 3];\n"
            "}\n"
            "func Main() -> Void with effects remote {\n"
            "    let view: Slice<Int> = Words().Slice(0, 2);\n"
            "    let pending: Future<Void> = spawn Inspect(view);\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticResult *result = semantic_analyze(program);

        EXPECT(!parser_has_error(parser));
        EXPECT(result != NULL && result->error_count > 0);
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "cannot cross spawn boundary"));
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "borrowed Slice view"));
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "SliceCopy(view)"));

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("CFG spawn rejects Array storage boundary crossing");
    {
        const char *source =
            "func Inspect(items: Array<Int>) -> Void {\n"
            "    Log(ArrayLength(items));\n"
            "}\n"
            "func Main() -> Void with effects remote {\n"
            "    let items: Array<Int> = [1, 2, 3];\n"
            "    let pending: Future<Void> = spawn Inspect(items);\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticResult *result = semantic_analyze(program);

        EXPECT(!parser_has_error(parser));
        EXPECT(result != NULL && result->error_count > 0);
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "Spawn argument cannot transport Array"));

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("CFG spawn rejects HashMap storage boundary crossing");
    {
        const char *source =
            "func Worker(items: HashMap<String, Int>) -> Void {\n"
            "    Log(MapSize(items));\n"
            "}\n"
            "func Main() -> Void with effects remote {\n"
            "    let items: HashMap<String, Int> = MapNew();\n"
            "    let pending: Future<Void> = spawn Worker(items);\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticResult *result = semantic_analyze(program);

        EXPECT(!parser_has_error(parser));
        EXPECT(result != NULL && result->error_count > 0);
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "Spawn argument cannot transport HashMap"));

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("CFG spawn rejects Channel storage boundary crossing");
    {
        const char *source =
            "func Worker(ch: Channel<Int>) -> Void {\n"
            "    Log(ChannelLength(ch));\n"
            "}\n"
            "func Main() -> Void with effects remote {\n"
            "    let ch: Channel<Int> = Channel(2);\n"
            "    let pending: Future<Void> = spawn Worker(ch);\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticResult *result = semantic_analyze(program);

        EXPECT(!parser_has_error(parser));
        EXPECT(result != NULL && result->error_count > 0);
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "Spawn argument cannot transport Channel"));

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("SliceCopy accepts borrowed Slice and returns owned Array");
    {
        const char *source =
            "func Words() -> Array<Int> {\n"
            "    return [1, 2, 3];\n"
            "}\n"
            "func Main() -> Void {\n"
            "    let view: Slice<Int> = Words().Slice(0, 2);\n"
            "    let owned: Array<Int> = SliceCopy(view);\n"
            "    Log(ArrayLength(owned));\n"
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

    TEST("SliceCopy rejects owned Array input");
    {
        const char *source =
            "func Main() -> Void {\n"
            "    let values: Array<Int> = [1, 2, 3];\n"
            "    let owned = SliceCopy(values);\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticResult *result = semantic_analyze(program);

        EXPECT(!parser_has_error(parser));
        EXPECT(result != NULL && result->error_count > 0);
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "SliceCopy requires Slice<T>"));

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("ref Slice return escape suggests SliceCopy snapshot");
    {
        const char *source =
            "func Leak(ref view: Slice<Int>) -> Slice<Int> {\n"
            "    return view;\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticResult *result = semantic_analyze(program);

        EXPECT(!parser_has_error(parser));
        EXPECT(result != NULL && result->error_count > 0);
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "borrowed Slice view"));
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "SliceCopy(view)"));

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("generic own/ref parameter requires ownership classifier fact");
    {
        const char *source =
            "func BorrowGeneric<T>(ref value: T) -> Void {\n"
            "    return;\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticResult *result = semantic_analyze(program);

        EXPECT(!parser_has_error(parser));
        EXPECT(result != NULL && result->error_count > 0);
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "parameter mode requires a boundary-visible type"));
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "type 'T' is not a copy-visible value"));

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }
