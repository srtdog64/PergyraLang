/*
 * Copyright (c) 2026 Pergyra Language Project
 * C backend MIR local-binding discovery helpers.
 */

#include <stdlib.h>
#include <string.h>

#include "transpiler_mir_local_binding.h"
#include "../parser/ast_api.h"
#include "codegen_slot_type_policy.h"
#include "transpiler_context.h"
#include "transpiler_format.h"
#include "transpiler_mir_effective_type.h"
#include "transpiler_mir_local_type_lookup.h"
#include "transpiler_mir_ssa_utils.h"
#include "transpiler_symbols.h"
#include "transpiler_type_render.h"
#include "../semantic/diag_codes.h"

static bool
transpiler_select_case_has_receive_binding(ASTNode *node,
                                           const char *base_name)
{
    if (node == NULL || base_name == NULL)
        return false;
    if (node->type == AST_BLOCK) {
        for (size_t i = 0; i < ast_block_statement_count(node); i++) {
            if (transpiler_select_case_has_receive_binding(
                    ast_block_statement(node, i), base_name)) {
                return true;
            }
        }
        return false;
    }
    if (node->type == AST_ASSIGNMENT) {
        ASTNode *target = ast_assignment_target(node);
        ASTNode *value = ast_assignment_value(node);
        return target != NULL
            && target->type == AST_IDENTIFIER
            && ast_identifier_name(target) != NULL
            && strcmp(ast_identifier_name(target), base_name) == 0
            && value != NULL
            && value->type == AST_CHANNEL_RECV;
    }
    return false;
}

bool
transpiler_has_local_binding_in_block(ASTNode *body, const char *base_name)
{
    if (body == NULL || base_name == NULL)
        return false;
    if (body->type == AST_BLOCK) {
        for (size_t i = 0; i < ast_block_statement_count(body); i++) {
            if (transpiler_has_local_binding_in_block(
                    ast_block_statement(body, i), base_name)) {
                return true;
            }
        }
        return false;
    }
    if (body->type == AST_LET_DECL
        && ast_let_name(body) != NULL
        && strcmp(ast_let_name(body), base_name) == 0) {
        return true;
    }
    if (body->type == AST_WITH_STMT) {
        if (ast_with_alias(body) != NULL
            && strcmp(ast_with_alias(body), base_name) == 0) {
            return true;
        }
        return transpiler_has_local_binding_in_block(ast_with_body(body), base_name);
    }
    if (body->type == AST_IF_STMT) {
        return transpiler_has_local_binding_in_block(ast_if_then_branch(body), base_name)
            || transpiler_has_local_binding_in_block(ast_if_else_branch(body), base_name);
    }
    if (body->type == AST_WHILE_LOOP)
        return transpiler_has_local_binding_in_block(ast_while_body(body), base_name);
    if (body->type == AST_FOR_LOOP) {
        if (ast_for_variable(body) != NULL
            && strcmp(ast_for_variable(body), base_name) == 0) {
            return true;
        }
        return transpiler_has_local_binding_in_block(ast_for_body(body), base_name);
    }
    if (body->type == AST_SELECT_STMT) {
        for (size_t i = 0; i < ast_select_case_count(body); i++) {
            if (transpiler_select_case_has_receive_binding(
                    ast_select_case(body, i), base_name)) {
                return true;
            }
            if (transpiler_has_local_binding_in_block(
                    ast_select_case(body, i), base_name)) {
                return true;
            }
        }
        return transpiler_has_local_binding_in_block(
            ast_select_default_case(body), base_name);
    }
    return false;
}

bool
transpiler_has_explicit_local_binding(const ASTNode *func_decl,
                                      const char *base_name)
{
    if (func_decl == NULL || func_decl->type != AST_FUNC_DECL || base_name == NULL)
        return false;
    for (size_t i = 0; i < ast_func_param_count(func_decl); i++) {
        FuncParam *p = ast_func_param(func_decl, i);
        if (p != NULL && p->name != NULL && strcmp(p->name, base_name) == 0)
            return true;
    }
    return transpiler_has_local_binding_in_block(ast_func_body(func_decl),
        base_name);
}

