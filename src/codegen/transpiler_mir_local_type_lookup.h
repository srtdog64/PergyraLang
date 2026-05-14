#ifndef PGY_TRANSPILER_MIR_LOCAL_TYPE_LOOKUP_H
#define PGY_TRANSPILER_MIR_LOCAL_TYPE_LOOKUP_H

#include <stdbool.h>
#include <stdio.h>

/* Consumed from transpiler_mir_ssa_names.h. Keep slot claim vocabulary in the
 * shared codegen slot policy instead of repeating raw builtin strings here. */

static const char *
transpiler_mir_arena_copy_type_name(TranspilerCtx *ctx, const char *type_name)
{
    if (ctx == NULL || type_name == NULL)
        return NULL;
    return pgy_arena_strdup(&ctx->arena, type_name);
}

static const char *
transpiler_mir_arena_render_type_name(TranspilerCtx *ctx,
                                      const char *prefix,
                                      const char *inner)
{
    if (ctx == NULL || prefix == NULL || inner == NULL)
        return NULL;
    return pgy_arena_fmt(&ctx->arena, "%s<%s>", prefix, inner);
}

static const char *
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
        return expr->data.number.is_long ? "Long" : "Int";
    case AST_STRING:
        return "String";
    case AST_BOOLEAN:
        return "Bool";
    case AST_IDENTIFIER:
        return transpiler_find_local_type_name(ctx, func_decl,
                                               expr->data.identifier.name);
    case AST_CHANNEL_RECV: {
        const char *channel_type = transpiler_infer_local_type_name_from_expr(
            ctx, func_decl, expr->data.channel_recv.channel);
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
        if (expr->data.member.object != NULL && expr->data.member.name != NULL) {
            const char *obj_type = transpiler_infer_local_type_name_from_expr(
                ctx, func_decl, expr->data.member.object);
            if (obj_type != NULL) {
                ASTNode *obj_decl = find_class_decl(ctx, obj_type);
                if (obj_decl != NULL) {
                    size_t field_count = 0;
                    ClassField **fields = ast_class_fields(obj_decl, &field_count);
                    for (size_t fi = 0; fi < field_count; fi++) {
                        ClassField *f = fields != NULL ? fields[fi] : NULL;
                        if (f != NULL && f->name != NULL && f->type != NULL
                            && strcmp(f->name, expr->data.member.name) == 0) {
                            char *rendered = render_type_name(f->type);
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
        switch (expr->data.binary.op.type) {
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
                ctx, func_decl, expr->data.binary.left);
        }
    case AST_UNARY:
        if (expr->data.unary.op.type == TOKEN_NOT)
            return "Bool";
        return transpiler_infer_local_type_name_from_expr(
            ctx, func_decl, expr->data.unary.operand);
    case AST_CALL:
        if (expr->data.call.callee != NULL
            && expr->data.call.callee->type == AST_MEMBER_ACCESS
            && expr->data.call.callee->data.member.name != NULL) {
            ASTNode *receiver = expr->data.call.callee->data.member.object;
            const char *method_name = expr->data.call.callee->data.member.name;
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
            if (method_decl != NULL && method_decl->type == AST_FUNC_DECL
                && method_decl->data.func_decl.return_type != NULL) {
                char *rendered =
                    render_type_name(method_decl->data.func_decl.return_type);
                const char *copied =
                    transpiler_mir_arena_copy_type_name(ctx, rendered);
                free(rendered);
                return copied;
            }
        }
        if (expr->data.call.callee != NULL
            && expr->data.call.callee->type == AST_IDENTIFIER
            && expr->data.call.callee->data.identifier.name != NULL) {
            const char *callee_name = expr->data.call.callee->data.identifier.name;
            ASTNode *callee_decl = find_function_decl(ctx, callee_name);
            if (callee_decl != NULL
                && callee_decl->type == AST_FUNC_DECL
                && callee_decl->data.func_decl.return_type != NULL) {
                char *rendered =
                    render_type_name(callee_decl->data.func_decl.return_type);
                const char *copied =
                    transpiler_mir_arena_copy_type_name(ctx, rendered);
                free(rendered);
                return copied;
            }
            if (find_class_decl(ctx, callee_name) != NULL
                || find_zone_decl(ctx, callee_name) != NULL
                || find_world_decl(ctx, callee_name) != NULL
                || find_relation_decl(ctx, callee_name) != NULL
                || find_effect_decl(ctx, callee_name) != NULL
                || find_party_decl(ctx, callee_name) != NULL
                || find_roster_decl(ctx, callee_name) != NULL) {
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
        for (size_t i = 0; i < body->data.block.count; i++) {
            const char *found = transpiler_find_local_type_name_in_block(
                ctx, func_decl, body->data.block.statements[i], base_name);
            if (found != NULL)
                return found;
        }
        return NULL;
    }
    if (body->type == AST_LET_DECL
        && body->data.let_decl.name != NULL
        && strcmp(body->data.let_decl.name, base_name) == 0) {
        if (body->data.let_decl.type != NULL) {
            char *effective = transpiler_render_effective_local_type_name(
                ctx, body->data.let_decl.type);
            const char *copied =
                transpiler_mir_arena_copy_type_name(ctx, effective);
            free(effective);
            return copied;
        }
        return transpiler_infer_local_type_name_from_expr(
            ctx, func_decl, body->data.let_decl.initializer);
    }
    if (body->type == AST_ASSIGNMENT
        && body->data.assignment.target != NULL
        && body->data.assignment.target->type == AST_IDENTIFIER
        && body->data.assignment.target->data.identifier.name != NULL
        && strcmp(body->data.assignment.target->data.identifier.name,
                  base_name) == 0
        && body->data.assignment.value != NULL
        && body->data.assignment.value->type == AST_CHANNEL_RECV) {
        return transpiler_infer_local_type_name_from_expr(
            ctx, func_decl, body->data.assignment.value);
    }
    if (body->type == AST_LET_DESTRUCTURE) {
        for (size_t i = 0; i < body->data.let_destructure.name_count; i++) {
            const char *pname = body->data.let_destructure.names[i];
            if (pname == NULL || strcmp(pname, base_name) != 0)
                continue;
            ASTNode *init = body->data.let_destructure.initializer;
            if (init != NULL
                && init->type == AST_CALL
                && init->data.call.callee != NULL
                && init->data.call.callee->type == AST_IDENTIFIER
                && init->data.call.callee->data.identifier.name != NULL) {
                const char *callee = init->data.call.callee->data.identifier.name;
                if (pgy_codegen_call_name_is_claim_secure_slot(callee)) {
                    const char *inner = NULL;
                    if (init->data.call.generic_args != NULL
                        && init->data.call.generic_args->count > 0
                        && init->data.call.generic_args->params[0] != NULL) {
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
                    if (init->data.call.generic_args != NULL
                        && init->data.call.generic_args->count > 0
                        && init->data.call.generic_args->params[0] != NULL) {
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
                && init->data.identifier.name != NULL) {
                const char *resolved = transpiler_find_local_type_name(
                    ctx, func_decl, init->data.identifier.name);
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
        if (body->data.with_stmt.alias != NULL
            && strcmp(body->data.with_stmt.alias, base_name) == 0) {
            char *inner = render_type_name(body->data.with_stmt.slot_type);
            const char *rendered_slot;
            if (inner == NULL || inner[0] == '\0')
                return NULL;
            rendered_slot = transpiler_mir_arena_render_type_name(
                ctx,
                body->data.with_stmt.is_secure ? "SecureSlot" : "Slot",
                inner);
            free(inner);
            return rendered_slot;
        }
        return transpiler_find_local_type_name_in_block(
            ctx, func_decl, body->data.with_stmt.body, base_name);
    }
    if (body->type == AST_IF_STMT) {
        const char *found = transpiler_find_local_type_name_in_block(
            ctx, func_decl, body->data.if_stmt.then_branch, base_name);
        if (found != NULL)
            return found;
        return transpiler_find_local_type_name_in_block(
            ctx, func_decl, body->data.if_stmt.else_branch, base_name);
    }
    if (body->type == AST_WHILE_LOOP) {
        return transpiler_find_local_type_name_in_block(
            ctx, func_decl, body->data.while_loop.body, base_name);
    }
    if (body->type == AST_SELECT_STMT) {
        for (size_t i = 0; i < body->data.select_stmt.case_count; i++) {
            const char *found = transpiler_find_local_type_name_in_block(
                ctx, func_decl, body->data.select_stmt.cases[i], base_name);
            if (found != NULL)
                return found;
        }
        return transpiler_find_local_type_name_in_block(
            ctx, func_decl, body->data.select_stmt.default_case, base_name);
    }
    return NULL;
}

#endif /* PGY_TRANSPILER_MIR_LOCAL_TYPE_LOOKUP_H */
