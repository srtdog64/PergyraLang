static void
test_codebuf(void)
{
    printf("\n[codebuf]\n");

    TEST("write simple string");
    {
        CodeBuf *b = codebuf_create();
        codebuf_write(b, "hello");
        EXPECT(strcmp(b->data, "hello") == 0 && b->len == 5);
        codebuf_destroy(b);
    }

    TEST("write formatted string");
    {
        CodeBuf *b = codebuf_create();
        codebuf_write(b, "int x = %d;", 42);
        EXPECT(strcmp(b->data, "int x = 42;") == 0);
        codebuf_destroy(b);
    }

    TEST("write triggers growth beyond initial capacity");
    {
        CodeBuf *b = codebuf_create();
        for (int i = 0; i < 1000; i++)
            codebuf_write(b, "a");
        EXPECT(b->len == 1000);
        codebuf_destroy(b);
    }

    TEST("multiple writes concatenate correctly");
    {
        CodeBuf *b = codebuf_create();
        codebuf_write(b, "foo");
        codebuf_write(b, "bar");
        codebuf_write(b, "baz");
        EXPECT(strcmp(b->data, "foobarbaz") == 0);
        codebuf_destroy(b);
    }
}

/* -----------------------------------------------------------------
 * Tests: type mapping
 * ----------------------------------------------------------------- */

static bool
expect_type_to_c_copy(const char *input, const char *expected)
{
    char out[256];

    return pergyra_type_to_c_copy(input, out, sizeof(out))
        && strcmp(out, expected) == 0;
}

static bool
expect_type_to_c_rejects(const char *input)
{
    char out[256];

    out[0] = 'x';
    return !pergyra_type_to_c_copy(input, out, sizeof(out));
}