void
transpiler_register_with_alias_bindings_in_block(TranspilerSSANameMap *ssa_map,
                                                 ASTNode *body)
{
    if (ssa_map == NULL || body == NULL)
        return;
    if (body->type == AST_BLOCK) {
        for (size_t i = 0; i < ast_block_statement_count(body); i++)
            transpiler_register_with_alias_bindings_in_block(ssa_map,
                ast_block_statement(body, i));
        return;
    }
    if (body->type == AST_WITH_STMT) {
        if (ast_with_alias(body) != NULL)
            transpiler_ssa_name_map_set(ssa_map,
                ast_with_alias(body), ast_with_alias(body));
        transpiler_register_with_alias_bindings_in_block(ssa_map,
            ast_with_body(body));
        return;
    }
    if (body->type == AST_LET_DESTRUCTURE) {
        for (size_t i = 0; i < ast_let_destructure_name_count(body); i++) {
            const char *pname = ast_let_destructure_name(body, i);
            if (pname != NULL)
                transpiler_ssa_name_map_set(ssa_map, pname, pname);
        }
        return;
    }
    if (body->type == AST_IF_STMT) {
        transpiler_register_with_alias_bindings_in_block(ssa_map,
            ast_if_then_branch(body));
        transpiler_register_with_alias_bindings_in_block(ssa_map,
            ast_if_else_branch(body));
        return;
    }
    if (body->type == AST_WHILE_LOOP) {
        transpiler_register_with_alias_bindings_in_block(ssa_map,
            ast_while_body(body));
        return;
    }
    if (body->type == AST_FOR_LOOP) {
        if (ast_for_variable(body) != NULL) {
            transpiler_ssa_name_map_set(ssa_map,
                ast_for_variable(body),
                ast_for_variable(body));
        }
        transpiler_register_with_alias_bindings_in_block(ssa_map,
            ast_for_body(body));
    }
}

