#ifndef PGY_TRANSPILER_DOMAIN_NOMINAL_EMIT_H
#define PGY_TRANSPILER_DOMAIN_NOMINAL_EMIT_H

static void
emit_included_role_impls(ASTNode *role, TranspilerCtx *ctx)
{
    for (size_t i = 0; i < role->data.role_decl.include_count; i++) {
        ASTNode *include_stmt = role->data.role_decl.includes[i];
        ASTNode *included_role = find_role_decl(ctx, include_stmt->data.include_stmt.role_name);

        if (included_role == NULL) {
            transpiler_set_backend_error_with_hints(ctx, PGY_CODE_C_TYPE_UNSUPPORTED, PGY_CAUSE_C_TYPE_UNSUPPORTED, PGY_FIX_USE_LLVM_BACKEND_OR_EXTEND_TRANSPILER, "cannot resolve included role '%s' while emitting role '%s'",
                include_stmt->data.include_stmt.role_name != NULL
                    ? include_stmt->data.include_stmt.role_name
                    : "<role>",
                role->data.role_decl.name != NULL
                    ? role->data.role_decl.name
                    : "<role>");
            return;
        }

        for (size_t j = 0; j < included_role->data.role_decl.impl_count; j++) {
            ASTNode *impl = included_role->data.role_decl.impl_abilities[j];
            if (impl == NULL || impl->type != AST_IMPL_ABILITY)
                continue;

            if (role_has_ability(role,
                    (impl->data.impl_ability.ability_ref != NULL
                     && impl->data.impl_ability.ability_ref->type == AST_TYPE)
                        ? impl->data.impl_ability.ability_ref->data.type.name
                        : NULL))
                continue;

            for (size_t k = 0; k < impl->data.impl_ability.method_count; k++) {
                ASTNode *method = impl->data.impl_ability.methods[k];
                if (method == NULL || method->type != AST_FUNC_DECL)
                    continue;
                if (role_has_method(role, method->data.func_decl.name))
                    continue;
                emit_role_method_impl(role->data.role_decl.name, method, ctx);
            }

            emit_role_vtable_instance(role->data.role_decl.name, impl, ctx);
        }
    }
}

/*
 * Ability → vtable struct typedef
 *
 *   typedef struct {
 *       RetType (*MethodName)(void* self, ParamType p1, ...);
 *       ...
 *   } AbilityName_vtable;
 */
void
emit_ability_decl(ASTNode *node, TranspilerCtx *ctx)
{
    const char *name = node->data.ability_decl.name;

    codebuf_write(ctx->out, "\n/* Ability: %s */\n", name);
    codebuf_write(ctx->out, "typedef struct\n{\n");

    for (size_t i = 0; i < node->data.ability_decl.method_count; i++) {
        ASTNode *method = node->data.ability_decl.methods[i];
        if (method == NULL || method->type != AST_FUNC_DECL)
            continue;

        const char *method_name = method->data.func_decl.name;
        const char *ret_type = "void";
        if (method->data.func_decl.return_type != NULL)
            ret_type = pergyra_ast_type_to_c(method->data.func_decl.return_type);

        codebuf_write(ctx->out, "    %s (*%s)(void *self", ret_type, method_name);

        for (size_t j = 0; j < method->data.func_decl.param_count; j++) {
            FuncParam *p = method->data.func_decl.params[j];
            char *param_name = NULL;
            if (p == NULL || p->name == NULL)
                continue;
            if (strcmp(p->name, "self") == 0 && p->type == NULL)
                continue;
            const char *pt = NULL;
            bool pointer_param = false;
            char surface_desc[256];
            snprintf(surface_desc, sizeof(surface_desc),
                "ability method parameter '%s.%s(%s)'",
                name != NULL ? name : "(anonymous)",
                method_name != NULL ? method_name : "(anonymous)",
                p != NULL && p->name != NULL ? p->name : "(anonymous)");
            pt = transpiler_require_ast_c_type(ctx, p != NULL ? p->type : NULL, surface_desc);
            if (pt == NULL)
                return;
            if (p != NULL && p->type != NULL)
                param_name = render_type_name(p->type);
            pointer_param = param_name != NULL
                && is_pointer_self_host_type_name(ctx, param_name);
            codebuf_write(ctx->out, ", %s%s %s", pt, pointer_param ? " *" : "", p->name);
            free(param_name);
        }
        codebuf_write(ctx->out, ");\n");
    }

    codebuf_write(ctx->out, "} %s_vtable;\n", name);
}

