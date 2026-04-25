static bool
transpiler_materialize_pending_inst_uses(CodeBuf *buf,
                                         TranspilerCtx *ctx,
                                         const ASTNode *func_decl,
                                         const MIRBasicBlock *block,
                                         const MIRInstruction *inst,
                                         TranspilerSSANameMap *ssa_map_out,
                                         int indent,
                                         bool emit_assignments,
                                         char *reason,
                                         size_t reason_cap)
{
    if (ctx == NULL || block == NULL || inst == NULL || ssa_map_out == NULL)
        return true;

    for (size_t i = 0; i < inst->use_count; i++) {
        const char *versioned_use = inst->uses[i];
        const char *exit_versioned;
        ASTNode *let_decl;
        ASTNode *initializer;
        const char *existing_type;
        ASTNode *binding_type_ast;
        char *binding_type_name = NULL;
        char base[128];
        size_t version = 0;
        char *lhs = NULL;
        char *rhs = NULL;
        const char *value_type = NULL;
        char *rendered_type = NULL;

        if (versioned_use == NULL)
            continue;
        if (!transpiler_parse_versioned_name(versioned_use, base, sizeof(base), &version))
            continue;
        if (transpiler_resolve_ssa_name(ssa_map_out, base) != NULL)
            continue;
        if (is_slot_var(ctx, base))
            continue;
        existing_type = lookup_typed_var(ctx, base);
        if (existing_type != NULL
            && (transpiler_type_name_is_slot_like(existing_type)
                || transpiler_type_name_is_claim_shape(existing_type)
                || strncmp(existing_type, "Channel<", 8) == 0)) {
            continue;
        }
        binding_type_ast = transpiler_find_local_type_ast(ctx, func_decl, base);
        if (binding_type_ast != NULL) {
            binding_type_name = transpiler_render_effective_local_type_name(
                ctx, binding_type_ast);
            if (binding_type_name != NULL
                && (transpiler_type_name_is_slot_like(binding_type_name)
                    || transpiler_type_name_is_claim_shape(binding_type_name)
                    || strncmp(binding_type_name, "Channel<", 8) == 0)) {
                free(binding_type_name);
                continue;
            }
            free(binding_type_name);
            binding_type_name = NULL;
        }

        exit_versioned = transpiler_find_block_exit_ssa_name(block, base);
        if (exit_versioned == NULL)
            continue;

        let_decl = NULL;
        if (block->source_statements != NULL) {
            for (size_t stmt_idx = 0; stmt_idx < block->source_statement_count; stmt_idx++) {
                ASTNode *source_stmt = block->source_statements[stmt_idx];
                if (source_stmt == NULL || source_stmt->type != AST_LET_DECL)
                    continue;
                if (source_stmt->data.let_decl.name != NULL
                    && strcmp(source_stmt->data.let_decl.name, base) == 0) {
                    let_decl = source_stmt;
                    break;
                }
            }
        }
        if (let_decl == NULL)
            let_decl = transpiler_find_let_decl_by_name(func_decl, base);
        if (let_decl == NULL || let_decl->type != AST_LET_DECL)
            continue;
        initializer = let_decl->data.let_decl.initializer;
        if (initializer == NULL)
            continue;
        if (initializer->type == AST_CALL
            && initializer->data.call.callee != NULL
            && initializer->data.call.callee->type == AST_IDENTIFIER
            && initializer->data.call.callee->data.identifier.name != NULL
            && strncmp(initializer->data.call.callee->data.identifier.name,
                       "Claim", 5) == 0) {
            continue;
        }

        if (emit_assignments) {
            lhs = transpiler_render_ssa_name(ctx, exit_versioned);
            rhs = emit_expression_with_ssa_map(initializer, ctx, ssa_map_out);
            if (lhs == NULL || rhs == NULL) {
                free(lhs);
                free(rhs);
                if (reason != NULL && reason_cap > 0) {
                    snprintf(reason, reason_cap,
                             "MIR block %llu emission failed: unable to materialize pending value '%s'",
                             (unsigned long long) block->id, base);
                }
                return false;
            }
            write_indent_to(buf, indent);
            codebuf_write(buf, "%s = %s;\n", lhs, rhs);
            free(lhs);
            free(rhs);
        }

        if (!transpiler_ssa_name_map_set(ssa_map_out, base, exit_versioned))
            return false;

        if (let_decl->data.let_decl.type != NULL) {
            rendered_type = transpiler_render_effective_local_type_name(
                ctx, let_decl->data.let_decl.type);
            value_type = rendered_type;
        } else {
            value_type = infer_expression_type_name(ctx, initializer);
        }
        if (value_type != NULL
            && value_type[0] != '\0'
            && strcmp(value_type, "Void") != 0) {
            register_typed_var(ctx, base, value_type);
        }
        free(rendered_type);
    }

    return true;
}
