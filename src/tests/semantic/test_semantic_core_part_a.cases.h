static void
test_type_system(void)
{
    printf("\n[type_system]\n");

    TEST("type_equals: Int == Int");
    EXPECT(type_equals(TYPE_INT, TYPE_INT));

    TEST("type_equals: Int != String");
    EXPECT(!type_equals(TYPE_INT, TYPE_STRING));

    TEST("type_is_assignable: Int -> Long widening");
    EXPECT(type_is_assignable(TYPE_INT, TYPE_LONG));

    TEST("type_is_assignable: String -> Int fails");
    EXPECT(!type_is_assignable(TYPE_STRING, TYPE_INT));

    TEST("type_create_slot: name is 'Slot<Int>'");
    Type *slot_int = type_create_slot(TYPE_INT, false);
    EXPECT(strcmp(slot_int->name, "Slot<Int>") == 0);

    TEST("type_create_slot secure: name is 'SecureSlot<Int>'");
    Type *sec_slot = type_create_slot(TYPE_INT, true);
    EXPECT(strcmp(sec_slot->name, "SecureSlot<Int>") == 0);

    TEST("type_equals: Slot<Int> == Slot<Int>");
    Type *slot_int2 = type_create_slot(TYPE_INT, false);
    EXPECT(type_equals(slot_int, slot_int2));

    TEST("type_equals: Slot<Int> != Slot<String>");
    Type *slot_str = type_create_slot(TYPE_STRING, false);
    EXPECT(!type_equals(slot_int, slot_str));

    TEST("function type carries parameter modes");
    {
        Type *params[2] = { TYPE_INT, TYPE_STRING };
        Type *fn_ref = type_create_function(params, 2, TYPE_BOOL);
        Type *fn_default = type_create_function(params, 2, TYPE_BOOL);

        type_function_set_param_mode(fn_ref, 0, PARAM_MODE_REF);

        EXPECT(type_function_param_mode(fn_ref, 0) == PARAM_MODE_REF);
        EXPECT(type_function_param_mode(fn_ref, 1) == PARAM_MODE_DEFAULT);
        EXPECT(!type_equals(fn_ref, fn_default));
    }

    TEST("function type carries parameter escape summaries");
    {
        Type *params[2] = { TYPE_INT, TYPE_STRING };
        Type *fn = type_create_function(params, 2, TYPE_BOOL);

        type_function_set_param_escape_summary(
            fn, 1, SLOT_PARAM_SUMMARY_RETURN_ESCAPE);
        type_function_finish_param_escape_summaries(fn);

        EXPECT(type_function_has_param_escape_summary(fn, 0));
        EXPECT(type_function_param_escape_summary(fn, 0)
               == SLOT_PARAM_SUMMARY_NONE);
        EXPECT(type_function_has_param_escape_summary(fn, 1));
        EXPECT(type_function_param_escape_summary(fn, 1)
               == SLOT_PARAM_SUMMARY_RETURN_ESCAPE);
    }

    TEST("type_infer_expression: identifier lookup returns bound type");
    {
        TypeEnv *env = type_env_create(NULL);
        type_env_add_variable(env, "count", TYPE_INT);
        ASTNode *id = make_identifier("count", 1);
        EXPECT(type_infer_expression(id, env) == TYPE_INT);
        ast_destroy(id);
        type_env_destroy(env);
    }

    TEST("type_infer_expression: array access returns element type");
    {
        Type *args[1] = { TYPE_FLOAT };
        Type *array_float = type_create_constructed(TYPE_ARRAY, args, 1);
        TypeEnv *env = type_env_create(NULL);
        type_env_add_variable(env, "items", array_float);

        ASTNode array_node; memset(&array_node, 0, sizeof(array_node));
        array_node.type = AST_IDENTIFIER;
        array_node.data.identifier.name = "items";

        ASTNode index_node; memset(&index_node, 0, sizeof(index_node));
        index_node.type = AST_NUMBER;
        index_node.data.number.value = 0;

        ASTNode access_node; memset(&access_node, 0, sizeof(access_node));
        access_node.type = AST_ARRAY_ACCESS;
        access_node.data.array_access.array = &array_node;
        access_node.data.array_access.index = &index_node;

        EXPECT(type_infer_expression(&access_node, env) == TYPE_FLOAT);

        free(array_float->data.constructed.args);
        free(array_float->name);
        free(array_float);
        type_env_destroy(env);
    }

    TEST("type_infer_expression: comparison returns Bool");
    {
        ASTNode left; memset(&left, 0, sizeof(left));
        left.type = AST_NUMBER;
        left.data.number.value = 1;

        ASTNode right; memset(&right, 0, sizeof(right));
        right.type = AST_NUMBER;
        right.data.number.value = 2;

        ASTNode expr; memset(&expr, 0, sizeof(expr));
        expr.type = AST_BINARY;
        expr.data.binary.left = &left;
        expr.data.binary.right = &right;
        expr.data.binary.op.type = TOKEN_LESS;

        EXPECT(type_infer_expression(&expr, NULL) == TYPE_BOOL);
    }

    TEST("type_infer_expression: logical operators return Bool");
    {
        ASTNode left; memset(&left, 0, sizeof(left));
        left.type = AST_BOOLEAN;
        left.data.boolean.value = true;

        ASTNode right; memset(&right, 0, sizeof(right));
        right.type = AST_BOOLEAN;
        right.data.boolean.value = false;

        ASTNode expr; memset(&expr, 0, sizeof(expr));
        expr.type = AST_BINARY;
        expr.data.binary.left = &left;
        expr.data.binary.right = &right;
        expr.data.binary.op.type = TOKEN_AND;

        EXPECT(type_infer_expression(&expr, NULL) == TYPE_BOOL);
    }
}

