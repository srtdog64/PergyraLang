#ifndef PGY_TRANSPILER_MIR_SSA_CONTRACT_H
#define PGY_TRANSPILER_MIR_SSA_CONTRACT_H

static bool
transpiler_seed_expr_identifier_mappings(const MIRBasicBlock *block,
                                         size_t inst_index,
                                         const ASTNode *expr,
                                         TranspilerSSANameMap *ssa_map_out)
{
    if (block == NULL || expr == NULL || ssa_map_out == NULL)
        return true;

    if (expr->type == AST_IDENTIFIER && expr->data.identifier.name != NULL) {
        const char *name = expr->data.identifier.name;
        const char *mapped_value;
        if (transpiler_resolve_ssa_name(
                (const TranspilerSSANameMap *)ssa_map_out, name) != NULL) {
            return true;
        }
        mapped_value = transpiler_find_prior_block_ssa_name(block, inst_index, name);
        if (mapped_value == NULL)
            mapped_value = transpiler_find_block_exit_ssa_name(block, name);
        if (mapped_value != NULL)
            return transpiler_ssa_name_map_set(ssa_map_out, name, mapped_value);
        return true;
    }

    switch (expr->type) {
        case AST_BINARY:
            return transpiler_seed_expr_identifier_mappings(
                       block, inst_index, expr->data.binary.left, ssa_map_out)
                && transpiler_seed_expr_identifier_mappings(
                       block, inst_index, expr->data.binary.right, ssa_map_out);
        case AST_UNARY:
            return transpiler_seed_expr_identifier_mappings(
                block, inst_index, expr->data.unary.operand, ssa_map_out);
        case AST_CALL:
            if (expr->data.call.callee != NULL
                && expr->data.call.callee->type != AST_IDENTIFIER
                && !transpiler_seed_expr_identifier_mappings(
                       block, inst_index, expr->data.call.callee, ssa_map_out)) {
                return false;
            }
            for (size_t i = 0; i < expr->data.call.arg_count; i++) {
                if (!transpiler_seed_expr_identifier_mappings(
                        block, inst_index, expr->data.call.arguments[i], ssa_map_out)) {
                    return false;
                }
            }
            return true;
        case AST_MEMBER_ACCESS:
            return transpiler_seed_expr_identifier_mappings(
                block, inst_index, expr->data.member.object, ssa_map_out);
        case AST_ARRAY_ACCESS:
            return transpiler_seed_expr_identifier_mappings(
                       block, inst_index, expr->data.array_access.array, ssa_map_out)
                && transpiler_seed_expr_identifier_mappings(
                       block, inst_index, expr->data.array_access.index, ssa_map_out);
        case AST_ARRAY_LITERAL:
            for (size_t i = 0; i < expr->data.array_literal.count; i++) {
                if (!transpiler_seed_expr_identifier_mappings(
                        block, inst_index, expr->data.array_literal.elements[i], ssa_map_out)) {
                    return false;
                }
            }
            return true;
        case AST_ASSIGNMENT:
            return transpiler_seed_expr_identifier_mappings(
                       block, inst_index, expr->data.assignment.target, ssa_map_out)
                && transpiler_seed_expr_identifier_mappings(
                       block, inst_index, expr->data.assignment.value, ssa_map_out);
        case AST_AWAIT_EXPR:
            return transpiler_seed_expr_identifier_mappings(
                block, inst_index, expr->data.await_expr.expression, ssa_map_out);
        case AST_CHANNEL_SEND:
            return transpiler_seed_expr_identifier_mappings(
                       block, inst_index, expr->data.channel_send.channel, ssa_map_out)
                && transpiler_seed_expr_identifier_mappings(
                       block, inst_index, expr->data.channel_send.value, ssa_map_out);
        case AST_CHANNEL_RECV:
            return transpiler_seed_expr_identifier_mappings(
                block, inst_index, expr->data.channel_recv.channel, ssa_map_out);
        default:
            return true;
    }
}

