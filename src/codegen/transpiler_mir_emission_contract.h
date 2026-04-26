
static bool
transpiler_has_mapping_for_all_emitted_blocks(const TranspilerCtx *ctx,
                                             const MIRRoutine *routine,
                                             const ASTNode *func_decl,
                                             bool require_non_cleanup,
                                             char *reason,
                                             size_t reason_cap)
{
    const char *routine_name = routine != NULL && routine->name != NULL ? routine->name : "<routine>";
    if (routine == NULL || routine->blocks == NULL)
        return false;
    for (size_t i = 0; i < routine->block_count; i++) {
        const MIRBasicBlock *block = &routine->blocks[i];
        TranspilerSSANameMap ssa_map = {0};

        if (block == NULL || (require_non_cleanup && block->is_cleanup)
            || !block->is_reachable)
            continue;
        if (!transpiler_emit_mir_block_with_ssa_map(&ssa_map, block))
            return false;
        /* Add function/intent parameters to SSA map - they're valid C identifiers, not SSA vars */
        if (func_decl != NULL) {
            if (func_decl->type == AST_FUNC_DECL) {
                for (size_t p = 0; p < func_decl->data.func_decl.param_count; p++) {
                    FuncParam *param = func_decl->data.func_decl.params[p];
                    if (param != NULL && param->name != NULL) {
                        transpiler_ssa_name_map_set(&ssa_map, param->name, param->name);
                    }
                }
                transpiler_register_with_alias_bindings_in_block(&ssa_map,
                    func_decl->data.func_decl.body);
            } else if (func_decl->type == AST_INTENT_DECL) {
                /* For intent, extract alias from involves/value bindings */
                for (size_t p = 0; p < func_decl->data.intent_decl.involve_count; p++) {
                    ASTNode *involves = func_decl->data.intent_decl.involves[p];
                    if (involves != NULL && involves->type == AST_INTENT_INVOLVES
                        && involves->data.intent_involves.alias != NULL) {
                        transpiler_ssa_name_map_set(&ssa_map, involves->data.intent_involves.alias, involves->data.intent_involves.alias);
                    }
                }
                for (size_t p = 0; p < func_decl->data.intent_decl.value_count; p++) {
                    ASTNode *value = func_decl->data.intent_decl.values[p];
                    if (value != NULL && value->type == AST_INTENT_VALUE
                        && value->data.intent_value.alias != NULL) {
                        transpiler_ssa_name_map_set(&ssa_map, value->data.intent_value.alias,
                            value->data.intent_value.alias);
                    }
                }
            }
        }
        for (size_t j = 0; j < block->instruction_count; j++) {
            const MIRInstruction *inst = &block->instructions[j];
            if (inst->kind == MIR_INST_STMT && inst->ast != NULL) {
                if (inst->ast->type == AST_LET_DECL
                    && inst->ast->data.let_decl.name != NULL) {
                    const char *versioned_name =
                        transpiler_find_block_exit_ssa_name(
                            block, inst->ast->data.let_decl.name);
                    if (versioned_name != NULL) {
                        if (!transpiler_ssa_name_map_set(
                                &ssa_map,
                                inst->ast->data.let_decl.name,
                                versioned_name)) {
                            return false;
                        }
                    }
                } else if (inst->ast->type == AST_LET_DESTRUCTURE
                           && inst->ast->data.let_destructure.names != NULL) {
                    for (size_t dn = 0;
                         dn < inst->ast->data.let_destructure.name_count;
                         dn++) {
                        const char *binding =
                            inst->ast->data.let_destructure.names[dn];
                        const char *versioned_name;
                        if (binding == NULL)
                            continue;
                        versioned_name =
                            transpiler_find_block_exit_ssa_name(block, binding);
                        if (versioned_name != NULL) {
                            if (!transpiler_ssa_name_map_set(&ssa_map,
                                                             binding,
                                                             versioned_name)) {
                                return false;
                            }
                        }
                    }
                } else if (inst->ast->type == AST_ASSIGNMENT
                           && inst->ast->data.assignment.target != NULL
                           && inst->ast->data.assignment.target->type == AST_IDENTIFIER
                           && inst->ast->data.assignment.target->data.identifier.name != NULL) {
                    const char *target_name =
                        inst->ast->data.assignment.target->data.identifier.name;
                    const char *versioned_name =
                        transpiler_find_block_exit_ssa_name(block, target_name);
                    if (versioned_name != NULL) {
                        if (!transpiler_ssa_name_map_set(&ssa_map,
                                                         target_name,
                                                         versioned_name)) {
                            return false;
                        }
                    }
                }
            }
            if (!transpiler_materialize_pending_inst_uses(NULL,
                                                          (TranspilerCtx *)ctx,
                                                          func_decl,
                                                          block,
                                                          inst,
                                                          &ssa_map,
                                                          0,
                                                          false,
                                                          reason,
                                                          reason_cap)) {
                return false;
            }
            if ((inst->kind == MIR_INST_BRANCH || inst->kind == MIR_INST_RETURN
                 || inst->kind == MIR_INST_RESOURCE_OP || inst->kind == MIR_INST_CLEANUP_EDGE)
                && inst->ast != NULL) {
                if (!transpiler_expr_identifiers_mapped(ctx, inst->ast, &ssa_map, routine_name,
                                                       reason, reason_cap))
                    return false;
            }
            if (inst->kind == MIR_INST_DEF || inst->kind == MIR_INST_PHI) {
                char base[128];
                size_t version = 0;
                const char *base_name = (inst->kind == MIR_INST_PHI
                                         ? inst->name : inst->arg0);
                if (base_name != NULL && base_name[0] != '\0'
                    && inst->result_name != NULL
                    && transpiler_parse_versioned_name(inst->result_name, base, sizeof(base), &version)) {
                    if (!transpiler_ssa_name_map_set(&ssa_map, base_name, inst->result_name))
                        return false;
                }
            }
        }
    }
    return true;
}

