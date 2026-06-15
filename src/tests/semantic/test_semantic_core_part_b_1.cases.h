static void
test_undefined_symbol(void)
{
    printf("\n[type_checker ->symbol resolution]\n");

    TEST("Undefined identifier ->error");
    {
        SemanticContext *ctx = semantic_context_create();
        ASTNode *id = make_identifier("nope", 1);
        type_check_expression(id, ctx);
        EXPECT(ctx->has_error);
        semantic_context_destroy(ctx);
    }

    TEST("Defined identifier ->no error");
    {
        SemanticContext *ctx = semantic_context_create();
        Symbol *sym = symbol_create_variable("x", TYPE_INT, 1, 1);
        scope_declare(ctx->scope, sym);
        ASTNode *id = make_identifier("x", 2);
        Type *t = type_check_expression(id, ctx);
        EXPECT(!ctx->has_error && type_equals(t, TYPE_INT));
        semantic_context_destroy(ctx);
    }

    TEST("Private namespace function access inside Log ->error");
    {
        SemanticContext *ctx = semantic_context_create();
        Type *param_types[2] = { TYPE_INT, TYPE_INT };
        Type *math_add = type_create_function(param_types, 2, TYPE_INT);
        scope_declare(ctx->scope,
            symbol_create_function("Math_Add", math_add, 1, 1));

        ASTNode *hidden_args[2] = {
            make_number(2, 2),
            make_number(5, 2)
        };
        ASTNode *hidden_call = make_call_expr(
            make_member_access(make_identifier("Math", 2), "HiddenAdd", 2),
            hidden_args, 2, 2);
        ASTNode *log_args[1] = { hidden_call };
        ASTNode *log_call = make_call("Log", log_args, 1, 2);

        type_check_expression(log_call, ctx);
        EXPECT(ctx->has_error
            && ctx_has_diagnostic_substring(ctx, "export visibility"));

        semantic_context_destroy(ctx);
        ast_destroy(log_call);
    }

    TEST("Raw namespace declaration is semantically traversed");
    {
        const char *source =
            "namespace Math {\n"
            "    func Id(x: Int) -> Int { return x; }\n"
            "}\n"
            "func Main() -> Void { return; }\n";
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

    TEST("duplicate stdlib use emits warning");
    {
        const char *source =
            "use storage;\n"
            "use storage;\n"
            "func Main() -> Void { return; }\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticResult *result = semantic_analyze(program);

        EXPECT(!parser_has_error(parser));
        EXPECT(result != NULL && result->error_count == 0);
        EXPECT(result != NULL && result->warning_count > 0);
        EXPECT(ctx_has_diagnostic_substring_from_result(result, "Duplicate stdlib use"));

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("use stdlib module exposes exported datetime symbols");
    {
        const char *main_path = "test_use_datetime_module_main.pgy";
        const char *main_source =
            "use datetime;\n"
            "func Main() -> Void {\n"
            "    let d: LocalDate = LocalDate(2026, 4, 10);\n"
            "    Log(FormatDate(d));\n"
            "}\n";
        FILE *main_file = fopen(main_path, "wb");
        char *error_message = NULL;
        ASTNode *program = NULL;
        SemanticResult *result = NULL;

        EXPECT(main_file != NULL);
        if (main_file != NULL) {
            fputs(main_source, main_file);
            fclose(main_file);
        }

        program = import_resolver_load_program(main_path, &error_message);
        EXPECT(program != NULL);
        if (program != NULL)
            result = semantic_analyze(program);
        EXPECT(error_message == NULL);
        EXPECT(result != NULL && result->error_count == 0);

        free(error_message);
        semantic_result_destroy(result);
        ast_destroy(program);
        remove(main_path);
    }
}

static void
test_while_loop(void)
{
    printf("\n[type_checker ->while loop]\n");

    TEST("While loop with Bool condition ->no error");
    {
        SemanticContext *ctx = semantic_context_create();
        /* Build: while true { } */
        ASTNode *wh = calloc(1, sizeof(ASTNode));
        wh->type = AST_WHILE_LOOP;
        wh->line = 1;
        ASTNode *cond = calloc(1, sizeof(ASTNode));
        cond->type = AST_BOOLEAN;
        cond->data.boolean.value = true;
        ASTNode *body = calloc(1, sizeof(ASTNode));
        body->type = AST_BLOCK;
        body->data.block.statements = NULL;
        body->data.block.count = 0;
        wh->data.while_loop.condition = cond;
        wh->data.while_loop.body = body;

        type_check_while_loop(wh, ctx);
        EXPECT(!ctx->has_error);
        semantic_context_destroy(ctx);
        free(wh); free(cond); free(body);
    }

    TEST("While loop with Int condition ->error");
    {
        SemanticContext *ctx = semantic_context_create();
        ASTNode *wh = calloc(1, sizeof(ASTNode));
        wh->type = AST_WHILE_LOOP;
        wh->line = 1;
        ASTNode *cond = calloc(1, sizeof(ASTNode));
        cond->type = AST_NUMBER;
        cond->data.number.value = 42;
        ASTNode *body = calloc(1, sizeof(ASTNode));
        body->type = AST_BLOCK;
        body->data.block.statements = NULL;
        body->data.block.count = 0;
        wh->data.while_loop.condition = cond;
        wh->data.while_loop.body = body;

        type_check_while_loop(wh, ctx);
        EXPECT(ctx->has_error);
        semantic_context_destroy(ctx);
        free(wh); free(cond); free(body);
    }

    TEST("break outside loop ->error");
    {
        SemanticContext *ctx = semantic_context_create();
        ASTNode *br = calloc(1, sizeof(ASTNode));
        br->type = AST_BREAK;
        br->line = 1;
        type_check_statement(br, ctx);
        EXPECT(ctx->has_error);
        semantic_context_destroy(ctx);
        free(br);
    }

    TEST("continue inside while loop ->no error");
    {
        SemanticContext *ctx = semantic_context_create();
        ASTNode *wh = calloc(1, sizeof(ASTNode));
        wh->type = AST_WHILE_LOOP;
        wh->line = 1;
        wh->data.while_loop.condition = make_boolean(true, 1);
        ASTNode *body = calloc(1, sizeof(ASTNode));
        body->type = AST_BLOCK;
        body->data.block.count = 1;
        body->data.block.statements = calloc(1, sizeof(ASTNode *));
        ASTNode *cont = calloc(1, sizeof(ASTNode));
        cont->type = AST_CONTINUE;
        cont->line = 2;
        body->data.block.statements[0] = cont;
        wh->data.while_loop.body = body;

        type_check_while_loop(wh, ctx);
        EXPECT(!ctx->has_error);
        semantic_context_destroy(ctx);
        ast_destroy(wh->data.while_loop.condition);
        free(body->data.block.statements);
        free(cont);
        free(body);
        free(wh);
    }

    TEST("labeled break to outer loop ->no error");
    {
        SemanticContext *ctx = semantic_context_create();
        ASTNode *outer = calloc(1, sizeof(ASTNode));
        outer->type = AST_WHILE_LOOP;
        outer->line = 1;
        outer->data.while_loop.label = pergyra_strdup("outer");
        outer->data.while_loop.condition = make_boolean(true, 1);

        ASTNode *inner = calloc(1, sizeof(ASTNode));
        inner->type = AST_WHILE_LOOP;
        inner->line = 2;
        inner->data.while_loop.condition = make_boolean(true, 2);

        ASTNode *break_stmt = calloc(1, sizeof(ASTNode));
        break_stmt->type = AST_BREAK;
        break_stmt->line = 3;
        break_stmt->data.break_stmt.label = pergyra_strdup("outer");

        ASTNode *inner_body = ast_create_block();
        ast_add_statement(inner_body, break_stmt);
        inner->data.while_loop.body = inner_body;

        ASTNode *outer_body = ast_create_block();
        ast_add_statement(outer_body, inner);
        outer->data.while_loop.body = outer_body;

        type_check_while_loop(outer, ctx);
        EXPECT(!ctx->has_error);

        semantic_context_destroy(ctx);
        ast_destroy(outer);
    }

    TEST("unknown labeled break ->error");
    {
        SemanticContext *ctx = semantic_context_create();
        ASTNode *outer = calloc(1, sizeof(ASTNode));
        outer->type = AST_WHILE_LOOP;
        outer->line = 1;
        outer->data.while_loop.label = pergyra_strdup("outer");
        outer->data.while_loop.condition = make_boolean(true, 1);

        ASTNode *break_stmt = calloc(1, sizeof(ASTNode));
        break_stmt->type = AST_BREAK;
        break_stmt->line = 2;
        break_stmt->data.break_stmt.label = pergyra_strdup("missing");

        ASTNode *body = ast_create_block();
        ast_add_statement(body, break_stmt);
        outer->data.while_loop.body = body;

        type_check_while_loop(outer, ctx);
        EXPECT(ctx->has_error);

        semantic_context_destroy(ctx);
        ast_destroy(outer);
    }

    TEST("labeled continue to outer loop ->no error");
    {
        SemanticContext *ctx = semantic_context_create();
        ASTNode *outer = calloc(1, sizeof(ASTNode));
        outer->type = AST_WHILE_LOOP;
        outer->line = 1;
        outer->data.while_loop.label = pergyra_strdup("outer");
        outer->data.while_loop.condition = make_boolean(true, 1);

        ASTNode *inner = calloc(1, sizeof(ASTNode));
        inner->type = AST_WHILE_LOOP;
        inner->line = 2;
        inner->data.while_loop.condition = make_boolean(true, 2);

        ASTNode *continue_stmt = calloc(1, sizeof(ASTNode));
        continue_stmt->type = AST_CONTINUE;
        continue_stmt->line = 3;
        continue_stmt->data.continue_stmt.label = pergyra_strdup("outer");

        ASTNode *inner_body = ast_create_block();
        ast_add_statement(inner_body, continue_stmt);
        inner->data.while_loop.body = inner_body;

        ASTNode *outer_body = ast_create_block();
        ast_add_statement(outer_body, inner);
        outer->data.while_loop.body = outer_body;

        type_check_while_loop(outer, ctx);
        EXPECT(!ctx->has_error);

        semantic_context_destroy(ctx);
        ast_destroy(outer);
    }

    TEST("break skips unreachable QubitSlot move in while loop");
    {
        SemanticContext *ctx = semantic_context_create();

        ASTNode *decl = ast_create_let_declaration("q");
        decl->data.let_decl.type = ast_create_type("QubitSlot");
        decl->data.let_decl.initializer = make_call("ClaimQubit", NULL, 0, 1);
        type_check_let_decl(decl, ctx);

        ASTNode *wh = calloc(1, sizeof(ASTNode));
        wh->type = AST_WHILE_LOOP;
        wh->line = 2;
        wh->data.while_loop.condition = make_boolean(true, 2);
        ASTNode *body = ast_create_block();
        ASTNode *br = calloc(1, sizeof(ASTNode));
        br->type = AST_BREAK;
        br->line = 3;
        ast_add_statement(body, br);
        ASTNode *moved = ast_create_let_declaration("a");
        moved->data.let_decl.type = ast_create_type("QubitSlot");
        moved->data.let_decl.initializer = make_identifier("q", 4);
        ast_add_statement(body, moved);
        wh->data.while_loop.body = body;

        type_check_while_loop(wh, ctx);
        EXPECT(!ctx->has_error);

        ASTNode *state_args[1] = { make_identifier("q", 5) };
        ASTNode *state_call = make_call("QubitState", state_args, 1, 5);
        Type *t = type_check_expression(state_call, ctx);
        EXPECT(!ctx->has_error && type_equals(t, TYPE_INT));

        semantic_context_destroy(ctx);
        ast_destroy(decl);
        ast_destroy(wh);
        ast_destroy(state_call);
    }

    TEST("continue skips unreachable QubitSlot move in while loop");
    {
        SemanticContext *ctx = semantic_context_create();

        ASTNode *decl = ast_create_let_declaration("q");
        decl->data.let_decl.type = ast_create_type("QubitSlot");
        decl->data.let_decl.initializer = make_call("ClaimQubit", NULL, 0, 1);
        type_check_let_decl(decl, ctx);

        ASTNode *wh = calloc(1, sizeof(ASTNode));
        wh->type = AST_WHILE_LOOP;
        wh->line = 2;
        wh->data.while_loop.condition = make_boolean(true, 2);
        ASTNode *body = ast_create_block();
        ASTNode *cont = calloc(1, sizeof(ASTNode));
        cont->type = AST_CONTINUE;
        cont->line = 3;
        ast_add_statement(body, cont);
        ASTNode *moved = ast_create_let_declaration("a");
        moved->data.let_decl.type = ast_create_type("QubitSlot");
        moved->data.let_decl.initializer = make_identifier("q", 4);
        ast_add_statement(body, moved);
        wh->data.while_loop.body = body;

        type_check_while_loop(wh, ctx);
        EXPECT(!ctx->has_error);

        ASTNode *state_args[1] = { make_identifier("q", 5) };
        ASTNode *state_call = make_call("QubitState", state_args, 1, 5);
        Type *t = type_check_expression(state_call, ctx);
        EXPECT(!ctx->has_error && type_equals(t, TYPE_INT));

        semantic_context_destroy(ctx);
        ast_destroy(decl);
        ast_destroy(wh);
        ast_destroy(state_call);
    }
}