/*
 * Role → vtable instance + free functions
 *
 *   static RetType RoleName_MethodName(void* self, ...) { body }
 *   static const AbilityName_vtable RoleName_AbilityName_vtable_instance = {
 *       .MethodName = RoleName_MethodName,
 *   };
 */
void
emit_role_decl(ASTNode *node, TranspilerCtx *ctx)
{
    const char *name = node->data.role_decl.name;


    codebuf_write(ctx->out, "\n/* Role: %s */\n", name);
    emit_included_role_impls(node, ctx);

    for (size_t i = 0; i < node->data.role_decl.impl_count; i++) {
        ASTNode *impl = node->data.role_decl.impl_abilities[i];

        if (impl == NULL)
            continue;

        if (impl->type == AST_IMPL_ABILITY) {
            for (size_t j = 0; j < impl->data.impl_ability.method_count; j++) {
                ASTNode *method = impl->data.impl_ability.methods[j];
                if (method == NULL || method->type != AST_FUNC_DECL)
                    continue;
                emit_role_method_impl(name, method, ctx);
            }

            emit_role_vtable_instance(name, impl, ctx);

        } else if (impl->type == AST_OVERRIDE_FUNC) {
            ASTNode *func = impl->data.override_func.func_decl;
            if (func == NULL || func->type != AST_FUNC_DECL)
                continue;

            const char *method_name = func->data.func_decl.name;
            const char *ret_type = "void";
            if (func->data.func_decl.return_type != NULL)
                ret_type = pergyra_ast_type_to_c(func->data.func_decl.return_type);

            codebuf_write(ctx->out, "\nstatic %s\n%s_%s(void *self",
                          ret_type, name, method_name);

            for (size_t k = 0; k < func->data.func_decl.param_count; k++) {
                FuncParam *p = func->data.func_decl.params[k];
                if (p == NULL || p->name == NULL)
                    continue;
                if (strcmp(p->name, "self") == 0 && p->type == NULL)
                    continue;
                const char *pt = NULL;
                char surface_desc[256];
                snprintf(surface_desc, sizeof(surface_desc),
                    "role override parameter '%s.%s(%s)'",
                    name != NULL ? name : "(anonymous)",
                    method_name != NULL ? method_name : "(anonymous)",
                    p != NULL && p->name != NULL ? p->name : "(anonymous)");
                pt = transpiler_require_ast_c_type(ctx, p != NULL ? p->type : NULL, surface_desc);
                if (pt == NULL)
                    return;
                codebuf_write(ctx->out, ", %s %s", pt, p->name);
            }
            codebuf_write(ctx->out, ")\n{\n");

            ctx->indent++;
            if (func->data.func_decl.body != NULL)
                emit_block(func->data.func_decl.body, ctx);
            ctx->indent--;

            codebuf_write(ctx->out, "}\n");
        }
    }

    emit_role_operator_aliases(node, ctx);
}

/* =================================================================
 * Party system emitters
 * ================================================================= */

/*
 * Party → C struct with role slot pointers + shared fields
 * Party methods → free functions
 */