static bool
transpiler_validate_mir_emission_contract(const TranspilerCtx *ctx,
                                         const MIRRoutine *routine,
                                         const ASTNode *decl,
                                         bool require_cleanup,
                                         bool require_cleanup_blocks,
                                         char *reason,
                                         size_t reason_cap)
{
    const char *routine_name = "<routine>";
    const char *decl_name = NULL;

    if (decl != NULL) {
        if (decl->type == AST_FUNC_DECL && decl->data.func_decl.name != NULL)
            decl_name = decl->data.func_decl.name;
        if (decl->type == AST_INTENT_DECL && decl->data.intent_decl.name != NULL)
            decl_name = decl->data.intent_decl.name;
    }
    routine_name = decl_name != NULL ? decl_name
        : (routine != NULL && routine->name != NULL ? routine->name : "<routine>");

    if (routine == NULL || routine->blocks == NULL) {
        if (reason != NULL && reason_cap > 0)
            snprintf(reason, reason_cap, "MIR contract invalid for %s: no routine", routine_name);
        return false;
    }

    {
        char *topology_error = NULL;
        if (!mir_validate_emission_topology(routine,
                                           require_cleanup,
                                           !require_cleanup_blocks,
                                           &topology_error)) {
            if (reason != NULL && reason_cap > 0) {
                if (topology_error != NULL)
                    snprintf(reason, reason_cap, "%s", topology_error);
                else
                    snprintf(reason, reason_cap,
                             "MIR contract invalid for %s: topology validation failed",
                             routine_name);
            }
            free(topology_error);
            return false;
        }
        free(topology_error);
    }


    for (size_t i = 0; i < routine->block_count; i++) {
        const MIRBasicBlock *block = &routine->blocks[i];

        if (block == NULL)
            return false;

        if (!transpiler_validate_mir_emission_block_shape(block,
                                                          routine_name,
                                                          require_cleanup,
                                                          reason,
                                                          reason_cap)) {
            return false;
        }

        if (block->has_succ_true && block->succ_true >= routine->block_count) {
            if (reason != NULL && reason_cap > 0)
                snprintf(reason, reason_cap, "MIR contract invalid for %s: block %llu bad true successor",
                         routine_name, (unsigned long long) block->id);
            return false;
        }
        if (block->has_succ_false && block->succ_false >= routine->block_count) {
            if (reason != NULL && reason_cap > 0)
                snprintf(reason, reason_cap, "MIR contract invalid for %s: block %llu bad false successor",
                         routine_name, (unsigned long long) block->id);
            return false;
        }
        if (block->has_cleanup_succ && block->cleanup_succ >= routine->block_count) {
            if (reason != NULL && reason_cap > 0)
                snprintf(reason, reason_cap, "MIR contract invalid for %s: block %llu bad cleanup successor",
                         routine_name, (unsigned long long) block->id);
            return false;
        }
        if (block->has_rollback_succ && block->rollback_succ >= routine->block_count) {
            if (reason != NULL && reason_cap > 0)
                snprintf(reason, reason_cap, "MIR contract invalid for %s: block %llu bad rollback successor",
                         routine_name, (unsigned long long) block->id);
            return false;
        }
        if (block->has_invalidation_succ && block->invalidation_succ >= routine->block_count) {
            if (reason != NULL && reason_cap > 0)
                snprintf(reason, reason_cap, "MIR contract invalid for %s: block %llu bad invalidation successor",
                         routine_name, (unsigned long long) block->id);
            return false;
        }

        for (size_t j = 0; j < block->instruction_count; j++) {
            MIRInstKind kind = block->instructions[j].kind;
            if (kind != MIR_INST_BRANCH && kind != MIR_INST_RETURN
                && kind != MIR_INST_RESOURCE_OP && kind != MIR_INST_CLEANUP_EDGE
                && kind != MIR_INST_PHI && kind != MIR_INST_DEF
                && kind != MIR_INST_STMT) {
                if (reason != NULL && reason_cap > 0)
                    snprintf(reason, reason_cap, "MIR contract invalid for %s: unsupported instruction kind %d in block %llu",
                             routine_name, (int)kind, (unsigned long long) block->id);
                return false;
            }
            if ((kind == MIR_INST_BRANCH || kind == MIR_INST_RETURN)
                && require_cleanup_blocks && block->is_cleanup) {
                if (reason != NULL && reason_cap > 0)
                    snprintf(reason, reason_cap,
                             "MIR contract invalid for %s: cleanup block %llu has terminal instruction",
                             routine_name, (unsigned long long) block->id);
                return false;
            }
        }
    }
    if (!transpiler_has_mapping_for_all_emitted_blocks(ctx, routine, decl,
                                                       !require_cleanup_blocks,
                                                       reason,
                                                       reason_cap)) {
        return false;
    }
    return true;
}