void
transpiler_register_explicit_local_bindings_in_block(TranspilerCtx *ctx,
                                                     const ASTNode *func_decl,
                                                     ASTNode *body)
{
    if (ctx == NULL || body == NULL || body->type != AST_BLOCK)
        return;
    for (size_t i = 0; i < ast_block_statement_count(body); i++) {
        ASTNode *stmt = ast_block_statement(body, i);
        if (stmt == NULL)
            continue;
        if (stmt->type == AST_BLOCK) {
            transpiler_register_explicit_local_bindings_in_block(ctx, func_decl, stmt);
            continue;
        }
        if (stmt->type == AST_LET_DECL && ast_let_name(stmt) != NULL) {
            const char *type_name = NULL;
            char *rendered_type = NULL;
            ASTNode *initializer = ast_let_initializer(stmt);
            if (ast_let_type(stmt) != NULL) {
                rendered_type = transpiler_render_effective_local_type_name(
                    ctx, ast_let_type(stmt));
                type_name = rendered_type;
            } else if (initializer != NULL) {
                type_name = transpiler_infer_local_type_name_from_expr(
                    ctx, func_decl, initializer);
            }
            if (type_name != NULL && type_name[0] != '\0') {
                bool registered_view_like = false;
                if (transpiler_type_name_is_view_like(type_name)
                    && initializer != NULL
                    && initializer->type == AST_CALL
                    && ast_call_callee(initializer) != NULL
                    && ast_call_callee(initializer)->type == AST_IDENTIFIER
                    && ast_call_arg_count(initializer) >= 1
                    && ast_call_argument(initializer, 0) != NULL
                    && ast_call_argument(initializer, 0)->type == AST_IDENTIFIER) {
                    const char *callee =
                        ast_identifier_name(ast_call_callee(initializer));
                    const char *source =
                        ast_identifier_name(ast_call_argument(initializer, 0));
                    if (callee != NULL
                        && source != NULL
                        && pgy_codegen_call_name_is_view_constructor(callee)) {
                        const char *source_type = lookup_typed_var(ctx, source);
                        bool source_secure = lookup_slot_is_secure(ctx, source);
                        if (!source_secure
                            && source_type != NULL
                            && (strcmp(source_type, "SecureSlot") == 0
                                || strncmp(source_type, "SecureSlot<", 11) == 0)) {
                            source_secure = true;
                        }
                        register_view_like_var(ctx, ast_let_name(stmt),
                            type_name, source, source_secure, false);
                        registered_view_like = true;
                    }
                }
                if (!registered_view_like)
                    register_typed_var(ctx, ast_let_name(stmt), type_name);
                if (transpiler_type_name_is_slot_like(type_name)) {
                    char slot_inner_buf[128];
                    const char *slot_inner = NULL;
                    if (slot_inner_type_name_copy(type_name, slot_inner_buf,
                            sizeof(slot_inner_buf)))
                        slot_inner = slot_inner_buf;
                    register_slot_var(ctx, ast_let_name(stmt),
                        slot_inner,
                        strncmp(type_name, "SecureSlot<", 11) == 0,
                        false);
                }
            }
            free(rendered_type);
            continue;
        }
        if (stmt->type == AST_WITH_STMT && ast_with_alias(stmt) != NULL) {
            char *inner = render_type_name(ast_with_slot_type(stmt));
            if (inner == NULL || inner[0] == '\0'
                || strcmp(inner, "Unknown") == 0) {
                if (ctx->backend_error == NULL) {
                    transpiler_set_backend_error_with_hints(ctx,
                        PGY_CODE_C_TYPE_UNSUPPORTED,
                        PGY_CAUSE_C_TYPE_UNSUPPORTED,
                        PGY_FIX_ANNOTATE_CONCRETE_TYPE,
                        "C MIR with-slot alias '%s' requires concrete slot type metadata",
                        ast_with_alias(stmt));
                }
                free(inner);
                continue;
            }
            char *slot_type = strdup_fmt("%s<%s>",
                ast_with_is_secure(stmt) ? "SecureSlot" : "Slot",
                inner);
            if (slot_type == NULL) {
                if (ctx->backend_error == NULL) {
                    transpiler_set_backend_error_with_hints(ctx,
                        PGY_CODE_C_TYPE_UNSUPPORTED,
                        PGY_CAUSE_C_TYPE_UNSUPPORTED,
                        PGY_FIX_ANNOTATE_CONCRETE_TYPE,
                        "C MIR with-slot alias '%s' cannot synthesize slot type metadata",
                        ast_with_alias(stmt));
                }
                free(inner);
                continue;
            }
            register_typed_var(ctx, ast_with_alias(stmt), slot_type);
            register_slot_var(ctx, ast_with_alias(stmt),
                inner,
                ast_with_is_secure(stmt), false);
            free(slot_type);
            free(inner);
            transpiler_register_explicit_local_bindings_in_block(ctx, func_decl,
                ast_with_body(stmt));
            continue;
        }
        if (stmt->type == AST_IF_STMT) {
            transpiler_register_explicit_local_bindings_in_block(ctx, func_decl,
                ast_if_then_branch(stmt));
            transpiler_register_explicit_local_bindings_in_block(ctx, func_decl,
                ast_if_else_branch(stmt));
            continue;
        }
        if (stmt->type == AST_WHILE_LOOP) {
            transpiler_register_explicit_local_bindings_in_block(ctx, func_decl,
                ast_while_body(stmt));
            continue;
        }
        if (stmt->type == AST_FOR_LOOP) {
            if (ast_for_variable(stmt) != NULL) {
                register_typed_var(ctx, ast_for_variable(stmt), "Int");
            }
            transpiler_register_explicit_local_bindings_in_block(ctx, func_decl,
                ast_for_body(stmt));
        }
    }
}
