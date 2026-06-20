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

    TEST("empty set let without annotation reports inference diagnostic");
    {
        SemanticContext *ctx = semantic_context_create();
        ASTNode *set = calloc(1, sizeof(ASTNode));
        set->type = AST_SET_LITERAL;
        set->line = 1;
        set->data.set_literal.count = 0;
        set->data.set_literal.elements = NULL;

        ASTNode *decl = ast_create_let_declaration("values");
        decl->data.let_decl.initializer = set;
        type_check_let_decl(decl, ctx);
        EXPECT(ctx->has_error
               && ctx_has_diagnostic_substring(ctx,
                   "Cannot infer Set<T> from an empty set literal"));
        semantic_context_destroy(ctx);
        ast_destroy(decl);
    }

    TEST("empty set let with annotation is concrete");
    {
        SemanticContext *ctx = semantic_context_create();
        ASTNode *set = calloc(1, sizeof(ASTNode));
        set->type = AST_SET_LITERAL;
        set->line = 1;
        set->data.set_literal.count = 0;
        set->data.set_literal.elements = NULL;

        ASTNode *decl = ast_create_let_declaration("values");
        decl->data.let_decl.type = make_generic_type("Set", "String");
        decl->data.let_decl.initializer = set;
        type_check_let_decl(decl, ctx);
        Symbol *sym = scope_lookup(ctx->scope, "values");
        EXPECT(!ctx->has_error && sym != NULL && sym->type != NULL
               && strcmp(sym->type->name, "Set<String>") == 0);
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