static bool
transpiler_validate_mir_emission_block_shape(const MIRBasicBlock *block,
                                            const char *routine_name,
                                            bool require_cleanup,
                                            char *reason,
                                            size_t reason_cap)
{
    bool has_branch = false;
    bool has_return = false;
    size_t branch_count = 0;
    size_t return_count = 0;
    size_t term_index = SIZE_MAX;

    if (block == NULL || routine_name == NULL)
        return false;

    for (size_t i = 0; i < block->instruction_count; i++) {
        const MIRInstruction *inst = &block->instructions[i];
        if (inst->kind == MIR_INST_BRANCH) {
            if (term_index == SIZE_MAX) {
                term_index = i;
            }
            has_branch = true;
            branch_count++;
            if (inst->ast == NULL && (reason != NULL && reason_cap > 0)) {
                snprintf(reason, reason_cap,
                         "MIR contract invalid for %s: block %llu branch instruction misses condition AST",
                         routine_name, (unsigned long long) block->id);
                return false;
            }
        } else if (inst->kind == MIR_INST_RETURN) {
            if (term_index == SIZE_MAX) {
                term_index = i;
            }
            has_return = true;
            return_count++;
        } else if (inst->kind != MIR_INST_RESOURCE_OP
                   && inst->kind != MIR_INST_DEF
                   && inst->kind != MIR_INST_PHI
                   && inst->kind != MIR_INST_CLEANUP_EDGE
                   && inst->kind != MIR_INST_STMT) {
            continue;
        }

        if (term_index != SIZE_MAX && i > term_index
            && inst->kind != MIR_INST_RESOURCE_OP
            && inst->kind != MIR_INST_CLEANUP_EDGE) {
            if (reason != NULL && reason_cap > 0)
                snprintf(reason, reason_cap,
                         "MIR contract invalid for %s: block %llu has non-trailing instructions after terminal",
                         routine_name, (unsigned long long) block->id);
            return false;
        }
    }

    if (branch_count > 1 || return_count > 1 || (has_branch && has_return)) {
        if (reason != NULL && reason_cap > 0)
            snprintf(reason, reason_cap,
                     "MIR contract invalid for %s: block %llu has %llu branch(es), %llu return(s)",
                     routine_name,
                     (unsigned long long) block->id,
                     (unsigned long long) branch_count,
                     (unsigned long long) return_count);
        return false;
    }

    if (has_branch) {
        if (!block->has_succ_true || !block->has_succ_false) {
            if (reason != NULL && reason_cap > 0)
                snprintf(reason, reason_cap,
                         "MIR contract invalid for %s: block %llu has branch without both true/false successors",
                         routine_name, (unsigned long long) block->id);
            return false;
        }
        if (block->has_cleanup_succ || block->has_rollback_succ || block->has_invalidation_succ) {
            if (!require_cleanup) {
                if (reason != NULL && reason_cap > 0)
                    snprintf(reason, reason_cap,
                             "MIR contract invalid for %s: block %llu has branch plus exceptional successor",
                             routine_name, (unsigned long long) block->id);
                return false;
            }
        }
        return true;
    }

    if (has_return) {
        if (block->has_succ_true || block->has_succ_false) {
            if (reason != NULL && reason_cap > 0)
                snprintf(reason, reason_cap,
                         "MIR contract invalid for %s: block %llu return has explicit successors",
                         routine_name, (unsigned long long) block->id);
            return false;
        }
        if (!require_cleanup
            && (block->has_cleanup_succ || block->has_rollback_succ
                || block->has_invalidation_succ)) {
            if (reason != NULL && reason_cap > 0)
                snprintf(reason, reason_cap,
                         "MIR contract invalid for %s: block %llu return has exceptional successor"
                         " without cleanup-capable routine",
                         routine_name, (unsigned long long) block->id);
            return false;
        }
        return true;
    }

    if (block->has_succ_false) {
        if (reason != NULL && reason_cap > 0)
            snprintf(reason, reason_cap,
                     "MIR contract invalid for %s: block %llu has false successor without branch",
                     routine_name, (unsigned long long) block->id);
        return false;
    }

    if (block->has_cleanup_succ || block->has_rollback_succ || block->has_invalidation_succ) {
        if (!require_cleanup) {
            if (reason != NULL && reason_cap > 0)
                snprintf(reason, reason_cap,
                         "MIR contract invalid for %s: block %llu has exceptional successor without cleanup-capable routine",
                         routine_name, (unsigned long long) block->id);
            return false;
        }
    }

    return true;
}

