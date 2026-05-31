/*
 * Copyright (c) 2026 Pergyra Language Project
 * MIR SSA expression mapping contract checks for the C backend.
 */

#include "transpiler_mir_ssa_contract.h"

#include <string.h>

#include "codegen_match_variant_policy.h"
#include "transpiler_decl_lookup.h"
#include "transpiler_enum.h"
#include "transpiler_mir_reason.h"
#include "transpiler_mir_ssa_lookup.h"
#include "transpiler_mir_ssa_utils.h"
#include "transpiler_symbols.h"
#include "transpiler_type_mapping.h"
#include "../parser/ast_api.h"

bool
transpiler_seed_expr_identifier_mappings(const MIRBasicBlock *block,
                                         size_t inst_index,
                                         const ASTNode *expr,
                                         TranspilerSSANameMap *ssa_map_out)
{
    if (block == NULL || expr == NULL || ssa_map_out == NULL)
        return true;

    if (expr->type == AST_IDENTIFIER && ast_identifier_name(expr) != NULL) {
        const char *name = ast_identifier_name(expr);
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
                       block, inst_index, ast_binary_left(expr), ssa_map_out)
                && transpiler_seed_expr_identifier_mappings(
                       block, inst_index, ast_binary_right(expr), ssa_map_out);
        case AST_UNARY:
            return transpiler_seed_expr_identifier_mappings(
                block, inst_index, ast_unary_operand(expr), ssa_map_out);
        case AST_CALL: {
            const ASTNode *callee = ast_call_callee(expr);
            size_t arg_count = ast_call_arg_count(expr);
            if (callee != NULL
                && callee->type != AST_IDENTIFIER
                && !transpiler_seed_expr_identifier_mappings(
                       block, inst_index, callee, ssa_map_out)) {
                return false;
            }
            for (size_t i = 0; i < arg_count; i++) {
                if (!transpiler_seed_expr_identifier_mappings(
                        block, inst_index, ast_call_argument(expr, i),
                        ssa_map_out)) {
                    return false;
                }
            }
            return true;
        }
        case AST_MEMBER_ACCESS:
            return transpiler_seed_expr_identifier_mappings(
                block, inst_index, ast_member_object(expr), ssa_map_out);
        case AST_ARRAY_ACCESS:
            return transpiler_seed_expr_identifier_mappings(
                       block, inst_index, ast_array_access_array(expr), ssa_map_out)
                && transpiler_seed_expr_identifier_mappings(
                       block, inst_index, ast_array_access_index(expr), ssa_map_out);
        case AST_ARRAY_LITERAL:
            for (size_t i = 0; i < ast_array_literal_count(expr); i++) {
                if (!transpiler_seed_expr_identifier_mappings(
                        block, inst_index, ast_array_literal_element(expr, i), ssa_map_out)) {
                    return false;
                }
            }
            return true;
        case AST_ASSIGNMENT:
            return transpiler_seed_expr_identifier_mappings(
                       block, inst_index, ast_assignment_target(expr), ssa_map_out)
                && transpiler_seed_expr_identifier_mappings(
                       block, inst_index, ast_assignment_value(expr), ssa_map_out);
        case AST_AWAIT_EXPR:
            return transpiler_seed_expr_identifier_mappings(
                block, inst_index, ast_await_expression(expr), ssa_map_out);
        case AST_CHANNEL_SEND:
            return transpiler_seed_expr_identifier_mappings(
                       block, inst_index, ast_channel_send_channel(expr), ssa_map_out)
                && transpiler_seed_expr_identifier_mappings(
                       block, inst_index, ast_channel_send_value(expr), ssa_map_out);
        case AST_CHANNEL_RECV:
            return transpiler_seed_expr_identifier_mappings(
                block, inst_index, ast_channel_recv_channel(expr), ssa_map_out);
        default:
            return true;
    }
}