void
emit_party_decl(ASTNode *node, TranspilerCtx *ctx)
{
    const char *name = node->data.party_decl.name;
    ASTNode *inventory_decl = transpiler_find_decl_in_inventory_local(
        ctx, AST_PARTY_DECL, name);

    if (inventory_decl != NULL)
        node = inventory_decl;

    codebuf_write(ctx->out, "\n/* Party: %s */\n", name);
    codebuf_write(ctx->out, "typedef struct %s\n{\n", name);

    /* Role slots as void* + vtable pointer */
    for (size_t i = 0; i < node->data.party_decl.role_count; i++) {
        ASTNode *rs = node->data.party_decl.role_slots[i];
        const char *slot_name = rs->data.role_slot.slot_name;
        bool is_dyn = rs->data.role_slot.is_dynamic;
        codebuf_write(ctx->out, "    void *%s;\n", slot_name);
        for (size_t j = 0; j < rs->data.role_slot.ability_count; j++) {
            ASTNode *ab = rs->data.role_slot.required_abilities[j];
            if (ab != NULL && ab->data.type.name != NULL) {
                char typedef_name[128];
                char *vtable_tag = render_ability_ref_vtable_tag(ab);
                ensure_ability_ref_vtable_decl(ab, ctx);
                if (!ability_ref_vtable_typedef_name(ab, typedef_name, sizeof(typedef_name), ctx)) {
                    free(vtable_tag);
                    return;
                }
                if (vtable_tag == NULL) {
                    transpiler_set_backend_error_with_hints(ctx, PGY_CODE_C_TYPE_UNSUPPORTED, PGY_CAUSE_C_TYPE_UNSUPPORTED, PGY_FIX_USE_LLVM_BACKEND_OR_EXTEND_TRANSPILER, "cannot render ability vtable tag for party '%s' slot '%s'",
                        name != NULL ? name : "<party>",
                        slot_name != NULL ? slot_name : "<slot>");
                    return;
                }
                if (is_dyn) {
                    /* dyn: mutable vtable pointer — swappable at runtime */
                    codebuf_write(ctx->out,
                        "    const %s *%s_%s_vt; /* dyn */\n",
                        typedef_name, slot_name, vtable_tag);
                } else {
                    codebuf_write(ctx->out,
                        "    const %s *%s_%s_vt;\n",
                        typedef_name, slot_name, vtable_tag);
                }
                free(vtable_tag);
            }
        }
    }

    /* Shared fields */
    for (size_t i = 0; i < node->data.party_decl.shared_count; i++) {
        ASTNode *shared = node->data.party_decl.shared_fields[i];
        const char *ft = NULL;
        char surface_desc[256];
        snprintf(surface_desc, sizeof(surface_desc),
            "party shared field '%s.%s'",
            name != NULL ? name : "(anonymous)",
            shared != NULL && shared->data.party_shared.name != NULL
                ? shared->data.party_shared.name
                : "(anonymous)");
        ft = transpiler_require_ast_c_type(
            ctx,
            shared != NULL ? shared->data.party_shared.type : NULL,
            surface_desc);
        if (ft == NULL)
            return;
        codebuf_write(ctx->out, "    %s %s;\n", ft, shared->data.party_shared.name);
    }

    codebuf_write(ctx->out, "} %s;\n", name);

    /* Methods as free functions */
    TranspilerHostedMethodView method_view =
        transpiler_hosted_method_view_from_decl(ctx, name, node);

    for (size_t i = 0; i < method_view.count; i++) {
        const MIRDeclMethod *method_meta =
            transpiler_hosted_method_view_metadata(&method_view, i);
        ASTNode *method = transpiler_hosted_method_view_ast(&method_view, i);
        if (method == NULL || method->type != AST_FUNC_DECL)
            continue;
        emit_hosted_method_forward_decl_from_metadata(name, method_meta,
            method, true, ctx->out, ctx);
    }

    emit_hosted_methods_from_mir_or_error_local(name, "(anonymous-party)",
        "party", &method_view, ctx);

    /* Emit bind helpers for dyn role slots */
    for (size_t i = 0; i < node->data.party_decl.role_count; i++) {
        ASTNode *rs = node->data.party_decl.role_slots[i];
        if (!rs->data.role_slot.is_dynamic)
            continue;
        const char *slot_name = rs->data.role_slot.slot_name;
        for (size_t j = 0; j < rs->data.role_slot.ability_count; j++) {
            ASTNode *ab = rs->data.role_slot.required_abilities[j];
            if (ab == NULL || ab->data.type.name == NULL)
                continue;
            char typedef_name[128];
            char *vtable_tag = render_ability_ref_vtable_tag(ab);
            ensure_ability_ref_vtable_decl(ab, ctx);
            if (!ability_ref_vtable_typedef_name(ab, typedef_name, sizeof(typedef_name), ctx)) {
                free(vtable_tag);
                return;
            }
            if (vtable_tag == NULL) {
                transpiler_set_backend_error_with_hints(ctx, PGY_CODE_C_TYPE_UNSUPPORTED, PGY_CAUSE_C_TYPE_UNSUPPORTED, PGY_FIX_USE_LLVM_BACKEND_OR_EXTEND_TRANSPILER, "cannot render ability vtable tag for party '%s' bind helper slot '%s'",
                    name != NULL ? name : "<party>",
                    slot_name != NULL ? slot_name : "<slot>");
                return;
            }
            codebuf_write(ctx->out,
                "\nstatic inline void\n"
                "%s_bind_%s(%s *self, void *impl, const %s *vt)\n"
                "{\n"
                "    self->%s = impl;\n"
                "    self->%s_%s_vt = vt;\n"
                "}\n",
                name, slot_name, name, typedef_name,
                slot_name,
                slot_name, vtable_tag);
            free(vtable_tag);
        }
    }
}