static void
test_symbol_table(void)
{
    printf("\n[symbol_table]\n");

    Scope *root = scope_create(NULL, SCOPE_GLOBAL);

    TEST("scope_declare and lookup in same scope");
    Symbol *sym = symbol_create_variable("x", TYPE_INT, 1, 1);
    scope_declare(root, sym);
    EXPECT(scope_lookup(root, "x") == sym);

    TEST("scope_lookup returns NULL for unknown symbol");
    EXPECT(scope_lookup(root, "y") == NULL);

    TEST("scope_declare: duplicate in same scope returns false");
    Symbol *dup = symbol_create_variable("x", TYPE_STRING, 2, 1);
    EXPECT(!scope_declare(root, dup));
    symbol_destroy(dup);

    TEST("child scope can see parent symbols");
    Scope *child = scope_create(root, SCOPE_BLOCK);
    EXPECT(scope_lookup(child, "x") == sym);

    TEST("shadowing: child declares same name, lookup returns child sym");
    Symbol *shadow = symbol_create_variable("x", TYPE_STRING, 3, 1);
    scope_declare(child, shadow);
    EXPECT(scope_lookup(child, "x") == shadow);
    EXPECT(scope_lookup(root, "x")  == sym); /* Parent unaffected */

    TEST("scope_release_slot: marks slot as RELEASED");
    Symbol *slot = symbol_create_slot("mySlot", type_create_slot(TYPE_INT, false),
                                       false, NULL, 5, 1);
    scope_declare(child, slot);
    EXPECT(slot->slot_info.state == SLOT_STATE_CLAIMED);
    scope_release_slot(child, "mySlot");
    EXPECT(slot->slot_info.state == SLOT_STATE_RELEASED);

    TEST("scope_auto_release_slots: releases all owned slots");
    Scope *with_scope = scope_create(child, SCOPE_WITH);
    Symbol *auto_slot = symbol_create_slot("autoSlot",
                            type_create_slot(TYPE_INT, false),
                            false, NULL, 6, 1);
    scope_declare(with_scope, auto_slot);
    scope_register_slot(with_scope, auto_slot);
    EXPECT(auto_slot->slot_info.state == SLOT_STATE_CLAIMED);
    scope_auto_release_slots(with_scope);
    EXPECT(auto_slot->slot_info.state == SLOT_STATE_RELEASED);

    scope_destroy(with_scope);
    scope_destroy(child);
    scope_destroy(root);
}