static void
test_type_mapping(void)
{
    printf("\n[type_mapping]\n");

    TEST("Int -> int32_t");
    EXPECT(expect_type_to_c_copy("Int", "int32_t"));

    TEST("Long -> int64_t");
    EXPECT(expect_type_to_c_copy("Long", "int64_t"));

    TEST("Float -> float");
    EXPECT(expect_type_to_c_copy("Float", "float"));

    TEST("Bool -> bool");
    EXPECT(expect_type_to_c_copy("Bool", "bool"));

    TEST("String -> char*");
    EXPECT(expect_type_to_c_copy("String", "char*"));

    TEST("Void -> void");
    EXPECT(expect_type_to_c_copy("Void", "void"));

    TEST("type fact policy rejects Unknown sentinel without rejecting names");
    {
        EXPECT(!transpiler_type_name_is_concrete_fact("Unknown"));
        EXPECT(!transpiler_type_name_is_concrete_fact("Option<Unknown>"));
        EXPECT(!transpiler_type_name_is_concrete_fact(
            "Result<Int, Unknown>"));
        EXPECT(transpiler_type_name_is_concrete_fact("UnknownError"));
        EXPECT(transpiler_type_name_is_concrete_fact(
            "Result<Int, UnknownError>"));
    }

    TEST("missing primitive type name fails closed");
    EXPECT(pergyra_primitive_to_c(NULL) == NULL);

    TEST("missing AST type name render fails closed");
    {
        char *rendered = render_type_name_in_ctx(NULL, NULL);

        EXPECT(rendered == NULL);
        free(rendered);
    }

    TEST("malformed AST type name render fails closed");
    {
        ASTNode malformed;
        char *rendered;

        memset(&malformed, 0, sizeof(malformed));
        malformed.type = AST_TYPE;
        rendered = render_type_name_in_ctx(NULL, &malformed);

        EXPECT(rendered == NULL);
        free(rendered);
    }

    TEST("Slot<Int> -> PgySlot_Int");
    EXPECT(expect_type_to_c_copy("Slot<Int>", "PgySlot_Int"));

    TEST("Slot<String> -> PgySlot_String");
    EXPECT(expect_type_to_c_copy("Slot<String>", "PgySlot_String"));

    TEST("Slot<Vec2> -> PgySlot_Vec2");
    EXPECT(expect_type_to_c_copy("Slot<Vec2>", "PgySlot_Vec2"));

    TEST("SecureSlot<Int> -> PgySecureSlot_Int");
    EXPECT(expect_type_to_c_copy("SecureSlot<Int>", "PgySecureSlot_Int"));

    TEST("SecureSlot<Vec2> -> PgySecureSlot_Vec2");
    EXPECT(expect_type_to_c_copy("SecureSlot<Vec2>", "PgySecureSlot_Vec2"));

    TEST("Array<Vertex> -> PgyArray_Vertex");
    EXPECT(expect_type_to_c_copy("Array<Vertex>", "PgyArray_Vertex"));

    TEST("Slice<Vertex> -> PgySlice_Vertex");
    EXPECT(expect_type_to_c_copy("Slice<Vertex>", "PgySlice_Vertex"));

    TEST("List<Vertex> -> PgyList_Vertex");
    EXPECT(expect_type_to_c_copy("List<Vertex>", "PgyList_Vertex"));

    TEST("Queue<Vertex> -> PgyQueue_Vertex");
    EXPECT(expect_type_to_c_copy("Queue<Vertex>", "PgyQueue_Vertex"));

    TEST("Channel<Int> -> PgyChannel_Int");
    EXPECT(expect_type_to_c_copy("Channel<Int>", "PgyChannel_Int"));

    TEST("Channel<String> -> PgyChannel_String");
    EXPECT(expect_type_to_c_copy("Channel<String>", "PgyChannel_String"));

    TEST("Rc<Int> -> PgyRc_Int");
    EXPECT(expect_type_to_c_copy("Rc<Int>", "PgyRc_Int"));

    TEST("Weak<Int> -> PgyWeak_Int");
    EXPECT(expect_type_to_c_copy("Weak<Int>", "PgyWeak_Int"));

    TEST("Allocator -> PgyAllocator");
    EXPECT(expect_type_to_c_copy("Allocator", "PgyAllocator"));

    TEST("Box<Array<Int>> -> PgyBoxArray_Int");
    EXPECT(expect_type_to_c_copy("Box<Array<Int>>", "PgyBoxArray_Int"));

    TEST("pergyra_type_to_c_copy preserves rendered type across later mapping");
    {
        char copied[128];
        bool copied_ok = pergyra_type_to_c_copy("Array<Vertex>",
            copied, sizeof(copied));

        {
            char later[128];
            (void)pergyra_type_to_c_copy("Slot<Int>", later, sizeof(later));
        }
        EXPECT(copied_ok && strcmp(copied, "PgyArray_Vertex") == 0);
    }

    TEST("pergyra_type_to_c_copy fails closed on too-small output buffer");
    {
        char copied[4] = {'x', 'x', 'x', '\0'};

        EXPECT(!pergyra_type_to_c_copy("Array<Vertex>",
            copied, sizeof(copied)));
        EXPECT(copied[0] == '\0');
    }

    TEST("pergyra_type_to_c_copy fails closed on missing type name");
    {
        char copied[32] = {'x', '\0'};

        EXPECT(!pergyra_type_to_c_copy(NULL, copied, sizeof(copied)));
        EXPECT(copied[0] == '\0');
    }

    TEST("slot_ref_expr fails closed on missing slot expression");
    {
        char *slot_ref = slot_ref_expr(NULL, "slot", NULL);

        EXPECT(slot_ref == NULL);
        free(slot_ref);
    }

    TEST("typed declarator fails closed on malformed AST type");
    {
        ASTNode malformed;
        char *decl;

        memset(&malformed, 0, sizeof(malformed));
        malformed.type = AST_TYPE;
        decl = pergyra_ast_typed_declarator_in_ctx(NULL, &malformed, "value");

        EXPECT(decl == NULL);
        free(decl);
    }

    TEST("function signature declarator fails closed on malformed return type");
    {
        ASTNode malformed;
        char *decl;

        memset(&malformed, 0, sizeof(malformed));
        malformed.type = AST_TYPE;
        decl = pergyra_func_signature_declarator_in_ctx(NULL,
            &malformed, "Fn", "void");

        EXPECT(decl == NULL);
        free(decl);
    }

    TEST("event handler declarator fails closed on malformed parameter type");
    {
        ASTNode *handler_type = ast_create_event_handler_type();
        ASTNode *malformed = calloc(1, sizeof(ASTNode));
        char *decl;

        malformed->type = AST_TYPE;
        handler_type->data.event_handler_type.param_types =
            calloc(1, sizeof(ASTNode *));
        handler_type->data.event_handler_type.param_types[0] = malformed;
        handler_type->data.event_handler_type.param_count = 1;
        handler_type->data.event_handler_type.param_capacity = 1;

        decl = pergyra_ast_typed_declarator_in_ctx(NULL, handler_type, "cb");

        EXPECT(decl == NULL);
        free(decl);
        ast_destroy(handler_type);
    }

    TEST("event handler type-to-C copy requires declarator owner");
    {
        ASTNode *handler_type = ast_create_event_handler_type();
        char copied[32] = {'x', '\0'};
        char *decl;

        handler_type->data.event_handler_type.param_types =
            calloc(1, sizeof(ASTNode *));
        handler_type->data.event_handler_type.param_types[0] =
            make_type_node("Int");
        handler_type->data.event_handler_type.param_count = 1;
        handler_type->data.event_handler_type.param_capacity = 1;
        handler_type->data.event_handler_type.return_type =
            make_type_node("Bool");

        EXPECT(!pergyra_ast_type_to_c_copy_in_ctx(NULL,
            handler_type, copied, sizeof(copied)));
        EXPECT(copied[0] == '\0');

        decl = pergyra_ast_typed_declarator_in_ctx(NULL, handler_type, "cb");
        EXPECT(strcmp(decl, "bool (*cb)(int32_t)") == 0);

        free(decl);
        ast_destroy(handler_type);
    }

    TEST("let type registry skips inferred Unknown facts");
    {
        TranspilerCtx *ctx = transpiler_ctx_create();
        ASTNode *unknown_call = make_call("MissingCallable", NULL, 0, 1);

        transpiler_register_let_type_after_emit(ctx, "x",
            unknown_call, NULL);

        EXPECT(lookup_typed_var(ctx, "x") == NULL);

        ast_destroy(unknown_call);
        transpiler_ctx_destroy(ctx);
    }

    TEST("let type registry skips generic Unknown sentinel wrappers");
    {
        TranspilerCtx *ctx = transpiler_ctx_create();
        ASTNode *none_call = make_call("None", NULL, 0, 1);

        transpiler_register_let_type_after_emit(ctx, "opt",
            none_call, NULL);
        transpiler_register_let_type_after_emit(ctx, "future",
            NULL, pergyra_strdup("Future<Unknown>"));

        EXPECT(lookup_typed_var(ctx, "opt") == NULL);
        EXPECT(lookup_typed_var(ctx, "future") == NULL);

        ast_destroy(none_call);
        transpiler_ctx_destroy(ctx);
    }

    TEST("let type registry keeps user types containing Unknown as a prefix");
    {
        TranspilerCtx *ctx = transpiler_ctx_create();
        const char *registered;

        transpiler_register_let_type_after_emit(ctx, "err",
            NULL, pergyra_strdup("UnknownError"));

        registered = lookup_typed_var(ctx, "err");
        EXPECT(registered != NULL && strcmp(registered, "UnknownError") == 0);

        transpiler_ctx_destroy(ctx);
    }

    TEST("Array<Unknown> fails closed");
    EXPECT(expect_type_to_c_rejects("Array<Unknown>"));

    TEST("Array<Unknown > fails closed");
    EXPECT(expect_type_to_c_rejects("Array<Unknown >"));

    TEST("HashMap<String, Unknown> fails closed");
    EXPECT(expect_type_to_c_rejects("HashMap<String, Unknown>"));

    TEST("Channel<Bool> fails closed without runtime ABI");
    EXPECT(expect_type_to_c_rejects("Channel<Bool>"));

    TEST("Option<Void> fails closed");
    EXPECT(expect_type_to_c_rejects("Option<Void>"));

    TEST("Result<Void> fails closed");
    EXPECT(expect_type_to_c_rejects("Result<Void>"));

    TEST("Box<Array<Unknown>> fails closed");
    EXPECT(expect_type_to_c_rejects("Box<Array<Unknown>>"));

    TEST("Box<Array<UnknownError>> keeps user type name");
    EXPECT(expect_type_to_c_copy("Box<Array<UnknownError>>",
        "PgyBoxArray_UnknownError"));

    TEST("slot_inner_type_name_copy(Slot<Float>) -> Float");
    {
        char inner[128];
        EXPECT(slot_inner_type_name_copy("Slot<Float>", inner, sizeof(inner)));
        EXPECT(strcmp(inner, "Float") == 0);
    }

    TEST("slot_inner_type_name_copy(SecureSlot<Long>) -> Long");
    {
        char inner[128];
        EXPECT(slot_inner_type_name_copy("SecureSlot<Long>", inner, sizeof(inner)));
        EXPECT(strcmp(inner, "Long") == 0);
    }
}
