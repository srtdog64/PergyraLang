static void
test_statement_emit(void)
{
    printf("\n[statement_emit]\n");

    TranspilerCtx *ctx;

    TEST("let x: Int = 42 -> int32_t x = 42;");
    {
        ASTNode *node = make_let("x", make_type_node("Int"), make_number(42, 1), 1);
        const char *out = emit_stmt_to_str(node, &ctx);
        EXPECT_STR_CONTAINS(out, "int32_t x = 42;");
        transpiler_ctx_destroy(ctx);
    }

    TEST("let s: String = \"hi\" -> char* s = \"hi\";");
    {
        ASTNode *node = make_let("s", make_type_node("String"),
                                  make_string_lit("hi", 1), 1);
        const char *out = emit_stmt_to_str(node, &ctx);
        EXPECT_STR_CONTAINS(out, "char* s = \"hi\";");
        transpiler_ctx_destroy(ctx);
    }

    TEST("let maybe: Option<Int> = Some(42) -> PgyOption_Int maybe = Some_Int(42);");
    {
        ASTNode *args[1] = { make_number(42, 1) };
        ASTNode *node = make_let("maybe", make_generic_type("Option", "Int"),
                                 make_call("Some", args, 1, 1), 1);
        const char *out = emit_stmt_to_str(node, &ctx);
        EXPECT_STR_CONTAINS(out, "PgyOption_Int maybe = Some_Int(42);");
        transpiler_ctx_destroy(ctx);
    }

    TEST("let none: Option<Int> = None() -> PgyOption_Int none = None_Int();");
    {
        ASTNode *node = make_let("none", make_generic_type("Option", "Int"),
                                 make_call("None", NULL, 0, 1), 1);
        const char *out = emit_stmt_to_str(node, &ctx);
        EXPECT_STR_CONTAINS(out, "PgyOption_Int none = None_Int();");
        transpiler_ctx_destroy(ctx);
    }

    TEST("Option coalescing lowers to single-evaluation C fallback");
    {
        const char *source =
            "func Main() -> Int {\n"
            "    let maybe: Option<Int> = Some(7);\n"
            "    return maybe ?? 0;\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        HIRProgram *hir = NULL;
        RIRProgram *rir = NULL;
        MIRProgram *mir = lower_program_to_mir_strict(program, &hir, &rir);
        ctx = transpiler_ctx_create();

        emit_program(ctx);

        EXPECT_STR_CONTAINS(ctx->out->data, "__auto_type _pgy_coalesce = ");
        EXPECT_STR_CONTAINS(ctx->out->data, "_pgy_coalesce.tag == PgyOptionSome");
        EXPECT_STR_CONTAINS(ctx->out->data, "_pgy_coalesce.value : (0)");

        transpiler_ctx_destroy(ctx);
        mir_destroy(mir);
        rir_destroy(rir);
        hir_destroy(hir);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("match Option<Int> destructures Some/None");
    {
        ctx = transpiler_ctx_create();

        ASTNode *some_args[1] = { make_number(42, 1) };
        emit_statement(
            make_let("maybe", make_generic_type("Option", "Int"),
                     make_call("Some", some_args, 1, 1), 1),
            ctx);

        ASTNode *bind_args[1] = { make_identifier("v", 2) };
        ASTNode *case_some_body_stmts[1] = {
            make_return(make_identifier("v", 2), 2)
        };
        ASTNode *case_none_body_stmts[1] = {
            make_return(make_number(0, 3), 3)
        };
        ASTNode *cases[2] = {
            make_match_case(
                make_call("Some", bind_args, 1, 2),
                make_block(case_some_body_stmts, 1)),
            make_match_case(
                make_call("None", NULL, 0, 3),
                make_block(case_none_body_stmts, 1))
        };

        emit_statement(make_match(make_identifier("maybe", 2), cases, 2), ctx);

        EXPECT_STR_CONTAINS(ctx->out->data, "__match_");
        EXPECT_STR_CONTAINS(ctx->out->data, "if (__match_");
        EXPECT_STR_CONTAINS(ctx->out->data, ".tag == PgyOptionSome");
        EXPECT_STR_CONTAINS(ctx->out->data, "int32_t v = __match_");
        EXPECT_STR_CONTAINS(ctx->out->data, ".value;");
        EXPECT_STR_CONTAINS(ctx->out->data, "else if (__match_");
        EXPECT_STR_CONTAINS(ctx->out->data, ".tag == PgyOptionNone");
        transpiler_ctx_destroy(ctx);
    }

    TEST("let Result<Int, NetError> = Ok(...) uses expected type specialization");
    {
        const char *source =
            "enum NetError { Timeout, Refused, Unknown, }\n"
            "func Main() -> Void {\n"
            "    let r: Result<Int, NetError> = Ok(42);\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        HIRProgram *hir = NULL;
        RIRProgram *rir = NULL;
        MIRProgram *mir = lower_program_to_mir_strict(program, &hir, &rir);
        ctx = transpiler_ctx_create();

        emit_program(ctx);

        EXPECT_STR_CONTAINS(ctx->out->data,
            "PGY_RESULT_DEFINE(Int_NetError, int32_t, NetError)");
        EXPECT_STR_CONTAINS(ctx->out->data,
            "PgyResult_Int_NetError");
        EXPECT_STR_CONTAINS(ctx->out->data,
            "Ok_Int_NetError(42)");

        transpiler_ctx_destroy(ctx);
        mir_destroy(mir);
        rir_destroy(rir);
        hir_destroy(hir);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("let Result<Int, NetError> = Err(...) uses expected type specialization");
    {
        const char *source =
            "enum NetError { Timeout, Refused, Unknown, }\n"
            "func Main() -> Void {\n"
            "    let r: Result<Int, NetError> = Err(NetError.Unknown);\n"
            "}\n";
        Lexer *lexer = lexer_create(source);
        Parser *parser = parser_create(lexer);
        ASTNode *program = parser_parse_program(parser);
        HIRProgram *hir = NULL;
        RIRProgram *rir = NULL;
        MIRProgram *mir = lower_program_to_mir_strict(program, &hir, &rir);
        ctx = transpiler_ctx_create();

        emit_program(ctx);

        EXPECT_STR_CONTAINS(ctx->out->data,
            "PGY_RESULT_DEFINE(Int_NetError, int32_t, NetError)");
        EXPECT_STR_CONTAINS(ctx->out->data,
            "PgyResult_Int_NetError");
        EXPECT_STR_CONTAINS(ctx->out->data,
            "Err_Int_NetError(NetError_Unknown)");

        transpiler_ctx_destroy(ctx);
        mir_destroy(mir);
        rir_destroy(rir);
        hir_destroy(hir);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("let slot: Slot<Int> = ClaimSlot<Int>() -> PgySlot_Int slot = pgy_claim_Int();");
    {
        ASTNode *args[0];
        ASTNode *init = make_call("ClaimSlot", args, 0, 1);
        ASTNode *node = make_let("slot", make_type_node("Slot<Int>"), init, 1);
        const char *out = emit_stmt_to_str(node, &ctx);
        EXPECT_STR_CONTAINS(out, "PgySlot_Int slot = pgy_claim_Int();");
        transpiler_ctx_destroy(ctx);
    }

    TEST("let slot = ClaimSlot<String>() -> PgySlot_String slot = pgy_claim_String();");
    {
        ASTNode *args[0];
        ASTNode *init = make_call_generic1("ClaimSlot", "String", args, 0, 1);
        ASTNode *node = make_let("slot", NULL, init, 1);
        const char *out = emit_stmt_to_str(node, &ctx);
        EXPECT_STR_CONTAINS(out, "PgySlot_String slot = pgy_claim_String();");
        transpiler_ctx_destroy(ctx);
    }

    TEST("let ss: SecureSlot<Int> = ClaimSecureSlot() -> PgySecureSlot_Int + token");
    {
        ASTNode *args[0];
        ASTNode *init = make_call("ClaimSecureSlot", args, 0, 1);
        ASTNode *node = make_let("ss", make_type_node("SecureSlot<Int>"), init, 1);
        const char *out = emit_stmt_to_str(node, &ctx);
        EXPECT_STR_CONTAINS(out, "PgyToken_Int ss_token;");
        EXPECT_STR_CONTAINS(out, "PgySecureSlot_Int ss = pgy_claim_secure_Int(");
        transpiler_ctx_destroy(ctx);
    }

    TEST("let rv: ReadView<Int> = ViewRead(slot) -> PgySlot_Int rv = slot;");
    {
        ASTNode *args[1] = { make_identifier("slot", 1) };
        ASTNode *node = make_let("rv", make_type_node("ReadView<Int>"),
                                 make_call("ViewRead", args, 1, 1), 1);
        const char *out = emit_stmt_to_str(node, &ctx);
        EXPECT_STR_CONTAINS(out, "PgySlot_Int rv = slot;");
        transpiler_ctx_destroy(ctx);
    }

    TEST("let wv: WriteView<Int> = ViewWrite(slot) -> PgySlot_Int wv = slot;");
    {
        ASTNode *args[1] = { make_identifier("slot", 1) };
        ASTNode *node = make_let("wv", make_type_node("WriteView<Int>"),
                                 make_call("ViewWrite", args, 1, 1), 1);
        const char *out = emit_stmt_to_str(node, &ctx);
        EXPECT_STR_CONTAINS(out, "PgySlot_Int wv = slot;");
        transpiler_ctx_destroy(ctx);
    }

    TEST("let srv: ReadView<Int> = ViewRead(ss) on SecureSlot emits token alias");
    {
        ctx = transpiler_ctx_create();
        ASTNode *claim = make_call("ClaimSecureSlot", NULL, 0, 1);
        ASTNode *secure = make_let("ss", make_type_node("SecureSlot<Int>"), claim, 1);
        emit_statement(secure, ctx);
        EXPECT_STR_CONTAINS(ctx->out->data, "PgyToken_Int ss_token;");

        ASTNode *args[1] = { make_identifier("ss", 2) };
        ASTNode *node = make_let("srv", make_type_node("ReadView<Int>"),
                                 make_call("ViewRead", args, 1, 2), 2);
        emit_statement(node, ctx);
        EXPECT_STR_CONTAINS(ctx->out->data, "srv = ss;");
        EXPECT_STR_CONTAINS(ctx->out->data, "srv_token = ss_token;");
        transpiler_ctx_destroy(ctx);
    }

    TEST("function-return Array<String> destructuring preserves MIR local C types");
    {
        const char *source =
            "func Words() -> Array<String> {\n"
            "    return [\"hello\", \"world\", \"foo\"];\n"
            "}\n"
            "func Main() -> Void {\n"
            "    let (first, second, third) = Words();\n"
            "    Log(first);\n"
            "    Log(second);\n"
            "    Log(third);\n"
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

        EXPECT(ctx->backend_error == NULL);
        EXPECT_STR_CONTAINS(ctx->out->data, "char* _pgy_ssa_first_1 = 0;");
        EXPECT_STR_CONTAINS(ctx->out->data, "char* _pgy_ssa_second_1 = 0;");
        EXPECT_STR_CONTAINS(ctx->out->data, "char* _pgy_ssa_third_1 = 0;");
        EXPECT_STR_CONTAINS(ctx->out->data, "_pgy_ssa_first_1 = _pgy_destr_");
        EXPECT_STR_CONTAINS(ctx->out->data, "_pgy_ssa_second_1 = _pgy_destr_");
        EXPECT_STR_CONTAINS(ctx->out->data, "_pgy_ssa_third_1 = _pgy_destr_");

        transpiler_ctx_destroy(ctx);
        mir_destroy(mir);
        rir_destroy(rir);
        hir_destroy(hir);
        ast_destroy(program);
    }

    TEST("let (slot, token) = ClaimSecureSlot<T>(lvl) emits paired claim + Write/Read/Release");
    {
        const char *source =
            "func Main() -> Void {\n"
            "    let (slot, token) = ClaimSecureSlot<Int>(1);\n"
            "    Write(slot, 42, token);\n"
            "    let v: Int = Read(slot, token);\n"
            "    Log(ToString(v));\n"
            "    Release(slot, token);\n"
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

        EXPECT(ctx->backend_error == NULL);
        EXPECT_STR_CONTAINS(ctx->out->data, "PgyToken_Int token;");
        EXPECT_STR_CONTAINS(ctx->out->data,
            "PgySecureSlot_Int slot = pgy_claim_secure_Int(&token);");
        EXPECT_STR_CONTAINS(ctx->out->data,
            "pgy_secure_write_Int(&slot, 42, &token);");
        EXPECT_STR_CONTAINS(ctx->out->data,
            "pgy_secure_read_Int(&slot, &token)");
        EXPECT_STR_CONTAINS(ctx->out->data,
            "pgy_secure_release_Int(&slot, &token);");

        transpiler_ctx_destroy(ctx);
        mir_destroy(mir);
        rir_destroy(rir);
        hir_destroy(hir);
        ast_destroy(program);
    }

    /* Class-body destructuring (`let (a, b) = expr`) is now an accepted
     * surface: the parser builds placeholder fields + a destructure group, and
     * semantic/codegen lower it. A positive end-to-end parity case lives in the
     * backend-compare suite; the old "rejected with targeted error" guard was
     * removed when the feature was implemented. */

    TEST("MIR secure slot identifier auto-read uses secure read for non-destructure claim");
    {
        const char *source =
            "func Main() -> Void {\n"
            "    let ss: SecureSlot<Int> = ClaimSecureSlot<Int>(1);\n"
            "    Write(ss, 42, ss_token);\n"
            "    Log(ss);\n"
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

        EXPECT(ctx->backend_error == NULL);
        EXPECT_STR_CONTAINS(ctx->out->data, "PgyToken_Int ss_token;");
        EXPECT_STR_CONTAINS(ctx->out->data,
            "PgySecureSlot_Int ss = pgy_claim_secure_Int(&ss_token);");
        EXPECT_STR_CONTAINS(ctx->out->data,
            "pgy_secure_write_Int(&ss, 42, &ss_token);");
        EXPECT_STR_CONTAINS(ctx->out->data, "pgy_secure_read_Int(&");
        EXPECT_STR_CONTAINS(ctx->out->data, "&ss_token)");

        transpiler_ctx_destroy(ctx);
        mir_destroy(mir);
        rir_destroy(rir);
        hir_destroy(hir);
        ast_destroy(program);
    }
