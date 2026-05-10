static void
test_mir_lowering_part_d(void)
{
    TEST("MIR validator rejects terminal CFG-owned control fallback statements");
    {
        const char *src =
            "func CfgOwnedTerminal(value: Int) -> Int {\n"
            "    return value;\n"
            "}\n";
        HIRProgram *hir = NULL;
        RIRProgram *rir = NULL;
        MIRProgram *mir = NULL;
        MIRRoutine *routine = NULL;
        char *mir_error = NULL;
        bool injected = false;
        bool rejected = false;
        bool ok = lower_mir_from_source(src, &hir, &rir, &mir);
        if (ok)
            routine = find_mir_routine_mut(mir, "CfgOwnedTerminal", MIR_SCOPE_FUNCTION);
        if (routine != NULL) {
            for (size_t bi = 0; bi < routine->block_count && !injected; bi++) {
                MIRBasicBlock *block = &routine->blocks[bi];
                ASTNode *stmt = block->source_statement_inventory.count > 0
                    && block->source_statement_inventory.items != NULL
                    ? block->source_statement_inventory.items[0]
                    : NULL;
                if (stmt == NULL || stmt->type != AST_RETURN
                    || block->has_succ_true || block->has_succ_false) {
                    continue;
                }
                MIRInstruction *grown = realloc(block->instructions,
                    (block->instruction_count + 1) * sizeof(MIRInstruction));
                if (grown == NULL)
                    break;
                block->instructions = grown;
                memset(&block->instructions[block->instruction_count], 0,
                    sizeof(MIRInstruction));
                block->instructions[block->instruction_count].id =
                    routine->instruction_count++;
                block->instructions[block->instruction_count].kind = MIR_INST_STMT;
                block->instructions[block->instruction_count].name = "stmt";
                block->instructions[block->instruction_count].ast = stmt;
                block->instruction_count++;
                injected = true;
            }
        }
        rejected = ok
                   && injected
                   && !mir_validate(mir, &mir_error)
                   && mir_error != NULL
                   && strstr(mir_error, "CFG-owned control statement") != NULL;
        EXPECT(rejected);
        free(mir_error);
        mir_destroy(mir);
        rir_destroy(rir);
        hir_destroy(hir);
    }
}
