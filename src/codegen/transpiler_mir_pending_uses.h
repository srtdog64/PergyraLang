#ifndef PGY_TRANSPILER_MIR_PENDING_USES_H
#define PGY_TRANSPILER_MIR_PENDING_USES_H

#include "transpiler_mir_reason.h"

#include "transpiler_mir_expr_ssa.h"
#include "transpiler_mir_local_type_ast_lookup.h"
#include "../parser/ast_api.h"

typedef struct TranspilerMirPendingBinding {
    ASTNode *initializer;
    ASTNode *type_annotation;
} TranspilerMirPendingBinding;

static bool
transpiler_pending_binding_from_source_statement_emit(
    const MIRInstruction *inst,
    const char *name,
    TranspilerMirPendingBinding *out)
{
    ASTNode *stmt;

    if (inst == NULL
        || name == NULL
        || !mir_instruction_uses_source_local_decl_emit(inst)) {
        return false;
    }

    stmt = mir_instruction_source_payload(inst);
    if (stmt == NULL
        || stmt->type != AST_LET_DECL
        || ast_let_name(stmt) == NULL
        || strcmp(ast_let_name(stmt), name) != 0) {
        return false;
    }

    if (out != NULL) {
        out->initializer = ast_let_initializer(stmt);
        out->type_annotation = ast_let_type(stmt);
    }
    return true;
}

static bool
transpiler_find_block_binding_from_mir_insts(const MIRBasicBlock *block,
                                             const char *name,
                                             TranspilerMirPendingBinding *out)
{
    if (block == NULL || name == NULL)
        return false;
    if (out != NULL)
        memset(out, 0, sizeof(*out));

    for (size_t i = 0; i < block->instruction_count; i++) {
        const MIRInstruction *inst = &block->instructions[i];

        if (inst->kind != MIR_INST_DEF || inst->arg0 == NULL)
            continue;
        if (strcmp(inst->arg0, name) != 0)
            continue;

        if (inst->expr0 != NULL
            && mir_instruction_uses_source_local_decl_emit(inst)) {
            if (out != NULL) {
                out->initializer = inst->expr0;
                out->type_annotation = inst->expr1;
            }
            return true;
        }

        if (transpiler_pending_binding_from_source_statement_emit(inst, name, out))
            return true;
    }

    return false;
}

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
        TranspilerMirPendingBinding binding;
        ASTNode *initializer = NULL;
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
            exit_versioned = transpiler_find_block_renamed_ssa_name(block, base);
        if (exit_versioned == NULL)
            continue;

        memset(&binding, 0, sizeof(binding));
        if (transpiler_find_block_binding_from_mir_insts(block, base, &binding)) {
            initializer = binding.initializer;
        }
        if (initializer == NULL) {
            continue;
        }
        if (initializer->type == AST_CALL
            && ast_call_callee(initializer) != NULL
            && ast_call_callee(initializer)->type == AST_IDENTIFIER
            && ast_identifier_name(ast_call_callee(initializer)) != NULL
            && strncmp(ast_identifier_name(ast_call_callee(initializer)),
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
                    transpiler_mir_reasonf(reason, reason_cap,
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

        if (binding.type_annotation != NULL) {
            rendered_type = transpiler_render_effective_local_type_name(
                ctx, binding.type_annotation);
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
#endif /* PGY_TRANSPILER_MIR_PENDING_USES_H */
