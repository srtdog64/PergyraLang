static void
test_shared_memory_features(void)
{
    printf("\n[shared_memory]\n");

    TEST("Rc<Int> annotation resolves to constructed type");
    {
        SemanticContext *ctx = semantic_context_create();
        ASTNode *rc_type = make_generic_type("Rc", "Int");
        Type *resolved = resolve_type_node(rc_type, ctx);

        EXPECT(resolved->kind == TYPE_KIND_CONSTRUCTED
               && strcmp(resolved->name, "Rc<Int>") == 0);

        semantic_context_destroy(ctx);
        ast_destroy(rc_type);
    }

    TEST("RcClone returns same Rc<T> type");
    {
        SemanticContext *ctx = semantic_context_create();
        Type *args[1] = { TYPE_INT };
        Type *rc_type = type_create_constructed(TYPE_RC, args, 1);
        scope_declare(ctx->scope,
            symbol_create_variable("shared", rc_type, 1, 1));

        ASTNode *call = make_call("RcClone", (ASTNode *[]){ make_identifier("shared", 2) }, 1, 2);
        Type *resolved = type_check_expression(call, ctx);

        EXPECT(type_equals(resolved, rc_type) && !ctx->has_error);

        semantic_context_destroy(ctx);
        ast_destroy(call);
    }

    TEST("RcDowngrade returns Weak<T>");
    {
        SemanticContext *ctx = semantic_context_create();
        Type *args[1] = { TYPE_INT };
        Type *rc_type = type_create_constructed(TYPE_RC, args, 1);
        scope_declare(ctx->scope,
            symbol_create_variable("shared", rc_type, 1, 1));

        ASTNode *call = make_call("RcDowngrade", (ASTNode *[]){ make_identifier("shared", 2) }, 1, 2);
        Type *resolved = type_check_expression(call, ctx);

        EXPECT(resolved->kind == TYPE_KIND_CONSTRUCTED
               && strcmp(resolved->name, "Weak<Int>") == 0
               && !ctx->has_error);

        semantic_context_destroy(ctx);
        ast_destroy(call);
    }

    TEST("Rc beta stable subset rejects non-primitive payloads");
    {
        SemanticContext *ctx = semantic_context_create();
        Type *hero_type = type_create_primitive("Hero", 0, false);
        Type *args[1] = { hero_type };
        Type *rc_type = type_create_constructed(TYPE_RC, args, 1);
        scope_declare(ctx->scope,
            symbol_create_variable("shared", rc_type, 1, 1));

        ASTNode *call = make_call("RcGet", (ASTNode *[]){ make_identifier("shared", 2) }, 1, 2);
        Type *resolved = type_check_expression(call, ctx);

        EXPECT(resolved == TYPE_UNKNOWN && ctx->has_error);
        EXPECT(ctx_has_diagnostic_substring(ctx,
            "beta-stable shared ownership supports only Int, Long, Float, Double, Bool, or String payloads"));

        semantic_context_destroy(ctx);
        ast_destroy(call);
    }

    TEST("AllocatorPool returns Allocator type");
    {
        SemanticContext *ctx = semantic_context_create();
        ASTNode *call = make_call("AllocatorPool", (ASTNode *[]){ make_number(1024, 1) }, 1, 1);
        Type *resolved = type_check_expression(call, ctx);

        EXPECT(resolved == TYPE_ALLOCATOR && !ctx->has_error);

        semantic_context_destroy(ctx);
        ast_destroy(call);
    }

    TEST("Box<Array<Int>> let declaration accepts BoxArray initializer");
    {
        SemanticContext *ctx = semantic_context_create();
        ASTNode *array_type = make_generic_type("Array", "Int");
        ASTNode *boxed_array = make_generic_type_from_node("Box", array_type);
        ASTNode *call = make_call("BoxArray", (ASTNode *[]){ make_number(64, 1) }, 1, 1);
        ASTNode *decl = ast_create_let_declaration("storage");
        decl->data.let_decl.type = boxed_array;
        decl->data.let_decl.initializer = call;

        type_check_let_decl(decl, ctx);
        EXPECT(!ctx->has_error);

        semantic_context_destroy(ctx);
        ast_destroy(decl);
    }

    TEST("BoxGet returns inner class object type");
    {
        const char *source =
            "class Vec2 {\n"
            "    let x: Int;\n"
            "    let y: Int;\n"
            "}\n"
            "func Main() -> Void {\n"
            "    let handle: Box<Vec2> = Box(Vec2(3, 7));\n"
            "    let value: Vec2 = BoxGet(handle);\n"
            "    Log(value.x);\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticResult *result = semantic_analyze(program);

        EXPECT(!parser_has_error(parser));
        if (result != NULL && result->error_count > 0)
            semantic_result_print(result);
        EXPECT(result != NULL && result->error_count == 0);

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("BoxSet requires assignable inner value");
    {
        SemanticContext *ctx = semantic_context_create();
        Type *args[1] = { TYPE_INT };
        Type *box_type = type_create_constructed(TYPE_BOX, args, 1);
        scope_declare(ctx->scope,
            symbol_create_variable("boxed", box_type, 1, 1));

        ASTNode *call = make_call("BoxSet",
            (ASTNode *[]){ make_identifier("boxed", 2), make_number(42, 2) }, 2, 2);
        Type *resolved = type_check_expression(call, ctx);

        EXPECT(resolved == TYPE_VOID && !ctx->has_error);

        semantic_context_destroy(ctx);
        ast_destroy(call);
    }

    TEST("BoxIsValid returns Bool");
    {
        SemanticContext *ctx = semantic_context_create();
        Type *args[1] = { TYPE_STRING };
        Type *box_type = type_create_constructed(TYPE_BOX, args, 1);
        scope_declare(ctx->scope,
            symbol_create_variable("boxed", box_type, 1, 1));

        ASTNode *call = make_call("BoxIsValid",
            (ASTNode *[]){ make_identifier("boxed", 2) }, 1, 2);
        Type *resolved = type_check_expression(call, ctx);

        EXPECT(resolved == TYPE_BOOL && !ctx->has_error);

        semantic_context_destroy(ctx);
        ast_destroy(call);
    }

    TEST("Box<class> can be returned and passed explicitly");
    {
        const char *source =
            "class Vec2 {\n"
            "    let x: Int;\n"
            "    let y: Int;\n"
            "}\n"
            "func MakeVec() -> Box<Vec2> {\n"
            "    return Box(Vec2(1, 2));\n"
            "}\n"
            "func SumX(cell: Box<Vec2>) -> Int {\n"
            "    return BoxGet(cell).x;\n"
            "}\n"
            "func Main() -> Void {\n"
            "    let handle: Box<Vec2> = MakeVec();\n"
            "    Log(SumX(handle));\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        SemanticResult *result = semantic_analyze(program);

        EXPECT(!parser_has_error(parser));
        if (result != NULL && result->error_count > 0)
            semantic_result_print(result);
        EXPECT(result != NULL && result->error_count == 0);

        semantic_result_destroy(result);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

