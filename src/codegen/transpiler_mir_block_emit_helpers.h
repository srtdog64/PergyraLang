#ifndef PGY_SRC_CODEGEN_TRANSPILER_MIR_BLOCK_EMIT_HELPERS_H
#define PGY_SRC_CODEGEN_TRANSPILER_MIR_BLOCK_EMIT_HELPERS_H

/* Small helpers for C backend MIR block statement emission. */
static bool
transpiler_mir_seed_block_phi_names(const MIRBasicBlock *block,
                                    TranspilerSSANameMap *ssa_map_out)
{
    for (size_t i = 0; i < block->instruction_count; i++) {
        const MIRInstruction *inst = &block->instructions[i];
        char base[128];
        size_t version = 0;
        if (inst->kind != MIR_INST_PHI || inst->result_name == NULL)
            continue;
        if (!transpiler_parse_versioned_name(inst->result_name,
                                             base, sizeof(base), &version))
            continue;
        if (!transpiler_ssa_name_map_set(ssa_map_out, base, inst->result_name))
            return false;
    }
    return true;
}

static ASTNode *
transpiler_mir_find_stmt_for_inst(const ASTNode *func_decl,
                                  const MIRInstruction *inst)
{
    ASTNode *stmt = inst != NULL ? inst->ast : NULL;
    if (stmt == NULL && inst != NULL
        && inst->kind == MIR_INST_DEF
        && inst->arg0 != NULL) {
        stmt = transpiler_find_let_decl_by_name(func_decl, inst->arg0);
    }
    return stmt;
}

static bool
transpiler_mir_def_uses_source_statement_emit(const MIRInstruction *inst,
                                              const ASTNode *stmt,
                                              ASTNodeType expected_type)
{
    return inst != NULL
        && inst->kind == MIR_INST_DEF
        && inst->requires_source_statement_emit
        && stmt != NULL
        && stmt->type == expected_type;
}

static bool
transpiler_mir_seed_pin_view_alias(const MIRBasicBlock *block,
                                   TranspilerSSANameMap *ssa_map_out)
{
    if (!block->is_pin_region
        || block->pin_view_name == NULL
        || block->pin_source_name == NULL) {
        return true;
    }
    return transpiler_ssa_name_map_set(ssa_map_out,
                                       block->pin_view_name,
                                       block->pin_source_name);
}
#endif /* PGY_SRC_CODEGEN_TRANSPILER_MIR_BLOCK_EMIT_HELPERS_H */