static bool
transpiler_can_emit_function_from_mir_with_reason(const TranspilerCtx *ctx,
                                                 const ASTNode *func_decl,
                                                 const MIRRoutine **mir_routine_out,
                                                 char *reason,
                                                 size_t reason_cap)
{
    const MIRRoutine *routine = transpiler_find_mir_function(ctx, func_decl);
    if (mir_routine_out != NULL)
        *mir_routine_out = NULL;
    if (reason != NULL && reason_cap > 0)
        reason[0] = '\0';
    if (routine == NULL || func_decl == NULL || func_decl->type != AST_FUNC_DECL) {
        if (reason != NULL && reason_cap > 0)
            snprintf(reason, reason_cap, "function cannot lower to MIR: no matching MIR routine");
        return false;
    }
    if (routine->kind != MIR_SCOPE_FUNCTION) {
        if (reason != NULL && reason_cap > 0)
            snprintf(reason, reason_cap, "function %s has wrong MIR kind: %d", func_decl->data.func_decl.name, routine->kind);
        return false;
    }
    if (routine->ast == NULL) {
        if (reason != NULL && reason_cap > 0)
            snprintf(reason, reason_cap, "function %s has no declaration AST in MIR", func_decl->data.func_decl.name);
        return false;
    }
    /* cleanup blocks are now fully supported - removed restriction */
    if (!transpiler_mir_function_signature_supported(func_decl)) {
        if (reason != NULL && reason_cap > 0)
            snprintf(reason, reason_cap, "function %s has unsupported MIR signature", func_decl->data.func_decl.name);
        return false;
    }
    const bool requires_cleanup = routine->has_cleanup_block;
    if (!transpiler_validate_mir_emission_contract(ctx,
                                                   routine,
                                                   func_decl,
                                                   requires_cleanup,
                                                   requires_cleanup,
                                                   reason,
                                                   reason_cap)) {
        return false;
    }
    if (!transpiler_has_mapping_for_all_emitted_blocks(ctx, routine, func_decl, true, reason, reason_cap)) {
        return false;
    }
    if (mir_routine_out != NULL)
        *mir_routine_out = routine;
    return true;
}

