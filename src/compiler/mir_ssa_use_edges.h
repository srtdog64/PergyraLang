static bool
mir_append_versioned_use(MIRInstruction *inst, const char *base, size_t version)
{
    char *versioned;
    if (inst == NULL || base == NULL)
        return true;
    versioned = mir_make_versioned_name(base, version);
    if (versioned == NULL)
        return false;
    return append_owned_name(&inst->uses, &inst->use_count, &inst->use_capacity, versioned);
}

static bool
mir_append_block_versioned_name(MIRBasicBlock *block,
                                bool is_entry,
                                const char *base,
                                size_t version)
{
    char *versioned;
    const char ***names;
    size_t *count;
    size_t *capacity;
    if (block == NULL || base == NULL)
        return true;
    versioned = mir_make_versioned_name(base, version);
    if (versioned == NULL)
        return false;
    names = is_entry ? &block->ssa_entry_values : &block->ssa_exit_values;
    count = is_entry ? &block->ssa_entry_value_count : &block->ssa_exit_value_count;
    capacity = is_entry ? &block->ssa_entry_value_capacity : &block->ssa_exit_value_capacity;
    return append_owned_name(names, count, capacity, versioned);
}

static bool
mir_parse_versioned_name(const char *versioned, char *base, size_t base_size, size_t *version_out)
{
    const char *dot;
    size_t len;
    if (versioned == NULL || base == NULL || base_size == 0 || version_out == NULL)
        return false;
    dot = strrchr(versioned, '.');
    if (dot == NULL)
        return false;
    len = (size_t)(dot - versioned);
    if (len + 1 > base_size)
        return false;
    memcpy(base, versioned, len);
    base[len] = '\0';
    *version_out = (size_t)strtoull(dot + 1, NULL, 10);
    return true;
}

