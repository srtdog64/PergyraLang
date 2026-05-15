#ifndef PGY_TRANSPILER_MIR_SSA_EMIT_H
#define PGY_TRANSPILER_MIR_SSA_EMIT_H

#include "../parser/ast_api.h"

#include "transpiler_mir_local_type_ast_lookup.h"

static char *
transpiler_render_effective_local_type_name(TranspilerCtx *ctx, ASTNode *type_node)
{
    if (type_node != NULL
        && type_node->type == AST_TYPE
        && ast_type_name(type_node) != NULL) {
        ASTNode *class_decl = find_class_decl(ctx, ast_type_name(type_node));
        if (class_decl != NULL && class_has_generic_params(class_decl)) {
            const char *spec_name =
                ensure_generic_class_specialization(ctx, class_decl, type_node);
            if (spec_name != NULL && strcmp(spec_name, ast_type_name(type_node)) != 0)
                return pergyra_strdup(spec_name);
        }
    }
    return render_type_name(type_node);
}

static void
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

static const char *
transpiler_lookup_current_owner_member_type_name(TranspilerCtx *ctx,
                                                 const char *member_name)
{
    const char *member_type = NULL;
    const char *host_name = NULL;
    ASTNode *host_decl = NULL;

    if (ctx == NULL || member_name == NULL)
        return NULL;

    host_decl = transpiler_current_host_decl_local(ctx);
    if (host_decl != NULL) {
        switch (host_decl->type) {
        case AST_CLASS_DECL:
            host_name = ast_class_name(host_decl);
            break;
        case AST_RELATION_DECL:
            host_name = ast_relation_name(host_decl);
            break;
        case AST_EFFECT_DECL:
            host_name = ast_effect_name(host_decl);
            break;
        case AST_ZONE_DECL:
            host_name = ast_zone_name(host_decl);
            break;
        case AST_WORLD_DECL:
            host_name = ast_world_name(host_decl);
            break;
        default:
            break;
        }
        if (host_name != NULL) {
            member_type = transpiler_lookup_nominal_host_member_type_name(ctx,
                host_name, member_name);
            if (member_type != NULL)
                return member_type;
        }
    }

    return NULL;
}

static const char *
transpiler_find_local_type_name(TranspilerCtx *ctx,
                                const ASTNode *func_decl,
                                const char *base_name)
{
    const char *typed_name = NULL;

    if (base_name == NULL)
        return NULL;
    if (ctx != NULL) {
        typed_name = lookup_typed_var(ctx, base_name);
        if (typed_name != NULL && strcmp(typed_name, "Unknown") != 0)
            return typed_name;
    }
    if (func_decl == NULL || func_decl->type != AST_FUNC_DECL)
        return transpiler_lookup_current_owner_member_type_name(ctx, base_name);
    size_t param_count = ast_func_param_count(func_decl);
    for (size_t i = 0; i < param_count; i++) {
        FuncParam *p = ast_func_param(func_decl, i);
        if (p != NULL && p->name != NULL && strcmp(p->name, base_name) == 0 && p->type != NULL) {
            char *owned_param =
                transpiler_render_effective_local_type_name(ctx, p->type);
            const char *rendered_param =
                transpiler_mir_arena_copy_type_name(ctx, owned_param);
            if (rendered_param == NULL) {
                free(owned_param);
                return NULL;
            }
            free(owned_param);
            if (ctx != NULL)
                register_typed_var(ctx, base_name, rendered_param);
            return rendered_param;
        }
    }
    typed_name = transpiler_find_local_type_name_in_block(ctx, func_decl,
        ast_func_body(func_decl), base_name);
    if (typed_name != NULL) {
        if (ctx != NULL)
            register_typed_var(ctx, base_name, typed_name);
        return typed_name;
    }

    return transpiler_lookup_current_owner_member_type_name(ctx, base_name);
}

#include "transpiler_mir_ssa_lookup.h"

#endif /* PGY_TRANSPILER_MIR_SSA_EMIT_H */
