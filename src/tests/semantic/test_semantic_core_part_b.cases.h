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

static void
test_arrays_and_enums(void)
{
    printf("\n[arrays_enums]\n");

    TEST("array literal infers Array<Int>");
    {
        SemanticContext *ctx = semantic_context_create();
        ASTNode *arr = calloc(1, sizeof(ASTNode));
        arr->type = AST_ARRAY_LITERAL;
        arr->line = 1;
        arr->data.array_literal.count = 3;
        arr->data.array_literal.elements = calloc(3, sizeof(ASTNode *));
        arr->data.array_literal.elements[0] = make_number(1, 1);
        arr->data.array_literal.elements[1] = make_number(2, 1);
        arr->data.array_literal.elements[2] = make_number(3, 1);

        Type *t = type_check_expression(arr, ctx);
        EXPECT(!ctx->has_error && t != NULL
               && strcmp(t->name, "Array<Int>") == 0);
        semantic_context_destroy(ctx);
        ast_destroy(arr);
    }

    TEST("mixed array literal elements ->error");
    {
        SemanticContext *ctx = semantic_context_create();
        ASTNode *arr = calloc(1, sizeof(ASTNode));
        arr->type = AST_ARRAY_LITERAL;
        arr->line = 1;
        arr->data.array_literal.count = 2;
        arr->data.array_literal.elements = calloc(2, sizeof(ASTNode *));
        arr->data.array_literal.elements[0] = make_number(1, 1);
        arr->data.array_literal.elements[1] = make_string("oops", 1);

        type_check_expression(arr, ctx);
        EXPECT(ctx->has_error);
        semantic_context_destroy(ctx);
        ast_destroy(arr);
    }

    TEST("empty array literal carries unresolved element type");
    {
        SemanticContext *ctx = semantic_context_create();
        ASTNode *arr = calloc(1, sizeof(ASTNode));
        arr->type = AST_ARRAY_LITERAL;
        arr->line = 1;
        arr->data.array_literal.count = 0;
        arr->data.array_literal.elements = NULL;

        Type *t = type_check_expression(arr, ctx);
        EXPECT(!ctx->has_error && type_is_constructed_named(t, "Array")
               && t->data.constructed.arg_count == 1
               && t->data.constructed.args[0] == TYPE_UNKNOWN);
        semantic_context_destroy(ctx);
        ast_destroy(arr);
    }

    TEST("empty array let without annotation reports inference diagnostic");
    {
        SemanticContext *ctx = semantic_context_create();
        ASTNode *arr = calloc(1, sizeof(ASTNode));
        arr->type = AST_ARRAY_LITERAL;
        arr->line = 1;
        arr->data.array_literal.count = 0;
        arr->data.array_literal.elements = NULL;

        ASTNode *decl = ast_create_let_declaration("values");
        decl->data.let_decl.initializer = arr;
        type_check_let_decl(decl, ctx);
        EXPECT(ctx->has_error
               && ctx_has_diagnostic_substring(ctx,
                   "Cannot infer Array<T> from an empty array literal"));
        semantic_context_destroy(ctx);
        ast_destroy(decl);
    }

    TEST("empty array let with annotation is concrete");
    {
        SemanticContext *ctx = semantic_context_create();
        ASTNode *arr = calloc(1, sizeof(ASTNode));
        arr->type = AST_ARRAY_LITERAL;
        arr->line = 1;
        arr->data.array_literal.count = 0;
        arr->data.array_literal.elements = NULL;

        ASTNode *decl = ast_create_let_declaration("values");
        decl->data.let_decl.type = make_generic_type("Array", "String");
        decl->data.let_decl.initializer = arr;
        type_check_let_decl(decl, ctx);
        Symbol *sym = scope_lookup(ctx->scope, "values");
        EXPECT(!ctx->has_error && sym != NULL && sym->type != NULL
               && strcmp(sym->type->name, "Array<String>") == 0);
        semantic_context_destroy(ctx);
        ast_destroy(decl);
    }

    TEST("enum variants are visible as enum-typed identifiers");
    {
        SemanticContext *ctx = semantic_context_create();
        ASTNode *enum_decl = calloc(1, sizeof(ASTNode));
        enum_decl->type = AST_ENUM_DECL;
        enum_decl->line = 1;
        enum_decl->data.enum_decl.name = pergyra_strdup("Color");
        enum_decl->data.enum_decl.variant_count = 2;
        enum_decl->data.enum_decl.variants = calloc(2, sizeof(char *));
        enum_decl->data.enum_decl.variants[0] = pergyra_strdup("Red");
        enum_decl->data.enum_decl.variants[1] = pergyra_strdup("Blue");
        ASTNode *stmts[1] = { enum_decl };
        ASTNode *prog = make_program(stmts, 1);

        type_check_program(prog, ctx);
        Type *t = type_check_expression(make_identifier("Red", 2), ctx);
        EXPECT(!ctx->has_error && t != NULL && strcmp(t->name, "Color") == 0);
        semantic_context_destroy(ctx);
        ast_destroy(prog);
    }
}

