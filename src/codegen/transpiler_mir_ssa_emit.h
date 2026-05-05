#include "transpiler_mir_local_type_ast_lookup.h"

static char *
transpiler_render_effective_local_type_name(TranspilerCtx *ctx, ASTNode *type_node)
{
    if (type_node != NULL
        && type_node->type == AST_TYPE
        && type_node->data.type.name != NULL) {
        ASTNode *class_decl = find_class_decl(ctx, type_node->data.type.name);
        if (class_decl != NULL && class_has_generic_params(class_decl)) {
            const char *spec_name =
                ensure_generic_class_specialization(ctx, class_decl, type_node);
            if (spec_name != NULL && strcmp(spec_name, type_node->data.type.name) != 0)
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
    for (size_t i = 0; i < body->data.block.count; i++) {
        ASTNode *stmt = body->data.block.statements[i];
        if (stmt == NULL)
            continue;
        if (stmt->type == AST_BLOCK) {
            transpiler_register_explicit_local_bindings_in_block(ctx, func_decl, stmt);
            continue;
        }
        if (stmt->type == AST_LET_DECL && stmt->data.let_decl.name != NULL) {
            const char *type_name = NULL;
            char *rendered_type = NULL;
            if (stmt->data.let_decl.type != NULL) {
                rendered_type = transpiler_render_effective_local_type_name(
                    ctx, stmt->data.let_decl.type);
                type_name = rendered_type;
            } else if (stmt->data.let_decl.initializer != NULL) {
                type_name = transpiler_infer_local_type_name_from_expr(
                    ctx, func_decl, stmt->data.let_decl.initializer);
            }
            if (type_name != NULL && type_name[0] != '\0') {
                bool registered_view_like = false;
                if (transpiler_type_name_is_view_like(type_name)
                    && stmt->data.let_decl.initializer != NULL
                    && stmt->data.let_decl.initializer->type == AST_CALL
                    && stmt->data.let_decl.initializer->data.call.callee != NULL
                    && stmt->data.let_decl.initializer->data.call.callee->type == AST_IDENTIFIER
                    && stmt->data.let_decl.initializer->data.call.arg_count >= 1
                    && stmt->data.let_decl.initializer->data.call.arguments[0] != NULL
                    && stmt->data.let_decl.initializer->data.call.arguments[0]->type == AST_IDENTIFIER) {
                    const char *callee =
                        stmt->data.let_decl.initializer->data.call.callee->data.identifier.name;
                    const char *source =
                        stmt->data.let_decl.initializer->data.call.arguments[0]->data.identifier.name;
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
                        register_view_like_var(ctx, stmt->data.let_decl.name,
                            type_name, source, source_secure, false);
                        registered_view_like = true;
                    }
                }
                if (!registered_view_like)
                    register_typed_var(ctx, stmt->data.let_decl.name, type_name);
                if (transpiler_type_name_is_slot_like(type_name)) {
                    register_slot_var(ctx, stmt->data.let_decl.name,
                        slot_inner_type_name(type_name),
                        strncmp(type_name, "SecureSlot<", 11) == 0,
                        false);
                }
            }
            free(rendered_type);
            continue;
        }
        if (stmt->type == AST_WITH_STMT && stmt->data.with_stmt.alias != NULL) {
            char *inner = render_type_name(stmt->data.with_stmt.slot_type);
            if (inner == NULL || inner[0] == '\0') {
                free(inner);
                continue;
            }
            char *slot_type = strdup_fmt("%s<%s>",
                stmt->data.with_stmt.is_secure ? "SecureSlot" : "Slot",
                inner);
            register_typed_var(ctx, stmt->data.with_stmt.alias,
                slot_type != NULL ? slot_type : "Slot<Unknown>");
            register_slot_var(ctx, stmt->data.with_stmt.alias,
                inner,
                stmt->data.with_stmt.is_secure, false);
            free(slot_type);
            free(inner);
            transpiler_register_explicit_local_bindings_in_block(ctx, func_decl,
                stmt->data.with_stmt.body);
            continue;
        }
        if (stmt->type == AST_IF_STMT) {
            transpiler_register_explicit_local_bindings_in_block(ctx, func_decl,
                stmt->data.if_stmt.then_branch);
            transpiler_register_explicit_local_bindings_in_block(ctx, func_decl,
                stmt->data.if_stmt.else_branch);
            continue;
        }
        if (stmt->type == AST_WHILE_LOOP) {
            transpiler_register_explicit_local_bindings_in_block(ctx, func_decl,
                stmt->data.while_loop.body);
            continue;
        }
        if (stmt->type == AST_FOR_LOOP) {
            if (stmt->data.for_loop.variable != NULL) {
                register_typed_var(ctx, stmt->data.for_loop.variable, "Int");
            }
            transpiler_register_explicit_local_bindings_in_block(ctx, func_decl,
                stmt->data.for_loop.body);
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
            host_name = host_decl->data.class_decl.name;
            break;
        case AST_RELATION_DECL:
            host_name = host_decl->data.relation_decl.name;
            break;
        case AST_EFFECT_DECL:
            host_name = host_decl->data.effect_decl.name;
            break;
        case AST_ZONE_DECL:
            host_name = host_decl->data.zone_decl.name;
            break;
        case AST_WORLD_DECL:
            host_name = host_decl->data.world_decl.name;
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
    for (size_t i = 0; i < func_decl->data.func_decl.param_count; i++) {
        FuncParam *p = func_decl->data.func_decl.params[i];
        if (p != NULL && p->name != NULL && strcmp(p->name, base_name) == 0 && p->type != NULL) {
            static char *rendered_param = NULL;
            free(rendered_param);
            rendered_param = transpiler_render_effective_local_type_name(ctx, p->type);
            if (ctx != NULL && rendered_param != NULL)
                register_typed_var(ctx, base_name, rendered_param);
            return rendered_param;
        }
    }
    typed_name = transpiler_find_local_type_name_in_block(ctx, func_decl,
        func_decl->data.func_decl.body, base_name);
    if (typed_name != NULL) {
        if (ctx != NULL)
            register_typed_var(ctx, base_name, typed_name);
        return typed_name;
    }

    return transpiler_lookup_current_owner_member_type_name(ctx, base_name);
}

#include "transpiler_mir_ssa_lookup.h"
