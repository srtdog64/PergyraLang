static void
test_stdlib_and_enum_emit(void)
{
    printf("\n[stdlib_enum]\n");

    TranspilerCtx *ctx;

    TEST("array literal let emits PgyArray_Int builder");
    {
        ASTNode *arr = calloc(1, sizeof(ASTNode));
        arr->type = AST_ARRAY_LITERAL;
        arr->line = 1;
        arr->data.array_literal.count = 3;
        arr->data.array_literal.elements = calloc(3, sizeof(ASTNode *));
        arr->data.array_literal.elements[0] = make_number(1, 1);
        arr->data.array_literal.elements[1] = make_number(2, 1);
        arr->data.array_literal.elements[2] = make_number(3, 1);

        ASTNode *node = make_let("values", make_generic_type("Array", "Int"), arr, 1);
        const char *out = emit_stmt_to_str(node, &ctx);
        EXPECT_STR_CONTAINS(out, "PgyArray_Int values = ({");
        EXPECT_STR_CONTAINS(out, "pgy_array_push_Int");
        transpiler_ctx_destroy(ctx);
    }

    TEST("empty array literal uses explicit Array<T> annotation");
    {
        ASTNode *arr = calloc(1, sizeof(ASTNode));
        arr->type = AST_ARRAY_LITERAL;
        arr->line = 1;
        arr->data.array_literal.count = 0;
        arr->data.array_literal.elements = NULL;

        ASTNode *node = make_let("values", make_generic_type("Array", "String"), arr, 1);
        const char *out = emit_stmt_to_str(node, &ctx);
        EXPECT(ctx->backend_error == NULL);
        EXPECT_STR_CONTAINS(out, "PgyArray_String values = ({");
        EXPECT_STR_CONTAINS(out, "pgy_array_new_String(0)");
        transpiler_ctx_destroy(ctx);
    }

    TEST("empty array literal without annotation is rejected by C backend");
    {
        ASTNode *arr = calloc(1, sizeof(ASTNode));
        arr->type = AST_ARRAY_LITERAL;
        arr->line = 1;
        arr->data.array_literal.count = 0;
        arr->data.array_literal.elements = NULL;

        ASTNode *node = make_let("values", NULL, arr, 1);
        (void)emit_stmt_to_str(node, &ctx);
        EXPECT(ctx->backend_error != NULL);
        EXPECT_STR_CONTAINS(ctx->backend_error, "requires an explicit Array<T> annotation");
        transpiler_ctx_destroy(ctx);
    }

    TEST("String built-ins map to runtime helpers");
    {
        ASTNode *args[2] = { make_string_lit("a", 1), make_string_lit("b", 1) };
        ASTNode *call = make_call("Concat", args, 2, 1);
        ASTNode *node = make_let("s", make_type_node("String"), call, 1);
        const char *out = emit_stmt_to_str(node, &ctx);
        EXPECT_STR_CONTAINS(out, "StringConcat(\"a\", \"b\")");
        transpiler_ctx_destroy(ctx);
    }

    TEST("StringIndexOf maps to runtime helper");
    {
        ASTNode *args[2] = { make_string_lit("hello", 1), make_string_lit("ell", 1) };
        ASTNode *call = make_call("StringIndexOf", args, 2, 1);
        ASTNode *node = make_let("idx", make_type_node("Int"), call, 1);
        const char *out = emit_stmt_to_str(node, &ctx);
        EXPECT_STR_CONTAINS(out, "StringIndexOf(\"hello\", \"ell\")");
        transpiler_ctx_destroy(ctx);
    }

    TEST("FileExists maps to runtime helper");
    {
        ASTNode *args[1] = { make_string_lit("io.txt", 1) };
        ASTNode *call = make_call("FileExists", args, 1, 1);
        ASTNode *node = make_let("ok", make_type_node("Bool"), call, 1);
        const char *out = emit_stmt_to_str(node, &ctx);
        EXPECT_STR_CONTAINS(out, "pgy_file_exists(\"io.txt\")");
        transpiler_ctx_destroy(ctx);
    }

    TEST("Set built-ins map to runtime helpers");
    {
        const char *source =
            "func Main() -> Void {\n"
            "    let seen: Set<Int> = SetNew();\n"
            "    SetAdd(seen, 7);\n"
            "    Log(SetHas(seen, 7));\n"
            "    Log(SetSize(seen));\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        HIRProgram *hir = NULL;
        RIRProgram *rir = NULL;
        MIRProgram *mir = lower_program_to_mir_strict(program, &hir, &rir);
        ctx = transpiler_ctx_create();

        ctx->mir = mir;
        emit_program(ctx);

        EXPECT_STR_CONTAINS(ctx->out->data, "pgy_set_new_int()");
        EXPECT_STR_CONTAINS(ctx->out->data, "pgy_set_add_int");
        EXPECT_STR_CONTAINS(ctx->out->data, "pgy_set_has_int");
        EXPECT_STR_CONTAINS(ctx->out->data, "pgy_set_size_int");

        transpiler_ctx_destroy(ctx);
        mir_destroy(mir);
        rir_destroy(rir);
        hir_destroy(hir);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("List builtin rejects non-addressable storage expression");
    {
        ASTNode *list_call = make_call("MakeList", NULL, 0, 1);
        ASTNode *args[2] = { list_call, make_number(7, 1) };
        ASTNode *call = make_call("ListPush", args, 2, 1);
        ctx = transpiler_ctx_create();
        char *result = emit_expression(call, ctx);

        EXPECT(result != NULL);
        EXPECT_STR_NOT_CONTAINS(result, "&MakeList");
        EXPECT(ctx->backend_error != NULL);
        EXPECT_STR_CONTAINS(ctx->backend_error,
            "requires addressable List storage");

        free(result);
        ast_destroy(call);
        transpiler_ctx_destroy(ctx);
    }

    TEST("Array mutator rejects non-addressable storage expression");
    {
        ASTNode *array_call = make_call("MakeArray", NULL, 0, 1);
        ASTNode *args[2] = { array_call, make_number(7, 1) };
        ASTNode *call = make_call("ArrayPush", args, 2, 1);
        ctx = transpiler_ctx_create();
        char *result = emit_expression(call, ctx);

        EXPECT(result != NULL);
        EXPECT_STR_NOT_CONTAINS(result, "&MakeArray");
        EXPECT(ctx->backend_error != NULL);
        EXPECT_STR_CONTAINS(ctx->backend_error,
            "requires addressable Array storage");

        free(result);
        ast_destroy(call);
        transpiler_ctx_destroy(ctx);
    }

    TEST("StringJoin rejects non-addressable array expression");
    {
        ASTNode *array_call = make_call("StringSplit", NULL, 0, 1);
        ASTNode *args[2] = { array_call, make_string_lit(",", 1) };
        ASTNode *call = make_call("StringJoin", args, 2, 1);
        ctx = transpiler_ctx_create();
        char *result = emit_expression(call, ctx);

        EXPECT(result != NULL);
        EXPECT_STR_NOT_CONTAINS(result, "&StringSplit");
        EXPECT(ctx->backend_error != NULL);
        EXPECT_STR_CONTAINS(ctx->backend_error,
            "requires addressable Array storage");

        free(result);
        ast_destroy(call);
        transpiler_ctx_destroy(ctx);
    }

    TEST("Map builtin rejects non-addressable storage expression");
    {
        ASTNode *map_call = make_call("MakeMap", NULL, 0, 1);
        ASTNode *args[2] = { map_call, make_string_lit("hero", 1) };
        ASTNode *call = make_call("MapGet", args, 2, 1);
        ctx = transpiler_ctx_create();
        char *result = emit_expression(call, ctx);

        EXPECT(result != NULL);
        EXPECT_STR_NOT_CONTAINS(result, "&MakeMap");
        EXPECT(ctx->backend_error != NULL);
        EXPECT_STR_CONTAINS(ctx->backend_error,
            "requires addressable HashMap storage");

        free(result);
        ast_destroy(call);
        transpiler_ctx_destroy(ctx);
    }

    TEST("FSM builtin rejects non-addressable storage expression");
    {
        ASTNode *fsm_call = make_call("MakeFsm", NULL, 0, 1);
        ASTNode *args[1] = { fsm_call };
        ASTNode *call = make_call("FsmCurrent", args, 1, 1);
        ctx = transpiler_ctx_create();
        char *result = emit_expression(call, ctx);

        EXPECT(result != NULL);
        EXPECT_STR_NOT_CONTAINS(result, "&MakeFsm");
        EXPECT(ctx->backend_error != NULL);
        EXPECT_STR_CONTAINS(ctx->backend_error,
            "requires addressable FSM storage");

        free(result);
        ast_destroy(call);
        transpiler_ctx_destroy(ctx);
    }

    TEST("string equality lowers through runtime helper");
    {
        const char *source =
            "func Match(name: String) -> Bool {\n"
            "    return name == \"audit\";\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        HIRProgram *hir = NULL;
        RIRProgram *rir = NULL;
        MIRProgram *mir = lower_program_to_mir_strict(program, &hir, &rir);
        ctx = transpiler_ctx_create();

        ctx->mir = mir;
        emit_program(ctx);

        EXPECT_STR_CONTAINS(ctx->out->data, "pgy_string_equals(name, \"audit\")");

        transpiler_ctx_destroy(ctx);
        mir_destroy(mir);
        rir_destroy(rir);
        hir_destroy(hir);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("string concat let inference emits char* local");
    {
        const char *source =
            "struct UnitDraft {\n"
            "    roleTitle: String;\n"
            "    originTitle: String;\n"
            "}\n"
            "func FinalizeUnitSpec(draft: UnitDraft) -> String {\n"
            "    let title = draft.roleTitle + \" of the \" + draft.originTitle;\n"
            "    return title;\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        HIRProgram *hir = NULL;
        RIRProgram *rir = NULL;
        MIRProgram *mir = lower_program_to_mir_strict(program, &hir, &rir);
        ctx = transpiler_ctx_create();

        ctx->mir = mir;
        emit_program(ctx);

        EXPECT_STR_CONTAINS(ctx->out->data, "StringConcat(");
        EXPECT_STR_CONTAINS(ctx->out->data, "draft.roleTitle");
        EXPECT_STR_CONTAINS(ctx->out->data, "draft.originTitle");

        transpiler_ctx_destroy(ctx);
        mir_destroy(mir);
        rir_destroy(rir);
        hir_destroy(hir);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("string interpolation lowers through ToString and StringConcat");
    {
        const char *source =
            "subject Event {\n"
            "    let title: String;\n"
            "    let day: Int;\n"
            "}\n"
            "func Render(event: Event) -> String {\n"
            "    return \"Day ${event.day}: ${event.title}\";\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = lexer != NULL ? parser_create(lexer) : NULL;
        ASTNode *program = parser != NULL ? parser_parse_program(parser) : NULL;
        HIRProgram *hir = NULL;
        RIRProgram *rir = NULL;
        MIRProgram *mir = lower_program_to_mir_strict(program, &hir, &rir);
        ctx = transpiler_ctx_create();

        ctx->mir = mir;
        emit_program(ctx);

        EXPECT_STR_CONTAINS(ctx->out->data, "StringConcat(");
        EXPECT_STR_CONTAINS(ctx->out->data, "pgy_int_to_string(event->day)");
        EXPECT_STR_CONTAINS(ctx->out->data, "event->title");

        transpiler_ctx_destroy(ctx);
        mir_destroy(mir);
        rir_destroy(rir);
        hir_destroy(hir);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("function type params lower to callable declarators and typed lambdas");
    {
        const char *source =
            "struct StrategyContext {\n"
            "    morale: Int;\n"
            "}\n"
            "func Apply(base: Int, ctx: StrategyContext, policy: func(Int, StrategyContext) -> Int) -> Int {\n"
            "    return policy(base, ctx);\n"
            "}\n"
            "func Run() -> Int {\n"
            "    let ctx = StrategyContext(3);\n"
            "    return Apply(2, ctx, (base: Int, ctx: StrategyContext) => base + ctx.morale);\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        HIRProgram *hir = NULL;
        RIRProgram *rir = NULL;
        MIRProgram *mir = lower_program_to_mir_strict(program, &hir, &rir);
        ctx = transpiler_ctx_create();

        ctx->mir = mir;
        emit_program(ctx);

        EXPECT_STR_CONTAINS(ctx->out->data,
            "int32_t Apply(int32_t base, StrategyContext ctx, int32_t (*policy)(int32_t, StrategyContext))");
        EXPECT(strstr(ctx->decls->data, "pgy_lambda_") != NULL
            || strstr(ctx->helpers->data, "pgy_lambda_") != NULL);
        EXPECT(strstr(ctx->decls->data, "int32_t base, StrategyContext ctx") != NULL
            || strstr(ctx->helpers->data, "int32_t base, StrategyContext ctx") != NULL);

        transpiler_ctx_destroy(ctx);
        mir_destroy(mir);
        rir_destroy(rir);
        hir_destroy(hir);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("generic function pointer declarators use concrete ctx bindings");
    {
        ASTNode *handler_type = ast_create_event_handler_type();
        char *ctx_decl = NULL;
        char *legacy_decl = NULL;
        ctx = transpiler_ctx_create();

        handler_type->data.event_handler_type.param_types =
            calloc(1, sizeof(ASTNode *));
        handler_type->data.event_handler_type.param_count = 1;
        handler_type->data.event_handler_type.param_capacity = 1;
        handler_type->data.event_handler_type.param_types[0] =
            make_type_node("T");
        handler_type->data.event_handler_type.return_type =
            make_type_node("T");

        strcpy(ctx->generic_bindings[0].name, "T");
        strcpy(ctx->generic_bindings[0].concrete_type, "Long");
        ctx->generic_binding_count = 1;

        ctx_decl = pergyra_ast_typed_declarator_in_ctx(ctx, handler_type, "f");
        legacy_decl = pergyra_ast_typed_declarator(handler_type, "f");

        EXPECT(strcmp(ctx_decl, "int64_t (*f)(int64_t)") == 0);
        EXPECT(strstr(ctx_decl, "T") == NULL);
        EXPECT(strcmp(ctx_decl, legacy_decl) != 0);

        free(ctx_decl);
        free(legacy_decl);
        transpiler_ctx_destroy(ctx);
        ast_destroy(handler_type);
    }

    TEST("function typed locals and returns lower as function pointers");
    {
        const char *source =
            "func AddOne(x: Int) -> Int {\n"
            "    return x + 1;\n"
            "}\n"
            "func MakeAdder() -> func(Int) -> Int {\n"
            "    return AddOne;\n"
            "}\n"
            "func Main() -> Void {\n"
            "    let f: func(Int) -> Int = AddOne;\n"
            "    let g = MakeAdder();\n"
            "    Log(f(4));\n"
            "    Log(g(9));\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        HIRProgram *hir = NULL;
        RIRProgram *rir = NULL;
        MIRProgram *mir = lower_program_to_mir_strict(program, &hir, &rir);
        ctx = transpiler_ctx_create();

        ctx->mir = mir;
        emit_program(ctx);

        EXPECT_STR_CONTAINS(ctx->decls->data, "MakeAdder");
        EXPECT_STR_CONTAINS(ctx->out->data, "MakeAdder");
        EXPECT_STR_CONTAINS(ctx->out->data, "AddOne");

        transpiler_ctx_destroy(ctx);
        mir_destroy(mir);
        rir_destroy(rir);
        hir_destroy(hir);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("MIR locals keep function-pointer declarators for inferred and annotated callables");
    {
        const char *source =
            "func Compact(route: String, ok: Bool, handle: Int) -> String {\n"
            "    return route;\n"
            "}\n"
            "func Pick(mode: String) -> func(String, Bool, Int) -> String {\n"
            "    return Compact;\n"
            "}\n"
            "func Main() -> Void {\n"
            "    let checkoutFormatter = Pick(\"verbose\");\n"
            "    let refundFormatter: func(String, Bool, Int) -> String = Compact;\n"
            "    Log(checkoutFormatter(\"/checkout\", true, 4101));\n"
            "    Log(refundFormatter(\"/refund\", false, 8831));\n"
            "}\n";
        ASTNode *program = NULL;
        HIRProgram *hir = NULL;
        RIRProgram *rir = NULL;
        MIRProgram *mir = NULL;
        bool ok = lower_pipeline_from_source(source, &program, &hir, &rir, &mir);
        ctx = transpiler_ctx_create();
        ctx->mir = mir;

        EXPECT(ok);
        emit_program(ctx);

        EXPECT_STR_CONTAINS(ctx->out->data,
            "char* (*_pgy_ssa_checkoutFormatter_");
        EXPECT_STR_CONTAINS(ctx->out->data,
            ")(char*, bool, int32_t) = 0;");
        EXPECT_STR_CONTAINS(ctx->out->data,
            "_pgy_ssa_checkoutFormatter_1 = Pick(\"verbose\");");
        EXPECT_STR_CONTAINS(ctx->out->data,
            "char* (*_pgy_ssa_refundFormatter_");
        EXPECT_STR_CONTAINS(ctx->out->data,
            ")(char*, bool, int32_t) = 0;");
        EXPECT_STR_CONTAINS(ctx->out->data,
            "_pgy_ssa_refundFormatter_1 = Compact;");

        transpiler_ctx_destroy(ctx);
        mir_destroy(mir);
        rir_destroy(rir);
        hir_destroy(hir);
        ast_destroy(program);
    }
