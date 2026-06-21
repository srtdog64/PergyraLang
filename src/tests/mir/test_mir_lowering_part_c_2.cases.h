static void
test_mir_lowering_part_c_2(void)
{
    TEST("MIR residual STMT policy rejects local dataflow statements");
    {
        MIRInstruction inst = {0};
        bool rejects_let;
        bool rejects_destructure;
        bool rejects_assignment;
        bool keeps_fail;
        bool rejects_with;
        bool rejects_pure_call;
        bool keeps_effect_call;
        bool keeps_defer;
        bool keeps_intent_step;
        bool redundant_unordered;
        bool redundant_return;
        bool redundant_destructure;
        bool effect_call_emit_allowed;
        bool non_call_emit_rejected;
        ASTNode call_expr = {0};

        inst.kind = MIR_INST_STMT;
        inst.name = "stmt";
        inst.has_source_location = true;
        inst.has_source_statement_index = true;
        inst.source_statement_index = 0;

        inst.source_node_type = AST_LET_DECL;
        rejects_let = !mir_instruction_source_stmt_fallback_is_allowed(&inst);
        inst.source_node_type = AST_LET_DESTRUCTURE;
        rejects_destructure =
            !mir_instruction_source_stmt_fallback_is_allowed(&inst);
        inst.source_node_type = AST_ASSIGNMENT;
        rejects_assignment =
            !mir_instruction_source_stmt_fallback_is_allowed(&inst);
        inst.source_node_type = AST_FAIL_STMT;
        keeps_fail =
            mir_instruction_source_stmt_fallback_is_allowed(&inst);
        inst.source_node_type = AST_WITH_STMT;
        rejects_with =
            !mir_instruction_source_stmt_fallback_is_allowed(&inst);
        inst.source_node_type = AST_CALL;
        inst.arg0 = "HasZone";
        rejects_pure_call =
            !mir_instruction_source_stmt_fallback_is_allowed(&inst);
        inst.source_node_type = AST_CALL;
        inst.arg0 = "Log";
        keeps_effect_call =
            mir_instruction_source_stmt_fallback_is_allowed(&inst);
        inst.source_node_type = AST_DEFER_STMT;
        inst.arg0 = NULL;
        keeps_defer =
            mir_instruction_source_stmt_fallback_is_allowed(&inst);
        inst.source_node_type = AST_INTENT_STEP;
        keeps_intent_step =
            mir_instruction_source_stmt_fallback_is_allowed(&inst);
        inst.source_node_type = AST_BLOCK;
        inst.has_source_statement_index = false;
        redundant_unordered =
            mir_instruction_source_stmt_reemit_is_redundant(&inst);
        inst.has_source_statement_index = true;
        inst.source_node_type = AST_RETURN;
        redundant_return =
            mir_instruction_source_stmt_reemit_is_redundant(&inst);
        inst.source_node_type = AST_LET_DESTRUCTURE;
        redundant_destructure =
            mir_instruction_source_stmt_reemit_is_redundant(&inst);
        inst.source_node_type = AST_CALL;
        inst.expr0 = &call_expr;
        effect_call_emit_allowed =
            mir_instruction_source_stmt_call_emit_is_allowed(&inst);
        inst.source_node_type = AST_FAIL_STMT;
        non_call_emit_rejected =
            !mir_instruction_source_stmt_call_emit_is_allowed(&inst);

        EXPECT(rejects_let
               && rejects_destructure
               && rejects_assignment
               && keeps_fail
               && rejects_with
               && rejects_pure_call
               && keeps_effect_call
               && keeps_defer
               && keeps_intent_step
               && redundant_unordered
               && redundant_return
               && redundant_destructure
               && effect_call_emit_allowed
               && non_call_emit_rejected);
    }

    TEST("MIR validator rejects residual STMT without source inventory fact");
    {
        const char *src =
            "func ResidualStmtInventoryFact() -> Void {\n"
            "    if true {\n"
            "        defer { Log(1); };\n"
            "    }\n"
            "}\n";
        HIRProgram *hir = NULL;
        RIRProgram *rir = NULL;
        MIRProgram *mir = NULL;
        MIRRoutine *routine = NULL;
        MIRInstruction *stmt_inst = NULL;
        char *mir_error = NULL;
        bool rejected = false;
        bool ok = lower_mir_from_source(src, &hir, &rir, &mir);
        if (ok)
            routine = find_mir_routine_mut(mir, "ResidualStmtInventoryFact",
                                           MIR_SCOPE_FUNCTION);
        if (routine != NULL) {
            for (size_t bi = 0; bi < routine->block_count
                    && stmt_inst == NULL; bi++) {
                MIRBasicBlock *block = &routine->blocks[bi];
                for (size_t ii = 0; ii < block->instruction_count; ii++) {
                    MIRInstruction *inst = &block->instructions[ii];
                    if (inst->kind == MIR_INST_STMT
                        && inst->source_node_type == AST_DEFER_STMT) {
                        stmt_inst = inst;
                        break;
                    }
                }
            }
        }
        if (stmt_inst != NULL) {
            stmt_inst->has_source_statement_index = false;
            rejected =
                !mir_validate(mir, &mir_error)
                && mir_error != NULL
                && strstr(mir_error,
                          "STMT fallback is missing source statement inventory fact") != NULL;
            stmt_inst->has_source_statement_index = true;
        }
        EXPECT(ok
               && routine != NULL
               && stmt_inst != NULL
               && rejected
               && mir_validate(mir, NULL));
        free(mir_error);
        mir_destroy(mir);
        rir_destroy(rir);
        hir_destroy(hir);
    }
}
