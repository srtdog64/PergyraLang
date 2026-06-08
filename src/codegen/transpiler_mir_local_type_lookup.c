#include "transpiler_mir_local_type_lookup.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../parser/ast_api.h"

#include "codegen_slot_type_policy.h"
#include "transpiler_context.h"
#include "transpiler_decl_lookup.h"
#include "transpiler_expr_type_infer.h"
#include "transpiler_generic_param_query.h"
#include "transpiler_let_slot_emit.h"
#include "transpiler_inventory_view.h"
#include "transpiler_mir_effective_type.h"
#include "transpiler_mir_inventory_intent_collect.h"
#include "transpiler_mir_signature.h"
#include "transpiler_nominal.h"
#include "transpiler_symbols.h"
#include "transpiler_type_mapping.h"
#include "transpiler_type_render.h"

/* Consumed from transpiler_mir_ssa_names.h. Keep slot claim vocabulary in the
 * shared codegen slot policy instead of repeating raw builtin strings here. */

const char *
transpiler_mir_arena_copy_type_name(TranspilerCtx *ctx, const char *type_name)
{
    if (ctx == NULL || type_name == NULL)
        return NULL;
    return pgy_arena_strdup(&ctx->arena, type_name);
}

const char *
transpiler_mir_arena_render_type_name(TranspilerCtx *ctx,
                                      const char *prefix,
                                      const char *inner)
{
    if (ctx == NULL || prefix == NULL || inner == NULL)
        return NULL;
    return pgy_arena_fmt(&ctx->arena, "%s<%s>", prefix, inner);
}

const char *
transpiler_find_local_type_name(TranspilerCtx *ctx,
                                const ASTNode *func_decl,
                                const char *base_name);