static void
test_type_checker_slot_rules(void)
{
    printf("\n[type_checker ->slot rules]\n");

    /* --- R1: Write type mismatch --- */
    TEST("R1: Write String to Slot<Int> ->error");
    {
        SemanticContext *ctx = semantic_context_create();

        /* Register a Slot<Int> named 's' */
        Symbol *slot_sym = symbol_create_slot(
            "s", type_create_slot(TYPE_INT, false), false, NULL, 1, 1);
        scope_declare(ctx->scope, slot_sym);

        /* Write(s, "hello") */
        ASTNode *args[2] = {
            make_identifier("s", 2),
            make_string("hello", 2)
        };
        ASTNode *call = make_call("Write", args, 2, 2);

        type_check_write_slot(call, ctx);
        EXPECT(ctx->has_error);
        semantic_context_destroy(ctx);
    }

    TEST("R1: Write Int to Slot<Int> ->no error");
    {
        SemanticContext *ctx = semantic_context_create();
        Symbol *slot_sym = symbol_create_slot(
            "s", type_create_slot(TYPE_INT, false), false, NULL, 1, 1);
        scope_declare(ctx->scope, slot_sym);

        ASTNode *args[2] = {
            make_identifier("s", 2),
            make_number(42, 2)
        };
        ASTNode *call = make_call("Write", args, 2, 2);

        type_check_write_slot(call, ctx);
        EXPECT(!ctx->has_error);
        semantic_context_destroy(ctx);
    }

    TEST("logical operators require Bool operands and return Bool");
    {
        SemanticContext *ctx = semantic_context_create();
        ASTNode *ok_expr = ast_create_binary(make_boolean(true, 2),
            (Token){ .type = TOKEN_AND }, make_boolean(false, 2));
        Type *ok_type = type_check_expression(ok_expr, ctx);
        EXPECT(!ctx->has_error && type_equals(ok_type, TYPE_BOOL));
        semantic_context_destroy(ctx);
        ast_destroy(ok_expr);

        ctx = semantic_context_create();
        ASTNode *bad_expr = ast_create_binary(make_number(1, 3),
            (Token){ .type = TOKEN_AND }, make_number(2, 3));
        Type *bad_type = type_check_expression(bad_expr, ctx);
        EXPECT(ctx->has_error && type_equals(bad_type, TYPE_BOOL)
            && ctx_has_diagnostic_substring(ctx,
                "Logical operator requires Bool operands"));
        semantic_context_destroy(ctx);
        ast_destroy(bad_expr);
    }

    TEST("Bool cannot be cast to numeric cursor delta");
    {
        const char *source =
            "func Main() -> Void {\n"
            "    let delta: Int = (1 == 1) as Int;\n"
            "    Log(delta);\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticResult *result = semantic_analyze(program);

        EXPECT(!parser_has_error(parser));
        EXPECT(result != NULL && result->error_count > 0);
        EXPECT(ctx_has_diagnostic_substring_from_result(result,
            "Cast to Int requires a numeric operand"));

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("ClaimSlot<T> let inference preserves generic payload type");
    {
        const char *source =
            "func Main() -> Void {\n"
            "    let s = ClaimSlot<String>();\n"
            "    Write(s, \"ok\");\n"
            "    return;\n"
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

    TEST("ClaimSlot without annotation or type argument is rejected");
    {
        const char *source =
            "func Main() -> Void {\n"
            "    let s = ClaimSlot();\n"
            "    return;\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticResult *result = semantic_analyze(program);

        EXPECT(!parser_has_error(parser));
        EXPECT(result != NULL
            && result->error_count > 0
            && ctx_has_diagnostic_substring_from_result(
                result, "Cannot infer Slot<T> from ClaimSlot"));

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    /* --- R2: SecureSlot requires token --- */
    TEST("R2: Write to SecureSlot without token ->error");
    {
        SemanticContext *ctx = semantic_context_create();
        Symbol *slot_sym = symbol_create_slot(
            "ss", type_create_slot(TYPE_INT, true),
            true, "tok", 1, 1);
        scope_declare(ctx->scope, slot_sym);

        ASTNode *args[2] = {
            make_identifier("ss", 2),
            make_number(42, 2)
        };
        ASTNode *call = make_call("Write", args, 2, 2);

        type_check_write_slot(call, ctx);
        EXPECT(ctx->has_error);
        semantic_context_destroy(ctx);
    }

    TEST("R2: Write to SecureSlot with correct token ->no error");
    {
        SemanticContext *ctx = semantic_context_create();
        Symbol *slot_sym = symbol_create_slot(
            "ss", type_create_slot(TYPE_INT, true),
            true, "tok", 1, 1);
        scope_declare(ctx->scope, slot_sym);

        Symbol *tok_sym = symbol_create_token("tok", "ss", 1, 2);
        scope_declare(ctx->scope, tok_sym);

        ASTNode *args[3] = {
            make_identifier("ss", 2),
            make_number(42, 2),
            make_identifier("tok", 2)
        };
        ASTNode *call = make_call("Write", args, 3, 2);

        type_check_write_slot(call, ctx);
        EXPECT(!ctx->has_error);
        semantic_context_destroy(ctx);
    }

    /* --- R3: wrong token --- */
    TEST("R3: Write to SecureSlot with wrong token ->error");
    {
        SemanticContext *ctx = semantic_context_create();
        Symbol *slot_sym = symbol_create_slot(
            "ss", type_create_slot(TYPE_INT, true),
            true, "tokA", 1, 1);
        scope_declare(ctx->scope, slot_sym);

        ASTNode *args[3] = {
            make_identifier("ss", 2),
            make_number(42, 2),
            make_identifier("tokB", 2)  /* wrong token */
        };
        ASTNode *call = make_call("Write", args, 3, 2);

        type_check_write_slot(call, ctx);
        EXPECT(ctx->has_error);
        semantic_context_destroy(ctx);
    }

    /* --- R4: write to released slot --- */
    TEST("R4: Write to released slot ->error");
    {
        SemanticContext *ctx = semantic_context_create();
        Symbol *slot_sym = symbol_create_slot(
            "s", type_create_slot(TYPE_INT, false), false, NULL, 1, 1);
        slot_sym->slot_info.state = SLOT_STATE_RELEASED;
        scope_declare(ctx->scope, slot_sym);

        ASTNode *args[2] = {
            make_identifier("s", 2),
            make_number(42, 2)
        };
        ASTNode *call = make_call("Write", args, 2, 2);

        type_check_write_slot(call, ctx);
        EXPECT(ctx->has_error);
        semantic_context_destroy(ctx);
    }

    /* --- Read from released slot --- */
    TEST("Read from released slot ->error");
    {
        SemanticContext *ctx = semantic_context_create();
        Symbol *slot_sym = symbol_create_slot(
            "s", type_create_slot(TYPE_INT, false), false, NULL, 1, 1);
        slot_sym->slot_info.state = SLOT_STATE_RELEASED;
        scope_declare(ctx->scope, slot_sym);

        ASTNode *args[1] = { make_identifier("s", 3) };
        ASTNode *call    = make_call("Read", args, 1, 3);

        Type *t = type_check_read_slot(call, ctx);
        EXPECT(ctx->has_error);
        (void)t;
        semantic_context_destroy(ctx);
    }

    /* --- Release twice --- */
    TEST("Release already-released slot ->error");
    {
        SemanticContext *ctx = semantic_context_create();
        Symbol *slot_sym = symbol_create_slot(
            "s", type_create_slot(TYPE_INT, false), false, NULL, 1, 1);
        scope_declare(ctx->scope, slot_sym);

        ASTNode *args[1] = { make_identifier("s", 4) };
        ASTNode *call    = make_call("Release", args, 1, 4);

        /* First release: OK */
        type_check_release_slot(call, ctx);
        EXPECT(!ctx->has_error);

        /* Second release: error */
        type_check_release_slot(call, ctx);
        EXPECT(ctx->has_error);

        semantic_context_destroy(ctx);
    }

    TEST("ReadView<T> reads but does not own");
    {
        SemanticContext *ctx = semantic_context_create();
        Symbol *slot_sym = symbol_create_slot(
            "s", type_create_slot(TYPE_INT, false), false, NULL, 1, 1);
        scope_declare(ctx->scope, slot_sym);

        ASTNode *decl = ast_create_let_declaration("rv");
        ASTNode *view_args[1] = { make_identifier("s", 1) };
        decl->data.let_decl.type = make_generic_type("ReadView", "Int");
        decl->data.let_decl.initializer = make_call("ViewRead", view_args, 1, 1);
        type_check_let_decl(decl, ctx);
        EXPECT(!ctx->has_error);

        ASTNode *read_args[1] = { make_identifier("rv", 2) };
        ASTNode *read_call = make_call("Read", read_args, 1, 2);
        Type *t = type_check_read_slot(read_call, ctx);
        EXPECT(!ctx->has_error && type_equals(t, TYPE_INT));

        semantic_context_destroy(ctx);
        ast_destroy(decl);
        ast_destroy(read_call);
    }

    TEST("Write through ReadView<T> ->error");
    {
        SemanticContext *ctx = semantic_context_create();
        Symbol *slot_sym = symbol_create_slot(
            "s", type_create_slot(TYPE_INT, false), false, NULL, 1, 1);
        scope_declare(ctx->scope, slot_sym);

        ASTNode *decl = ast_create_let_declaration("rv");
        ASTNode *view_args[1] = { make_identifier("s", 1) };
        decl->data.let_decl.type = make_generic_type("ReadView", "Int");
        decl->data.let_decl.initializer = make_call("ViewRead", view_args, 1, 1);
        type_check_let_decl(decl, ctx);
        EXPECT(!ctx->has_error);

        ASTNode *write_args[2] = { make_identifier("rv", 2), make_number(42, 2) };
        ASTNode *write_call = make_call("Write", write_args, 2, 2);
        type_check_write_slot(write_call, ctx);
        EXPECT(ctx->has_error
            && ctx_has_diagnostic_substring(ctx, "Cannot write through ReadView"));

        semantic_context_destroy(ctx);
        ast_destroy(decl);
        ast_destroy(write_call);
    }

    TEST("WriteView<T> writes but cannot be read");
    {
        SemanticContext *ctx = semantic_context_create();
        Symbol *slot_sym = symbol_create_slot(
            "s", type_create_slot(TYPE_INT, false), false, NULL, 1, 1);
        scope_declare(ctx->scope, slot_sym);

        ASTNode *decl = ast_create_let_declaration("wv");
        ASTNode *view_args[1] = { make_identifier("s", 1) };
        decl->data.let_decl.type = make_generic_type("WriteView", "Int");
        decl->data.let_decl.initializer = make_call("ViewWrite", view_args, 1, 1);
        type_check_let_decl(decl, ctx);
        EXPECT(!ctx->has_error);

        ASTNode *write_args[2] = { make_identifier("wv", 2), make_number(7, 2) };
        ASTNode *write_call = make_call("Write", write_args, 2, 2);
        type_check_write_slot(write_call, ctx);
        EXPECT(!ctx->has_error);

        ASTNode *read_args[1] = { make_identifier("wv", 3) };
        ASTNode *read_call = make_call("Read", read_args, 1, 3);
        Type *t = type_check_read_slot(read_call, ctx);
        EXPECT(ctx->has_error
            && t == TYPE_UNKNOWN
            && ctx_has_diagnostic_substring(ctx, "Cannot read through WriteView"));

        semantic_context_destroy(ctx);
        ast_destroy(decl);
        ast_destroy(write_call);
        ast_destroy(read_call);
    }

    TEST("Release(ReadView<T>) ->error");
    {
        SemanticContext *ctx = semantic_context_create();
        Symbol *slot_sym = symbol_create_slot(
            "s", type_create_slot(TYPE_INT, false), false, NULL, 1, 1);
        scope_declare(ctx->scope, slot_sym);

        ASTNode *decl = ast_create_let_declaration("rv");
        ASTNode *view_args[1] = { make_identifier("s", 1) };
        decl->data.let_decl.type = make_generic_type("ReadView", "Int");
        decl->data.let_decl.initializer = make_call("ViewRead", view_args, 1, 1);
        type_check_let_decl(decl, ctx);
        EXPECT(!ctx->has_error);

        ASTNode *release_args[1] = { make_identifier("rv", 2) };
        ASTNode *release_call = make_call("Release", release_args, 1, 2);
        type_check_release_slot(release_call, ctx);
        EXPECT(ctx->has_error
            && ctx_has_diagnostic_substring(ctx, "views are non-owning"));

        semantic_context_destroy(ctx);
        ast_destroy(decl);
        ast_destroy(release_call);
    }

    TEST("ReadView on SecureSlot<T> reads with implicit capability");
    {
        SemanticContext *ctx = semantic_context_create();
        Symbol *slot_sym = symbol_create_slot(
            "ss", type_create_slot(TYPE_INT, true), true, "ss_token", 1, 1);
        scope_declare(ctx->scope, slot_sym);
        Symbol *token_sym = symbol_create_token("ss_token", "ss", 1, 1);
        if (token_sym != NULL) {
            Type *token_args[1] = { TYPE_INT };
            token_sym->type =
                type_create_constructed(TYPE_TOKEN, token_args, 1);
        }
        scope_declare(ctx->scope, token_sym);

        ASTNode *decl = ast_create_let_declaration("srv");
        ASTNode *view_args[1] = { make_identifier("ss", 1) };
        decl->data.let_decl.type = make_generic_type("ReadView", "Int");
        decl->data.let_decl.initializer = make_call("ViewRead", view_args, 1, 1);
        type_check_let_decl(decl, ctx);
        EXPECT(!ctx->has_error);

        ASTNode *read_args[1] = { make_identifier("srv", 2) };
        ASTNode *read_call = make_call("Read", read_args, 1, 2);
        Type *t = type_check_read_slot(read_call, ctx);
        EXPECT(!ctx->has_error && type_equals(t, TYPE_INT));

        semantic_context_destroy(ctx);
        ast_destroy(decl);
        ast_destroy(read_call);
    }

    TEST("ViewRead on SecureSlot<T> without paired token in scope is rejected");
    {
        SemanticContext *ctx = semantic_context_create();
        Symbol *slot_sym = symbol_create_slot(
            "ss", type_create_slot(TYPE_INT, true), true, "ss_token", 1, 1);
        scope_declare(ctx->scope, slot_sym);

        ASTNode *decl = ast_create_let_declaration("srv");
        ASTNode *view_args[1] = { make_identifier("ss", 1) };
        decl->data.let_decl.type = make_generic_type("ReadView", "Int");
        decl->data.let_decl.initializer = make_call("ViewRead", view_args, 1, 1);
        type_check_let_decl(decl, ctx);
        EXPECT(ctx->has_error);
        EXPECT(ctx_has_diagnostic_code(ctx, PGY_CODE_SEM_PIN_TOKEN_INVALID));
        EXPECT(ctx_has_diagnostic_substring(ctx,
            "no reachable paired token"));

        semantic_context_destroy(ctx);
        ast_destroy(decl);
    }

    TEST("ViewWrite on SecureSlot<T> without paired token in scope is rejected");
    {
        SemanticContext *ctx = semantic_context_create();
        Symbol *slot_sym = symbol_create_slot(
            "ss", type_create_slot(TYPE_INT, true), true, "ss_token", 1, 1);
        scope_declare(ctx->scope, slot_sym);

        ASTNode *decl = ast_create_let_declaration("swv");
        ASTNode *view_args[1] = { make_identifier("ss", 1) };
        decl->data.let_decl.type = make_generic_type("WriteView", "Int");
        decl->data.let_decl.initializer = make_call("ViewWrite", view_args, 1, 1);
        type_check_let_decl(decl, ctx);
        EXPECT(ctx->has_error);
        EXPECT(ctx_has_diagnostic_code(ctx, PGY_CODE_SEM_PIN_TOKEN_INVALID));

        semantic_context_destroy(ctx);
        ast_destroy(decl);
    }

    TEST("MoveToken<T> materializes into a new owning Slot<T>");
    {
        SemanticContext *ctx = semantic_context_create();
        Symbol *slot_sym = symbol_create_slot(
            "s", type_create_slot(TYPE_INT, false), false, NULL, 1, 1);
        scope_declare(ctx->scope, slot_sym);

        ASTNode *move_decl = ast_create_let_declaration("mt");
        ASTNode *move_args[1] = { make_identifier("s", 1) };
        move_decl->data.let_decl.type = make_generic_type("MoveToken", "Int");
        move_decl->data.let_decl.initializer = make_call("Move", move_args, 1, 1);
        type_check_let_decl(move_decl, ctx);
        EXPECT(!ctx->has_error);

        ASTNode *slot_decl = ast_create_let_declaration("dst");
        slot_decl->data.let_decl.type = make_generic_type("Slot", "Int");
        slot_decl->data.let_decl.initializer = make_identifier("mt", 2);
        type_check_let_decl(slot_decl, ctx);
        EXPECT(!ctx->has_error);

        ASTNode *read_src_args[1] = { make_identifier("s", 3) };
        ASTNode *read_src = make_call("Read", read_src_args, 1, 3);
        type_check_read_slot(read_src, ctx);
        EXPECT(ctx->has_error
            && ctx_has_diagnostic_substring(ctx, "released slot"));

        semantic_context_destroy(ctx);
        ast_destroy(move_decl);
        ast_destroy(slot_decl);
        ast_destroy(read_src);
    }
}