static void
test_stdlib_and_io(void)
{
    printf("\n[stdlib_io]\n");

    TEST("StringContains returns Bool with valid args");
    {
        SemanticContext *ctx = semantic_context_create();
        ASTNode *args[2] = { make_string("hello", 1), make_string("ell", 1) };
        Type *t = type_check_expression(make_call("StringContains", args, 2, 1), ctx);
        EXPECT(!ctx->has_error && type_equals(t, TYPE_BOOL));
        semantic_context_destroy(ctx);
    }

    TEST("StringIndexOf returns Int with valid args");
    {
        SemanticContext *ctx = semantic_context_create();
        ASTNode *args[2] = { make_string("hello", 1), make_string("ell", 1) };
        Type *t = type_check_expression(make_call("StringIndexOf", args, 2, 1), ctx);
        EXPECT(!ctx->has_error && type_equals(t, TYPE_INT));
        semantic_context_destroy(ctx);
    }

    TEST("ArrayLength requires Array<T>");
    {
        SemanticContext *ctx = semantic_context_create();
        ASTNode *args[1] = { make_number(42, 1) };
        type_check_expression(make_call("ArrayLength", args, 1, 1), ctx);
        EXPECT(ctx->has_error);
        semantic_context_destroy(ctx);
    }

    TEST("ReadFile requires String path");
    {
        SemanticContext *ctx = semantic_context_create();
        ASTNode *args[1] = { make_number(1, 1) };
        type_check_expression(make_call("ReadFile", args, 1, 1), ctx);
        EXPECT(ctx->has_error);
        semantic_context_destroy(ctx);
    }

    TEST("FileExists returns Bool for String path");
    {
        SemanticContext *ctx = semantic_context_create();
        ASTNode *args[1] = { make_string("input.txt", 1) };
        Type *t = type_check_expression(make_call("FileExists", args, 1, 1), ctx);
        EXPECT(!ctx->has_error && type_equals(t, TYPE_BOOL));
        semantic_context_destroy(ctx);
    }

    TEST("WriteFile accepts String path and data");
    {
        SemanticContext *ctx = semantic_context_create();
        ASTNode *args[2] = { make_string("out.txt", 1), make_string("data", 1) };
        Type *t = type_check_expression(make_call("WriteFile", args, 2, 1), ctx);
        EXPECT(!ctx->has_error && type_equals(t, TYPE_VOID));
        semantic_context_destroy(ctx);
    }

    TEST("ClaimDeviceSlot carries unresolved DeviceSlot<T> without annotation");
    {
        SemanticContext *ctx = semantic_context_create();
        Type *t = type_check_expression(make_call("ClaimDeviceSlot", NULL, 0, 1), ctx);
        EXPECT(!ctx->has_error
            && t != NULL
            && t->kind == TYPE_KIND_CONSTRUCTED
            && t->data.constructed.constructor == TYPE_DEVICE_SLOT
            && t->data.constructed.arg_count == 1
            && t->data.constructed.args[0] == TYPE_UNKNOWN);
        semantic_context_destroy(ctx);
    }

    TEST("ClaimDeviceSlot let without annotation reports inference diagnostic");
    {
        SemanticContext *ctx = semantic_context_create();
        ASTNode *decl = ast_create_let_declaration("device");
        decl->data.let_decl.initializer =
            make_call("ClaimDeviceSlot", NULL, 0, 1);
        type_check_let_decl(decl, ctx);
        EXPECT(ctx->has_error
               && ctx_has_diagnostic_substring(ctx,
                   "Cannot infer DeviceSlot<T> from ClaimDeviceSlot"));
        semantic_context_destroy(ctx);
        ast_destroy(decl);
    }

    TEST("ClaimDeviceSlot let with annotation is concrete");
    {
        SemanticContext *ctx = semantic_context_create();
        ASTNode *decl = ast_create_let_declaration("device");
        decl->data.let_decl.type = make_generic_type("DeviceSlot", "Long");
        decl->data.let_decl.initializer =
            make_call("ClaimDeviceSlot", NULL, 0, 1);
        type_check_let_decl(decl, ctx);
        Symbol *sym = scope_lookup(ctx->scope, "device");
        EXPECT(!ctx->has_error && sym != NULL && sym->type != NULL
               && strcmp(sym->type->name, "DeviceSlot<Long>") == 0);
        semantic_context_destroy(ctx);
        ast_destroy(decl);
    }

    TEST("DeviceRead returns inner type");
    {
        SemanticContext *ctx = semantic_context_create();
        scope_enter(&ctx->scope, SCOPE_GLOBAL);

        Type *args[1] = { TYPE_INT };
        Type *device_type = type_create_constructed(TYPE_DEVICE_SLOT, args, 1);
        scope_declare(ctx->scope,
            symbol_create_variable("dev", device_type, 1, 1));

        ASTNode *call_args[1] = { make_identifier("dev", 1) };
        Type *t = type_check_expression(make_call("DeviceRead", call_args, 1, 1), ctx);
        EXPECT(!ctx->has_error && type_equals(t, TYPE_INT));
        semantic_context_destroy(ctx);
    }

    TEST("SubmitDeviceRead returns RemoteFuture<Int>");
    {
        SemanticContext *ctx = semantic_context_create();
        scope_enter(&ctx->scope, SCOPE_GLOBAL);

        Type *args[1] = { TYPE_INT };
        Type *device_type = type_create_constructed(TYPE_DEVICE_SLOT, args, 1);
        scope_declare(ctx->scope,
            symbol_create_variable("dev", device_type, 1, 1));

        ASTNode *call_args[1] = { make_identifier("dev", 1) };
        Type *t = type_check_expression(make_call("SubmitDeviceRead", call_args, 1, 1), ctx);
        EXPECT(!ctx->has_error
            && t != NULL
            && t->kind == TYPE_KIND_CONSTRUCTED
            && t->data.constructed.constructor == TYPE_REMOTE_FUTURE
            && t->data.constructed.arg_count == 1
            && type_equals(t->data.constructed.args[0], TYPE_INT));
        semantic_context_destroy(ctx);
    }

    TEST("slot sugar let declaration registers Slot symbol");
    {
        SemanticContext *ctx = semantic_context_create();
        ASTNode *decl = ast_create_let_declaration("s");
        decl->data.let_decl.type = make_generic_type("Slot", "Int");
        decl->data.let_decl.initializer = make_number(42, 1);
        type_check_let_decl(decl, ctx);
        Symbol *sym = scope_lookup(ctx->scope, "s");
        EXPECT(!ctx->has_error && sym != NULL && sym->kind == SYMBOL_SLOT);
        semantic_context_destroy(ctx);
        ast_destroy(decl);
    }

    TEST("slot handle alias in let declaration is rejected");
    {
        SemanticContext *ctx = semantic_context_create();
        Symbol *slot_sym = symbol_create_slot(
            "s", type_create_slot(TYPE_INT, false), false, NULL, 1, 1);
        scope_declare(ctx->scope, slot_sym);
        scope_register_slot(ctx->scope, slot_sym);

        ASTNode *decl = ast_create_let_declaration("t");
        decl->data.let_decl.type = make_generic_type("Slot", "Int");
        decl->data.let_decl.initializer = make_identifier("s", 2);
        type_check_let_decl(decl, ctx);

        EXPECT(ctx->has_error);
        semantic_context_destroy(ctx);
        ast_destroy(decl);
    }

    TEST("secure slot handle alias in let declaration is rejected");
    {
        SemanticContext *ctx = semantic_context_create();
        Symbol *slot_sym = symbol_create_slot(
            "ss", type_create_slot(TYPE_INT, true), true, "tok", 1, 1);
        scope_declare(ctx->scope, slot_sym);
        scope_register_slot(ctx->scope, slot_sym);

        ASTNode *decl = ast_create_let_declaration("copy");
        decl->data.let_decl.type = make_generic_type("SecureSlot", "Int");
        decl->data.let_decl.initializer = make_identifier("ss", 2);
        type_check_let_decl(decl, ctx);

        EXPECT(ctx->has_error);
        semantic_context_destroy(ctx);
        ast_destroy(decl);
    }

    TEST("slot sugar assignment from inner value remains valid");
    {
        SemanticContext *ctx = semantic_context_create();
        Symbol *slot_sym = symbol_create_slot(
            "s", type_create_slot(TYPE_INT, false), false, NULL, 1, 1);
        scope_declare(ctx->scope, slot_sym);
        scope_register_slot(ctx->scope, slot_sym);

        ASTNode *assign = ast_create_assignment(make_identifier("s", 2), make_number(7, 2));
        Type *t = type_check_expression(assign, ctx);

        EXPECT(!ctx->has_error && t != NULL && t->kind == TYPE_KIND_SLOT);
        semantic_context_destroy(ctx);
        ast_destroy(assign);
    }

    TEST("slot sugar binary expression auto-reads inner value");
    {
        SemanticContext *ctx = semantic_context_create();
        Symbol *slot_sym = symbol_create_slot(
            "s", type_create_slot(TYPE_INT, false), false, NULL, 1, 1);
        scope_declare(ctx->scope, slot_sym);
        scope_register_slot(ctx->scope, slot_sym);

        ASTNode *expr = ast_create_binary(make_identifier("s", 2),
            (Token){ .type = TOKEN_PLUS }, make_number(1, 2));
        Type *t = type_check_expression(expr, ctx);

        EXPECT(!ctx->has_error && t != NULL && type_equals(t, TYPE_INT));
        semantic_context_destroy(ctx);
        ast_destroy(expr);
    }

    TEST("slot handle assignment from another slot is rejected");
    {
        SemanticContext *ctx = semantic_context_create();
        Symbol *slot_a = symbol_create_slot(
            "a", type_create_slot(TYPE_INT, false), false, NULL, 1, 1);
        Symbol *slot_b = symbol_create_slot(
            "b", type_create_slot(TYPE_INT, false), false, NULL, 1, 1);
        scope_declare(ctx->scope, slot_a);
        scope_register_slot(ctx->scope, slot_a);
        scope_declare(ctx->scope, slot_b);
        scope_register_slot(ctx->scope, slot_b);

        ASTNode *assign = ast_create_assignment(make_identifier("a", 2), make_identifier("b", 2));
        type_check_expression(assign, ctx);

        EXPECT(ctx->has_error
            && ctx_has_diagnostic_substring(ctx, "cannot be copied or rebound with '='"));
        semantic_context_destroy(ctx);
        ast_destroy(assign);
    }
}