const char *
transpiler_infer_local_type_name_from_expr(TranspilerCtx *ctx,
                                           const ASTNode *func_decl,
                                           ASTNode *expr)
{
    const char *semantic_type = infer_expression_type_name(ctx, expr);
    if (semantic_type != NULL
        && semantic_type[0] != '\0'
        && strcmp(semantic_type, "Int") != 0)
        return semantic_type;
    if (expr == NULL)
        return NULL;
    switch (expr->type) {
    case AST_NUMBER:
        if (ast_number_is_long(expr))
            return "Long";
        return ast_number_is_float(expr) ? "Float" : "Int";
    case AST_STRING:
        return "String";
    case AST_BOOLEAN:
        return "Bool";
    case AST_IDENTIFIER:
        return transpiler_find_local_type_name(ctx, func_decl,
                                               ast_identifier_name(expr));
    case AST_CHANNEL_RECV: {
        const char *channel_type = transpiler_infer_local_type_name_from_expr(
            ctx, func_decl, ast_channel_recv_channel(expr));
        char inner_buf[128];
        if (slot_inner_type_name_copy(channel_type, inner_buf,
                sizeof(inner_buf))
            && inner_buf[0] != '\0') {
            return transpiler_mir_arena_copy_type_name(ctx, inner_buf);
        }
        return NULL;
    }
    case AST_MEMBER_ACCESS: {
        const char *resolved =
            transpiler_resolve_nominal_host_expr_type_name(ctx, expr);
        if (resolved != NULL && resolved[0] != '\0')
            return resolved;
        if (ast_member_object(expr) != NULL && ast_member_name(expr) != NULL) {
            const char *obj_type = transpiler_infer_local_type_name_from_expr(
                ctx, func_decl, ast_member_object(expr));
            if (obj_type != NULL) {
                const char *member_type =
                    transpiler_lookup_nominal_host_member_type_name(
                        ctx, obj_type, ast_member_name(expr));
                if (member_type != NULL)
                    return member_type;
            }
        }
        return semantic_type != NULL && semantic_type[0] != '\0'
            ? semantic_type
            : NULL;
    }
    case AST_BINARY:
        switch (ast_binary_operator(expr).type) {
        case TOKEN_EQUAL:
        case TOKEN_NOT_EQUAL:
        case TOKEN_LESS:
        case TOKEN_GREATER:
        case TOKEN_LESS_EQUAL:
        case TOKEN_GREATER_EQUAL:
        case TOKEN_AND:
        case TOKEN_OR:
            return "Bool";
        default:
            return transpiler_infer_local_type_name_from_expr(
                ctx, func_decl, ast_binary_left(expr));
        }
    case AST_UNARY:
        if (ast_unary_operator(expr).type == TOKEN_NOT)
            return "Bool";
        return transpiler_infer_local_type_name_from_expr(
            ctx, func_decl, ast_unary_operand(expr));
    case AST_CALL:
        if (ast_call_callee(expr) != NULL
            && ast_call_callee(expr)->type == AST_MEMBER_ACCESS
            && ast_member_name(ast_call_callee(expr)) != NULL) {
            ASTNode *receiver = ast_member_object(ast_call_callee(expr));
            const char *method_name = ast_member_name(ast_call_callee(expr));
            const char *receiver_type = transpiler_infer_local_type_name_from_expr(
                ctx, func_decl, receiver);
            ASTNode *method_decl = NULL;
            const MIRDeclMethod *method_meta = NULL;
            ASTNode *method_return_type = NULL;
            if (receiver_type != NULL
                && method_name != NULL
                && strcmp(method_name, "Slice") == 0
                && transpiler_type_name_is_array_or_slice(receiver_type)) {
                char inner_buf[128];
                if (!slot_inner_type_name_copy(receiver_type, inner_buf,
                        sizeof(inner_buf))
                    || inner_buf[0] == '\0')
                    return NULL;
                return transpiler_mir_arena_render_type_name(
                    ctx, "Slice", inner_buf);
            }
            if (receiver_type != NULL) {
                const char *method_return_type_name = NULL;
                method_meta = transpiler_find_host_method_metadata_in_context(
                    ctx, receiver_type, method_name);
                if (!transpiler_mir_decl_method_metadata_complete_for(ctx,
                        method_meta,
                        receiver_type,
                        method_name,
                        TRANSPILER_MIR_DECL_METHOD_REQUIRE_RETURN_TYPE_NAME,
                        "MIR-only C path missing MIR local member-call return type-name metadata for '%s.%s'",
                        NULL)) {
                    return NULL;
                }
                method_return_type_name =
                    transpiler_mir_decl_method_return_type_name(method_meta);
                if (method_return_type_name != NULL)
                    return transpiler_mir_arena_copy_type_name(
                        ctx, method_return_type_name);
                method_return_type =
                    transpiler_mir_decl_method_return_type(method_meta);
                if (method_return_type == NULL && method_meta == NULL
                    && !transpiler_active_has_mir(ctx)) {
                    method_decl = find_nominal_host_method_decl(
                        ctx, receiver_type, method_name);
                    method_return_type = ast_func_return_type(method_decl);
                }
            }
            if (method_return_type != NULL) {
                char *rendered = render_type_name_in_ctx(ctx,
                    method_return_type);
                const char *copied =
                    transpiler_mir_arena_copy_type_name(ctx, rendered);
                free(rendered);
                return copied;
            }
        }
        if (ast_call_callee(expr) != NULL
            && ast_call_callee(expr)->type == AST_IDENTIFIER
            && ast_identifier_name(ast_call_callee(expr)) != NULL) {
            const char *callee_name = ast_identifier_name(ast_call_callee(expr));
            ASTNode *callee_decl = find_callable_decl(ctx, callee_name);
            ASTNode *callee_return_type = NULL;
            /* Unqualified call inside a host method body (e.g.
             * `let gained = TakeLoot(...)` inside world RaidWorld where
             * TakeLoot is a method on the world) needs the host-method
             * lookup, not just the global callable. Try it before
             * falling through to the unresolved branch. */
            if (callee_decl == NULL) {
                ASTNode *host_decl = transpiler_current_host_decl_local(ctx);
                const char *host_name = host_decl != NULL
                    ? ast_declaration_name(host_decl) : NULL;
                if (host_name != NULL && callee_name != NULL) {
                    const MIRDeclMethod *method_meta =
                        transpiler_find_host_method_metadata_in_context(
                            ctx, host_name, callee_name);
                    if (method_meta != NULL) {
                        const char *method_return_type_name =
                            transpiler_mir_decl_method_return_type_name(
                                method_meta);
                        if (method_return_type_name != NULL)
                            return transpiler_mir_arena_copy_type_name(
                                ctx, method_return_type_name);
                    }
                    if (!transpiler_active_has_mir(ctx)) {
                        ASTNode *method_decl =
                            find_nominal_host_method_decl(
                                ctx, host_name, callee_name);
                        if (method_decl != NULL) {
                            ASTNode *ret = ast_func_return_type(method_decl);
                            if (ret != NULL) {
                                char *rendered =
                                    render_type_name_in_ctx(ctx, ret);
                                const char *copied =
                                    transpiler_mir_arena_copy_type_name(
                                        ctx, rendered);
                                free(rendered);
                                return copied;
                            }
                        }
                    }
                }
            }
            if (callee_decl != NULL && callee_decl->type == AST_INTENT_DECL)
                return "Bool";
            bool generic_call = callee_decl != NULL
                && callee_decl->type == AST_FUNC_DECL
                && transpiler_func_has_generic_params(callee_decl);
            bool extern_func = callee_decl != NULL
                && callee_decl->type == AST_FUNC_DECL
                && transpiler_decl_is_extern_function(ctx, callee_decl);
            if (callee_decl != NULL && callee_decl->type == AST_FUNC_DECL
                && transpiler_active_has_mir(ctx)
                && !generic_call
                && !extern_func) {
                const MIRRoutine *callee_routine =
                    transpiler_find_mir_function(ctx, callee_decl);
                if (callee_routine == NULL) {
                    transpiler_set_mir_inventory_missing(
                        ctx,
                        "MIR-only C path missing function call routine metadata for '%s'",
                        callee_name);
                    return NULL;
                }
                if (!transpiler_mir_routine_signature_metadata_complete_for(
                        ctx,
                        callee_routine,
                        callee_decl,
                        TRANSPILER_MIR_SIGNATURE_REQUIRE_RETURN_TYPE_NAME,
                        "MIR-only C path missing function call return signature metadata for '%s'",
                        "MIR-only C path missing function call return type-name metadata for '%s'",
                        NULL)) {
                    return NULL;
                }
                const char *return_type_name =
                    transpiler_mir_routine_return_type_name(callee_routine);
                if (return_type_name != NULL)
                    return transpiler_mir_arena_copy_type_name(
                        ctx, return_type_name);
                callee_return_type =
                    transpiler_mir_routine_return_type(callee_routine);
            } else if (!transpiler_active_has_mir(ctx)
                       || generic_call || extern_func) {
                callee_return_type = callee_decl != NULL
                    && callee_decl->type == AST_FUNC_DECL
                        ? ast_func_return_type(callee_decl)
                        : NULL;
            } else {
                callee_return_type = NULL;
            }
            if (callee_return_type != NULL) {
                char *rendered = render_type_name_in_ctx(ctx,
                    callee_return_type);
                const char *copied =
                    transpiler_mir_arena_copy_type_name(ctx, rendered);
                free(rendered);
                return copied;
            }
            if (find_class_decl(ctx, callee_name) != NULL
                || transpiler_find_domain_constructor_decl_local(
                    ctx, callee_name) != NULL) {
                return callee_name;
            }
        }
        return semantic_type != NULL && semantic_type[0] != '\0'
            ? semantic_type
            : NULL;
    default:
        return NULL;
    }
}