bool
transpiler_can_emit_function_from_mir_with_reason_for_test(
    const ASTNode *func_decl,
    const MIRProgram *mir,
    char *reason,
    size_t reason_cap)
{
    bool can_emit;
    TranspilerCtx *ctx = transpiler_ctx_create();
    if (ctx == NULL) {
        if (reason != NULL && reason_cap > 0)
            snprintf(reason, reason_cap, "Out of memory while creating transpiler context");
        return false;
    }
    ctx->mir = mir;
    can_emit = transpiler_can_emit_function_from_mir_with_reason(ctx, func_decl, NULL,
                                                                reason, reason_cap);
    transpiler_ctx_destroy(ctx);
    return can_emit;
}

static bool
transpiler_can_emit_intent_cleanup_from_mir_with_reason(const TranspilerCtx *ctx,
                                                       const ASTNode *intent_decl,
                                                       const MIRRoutine **mir_routine_out,
                                                       char *reason,
                                                       size_t reason_cap)
{
    const MIRRoutine *routine = transpiler_find_mir_intent(ctx, intent_decl);
    if (mir_routine_out != NULL)
        *mir_routine_out = NULL;
    if (reason != NULL && reason_cap > 0)
        reason[0] = '\0';
    if (routine == NULL || intent_decl == NULL || intent_decl->type != AST_INTENT_DECL) {
        if (reason != NULL && reason_cap > 0)
            snprintf(reason, reason_cap, "intent cannot lower to MIR: no matching MIR routine (found %llu routines)", (unsigned long long) (ctx != NULL && ctx->mir != NULL ? ctx->mir->routine_count : 0));
        return false;
    }
    if (routine->kind != MIR_SCOPE_INTENT
        || routine->ast == NULL
        || !routine->has_cleanup_block) {
        if (reason != NULL && reason_cap > 0)
            snprintf(reason, reason_cap, "intent %s has no MIR cleanup section (kind=%d, ast=%p, has_cleanup_block=%d)",
                intent_decl->data.intent_decl.name, routine->kind, (void*)routine->ast, routine->has_cleanup_block);
        return false;
    }
    if (!transpiler_validate_mir_emission_contract(ctx,
                                                   routine,
                                                   intent_decl,
                                                   true,
                                                   true,
                                                   reason,
                                                   reason_cap)) {
        if (reason != NULL && reason_cap > 0 && reason[0] == '\0')
            snprintf(reason, reason_cap, "intent %s MIR emission contract validation failed", intent_decl->data.intent_decl.name);
        return false;
    }
    if (!transpiler_has_mapping_for_all_emitted_blocks(ctx, routine, intent_decl, false, reason, reason_cap)) {
        if (reason != NULL && reason_cap > 0 && reason[0] == '\0')
            snprintf(reason, reason_cap, "intent %s SSA mapping incomplete", intent_decl->data.intent_decl.name);
        return false;
    }
    if (mir_routine_out != NULL)
        *mir_routine_out = routine;
    return true;
}

