#include "transpiler_mir_local_type_lookup.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../parser/ast_api.h"

#include "codegen_slot_type_policy.h"
#include "transpiler_decl_lookup.h"
#include "transpiler_expr_type_infer.h"
#include "transpiler_let_slot_emit.h"
#include "transpiler_mir_effective_type.h"
#include "transpiler_nominal.h"
#include "transpiler_symbols.h"
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
        return ast_number_is_long(expr) ? "Long" : "Int";
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
                ASTNode *obj_decl = find_class_decl(ctx, obj_type);
                if (obj_decl != NULL) {
                    size_t field_count = 0;
                    ClassField **fields = ast_class_fields(obj_decl, &field_count);
                    for (size_t fi = 0; fi < field_count; fi++) {
                        ClassField *f = fields != NULL ? fields[fi] : NULL;
                        if (f != NULL && f->name != NULL && f->type != NULL
                            && strcmp(f->name, ast_member_name(expr)) == 0) {
                            char *rendered = render_type_name_in_ctx(ctx,
                                f->type);
                            const char *copied =
                                transpiler_mir_arena_copy_type_name(ctx, rendered);
                            free(rendered);
                            return copied;
                        }
                    }
                }
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
            if (receiver_type != NULL
                && method_name != NULL
                && strcmp(method_name, "Slice") == 0
                && (strncmp(receiver_type, "Array<", 6) == 0
                    || strncmp(receiver_type, "Slice<", 6) == 0)) {
                char inner_buf[128];
                if (!slot_inner_type_name_copy(receiver_type, inner_buf,
                        sizeof(inner_buf))
                    || inner_buf[0] == '\0')
                    return NULL;
                return transpiler_mir_arena_render_type_name(
                    ctx, "Slice", inner_buf);
            }
            if (receiver_type != NULL)
                method_decl = find_nominal_host_method_decl(ctx, receiver_type, method_name);
            ASTNode *method_return_type = ast_func_return_type(method_decl);
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
            ASTNode *callee_decl = find_function_decl(ctx, callee_name);
            ASTNode *callee_return_type = ast_func_return_type(callee_decl);
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
            if (init_type != NULL
                && (strncmp(init_type, "Array<", 6) == 0
                    || strncmp(init_type, "Slice<", 6) == 0)) {
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
