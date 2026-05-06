#ifndef PGY_TRANSPILER_MIR_FUNC_SSA_LOCALS_EMIT_H
#define PGY_TRANSPILER_MIR_FUNC_SSA_LOCALS_EMIT_H

static bool
transpiler_mir_ssa_local_limit_fail(TranspilerCtx *ctx, const char *name)
{
    transpiler_set_backend_error_with_hints(
        ctx,
        PGY_CODE_MIR_SSA_LIMIT,
        PGY_CAUSE_MIR_SSA_CAPACITY_EXCEEDED,
        PGY_FIX_INSPECT_HIR_TO_MIR_LOWERING,
        "too many MIR SSA locals while emitting function '%s'",
        name != NULL ? name : "<function>");
    return false;
}

static bool
transpiler_emit_mir_func_ssa_local_decls(TranspilerCtx *ctx,
                                         ASTNode *node,
                                         const MIRRoutine *mir_routine,
                                         const char *name)
{
    const char *declared_versioned_names[4096];
    size_t declared_versioned_count = 0;

    for (size_t i = 0; i < mir_routine->block_count; i++) {
        const MIRBasicBlock *block = &mir_routine->blocks[i];
        if (!block->is_reachable || block->is_cleanup)
            continue;
        for (size_t j = 0; j < block->live_in_name_count; j++) {
            if (!transpiler_versioned_name_list_add(declared_versioned_names,
                                                    &declared_versioned_count,
                                                    4096,
                                                    block->live_in_names[j])) {
                return transpiler_mir_ssa_local_limit_fail(ctx, name);
            }
        }
        for (size_t j = 0; j < block->ssa_entry_value_count; j++) {
            if (!transpiler_versioned_name_list_add(declared_versioned_names,
                                                    &declared_versioned_count,
                                                    4096,
                                                    block->ssa_entry_values[j])) {
                return transpiler_mir_ssa_local_limit_fail(ctx, name);
            }
        }
        for (size_t j = 0; j < block->ssa_exit_value_count; j++) {
            if (!transpiler_versioned_name_list_add(declared_versioned_names,
                                                    &declared_versioned_count,
                                                    4096,
                                                    block->ssa_exit_values[j])) {
                return transpiler_mir_ssa_local_limit_fail(ctx, name);
            }
        }
        for (size_t j = 0; j < block->renamed_local_count; j++) {
            if (!transpiler_versioned_name_list_add(declared_versioned_names,
                                                    &declared_versioned_count,
                                                    4096,
                                                    block->renamed_locals[j])) {
                return transpiler_mir_ssa_local_limit_fail(ctx, name);
            }
        }
        for (size_t j = 0; j < block->instruction_count; j++) {
            const MIRInstruction *inst = &block->instructions[j];
            for (size_t u = 0; u < inst->use_count; u++) {
                if (!transpiler_versioned_name_list_add(declared_versioned_names,
                                                        &declared_versioned_count,
                                                        4096,
                                                        inst->uses[u])) {
                    return transpiler_mir_ssa_local_limit_fail(ctx, name);
                }
            }
            if ((inst->kind == MIR_INST_DEF || inst->kind == MIR_INST_PHI)
                && inst->result_name != NULL
                && !transpiler_versioned_name_list_add(declared_versioned_names,
                                                       &declared_versioned_count,
                                                       4096,
                                                       inst->result_name)) {
                return transpiler_mir_ssa_local_limit_fail(ctx, name);
            }
        }
    }

    for (size_t i = 0; i < declared_versioned_count; i++) {
        const char *versioned_name = declared_versioned_names[i];
        char base[128];
        size_t version = 0;
        const char *type_name = NULL;
        const char *c_type = NULL;
        ASTNode *type_ast = NULL;
        char *c_name = NULL;
        char *initial_expr = NULL;
        char *decl = NULL;

        if (versioned_name == NULL
            || !transpiler_parse_versioned_name(versioned_name, base, sizeof(base), &version)) {
            continue;
        }
        if (transpiler_is_implicit_field(ctx, base))
            continue;
        type_name = transpiler_find_local_type_name(ctx, node, base);
        if (transpiler_type_name_is_claim_shape(type_name))
            continue;
        type_ast = transpiler_find_local_type_ast(ctx, node, base);
        if (type_ast != NULL && type_ast->type == AST_EVENT_HANDLER_TYPE) {
            c_name = transpiler_render_ssa_name(ctx, versioned_name);
            decl = pergyra_ast_typed_declarator(type_ast, c_name);
            write_indent(ctx);
            codebuf_write(ctx->out, "%s = 0;\n", decl);
            write_indent(ctx);
            codebuf_write(ctx->out, "(void)%s;\n", c_name);
            free(decl);
            free(c_name);
            continue;
        }
        if (type_name != NULL)
            c_type = pergyra_type_to_c(type_name);
        if (c_type == NULL
            || c_type[0] == '\0'
            || (type_name != NULL && strcmp(type_name, "Unknown") == 0)
            || strcmp(c_type, "Unknown") == 0) {
            transpiler_set_backend_error_with_hints(
                ctx,
                PGY_CODE_C_TYPE_UNSUPPORTED,
                PGY_CAUSE_C_TYPE_UNSUPPORTED,
                PGY_FIX_USE_LLVM_BACKEND_OR_EXTEND_TRANSPILER,
                "cannot determine C type for MIR local '%s' in function '%s'",
                versioned_name,
                name != NULL ? name : "<function>");
            return false;
        }
        c_name = transpiler_render_ssa_name(ctx, versioned_name);
        write_indent(ctx);
        if (version == 0) {
            bool has_param = false;
            bool has_top_level = false;
            for (size_t p = 0; p < node->data.func_decl.param_count; p++) {
                FuncParam *param = node->data.func_decl.params[p];
                if (param != NULL && param->name != NULL
                    && strcmp(param->name, base) == 0) {
                    has_param = true;
                    break;
                }
            }
            if (!has_param && node->data.func_decl.body != NULL
                && node->data.func_decl.body->type == AST_BLOCK) {
                ASTNode *body = node->data.func_decl.body;
                for (size_t s = 0; s < body->data.block.count; s++) {
                    ASTNode *stmt = body->data.block.statements[s];
                    if (stmt == NULL)
                        continue;
                    if (stmt->type == AST_WITH_STMT
                        && stmt->data.with_stmt.alias != NULL
                        && strcmp(stmt->data.with_stmt.alias, base) == 0) {
                        has_top_level = true;
                        break;
                    }
                }
            }
            if (has_param || has_top_level)
                initial_expr = pergyra_strdup(base);
        }
        if (initial_expr != NULL) {
            codebuf_write(ctx->out, "%s %s = %s;\n", c_type, c_name, initial_expr);
        } else if (transpiler_c_type_uses_scalar_zero(c_type)) {
            codebuf_write(ctx->out, "%s %s = 0;\n", c_type, c_name);
        } else {
            codebuf_write(ctx->out, "%s %s = (%s){0};\n", c_type, c_name, c_type);
        }
        write_indent(ctx);
        codebuf_write(ctx->out, "(void)%s;\n", c_name);
        free(initial_expr);
        free(c_name);
    }

    return true;
}

#endif /* PGY_TRANSPILER_MIR_FUNC_SSA_LOCALS_EMIT_H */