bool
transpiler_can_emit_intent_cleanup_from_mir_with_reason_for_test(
    const ASTNode *intent_decl,
    const MIRProgram *mir,
    char *reason,
    size_t reason_cap)
{
    bool can_emit;
    TranspilerCtx *ctx = transpiler_ctx_create();
    if (ctx == NULL) {
        if (reason != NULL && reason_cap > 0)
            snprintf(reason, reason_cap, "Out of memory while creating transpiler context");
        return false;
    }
    ctx->mir = mir;
    can_emit = transpiler_can_emit_intent_cleanup_from_mir_with_reason(ctx, intent_decl, NULL,
                                                                       reason, reason_cap);
    transpiler_ctx_destroy(ctx);
    return can_emit;
}

static bool
transpiler_emit_mir_resource_hook(TranspilerCtx *ctx,
                                  CodeBuf *out,
                                  int indent,
                                  const MIRInstruction *inst,
                                  const char *handle_expr,
                                  bool cleanup_hook)
{
    const char *slot_anchor = "";
    const char *arg_name = "";
    const MIRInstruction *emit_inst = inst;
    MIRInstruction inst_copy;
    char *write_value_expr = NULL;
    const char *helper = cleanup_hook
        ? "pgy_mir_cleanup_op_export"
        : "pgy_mir_resource_op_export";

    if (out == NULL || inst == NULL)
        return false;

    if (inst->kind == MIR_INST_RESOURCE_OP) {
        if (!cleanup_hook
            && ctx != NULL
            && ctx->active_ssa_map != NULL
            && inst->name != NULL
            && strcmp(inst->name, "Write") == 0
            && inst->ast != NULL
            && inst->ast->type == AST_CALL) {
            ASTNode *callee = inst->ast->data.call.callee;
            ASTNode *value_node = NULL;
            if (callee != NULL
                && callee->type == AST_IDENTIFIER
                && callee->data.identifier.name != NULL
                && strcmp(callee->data.identifier.name, "Write") == 0
                && inst->ast->data.call.arg_count >= 2) {
                value_node = inst->ast->data.call.arguments[1];
            } else if (callee != NULL
                       && callee->type == AST_MEMBER_ACCESS
                       && callee->data.member.name != NULL
                       && strcmp(callee->data.member.name, "Write") == 0
                       && inst->ast->data.call.arg_count >= 1) {
                value_node = inst->ast->data.call.arguments[0];
            }
            if (value_node != NULL) {
                write_value_expr = emit_expression_with_ssa_map(
                    value_node,
                    ctx,
                    (TranspilerSSANameMap *)ctx->active_ssa_map);
                if (write_value_expr != NULL && write_value_expr[0] != '\0') {
                    inst_copy = *inst;
                    inst_copy.arg1 = write_value_expr;
                    emit_inst = &inst_copy;
                }
            }
        }
        bool claim_already_materialized_by_stmt = false;
        bool is_claim_op = inst->name != NULL && strcmp(inst->name, "Claim") == 0;
        if (!cleanup_hook
            && is_claim_op) {
            if (ctx != NULL && ctx->mir != NULL) {
                claim_already_materialized_by_stmt = false;
            } else {
            const char *claim_name = inst->slot_anchor != NULL
                ? inst->slot_anchor
                : inst->arg0;
            const char *existing_type = claim_name != NULL
                ? lookup_typed_var(ctx, claim_name)
                : NULL;
            if (inst->ast != NULL
                && inst->ast->type == AST_LET_DECL
                && inst->ast->data.let_decl.name != NULL
                && claim_name != NULL
                && strcmp(claim_name, inst->ast->data.let_decl.name) == 0) {
                claim_already_materialized_by_stmt = true;
            } else if (claim_name != NULL
                       && existing_type != NULL
                       && (strncmp(existing_type, "Slot<", 5) == 0
                           || strncmp(existing_type, "SecureSlot<", 11) == 0
                           || strncmp(existing_type, "DeviceSlot<", 11) == 0)) {
                claim_already_materialized_by_stmt = true;
            }
            }
        }
        bool needs_concrete_emit = (cleanup_hook && !is_claim_op)
            || (is_claim_op && !claim_already_materialized_by_stmt && !cleanup_hook);
        if (ctx != NULL && ctx->mir != NULL && !cleanup_hook) {
            bool slot_is_secure = inst->slot_anchor != NULL
                && lookup_slot_is_secure(ctx, inst->slot_anchor);
            if (is_claim_op
                || (inst->name != NULL
                    && (strcmp(inst->name, "Write") == 0
                        || strcmp(inst->name, "Release") == 0
                        || strcmp(inst->name, "Move") == 0)
                    && !slot_is_secure)) {
                needs_concrete_emit = true;
            }
        }
        if (needs_concrete_emit) {
            if (!transpiler_emit_mir_resource_op(ctx, out, indent, emit_inst, emit_inst->type_layout, NULL)) {
                if (inst->name != NULL
                    && (strcmp(inst->name, "ProjectRefresh") == 0
                        || strcmp(inst->name, "ProjectPublish") == 0)) {
                    /* Direct projection expressions already emit the concrete value
                     * via MIR DEF/STMT paths; keep only the observability/export hook
                     * for projection resource ops that do not have a slot runtime ABI. */
                } else {
                    if (ctx != NULL && ctx->backend_error == NULL) {
                        ctx->backend_error = strdup_fmt(
                            "cannot emit MIR resource op '%s' for slot '%s': missing typed runtime layout",
                            inst->name != NULL ? inst->name : "<op>",
                            inst->slot_anchor != NULL ? inst->slot_anchor : "<slot>");
                    }
                    free(write_value_expr);
                    return false;
                }
            }
            if (!cleanup_hook) {
                free(write_value_expr);
                return true;
            }
        }
    }

    if (inst->name != NULL) {
        codebuf_write(out, "%*s%s(%s, ", indent * 4, "", helper,
            handle_expr != NULL ? handle_expr : "0");
        codebuf_write(out, "\"%s\", ", inst->name);
    } else {
        codebuf_write(out, "%*s%s(%s, \"unknown\", ", indent * 4, "", helper,
            handle_expr != NULL ? handle_expr : "0");
    }

    if (inst->rir_op != NULL && inst->rir_op->slot_anchor != NULL)
        slot_anchor = inst->rir_op->slot_anchor;
    else if (inst->arg0 != NULL)
        slot_anchor = inst->arg0;
    if (inst->arg1 != NULL)
        arg_name = inst->arg1;
    else if (inst->rir_op != NULL && inst->rir_op->arg0 != NULL)
        arg_name = inst->rir_op->arg0;

    codebuf_write(out, "\"%s\", \"%s\");\n",
        slot_anchor != NULL ? slot_anchor : "",
        arg_name != NULL ? arg_name : "");
    free(write_value_expr);
    return true;
}