/* =================================================================
 * Roster/World system emitters
 * ================================================================= */

void
emit_roster_decl(ASTNode *node, TranspilerCtx *ctx)
{
    const char *name = node->data.roster_decl.name;
    ASTNode *inventory_decl = transpiler_find_decl_in_inventory_local(
        ctx, AST_ROSTER_DECL, name);

    if (inventory_decl != NULL)
        node = inventory_decl;

    codebuf_write(ctx->out, "\n/* Roster: %s */\n", name);
    codebuf_write(ctx->out, "typedef struct %s\n{\n", name);

    /* Party slots */
    for (size_t i = 0; i < node->data.roster_decl.party_count; i++) {
        ASTNode *ps = node->data.roster_decl.party_slots[i];
        codebuf_write(ctx->out, "    %s %s;\n",
            ps->data.roster_slot.party_type,
            ps->data.roster_slot.slot_name);
    }

    /* Shared fields */
    for (size_t i = 0; i < node->data.roster_decl.shared_count; i++) {
        ASTNode *shared = node->data.roster_decl.shared_fields[i];
        const char *ft = NULL;
        char surface_desc[256];
        snprintf(surface_desc, sizeof(surface_desc),
            "roster shared field '%s.%s'",
            name != NULL ? name : "(anonymous)",
            shared != NULL && shared->data.party_shared.name != NULL
                ? shared->data.party_shared.name
                : "(anonymous)");
        ft = transpiler_require_ast_c_type(
            ctx,
            shared != NULL ? shared->data.party_shared.type : NULL,
            surface_desc);
        if (ft == NULL)
            return;
        codebuf_write(ctx->out, "    %s %s;\n", ft, shared->data.party_shared.name);
    }

    codebuf_write(ctx->out, "} %s;\n", name);

    /* Methods */
    TranspilerHostedMethodView method_view =
        transpiler_hosted_method_view_from_decl(ctx, name, node);

    for (size_t i = 0; i < method_view.count; i++) {
        const MIRDeclMethod *method_meta =
            transpiler_hosted_method_view_metadata(&method_view, i);
        ASTNode *method = transpiler_hosted_method_view_ast(&method_view, i);
        if (method == NULL || method->type != AST_FUNC_DECL)
            continue;
        emit_hosted_method_forward_decl_from_metadata(name, method_meta,
            method, true, ctx->out, ctx);
    }

    emit_hosted_methods_from_mir_or_error_local(name, "(anonymous-roster)",
        "roster", &method_view, ctx);
}

#include "transpiler_relation_effect_emit.h"

#endif /* PGY_TRANSPILER_DOMAIN_NOMINAL_EMIT_H */
