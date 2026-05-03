#ifndef PERGYRA_TRANSPILER_MIR_PRESERVED_LET_EMIT_H
#define PERGYRA_TRANSPILER_MIR_PRESERVED_LET_EMIT_H

/* Preserved source let emission owner for MIR blocks. */
static bool
transpiler_emit_mir_preserved_let_stmt(CodeBuf *buf,
                                       const ASTNode *func_decl,
                                       const MIRRoutine *mir_routine,
                                       const MIRBasicBlock *block,
                                       ASTNode *stmt,
                                       TranspilerCtx *ctx,
                                       TranspilerSSANameMap *ssa_map_out,
                                       bool *handled_out,
                                       char *reason,
                                       size_t reason_cap)
{
    const char *versioned_local;

    if (handled_out != NULL)
        *handled_out = false;
    if (buf == NULL || func_decl == NULL || mir_routine == NULL
        || block == NULL || stmt == NULL || ctx == NULL
        || ssa_map_out == NULL || stmt->type != AST_LET_DECL
        || stmt->data.let_decl.name == NULL
        || stmt->data.let_decl.initializer == NULL) {
        return true;
    }

    versioned_local = transpiler_find_block_exit_ssa_name(
        block, stmt->data.let_decl.name);
    if (versioned_local == NULL) {
        versioned_local = transpiler_find_block_renamed_ssa_name(
            block, stmt->data.let_decl.name);
    }
    if (versioned_local == NULL) {
        versioned_local = transpiler_resolve_ssa_name(
            (const TranspilerSSANameMap *)ssa_map_out,
            stmt->data.let_decl.name);
    }
    if (versioned_local == NULL) {
        if (transpiler_find_routine_exit_ssa_name(
                mir_routine, stmt->data.let_decl.name) != NULL) {
            if (handled_out != NULL)
                *handled_out = true;
        }
        return true;
    }

    {
        char *lhs = transpiler_render_ssa_name(ctx, versioned_local);
        char *rhs = emit_expression_with_ssa_map(
            stmt->data.let_decl.initializer, ctx, ssa_map_out);
        char *rendered_type = NULL;
        const char *value_type = NULL;

        if (stmt->data.let_decl.type != NULL) {
            rendered_type = transpiler_render_effective_local_type_name(
                ctx, stmt->data.let_decl.type);
            value_type = rendered_type;
        } else {
            value_type = infer_expression_type_name(
                ctx, stmt->data.let_decl.initializer);
        }

        if (value_type != NULL && strcmp(value_type, "Unknown") != 0) {
            ASTNode *binding_type_ast =
                transpiler_find_local_type_ast(ctx, func_decl,
                                               stmt->data.let_decl.name);
            char *binding_type_name = NULL;
            if (binding_type_ast != NULL) {
                binding_type_name =
                    transpiler_render_effective_local_type_name(
                        ctx, binding_type_ast);
            }
            if (transpiler_type_name_is_view_like(binding_type_name)
                || transpiler_type_name_is_view_like(value_type)) {
                emit_statement(stmt, ctx);
                free(binding_type_name);
                free(lhs);
                free(rhs);
                free(rendered_type);
                if (handled_out != NULL)
                    *handled_out = true;
                return true;
            }
            if (is_slot_var(ctx, stmt->data.let_decl.name)
                || (binding_type_name != NULL
                    && (transpiler_type_name_is_slot_like(binding_type_name)
                        || transpiler_type_name_is_claim_shape(binding_type_name)
                        || strncmp(binding_type_name, "Channel<", 8) == 0))
                || (value_type != NULL
                    && (transpiler_type_name_is_slot_like(value_type)
                        || transpiler_type_name_is_claim_shape(value_type)
                        || strncmp(value_type, "Channel<", 8) == 0))) {
                free(binding_type_name);
                free(lhs);
                free(rhs);
                free(rendered_type);
                if (handled_out != NULL)
                    *handled_out = true;
                return true;
            }
            free(binding_type_name);
        }

        if (lhs == NULL || rhs == NULL) {
            free(lhs);
            free(rhs);
            free(rendered_type);
            if (reason != NULL && reason_cap > 0) {
                snprintf(reason, reason_cap,
                         "MIR block %zu emission failed: unable to materialize preserved let-binding '%s'",
                         block->id, stmt->data.let_decl.name);
            }
            return false;
        }

        write_indent_to(buf, ctx->indent);
        codebuf_write(buf, "%s = %s;\n", lhs, rhs);
        free(lhs);
        free(rhs);

        if (!transpiler_ssa_name_map_set(ssa_map_out,
                                         stmt->data.let_decl.name,
                                         versioned_local)) {
            free(rendered_type);
            return false;
        }
        if (value_type != NULL && value_type[0] != '\0')
            register_typed_var(ctx, stmt->data.let_decl.name, value_type);
        free(rendered_type);
    }

    if (handled_out != NULL)
        *handled_out = true;
    return true;
}

#endif /* PERGYRA_TRANSPILER_MIR_PRESERVED_LET_EMIT_H */
