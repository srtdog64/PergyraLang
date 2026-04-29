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

    TEST("CFG spawn rejects anonymous async body until capture lifetime is closed");
    {
        const char *source =
            "func Main() -> Void with effects remote {\n"
            "    let pending = spawn async () {\n"
            "        Log(1);\n"
            "    };\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticResult *result = semantic_analyze(program);

        EXPECT(!parser_has_error(parser));
        EXPECT(result != NULL && result->error_count > 0);
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "Anonymous async spawn bodies are beta-out-of-scope"));

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("CFG parallel channel send consumes resource after join");
    {
        const char *source =
            "func Main(ch: Channel<QubitSlot>) -> Void {\n"
            "    let q: QubitSlot = ClaimQubit();\n"
            "    parallel {\n"
            "        ch <- q;\n"
            "    }\n"
            "    QubitState(q);\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticResult *result = semantic_analyze(program);

        EXPECT(!parser_has_error(parser));
        EXPECT(result != NULL && result->error_count > 0);
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "was moved or released and cannot be used again"));

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("CFG parallel channel sends reject double resource consume");
    {
        const char *source =
            "func Main(ch: Channel<QubitSlot>) -> Void {\n"
            "    let q: QubitSlot = ClaimQubit();\n"
            "    parallel {\n"
            "        ch <- q;\n"
            "        ch <- q;\n"
            "    }\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticResult *result = semantic_analyze(program);

        EXPECT(!parser_has_error(parser));
        EXPECT(result != NULL && result->error_count > 0);
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "Parallel tasks cannot consume the same resource"));

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    /* ---- RemoteFuture design: await returns Result, Claim/Read/Write/Release rejected ---- */

    TEST("await on local Future<Int> returns Int (not Result)");
    {
        SemanticContext *ctx = semantic_context_create();
        scope_enter(&ctx->scope, SCOPE_GLOBAL);
        ctx->in_async_func = true;

        Type *args[1] = { TYPE_INT };
        Type *future_type = type_create_constructed(TYPE_FUTURE, args, 1);
        scope_declare(ctx->scope,
            symbol_create_variable("local_future", future_type, 1, 1));

        ASTNode *await_expr =
            ast_create_await_expression(make_identifier("local_future", 1));
        Type *t = type_check_expression(await_expr, ctx);
        EXPECT(!ctx->has_error);
        EXPECT(t == TYPE_INT);

        semantic_context_destroy(ctx);
        ast_destroy(await_expr);
    }

    TEST("await on RemoteFuture<Int> returns Result<Int>");
    {
        SemanticContext *ctx = semantic_context_create();
        scope_enter(&ctx->scope, SCOPE_GLOBAL);
        ctx->in_async_func = true;

        Type *args[1] = { TYPE_INT };
        Type *future_type = type_create_constructed(TYPE_REMOTE_FUTURE, args, 1);
        scope_declare(ctx->scope,
            symbol_create_variable("remote_future", future_type, 1, 1));

        ASTNode *await_expr =
            ast_create_await_expression(make_identifier("remote_future", 1));
        Type *t = type_check_expression(await_expr, ctx);
        EXPECT(!ctx->has_error);
        EXPECT(t != NULL
            && t->kind == TYPE_KIND_CONSTRUCTED
            && type_equals(t->data.constructed.constructor, TYPE_RESULT));
        EXPECT(t != NULL
            && t->data.constructed.arg_count >= 1
            && t->data.constructed.args[0] == TYPE_INT);

        semantic_context_destroy(ctx);
        ast_destroy(await_expr);
    }

    TEST("Read on RemoteFuture is rejected with helpful error");
    {
        SemanticContext *ctx = semantic_context_create();
        scope_enter(&ctx->scope, SCOPE_GLOBAL);

        Type *args[1] = { TYPE_INT };
        Type *future_type = type_create_constructed(TYPE_REMOTE_FUTURE, args, 1);
        scope_declare(ctx->scope,
            symbol_create_variable("rf", future_type, 1, 1));

        ASTNode *read_args[1] = { make_identifier("rf", 1) };
        ASTNode *read_call = make_call("Read", read_args, 1, 1);
        type_check_expression(read_call, ctx);
        EXPECT(ctx->has_error
            && ctx_has_diagnostic_substring(ctx, "RemoteFuture does not support Read"));

        semantic_context_destroy(ctx);
        ast_destroy(read_call);
    }

    TEST("Write on RemoteFuture is rejected with helpful error");
    {
        SemanticContext *ctx = semantic_context_create();
        scope_enter(&ctx->scope, SCOPE_GLOBAL);

        Type *args[1] = { TYPE_INT };
        Type *future_type = type_create_constructed(TYPE_REMOTE_FUTURE, args, 1);
        scope_declare(ctx->scope,
            symbol_create_variable("rf", future_type, 1, 1));

        ASTNode *write_args[2] = { make_identifier("rf", 1), make_number(42, 1) };
        ASTNode *write_call = make_call("Write", write_args, 2, 1);
        type_check_expression(write_call, ctx);
        EXPECT(ctx->has_error
            && ctx_has_diagnostic_substring(ctx, "RemoteFuture does not support Write"));

        semantic_context_destroy(ctx);
        ast_destroy(write_call);
    }

    TEST("Release on RemoteFuture is rejected with helpful error");
    {
        SemanticContext *ctx = semantic_context_create();
        scope_enter(&ctx->scope, SCOPE_GLOBAL);

        Type *args[1] = { TYPE_INT };
        Type *future_type = type_create_constructed(TYPE_REMOTE_FUTURE, args, 1);
        scope_declare(ctx->scope,
            symbol_create_variable("rf", future_type, 1, 1));

        ASTNode *release_args[1] = { make_identifier("rf", 1) };
        ASTNode *release_call = make_call("Release", release_args, 1, 1);
        type_check_expression(release_call, ctx);
        EXPECT(ctx->has_error
            && ctx_has_diagnostic_substring(ctx, "RemoteFuture does not support Release"));

        semantic_context_destroy(ctx);
        ast_destroy(release_call);
    }

    TEST("Option<Int> annotation resolves to constructed type");
    {
        SemanticContext *ctx = semantic_context_create();
        scope_enter(&ctx->scope, SCOPE_GLOBAL);
        ASTNode *opt = make_generic_type("Option", "Int");
        Type *t = semantic_type_resolution_lookup_type_ref_or_materialize(ctx,
                                                                          opt);
        EXPECT(!ctx->has_error);
        EXPECT(t != NULL
            && t->kind == TYPE_KIND_CONSTRUCTED
            && type_equals(t->data.constructed.constructor, TYPE_OPTION));
        EXPECT(t != NULL
            && t->data.constructed.arg_count >= 1
            && t->data.constructed.args[0] == TYPE_INT);
        semantic_context_destroy(ctx);
        ast_destroy(opt);
    }

    TEST("type alias annotations resolve in program declarations");
    {
        const char *source =
            "type UserId = Int;\n"
            "type NameList = List<String>;\n"
            "func Main() -> Void {\n"
            "    let id: UserId = 7;\n"
            "    let names: NameList = ListNew();\n"
            "    ListPush(names, \"gyri\");\n"
            "    Log(id);\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticResult *result = semantic_analyze(program);        EXPECT(!parser_has_error(parser));
        EXPECT(result != NULL && result->error_count == 0);

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("function-body let with annotation and no initializer is parser-rejected");
    {
        const char *source =
            "func Main() -> Void {\n"
            "    let x: Int;\n"
            "    Log(x);\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticResult *result = semantic_analyze(program);

        EXPECT(parser_has_error(parser));

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("function-body let with aggregate annotation and no initializer is parser-rejected");
    {
        const char *source =
            "class Box<T> {\n"
            "    let value: T;\n"
            "}\n"
            "func Main() -> Void {\n"
            "    let b: Box<Int>;\n"
            "    Log(0);\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticResult *result = semantic_analyze(program);

        EXPECT(parser_has_error(parser));

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("subject field let with no initializer does not trigger the uninit-local guard");
    {
        /* Class/subject fields use a distinct parser path (ClassField), so
         * the function-body uninit-local guard must not fire on them.  The
         * substring check stays negative to pin the behavior in place. */
        const char *source =
            "subject Hero {\n"
            "    let hp: Int;\n"
            "}\n"
            "func Main() -> Void {\n"
            "    Log(1);\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticResult *result = semantic_analyze(program);

        EXPECT(!parser_has_error(parser));
        EXPECT(!ctx_has_diagnostic_substring_from_result(result,
            "type annotation but no initializer"));

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("WriteView requires exclusive slot view access");
    {
        const char *source =
            "func Main() -> Void {\n"
            "    let s: Slot<Int> = ClaimSlot();\n"
            "    let r: ReadView<Int> = ViewRead(s);\n"
            "    let w: WriteView<Int> = ViewWrite(s);\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticResult *result = semantic_analyze(program);

        EXPECT(!parser_has_error(parser));
        EXPECT(result != NULL && result->error_count == 1);
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "WriteView<T> is exclusive"));
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "conflicts with existing ReadView"));

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("ReadView after WriteView is rejected by exclusive view gate");
    {
        const char *source =
            "func Main() -> Void {\n"
            "    let s: Slot<Int> = ClaimSlot();\n"
            "    let w: WriteView<Int> = ViewWrite(s);\n"
            "    let r: ReadView<Int> = ViewRead(s);\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticResult *result = semantic_analyze(program);

        EXPECT(!parser_has_error(parser));
        EXPECT(result != NULL && result->error_count == 1);
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "WriteView<T> is exclusive"));

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("multiple ReadView bindings are accepted");
    {
        const char *source =
            "func Main() -> Void {\n"
            "    let s: Slot<Int> = ClaimSlot();\n"
            "    let r1: ReadView<Int> = ViewRead(s);\n"
            "    let r2: ReadView<Int> = ViewRead(s);\n"
            "    Log(Read(r1));\n"
            "    Log(Read(r2));\n"
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