static bool
transpiler_expr_identifiers_mapped(const TranspilerCtx *ctx,
                                  const ASTNode *expr,
                                  const TranspilerSSANameMap *ssa_map,
                                  const char *routine_name,
                                  char *reason,
                                  size_t reason_cap)
{
    if (expr == NULL)
        return true;
    if (expr->type == AST_IDENTIFIER && expr->data.identifier.name != NULL) {
        const char *name = expr->data.identifier.name;
        const char *existing_type = NULL;
        if (transpiler_resolve_ssa_name(ssa_map, expr->data.identifier.name) != NULL)
            return true;
        if (transpiler_name_is_token_local(name))
            return true;
        if (ctx != NULL) {
            existing_type = lookup_typed_var((TranspilerCtx *)ctx, name);
            if (is_slot_var((TranspilerCtx *)ctx, name))
                return true;
            if (existing_type != NULL
                && (transpiler_type_name_is_slot_like(existing_type)
                    || transpiler_type_name_is_claim_shape(existing_type)
                    || strncmp(existing_type, "Channel<", 8) == 0)) {
                return true;
            }
        }
        if (ctx != NULL && find_enum_decl((TranspilerCtx *)ctx, name) != NULL)
            return true;
        if (ctx != NULL && lookup_enum_variant_qualified_name((TranspilerCtx *)ctx, name) != NULL)
            return true;
        if (ctx != NULL && find_function_decl((TranspilerCtx *)ctx, name) != NULL)
            return true;
        if (reason != NULL && reason_cap > 0) {
            snprintf(reason, reason_cap,
                     "MIR contract breach in %s at line %u: unresolved identifier `%s` (expected SSA-mapped local)",
                     routine_name != NULL ? routine_name : "<routine>",
                     expr->line,
                     expr->data.identifier.name);
        }
        return false;
    }

    switch (expr->type) {
        case AST_BINARY:
            if (!transpiler_expr_identifiers_mapped(ctx, expr->data.binary.left, ssa_map,
                                                   routine_name, reason, reason_cap))
                return false;
            return transpiler_expr_identifiers_mapped(ctx, expr->data.binary.right, ssa_map,
                                                     routine_name, reason, reason_cap);
        case AST_UNARY:
            return transpiler_expr_identifiers_mapped(ctx, expr->data.unary.operand, ssa_map,
                                                     routine_name, reason, reason_cap);
        case AST_CALL:
            if (expr->data.call.callee != NULL
                && expr->data.call.callee->type == AST_IDENTIFIER
                && expr->data.call.callee->data.identifier.name != NULL
                && (strcmp(expr->data.call.callee->data.identifier.name, "ToObject") == 0
                    || strcmp(expr->data.call.callee->data.identifier.name, "ToTObject") == 0)) {
                for (size_t i = 1; i < expr->data.call.arg_count; i++) {
                    if (!transpiler_expr_identifiers_mapped(ctx, expr->data.call.arguments[i], ssa_map,
                                                           routine_name, reason, reason_cap))
                        return false;
                }
                return true;
            }
            if (expr->data.call.callee != NULL
                && expr->data.call.callee->type != AST_IDENTIFIER) {
                if (!transpiler_expr_identifiers_mapped(ctx, expr->data.call.callee, ssa_map,
                                                       routine_name, reason, reason_cap))
                    return false;
            } else if (expr->data.call.callee != NULL
                       && expr->data.call.callee->type == AST_IDENTIFIER
                       && expr->data.call.callee->data.identifier.name != NULL) {
                const char *call_target = expr->data.call.callee->data.identifier.name;
                if (transpiler_resolve_ssa_name(ssa_map, call_target) != NULL
                    && !transpiler_expr_identifiers_mapped(ctx, expr->data.call.callee, ssa_map,
                                                          routine_name, reason, reason_cap))
                    return false;
            }
            for (size_t i = 0; i < expr->data.call.arg_count; i++) {
                if (!transpiler_expr_identifiers_mapped(ctx, expr->data.call.arguments[i], ssa_map,
                                                       routine_name, reason, reason_cap))
                    return false;
            }
            return true;
        case AST_MEMBER_ACCESS:
            return transpiler_expr_identifiers_mapped(ctx, expr->data.member.object, ssa_map,
                                                     routine_name, reason, reason_cap);
        case AST_ARRAY_ACCESS:
            if (!transpiler_expr_identifiers_mapped(ctx, expr->data.array_access.array, ssa_map,
                                                   routine_name, reason, reason_cap))
                return false;
            return transpiler_expr_identifiers_mapped(ctx, expr->data.array_access.index, ssa_map,
                                                     routine_name, reason, reason_cap);
        case AST_ARRAY_LITERAL: {
            for (size_t i = 0; i < expr->data.array_literal.count; i++) {
                if (!transpiler_expr_identifiers_mapped(ctx, expr->data.array_literal.elements[i], ssa_map,
                                                       routine_name, reason, reason_cap))
                    return false;
            }
            return true;
        }
        case AST_ASSIGNMENT:
            return transpiler_expr_identifiers_mapped(ctx, expr->data.assignment.target, ssa_map,
                                                     routine_name, reason, reason_cap)
                && transpiler_expr_identifiers_mapped(ctx, expr->data.assignment.value, ssa_map,
                                                     routine_name, reason, reason_cap);
        case AST_AWAIT_EXPR:
            return transpiler_expr_identifiers_mapped(ctx, expr->data.await_expr.expression, ssa_map,
                                                     routine_name, reason, reason_cap);
        case AST_CHANNEL_SEND:
            return transpiler_expr_identifiers_mapped(ctx, expr->data.channel_send.channel, ssa_map,
                                                     routine_name, reason, reason_cap)
                && transpiler_expr_identifiers_mapped(ctx, expr->data.channel_send.value, ssa_map,
                                                     routine_name, reason, reason_cap);
        case AST_CHANNEL_RECV:
            return transpiler_expr_identifiers_mapped(ctx, expr->data.channel_recv.channel, ssa_map,
                                                     routine_name, reason, reason_cap);
        default:
            return true;
    }
}

#endif /* PGY_TRANSPILER_MIR_SSA_CONTRACT_H */