static bool
mir_populate_use_edges(MIRRoutine *routine)
{
    const char **ssa_names = NULL;
    size_t ssa_name_count = 0;

    if (routine == NULL || routine->hir_routine == NULL)
        return false;
    if (!routine->hir_routine->has_cfg)
        return true;
    if (!mir_collect_ssa_names(routine, &ssa_names, &ssa_name_count))
        return false;
    if (ssa_name_count == 0) {
        free((void *)ssa_names);
        return true;
    }

    for (size_t block_id = 0; block_id < routine->block_count; block_id++) {
        MIRBasicBlock *block = &routine->blocks[block_id];
        size_t *current_versions;
        size_t stmt_index = 0;
        if (block->ssa_entry_versions == NULL || block->ssa_version_count != ssa_name_count)
            continue;
        for (size_t n = 0; n < ssa_name_count; n++) {
            if (block->ssa_entry_versions[n] == 0)
                continue;
            if (!mir_append_block_versioned_name(block, true, ssa_names[n], block->ssa_entry_versions[n])) {
                free((void *)ssa_names);
                return false;
            }
        }
        {
            current_versions = calloc(ssa_name_count, sizeof(size_t));
            if (current_versions == NULL) {
                free((void *)ssa_names);
                return false;
            }
            memcpy(current_versions,
                   block->ssa_entry_versions,
                   ssa_name_count * sizeof(size_t));
        for (size_t i = 0; i < block->instruction_count; i++) {
            MIRInstruction *inst = &block->instructions[i];
            if (inst->kind == MIR_INST_PHI) {
	                for (size_t j = 0; j < inst->phi_incoming_count; j++) {
                    if (!append_owned_name(&inst->uses,
                                           &inst->use_count,
                                           &inst->use_capacity,
                                           pergyra_strdup(inst->phi_incomings[j].value_name))) {
	                        free(current_versions);
	                        free((void *)ssa_names);
	                        return false;
	                    }
	                    routine->use_edge_count++;
	                }
                if (inst->result_name != NULL) {
                    char base[128];
                    size_t version = 0;
                    int idx;
                    if (mir_parse_versioned_name(inst->result_name, base, sizeof(base), &version)) {
                        idx = mir_find_ssa_name_index(ssa_names, ssa_name_count, base);
                        if (idx >= 0)
                            current_versions[idx] = version;
                    }
                }
                continue;
            }
            if (inst->kind == MIR_INST_DEF) {
                while (stmt_index < block->source_statement_count) {
                    ASTNode *stmt = block->source_statements[stmt_index];
                    if (stmt != NULL
                        && (stmt->type == AST_LET_DECL
                            || (stmt->type == AST_ASSIGNMENT
                                && stmt->data.assignment.target != NULL
                                && stmt->data.assignment.target->type == AST_IDENTIFIER))) {
                        break;
                    }
                    stmt_index++;
                }
                if (stmt_index < block->source_statement_count) {
                    ASTNode *stmt = block->source_statements[stmt_index];
                    ASTNode *expr = NULL;
                    const char **raw_uses = NULL;
                    size_t raw_use_count = 0;
                    size_t raw_use_capacity = 0;
                    if (stmt != NULL && stmt->type == AST_LET_DECL)
                        expr = stmt->data.let_decl.initializer;
                    else if (stmt != NULL && stmt->type == AST_ASSIGNMENT)
                        expr = stmt->data.assignment.value;
                    if (expr != NULL
                        && !mir_collect_expr_identifier_uses(expr,
                                                            &raw_uses,
                                                            &raw_use_count,
                                                            &raw_use_capacity)) {
                        free((void *)raw_uses);
                        free(current_versions);
                        free((void *)ssa_names);
                        return false;
                    }
                    for (size_t j = 0; j < raw_use_count; j++) {
                        int idx = mir_find_ssa_name_index(ssa_names, ssa_name_count, raw_uses[j]);
                        if (idx >= 0) {
                            if (!mir_append_versioned_use(inst, raw_uses[j], current_versions[idx])) {
                                free((void *)raw_uses);
                                free(current_versions);
                                free((void *)ssa_names);
                                return false;
                            }
                            routine->use_edge_count++;
                        }
                    }
                    free((void *)raw_uses);
                    stmt_index++;
                }
                if (inst->result_name != NULL) {
                    char base[128];
                    size_t version = 0;
                    int idx;
                    if (mir_parse_versioned_name(inst->result_name, base, sizeof(base), &version)) {
                        idx = mir_find_ssa_name_index(ssa_names, ssa_name_count, base);
                        if (idx >= 0)
                            current_versions[idx] = version;
                    }
                }
                continue;
            }
            if (inst->kind == MIR_INST_BRANCH || inst->kind == MIR_INST_RETURN) {
                const char **raw_uses = NULL;
                size_t raw_use_count = 0;
                size_t raw_use_capacity = 0;
                ASTNode *expr = (inst->kind == MIR_INST_BRANCH)
                                    ? block->source_terminator_condition
                                    : block->source_terminator_value;
	                if (!mir_collect_expr_identifier_uses(expr,
                                                       &raw_uses,
                                                       &raw_use_count,
                                                       &raw_use_capacity)) {
	                    free((void *)raw_uses);
	                    free(current_versions);
	                    free((void *)ssa_names);
	                    return false;
	                }
                for (size_t j = 0; j < raw_use_count; j++) {
                    int idx = mir_find_ssa_name_index(ssa_names, ssa_name_count, raw_uses[j]);
	                    if (idx >= 0) {
	                        if (!mir_append_versioned_use(inst, raw_uses[j], current_versions[idx])) {
	                            free((void *)raw_uses);
	                            free(current_versions);
	                            free((void *)ssa_names);
	                            return false;
	                        }
	                        routine->use_edge_count++;
	                    }
                }
                free((void *)raw_uses);
                continue;
            }
            if (inst->kind == MIR_INST_STMT) {
                const char **raw_uses = NULL;
                size_t raw_use_count = 0;
                size_t raw_use_capacity = 0;
                if (inst->ast != NULL
                    && !mir_collect_expr_identifier_uses(inst->ast,
                                                         &raw_uses,
                                                         &raw_use_count,
                                                         &raw_use_capacity)) {
                    free((void *)raw_uses);
                    free(current_versions);
                    free((void *)ssa_names);
                    return false;
                }
                for (size_t j = 0; j < raw_use_count; j++) {
                    int idx = mir_find_ssa_name_index(ssa_names, ssa_name_count,
                                                      raw_uses[j]);
                    if (idx >= 0) {
                        if (!mir_append_versioned_use(inst, raw_uses[j],
                                                      current_versions[idx])) {
                            free((void *)raw_uses);
                            free(current_versions);
                            free((void *)ssa_names);
                            return false;
                        }
                        routine->use_edge_count++;
                    }
                }
                free((void *)raw_uses);
                continue;
            }
            if (inst->kind == MIR_INST_RESOURCE_OP || inst->kind == MIR_INST_CLEANUP_EDGE) {
                const char **raw_uses = NULL;
                size_t raw_use_count = 0;
                size_t raw_use_capacity = 0;
	                if (inst->ast != NULL && !mir_collect_expr_identifier_uses(inst->ast,
                                                                            &raw_uses,
                                                                            &raw_use_count,
                                                                            &raw_use_capacity)) {
	                    free((void *)raw_uses);
	                    free(current_versions);
	                    free((void *)ssa_names);
	                    return false;
	                }
                if (raw_use_count == 0) {
                    const char *candidates[2] = {inst->arg0, inst->arg1};
                    for (size_t j = 0; j < 2; j++) {
	                        int idx = mir_find_ssa_name_index(ssa_names, ssa_name_count, candidates[j]);
	                        if (idx >= 0) {
	                            if (!mir_append_versioned_use(inst, candidates[j], current_versions[idx])) {
	                                free((void *)raw_uses);
	                                free(current_versions);
	                                free((void *)ssa_names);
	                                return false;
	                            }
	                            routine->use_edge_count++;
	                        }
                    }
                } else {
                    for (size_t j = 0; j < raw_use_count; j++) {
	                        int idx = mir_find_ssa_name_index(ssa_names, ssa_name_count, raw_uses[j]);
	                        if (idx >= 0) {
	                            if (!mir_append_versioned_use(inst, raw_uses[j], current_versions[idx])) {
	                                free((void *)raw_uses);
	                                free(current_versions);
	                                free((void *)ssa_names);
	                                return false;
	                            }
	                            routine->use_edge_count++;
	                        }
                    }
                }
                free((void *)raw_uses);
            }
        }
            for (size_t n = 0; n < ssa_name_count; n++) {
                if (current_versions[n] == 0)
                    continue;
                if (!mir_append_block_versioned_name(block, false, ssa_names[n], current_versions[n])) {
                    free(current_versions);
                    free((void *)ssa_names);
                    return false;
                }
            }
            free(current_versions);
        }
    }
    free((void *)ssa_names);
    return true;
}