bool
transpiler_expr_identifiers_mapped(const TranspilerCtx *ctx,
                                  const ASTNode *expr,
                                  const TranspilerSSANameMap *ssa_map,
                                  const char *routine_name,
                                  char *reason,
                                  size_t reason_cap)
{
    if (expr == NULL)
        return true;
    if (expr->type == AST_IDENTIFIER && ast_identifier_name(expr) != NULL) {
        const char *name = ast_identifier_name(expr);
        const char *existing_type = NULL;
        if (transpiler_resolve_ssa_name(ssa_map, name) != NULL)
            return true;
        if (transpiler_name_is_token_local(name))
            return true;
        if (pgy_codegen_match_variant_lookup(name)
                == PGY_MATCH_VARIANT_NONE_CTOR)
            return true;
        if (ctx != NULL) {
            existing_type = lookup_typed_var((TranspilerCtx *)ctx, name);
            if (is_slot_var((TranspilerCtx *)ctx, name))
                return true;
            if (existing_type != NULL
                && (transpiler_type_name_is_slot_like(existing_type)
                    || transpiler_type_name_is_claim_shape(existing_type)
                    || transpiler_type_name_is_channel(existing_type))) {
                return true;
            }
        }
        if (ctx != NULL && find_enum_decl((TranspilerCtx *)ctx, name) != NULL)
            return true;
        if (ctx != NULL) {
            char enum_variant[128];
            if (lookup_enum_variant_qualified_name_copy((TranspilerCtx *)ctx,
                    name, enum_variant, sizeof(enum_variant))) {
                return true;
            }
        }
        if (ctx != NULL && find_function_decl((TranspilerCtx *)ctx, name) != NULL)
            return true;
        if (reason != NULL && reason_cap > 0) {
            transpiler_mir_reasonf(reason, reason_cap,
                     "MIR contract breach in %s at line %u: unresolved identifier `%s` (expected SSA-mapped local)",
                     routine_name != NULL ? routine_name : "<routine>",
                     expr->line,
                     name);
        }
        return false;
    }

    switch (expr->type) {
        case AST_BINARY:
            if (!transpiler_expr_identifiers_mapped(ctx, ast_binary_left(expr), ssa_map,
                                                   routine_name, reason, reason_cap))
                return false;
            return transpiler_expr_identifiers_mapped(ctx, ast_binary_right(expr), ssa_map,
                                                     routine_name, reason, reason_cap);
        case AST_UNARY:
            return transpiler_expr_identifiers_mapped(ctx, ast_unary_operand(expr), ssa_map,
                                                     routine_name, reason, reason_cap);
        case AST_CALL: {
            const ASTNode *callee = ast_call_callee(expr);
            size_t arg_count = ast_call_arg_count(expr);
            if (callee != NULL
                && callee->type == AST_IDENTIFIER
                && ast_identifier_name(callee) != NULL
                && (strcmp(ast_identifier_name(callee), "ToObject") == 0
                    || strcmp(ast_identifier_name(callee), "ToTObject") == 0)) {
                for (size_t i = 1; i < arg_count; i++) {
                    if (!transpiler_expr_identifiers_mapped(ctx,
                                                           ast_call_argument(expr, i), ssa_map,
                                                           routine_name, reason, reason_cap))
                        return false;
                }
                return true;
            }
            if (callee != NULL
                && callee->type != AST_IDENTIFIER) {
                if (!transpiler_expr_identifiers_mapped(ctx, callee, ssa_map,
                                                       routine_name, reason, reason_cap))
                    return false;
            } else if (callee != NULL
                       && callee->type == AST_IDENTIFIER
                       && ast_identifier_name(callee) != NULL) {
                const char *call_target = ast_identifier_name(callee);
                if (transpiler_resolve_ssa_name(ssa_map, call_target) != NULL
                    && !transpiler_expr_identifiers_mapped(ctx, callee, ssa_map,
                                                          routine_name, reason, reason_cap))
                    return false;
            }
            for (size_t i = 0; i < arg_count; i++) {
                if (!transpiler_expr_identifiers_mapped(ctx,
                                                       ast_call_argument(expr, i), ssa_map,
                                                       routine_name, reason, reason_cap))
                    return false;
            }
            return true;
        }
        case AST_MEMBER_ACCESS:
            return transpiler_expr_identifiers_mapped(ctx, ast_member_object(expr), ssa_map,
                                                     routine_name, reason, reason_cap);
        case AST_ARRAY_ACCESS:
            if (!transpiler_expr_identifiers_mapped(ctx, ast_array_access_array(expr), ssa_map,
                                                   routine_name, reason, reason_cap))
                return false;
            return transpiler_expr_identifiers_mapped(ctx, ast_array_access_index(expr), ssa_map,
                                                     routine_name, reason, reason_cap);
        case AST_ARRAY_LITERAL: {
            for (size_t i = 0; i < ast_array_literal_count(expr); i++) {
                if (!transpiler_expr_identifiers_mapped(ctx, ast_array_literal_element(expr, i), ssa_map,
                                                       routine_name, reason, reason_cap))
                    return false;
            }
            return true;
        }
        case AST_ASSIGNMENT:
            return transpiler_expr_identifiers_mapped(ctx, ast_assignment_target(expr), ssa_map,
                                                     routine_name, reason, reason_cap)
                && transpiler_expr_identifiers_mapped(ctx, ast_assignment_value(expr), ssa_map,
                                                     routine_name, reason, reason_cap);
        case AST_AWAIT_EXPR:
            return transpiler_expr_identifiers_mapped(ctx, ast_await_expression(expr), ssa_map,
                                                     routine_name, reason, reason_cap);
        case AST_CHANNEL_SEND:
            return transpiler_expr_identifiers_mapped(ctx, ast_channel_send_channel(expr), ssa_map,
                                                     routine_name, reason, reason_cap)
                && transpiler_expr_identifiers_mapped(ctx, ast_channel_send_value(expr), ssa_map,
                                                     routine_name, reason, reason_cap);
        case AST_CHANNEL_RECV:
            return transpiler_expr_identifiers_mapped(ctx, ast_channel_recv_channel(expr), ssa_map,
                                                     routine_name, reason, reason_cap);
        default:
            return true;
    }
}