const char *
transpiler_mir_for_loop_variable_type_name(TranspilerCtx *ctx,
                                           const ASTNode *func_decl,
                                           ASTNode *for_loop)
{
    ASTNode *iterable;
    const char *iterable_type;
    char inner_buf[128];

    if (for_loop == NULL || for_loop->type != AST_FOR_LOOP
        || ast_for_variable(for_loop) == NULL) {
        return NULL;
    }

    iterable = ast_for_iterable(for_loop);
    if (iterable == NULL)
        return "Int";

    iterable_type = transpiler_infer_local_type_name_from_expr(
        ctx, func_decl, iterable);
    if (!transpiler_type_name_is_array_or_slice(iterable_type)
        && !transpiler_type_name_is_list(iterable_type)) {
        return NULL;
    }
    if (!slot_inner_type_name_copy(iterable_type, inner_buf,
            sizeof(inner_buf))
        || inner_buf[0] == '\0'
        || strcmp(inner_buf, "Unknown") == 0) {
        return NULL;
    }
    return transpiler_mir_arena_copy_type_name(ctx, inner_buf);
}

static const char *
transpiler_find_local_type_name_in_block(TranspilerCtx *ctx,
                                         const ASTNode *func_decl,
                                         ASTNode *body,
                                         const char *base_name)
{
    if (body == NULL || base_name == NULL)
        return NULL;
    if (body->type == AST_BLOCK) {
        for (size_t i = 0; i < ast_block_statement_count(body); i++) {
            const char *found = transpiler_find_local_type_name_in_block(
                ctx, func_decl, ast_block_statement(body, i), base_name);
            if (found != NULL)
                return found;
        }
        return NULL;
    }
    if (body->type == AST_LET_DECL
        && ast_let_name(body) != NULL
        && strcmp(ast_let_name(body), base_name) == 0) {
        ASTNode *let_type = ast_let_type(body);
        ASTNode *let_init = ast_let_initializer(body);
        if (let_type != NULL) {
            char *effective = transpiler_render_effective_local_type_name(
                ctx, let_type);
            const char *copied =
                transpiler_mir_arena_copy_type_name(ctx, effective);
            free(effective);
            return copied;
        }
        return transpiler_infer_local_type_name_from_expr(
            ctx, func_decl, let_init);
    }
    if (body->type == AST_ASSIGNMENT
        && ast_assignment_target(body) != NULL
        && ast_assignment_target(body)->type == AST_IDENTIFIER
        && ast_identifier_name(ast_assignment_target(body)) != NULL
        && strcmp(ast_identifier_name(ast_assignment_target(body)),
                  base_name) == 0
        && ast_assignment_value(body) != NULL
        && ast_assignment_value(body)->type == AST_CHANNEL_RECV) {
        return transpiler_infer_local_type_name_from_expr(
            ctx, func_decl, ast_assignment_value(body));
    }
    if (body->type == AST_LET_DESTRUCTURE) {
        for (size_t i = 0; i < ast_let_destructure_name_count(body); i++) {
            const char *pname = ast_let_destructure_name(body, i);
            if (pname == NULL || strcmp(pname, base_name) != 0)
                continue;
            ASTNode *init = ast_let_destructure_initializer(body);
            if (init != NULL
                && init->type == AST_CALL
                && ast_call_callee(init) != NULL
                && ast_call_callee(init)->type == AST_IDENTIFIER
                && ast_identifier_name(ast_call_callee(init)) != NULL) {
                const char *callee = ast_identifier_name(ast_call_callee(init));
                if (pgy_codegen_call_name_is_claim_secure_slot(callee)) {
                    const char *inner = NULL;
                    if (ast_call_generic_arg(init, 0) != NULL) {
                        inner = transpiler_let_slot_inner_from_call_type_arg(ctx, init);
                    } else {
                        const char *init_type = infer_expression_type_name(ctx, init);
                        if (init_type != NULL && strncmp(init_type, "SecureSlot<", 11) == 0) {
                            char resolved_inner_buf[128];
                            if (slot_inner_type_name_copy(init_type,
                                    resolved_inner_buf,
                                    sizeof(resolved_inner_buf))
                                && resolved_inner_buf[0] != '\0') {
                                inner = transpiler_mir_arena_copy_type_name(
                                    ctx, resolved_inner_buf);
                            }
                        }
                    }
                    if (inner == NULL || inner[0] == '\0')
                        return NULL;
                    return transpiler_mir_arena_render_type_name(
                        ctx, i == 0 ? "SecureSlot" : "Token", inner);
                }
                if (pgy_codegen_call_name_is_claim_slot(callee) && i == 0) {
                    const char *inner = NULL;
                    if (ast_call_generic_arg(init, 0) != NULL) {
                        inner = transpiler_let_slot_inner_from_call_type_arg(ctx, init);
                    }
                    if (inner == NULL || inner[0] == '\0')
                        return NULL;
                    return transpiler_mir_arena_render_type_name(ctx, "Slot", inner);
                }
            }
            const char *init_type = infer_expression_type_name(ctx, init);
            if ((init_type == NULL || strcmp(init_type, "Unknown") == 0)
                && init != NULL
                && init->type == AST_IDENTIFIER
                && ast_identifier_name(init) != NULL) {
                const char *resolved = transpiler_find_local_type_name(
                    ctx, func_decl, ast_identifier_name(init));
                if (resolved != NULL)
                    init_type = resolved;
            }
            if (transpiler_type_name_is_array_or_slice(init_type)) {
                char inner_buf[128];
                if (slot_inner_type_name_copy(init_type, inner_buf,
                        sizeof(inner_buf))) {
                    return transpiler_mir_arena_copy_type_name(ctx, inner_buf);
                }
            }
            if (init_type != NULL && init_type[0] == '(') {
                size_t idx = i;
                size_t pi = 1;
                size_t plen = strlen(init_type);
                size_t cur = 0;
                while (pi < plen && init_type[pi] != ')') {
                    while (pi < plen && (init_type[pi] == ' ' || init_type[pi] == '\t'))
                        pi++;
                    char rendered_tup[128];
                    size_t eo = 0;
                    int depth = 0;
                    while (pi < plen && eo + 1 < sizeof(rendered_tup)) {
                        char c = init_type[pi];
                        if (depth == 0 && (c == ',' || c == ')'))
                            break;
                        if (c == '<' || c == '(')
                            depth++;
                        if (c == '>' || c == ')')
                            depth--;
                        rendered_tup[eo++] = c;
                        pi++;
                    }
                    rendered_tup[eo] = '\0';
                    while (eo > 0 && (rendered_tup[eo - 1] == ' '
                                      || rendered_tup[eo - 1] == '\t')) {
                        rendered_tup[--eo] = '\0';
                    }
                    if (cur == idx)
                        return transpiler_mir_arena_copy_type_name(ctx, rendered_tup);
                    cur++;
                    if (pi < plen && init_type[pi] == ',')
                        pi++;
                }
            }
            return NULL;
        }
    }
    if (body->type == AST_WITH_STMT) {
        if (ast_with_alias(body) != NULL
            && strcmp(ast_with_alias(body), base_name) == 0) {
            char *inner = render_type_name_in_ctx(ctx,
                ast_with_slot_type(body));
            const char *rendered_slot;
            if (inner == NULL || inner[0] == '\0')
                return NULL;
            rendered_slot = transpiler_mir_arena_render_type_name(
                ctx,
                ast_with_is_secure(body) ? "SecureSlot" : "Slot",
                inner);
            free(inner);
            return rendered_slot;
        }
        return transpiler_find_local_type_name_in_block(
            ctx, func_decl, ast_with_body(body), base_name);
    }
    if (body->type == AST_IF_STMT) {
        const char *found = transpiler_find_local_type_name_in_block(
            ctx, func_decl, ast_if_then_branch(body), base_name);
        if (found != NULL)
            return found;
        return transpiler_find_local_type_name_in_block(
            ctx, func_decl, ast_if_else_branch(body), base_name);
    }
    if (body->type == AST_WHILE_LOOP) {
        return transpiler_find_local_type_name_in_block(
            ctx, func_decl, ast_while_body(body), base_name);
    }
    if (body->type == AST_FOR_LOOP) {
        if (ast_for_variable(body) != NULL
            && strcmp(ast_for_variable(body), base_name) == 0) {
            return transpiler_mir_for_loop_variable_type_name(ctx,
                func_decl, body);
        }
        return transpiler_find_local_type_name_in_block(
            ctx, func_decl, ast_for_body(body), base_name);
    }
    if (body->type == AST_SELECT_STMT) {
        for (size_t i = 0; i < ast_select_case_count(body); i++) {
            const char *found = transpiler_find_local_type_name_in_block(
                ctx, func_decl, ast_select_case(body, i), base_name);
            if (found != NULL)
                return found;
        }
        return transpiler_find_local_type_name_in_block(
            ctx, func_decl, ast_select_default_case(body), base_name);
    }
    if (body->type == AST_MATCH_STMT) {
        for (size_t i = 0; i < ast_match_case_count(body); i++) {
            ASTNode *mc = ast_match_case_at(body, i);
            if (mc == NULL)
                continue;
            const char *found = transpiler_find_local_type_name_in_block(
                ctx, func_decl, ast_match_case_body(mc), base_name);
            if (found != NULL)
                return found;
        }
        return transpiler_find_local_type_name_in_block(
            ctx, func_decl, ast_match_default_body(body), base_name);
    }
    return NULL;
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
        case AST_RELATION_DECL:
        case AST_EFFECT_DECL:
        case AST_ZONE_DECL:
        case AST_WORLD_DECL:
            host_name = transpiler_decl_name_local(host_decl);
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

const char *
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
    const MIRRoutine *routine =
        transpiler_find_mir_function(ctx, func_decl);
    if (routine != NULL) {
        if (!transpiler_mir_routine_signature_metadata_complete_for(ctx,
                routine,
                func_decl,
                TRANSPILER_MIR_SIGNATURE_REQUIRE_PARAM_TYPE_NAMES,
                "MIR-only C path missing local parameter signature metadata for '%s'",
                NULL,
                "MIR-only C path missing local parameter type-name metadata for '%s'")) {
            return NULL;
        }
        for (size_t i = 0;
             i < transpiler_mir_routine_param_count(routine);
             i++) {
            FuncParam *p = transpiler_mir_routine_param(routine, i);
            const char *param_type_name =
                transpiler_mir_routine_param_type_name(routine, i);
            if (p == NULL || p->name == NULL
                || strcmp(p->name, base_name) != 0) {
                continue;
            }
            if (param_type_name != NULL && param_type_name[0] != '\0') {
                const char *rendered_param =
                    transpiler_mir_arena_copy_type_name(ctx, param_type_name);
                if (ctx != NULL && rendered_param != NULL)
                    register_typed_var(ctx, base_name, rendered_param);
                return rendered_param;
            }
            if (p->type != NULL) {
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
    } else if (!transpiler_active_has_mir(ctx)) {
        size_t param_count = ast_func_param_count(func_decl);
        for (size_t i = 0; i < param_count; i++) {
            FuncParam *p = ast_func_param(func_decl, i);
            if (p != NULL && p->name != NULL
                && strcmp(p->name, base_name) == 0 && p->type != NULL) {
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
    }
    /* The AST body scan is a constrained fallback that still runs
     * with MIR active: hosted self-calls (e.g. `let gained =
     * TakeLoot(...)` inside a world method) need to resolve their
     * initializer types through `let`-decl inspection because the
     * MIR routine inventory doesn't propagate the receiver-less call
     * shape into a versioned-local type fact. We pass through
     * `transpiler_find_local_type_name_in_block` here, but every
     * leaf type decision still goes back through the MIR-aware
     * `infer_expression_type_name`/host-method metadata path. */
    typed_name = transpiler_find_local_type_name_in_block(ctx, func_decl,
        ast_func_body(func_decl), base_name);
    if (typed_name != NULL) {
        if (ctx != NULL)
            register_typed_var(ctx, base_name, typed_name);
        return typed_name;
    }

    return transpiler_lookup_current_owner_member_type_name(ctx, base_name);
}
