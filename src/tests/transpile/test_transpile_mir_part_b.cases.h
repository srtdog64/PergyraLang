static void
test_mir_select_dispatch_emit(void)
{
    printf("\n[mir_select_dispatch_emit]\n");

    TEST("MIR select dispatch emits channel readiness in C backend");
    {
        const char *source =
            "func SelectEdges(ch: Channel<Int>) -> Int {\n"
            "    ch <- 1;\n"
            "    select {\n"
            "        case <-ch:\n"
            "            return 1;\n"
            "        default:\n"
            "            return 0;\n"
            "    }\n"
            "    return 0;\n"
            "}\n";
        ASTNode *program = NULL;
        HIRProgram *hir = NULL;
        RIRProgram *rir = NULL;
        MIRProgram *mir = NULL;
        Lexer *lexer = NULL;
        Parser *parser = NULL;
        char *hir_error = NULL;
        char *rir_error = NULL;
        char *mir_error = NULL;
        char path_buf[512];
        char *output = NULL;
        bool mir_emit_ok = false;
        bool ok = false;

        lexer = lexer_create(source);
        parser = parser_create(lexer);
        program = parser_parse_program(parser);
        ok = !parser_has_error(parser) && program != NULL;
        if (ok)
            hir = hir_lower(program, &hir_error);
        if (ok && hir != NULL)
            rir = rir_lower(program, &rir_error);
        if (ok && hir != NULL && rir != NULL)
            (void)rir_enrich_with_hir_flow(rir, hir, &rir_error);
        if (ok && hir != NULL && rir != NULL)
            mir = mir_lower(hir, rir, &mir_error);
        ok = (ok && hir != NULL && rir != NULL && mir != NULL);
        if (ok)
            mir_emit_ok = check_function_mir_emitability(hir, mir, "SelectEdges");

        make_tmp_path(path_buf, sizeof(path_buf), "pgy_test_mir_select_dispatch.c");
        if (ok) {
            TranspileResult *res = transpile_with_mir(hir, mir, path_buf);
            ok = (res != NULL && res->success);
            transpile_result_destroy(res);
        }
        if (ok)
            output = read_file_text(path_buf);

        EXPECT(ok && output != NULL);
        if (ok && output != NULL) {
            EXPECT(mir_emit_ok);
            EXPECT(strstr(output, "/* emitted-from-mir */") != NULL);
            EXPECT(strstr(output, "pgy_channel_ready_Int(&ch)") != NULL
                   || strstr(output, "pgy_channel_ready_Int(&_pgy_ssa_ch_") != NULL);
            EXPECT(strstr(output, "if (pgy_channel_ready_Int(") != NULL);
        }

        free(output);
        free(hir_error);
        free(rir_error);
        free(mir_error);
        mir_destroy(mir);
        rir_destroy(rir);
        hir_destroy(hir);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("MIR select dispatch materializes bound receive local type");
    {
        const char *source =
            "func SelectBound(ch: Channel<Int>) -> Int {\n"
            "    ch <- 7;\n"
            "    select {\n"
            "        case v = <-ch:\n"
            "            return v;\n"
            "        default:\n"
            "            return 0;\n"
            "    }\n"
            "    return 0;\n"
            "}\n";
        ASTNode *program = NULL;
        HIRProgram *hir = NULL;
        RIRProgram *rir = NULL;
        MIRProgram *mir = NULL;
        Lexer *lexer = NULL;
        Parser *parser = NULL;
        char *hir_error = NULL;
        char *rir_error = NULL;
        char *mir_error = NULL;
        char path_buf[512];
        char *output = NULL;
        bool mir_emit_ok = false;
        bool ok = false;

        lexer = lexer_create(source);
        parser = parser_create(lexer);
        program = parser_parse_program(parser);
        ok = !parser_has_error(parser) && program != NULL;
        if (ok)
            hir = hir_lower(program, &hir_error);
        if (ok && hir != NULL)
            rir = rir_lower(program, &rir_error);
        if (ok && hir != NULL && rir != NULL)
            (void)rir_enrich_with_hir_flow(rir, hir, &rir_error);
        if (ok && hir != NULL && rir != NULL)
            mir = mir_lower(hir, rir, &mir_error);
        ok = (ok && hir != NULL && rir != NULL && mir != NULL);
        if (ok)
            mir_emit_ok = check_function_mir_emitability(hir, mir, "SelectBound");

        make_tmp_path(path_buf, sizeof(path_buf), "pgy_test_mir_select_bound.c");
        if (ok) {
            TranspileResult *res = transpile_with_mir(hir, mir, path_buf);
            ok = (res != NULL && res->success);
            transpile_result_destroy(res);
        }
        if (ok)
            output = read_file_text(path_buf);

        EXPECT(ok && output != NULL);
        if (ok && output != NULL) {
            EXPECT(mir_emit_ok);
            EXPECT(strstr(output, "/* emitted-from-mir */") != NULL);
            EXPECT(strstr(output, "pgy_channel_ready_Int(&ch)") != NULL
                   || strstr(output, "pgy_channel_ready_Int(&_pgy_ssa_ch_") != NULL);
            EXPECT(strstr(output, "int32_t _pgy_ssa_v_") != NULL);
            EXPECT(strstr(output, "_pgy_ssa_v_1 = pgy_channel_recv_val_Int(&ch)") != NULL);
            EXPECT(strstr(output, "\nv = pgy_channel_recv_val_Int(&ch)") == NULL);
        }

        free(output);
        free(hir_error);
        free(rir_error);
        free(mir_error);
        mir_destroy(mir);
        rir_destroy(rir);
        hir_destroy(hir);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }

    TEST("MIR select dispatch preserves implicit field channel lvalue");
    {
        const char *source =
            "class SelectBox {\n"
            "    let ch: Channel<Int>;\n"
            "\n"
            "    func Pull(self) -> Int {\n"
            "        ch <- 9;\n"
            "        select {\n"
            "            case v = <-ch:\n"
            "                return v;\n"
            "            default:\n"
            "                return 0;\n"
            "        }\n"
            "        return 0;\n"
            "    }\n"
            "}\n";
        ASTNode *program = NULL;
        HIRProgram *hir = NULL;
        RIRProgram *rir = NULL;
        MIRProgram *mir = NULL;
        Lexer *lexer = NULL;
        Parser *parser = NULL;
        char *hir_error = NULL;
        char *rir_error = NULL;
        char *mir_error = NULL;
        char path_buf[512];
        char *output = NULL;
        bool mir_emit_ok = false;
        bool ok = false;

        lexer = lexer_create(source);
        parser = parser_create(lexer);
        program = parser_parse_program(parser);
        ok = !parser_has_error(parser) && program != NULL;
        if (ok)
            hir = hir_lower(program, &hir_error);
        if (ok && hir != NULL)
            rir = rir_lower(program, &rir_error);
        if (ok && hir != NULL && rir != NULL)
            (void)rir_enrich_with_hir_flow(rir, hir, &rir_error);
        if (ok && hir != NULL && rir != NULL)
            mir = mir_lower(hir, rir, &mir_error);
        ok = (ok && hir != NULL && rir != NULL && mir != NULL);
        if (ok)
            mir_emit_ok = find_mir_routine_by_name(mir, "Pull") != NULL;

        make_tmp_path(path_buf, sizeof(path_buf), "pgy_test_mir_select_field_channel.c");
        if (ok) {
            TranspileResult *res = transpile_with_mir(hir, mir, path_buf);
            ok = (res != NULL && res->success);
            transpile_result_destroy(res);
        }
        if (ok)
            output = read_file_text(path_buf);

        EXPECT(ok && output != NULL);
        if (ok && output != NULL) {
            EXPECT(mir_emit_ok);
            EXPECT(strstr(output, "/* emitted-from-mir */") != NULL);
            EXPECT(strstr(output, "pgy_channel_ready_Int(&self.ch)") != NULL);
            EXPECT(strstr(output, "pgy_channel_ready_Int(&ch)") == NULL);
            EXPECT(strstr(output, "pgy_channel_recv_val_Int(&self.ch)") != NULL);
        }

        free(output);
        free(hir_error);
        free(rir_error);
        free(mir_error);
        mir_destroy(mir);
        rir_destroy(rir);
        hir_destroy(hir);
        ast_destroy(program);
        parser_destroy(parser);
        lexer_destroy(lexer);
    }
}
