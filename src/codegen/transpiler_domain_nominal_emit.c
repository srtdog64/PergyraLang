#include "transpiler_domain_nominal_emit.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../parser/ast_api.h"
#include "../semantic/diag_codes.h"
#include "host_decl_compat.h"
#include "transpiler_decl_lookup.h"
#include "transpiler_domain_role_ability_emit.h"
#include "transpiler_domain_role_methods_emit.h"
#include "transpiler_func_forward_metadata.h"
#include "transpiler_host_self_policy.h"
#include "transpiler_hosted_method_body_emit.h"
#include "transpiler_role_ability_helpers.h"
#include "transpiler_type_require.h"
#include "transpiler_type_render.h"

bool
transpiler_domain_nominal_surface_desc(char *out, size_t out_size,
                                       const char *prefix,
                                       const char *owner_name,
                                       const char *member_name,
                                       const char *param_name)
{
    int written;

    if (out == NULL || out_size == 0 || prefix == NULL)
        return false;

    if (param_name != NULL) {
        written = snprintf(out, out_size, "%s '%s.%s(%s)'",
            prefix,
            owner_name != NULL ? owner_name : "(anonymous)",
            member_name != NULL ? member_name : "(anonymous)",
            param_name);
    } else {
        written = snprintf(out, out_size, "%s '%s.%s'",
            prefix,
            owner_name != NULL ? owner_name : "(anonymous)",
            member_name != NULL ? member_name : "(anonymous)");
    }

    return written >= 0 && (size_t)written < out_size;
}

void
transpiler_domain_nominal_surface_desc_too_long(TranspilerCtx *ctx,
                                                const char *surface_kind)
{
    transpiler_set_backend_error_with_hints(ctx,
        PGY_CODE_C_TYPE_UNSUPPORTED,
        PGY_CAUSE_C_TYPE_UNSUPPORTED,
        PGY_FIX_USE_LLVM_BACKEND_OR_EXTEND_TRANSPILER,
        "%s diagnostic surface is too long for C backend emission",
        surface_kind != NULL ? surface_kind : "domain nominal");
}

static void
emit_included_role_method_wrapper(const char *role_name,
                                  const char *included_role_name,
                                  ASTNode *method,
                                  TranspilerCtx *ctx)
{
    const char *method_name;
    const char *ret_type = "void";
    char ret_type_storage[128];

    if (ctx != NULL && ctx->backend_error != NULL)
        return;
    if (role_name == NULL || included_role_name == NULL
        || method == NULL || method->type != AST_FUNC_DECL
        || ast_declaration_name(method) == NULL) {
        return;
    }

    method_name = ast_declaration_name(method);
    if (ast_func_return_type(method) != NULL) {
        if (pergyra_ast_type_to_c_copy_in_ctx(ctx, ast_func_return_type(method),
                ret_type_storage, sizeof(ret_type_storage))) {
            ret_type = ret_type_storage;
        }
    }

    codebuf_write(ctx->out, "\nstatic %s\n%s_%s(void *_raw_self",
                  ret_type, role_name, method_name);
    for (size_t i = 0; i < ast_func_param_count(method); i++) {
        FuncParam *param = ast_func_param(method, i);
        char param_type[256];
        char *param_type_name = NULL;
        bool pointer_param = false;
        char surface_desc[256];
        if (param == NULL || param->name == NULL)
            continue;
        if (strcmp(param->name, "self") == 0 && param->type == NULL)
            continue;
        if (!transpiler_domain_nominal_surface_desc(surface_desc,
                sizeof(surface_desc), "included role method parameter",
                role_name, method_name, param->name)) {
            transpiler_domain_nominal_surface_desc_too_long(
                ctx, "included role method parameter");
            return;
        }
        if (!transpiler_require_ast_c_type_copy(
                ctx, param->type, surface_desc,
                param_type, sizeof(param_type))) {
            return;
        }
        if (param->type != NULL)
            param_type_name = render_type_name_in_ctx(ctx, param->type);
        pointer_param = param_type_name != NULL
            && is_pointer_self_host_type_name(ctx, param_type_name);
        codebuf_write(ctx->out, ", %s%s %s",
                      param_type, pointer_param ? " *" : "", param->name);
        free(param_type_name);
    }
    codebuf_write(ctx->out, ")\n{\n    ");
    if (ast_func_return_type(method) != NULL
        && strcmp(ret_type, "void") != 0) {
        codebuf_write(ctx->out, "return ");
    }
    codebuf_write(ctx->out, "%s_%s(_raw_self",
                  included_role_name, method_name);
    for (size_t i = 0; i < ast_func_param_count(method); i++) {
        FuncParam *param = ast_func_param(method, i);
        if (param == NULL || param->name == NULL)
            continue;
        if (strcmp(param->name, "self") == 0 && param->type == NULL)
            continue;
        codebuf_write(ctx->out, ", %s", param->name);
    }
    codebuf_write(ctx->out, ");\n}\n");
}

static void
emit_included_role_impls(ASTNode *role, TranspilerCtx *ctx)
{
    const char *owner_role_name = transpiler_decl_name_local(role);

    for (size_t i = 0; i < ast_role_include_count(role); i++) {
        ASTNode *include_stmt = ast_role_include(role, i);
        const char *role_name = ast_include_role_name(include_stmt);
        ASTNode *included_role;

        if (role_name == NULL)
            continue;

        included_role = find_role_decl(ctx, role_name);

        if (included_role == NULL) {
            transpiler_set_backend_error_with_hints(ctx,
                PGY_CODE_C_TYPE_UNSUPPORTED,
                PGY_CAUSE_C_TYPE_UNSUPPORTED,
                PGY_FIX_USE_LLVM_BACKEND_OR_EXTEND_TRANSPILER,
                "cannot resolve included role '%s' while emitting role '%s'",
                role_name,
                owner_role_name != NULL ? owner_role_name : "<role>");
            return;
        }

        for (size_t j = 0; j < ast_role_impl_count(included_role); j++) {
            ASTNode *impl = ast_role_impl(included_role, j);
            if (impl == NULL || impl->type != AST_IMPL_ABILITY)
                continue;

            if (role_has_ability(role, ast_impl_ability_name(impl)))
                continue;

            for (size_t k = 0; k < ast_impl_ability_method_count(impl); k++) {
                ASTNode *method = ast_impl_ability_method(impl, k);
                if (method == NULL || method->type != AST_FUNC_DECL)
                    continue;
                if (role_has_method(role, ast_declaration_name(method)))
                    continue;
                emit_included_role_method_wrapper(
                    owner_role_name,
                    transpiler_decl_name_local(included_role),
                    method,
                    ctx);
                if (ctx != NULL && ctx->backend_error != NULL)
                    return;
            }

            emit_role_vtable_instance(owner_role_name, impl, ctx);
            if (ctx != NULL && ctx->backend_error != NULL)
                return;
        }
    }
}

void
emit_ability_decl(ASTNode *node, TranspilerCtx *ctx)
{
    const char *name = ast_ability_name(node);
    GenericParams *generic_params = ast_declaration_generic_params(node);

    if (ast_generic_param_count(generic_params) > 0) {
        codebuf_write(ctx->out,
            "\n/* Generic ability: %s (vtable emitted per concrete ability reference) */\n",
            name != NULL ? name : "<anonymous>");
        return;
    }

    codebuf_write(ctx->out, "\n/* Ability: %s */\n", name);
    codebuf_write(ctx->out, "typedef struct\n{\n");

    for (size_t i = 0; i < ast_ability_method_count(node); i++) {
        ASTNode *method = ast_ability_method(node, i);
        if (method == NULL || method->type != AST_FUNC_DECL)
            continue;

        const char *method_name = ast_declaration_name(method);
        char ret_type_buf[256];
        const char *ret_type = "void";
        if (ast_func_return_type(method) != NULL
            && pergyra_ast_type_to_c_copy_in_ctx(ctx, ast_func_return_type(method),
                ret_type_buf, sizeof(ret_type_buf))) {
            ret_type = ret_type_buf;
        }

        codebuf_write(ctx->out, "    %s (*%s)(void *self", ret_type, method_name);

        for (size_t j = 0; j < ast_func_param_count(method); j++) {
            FuncParam *p = ast_func_param(method, j);
            char *param_name = NULL;
            char pt[256];
            bool pointer_param = false;
            char surface_desc[256];
            if (p == NULL || p->name == NULL)
                continue;
            if (strcmp(p->name, "self") == 0 && p->type == NULL)
                continue;
            if (!transpiler_domain_nominal_surface_desc(surface_desc,
                    sizeof(surface_desc), "ability method parameter",
                    name, method_name, p != NULL ? p->name : NULL)) {
                transpiler_domain_nominal_surface_desc_too_long(
                    ctx, "ability method parameter");
                return;
            }
            if (!transpiler_require_ast_c_type_copy(ctx,
                    p != NULL ? p->type : NULL,
                    surface_desc,
                    pt,
                    sizeof(pt))) {
                return;
            }
            if (p != NULL && p->type != NULL)
                param_name = render_type_name_in_ctx(ctx, p->type);
            pointer_param = param_name != NULL
                && is_pointer_self_host_type_name(ctx, param_name);
            codebuf_write(ctx->out, ", %s%s %s", pt,
                pointer_param ? " *" : "", p->name);
            free(param_name);
        }
        codebuf_write(ctx->out, ");\n");
    }

    codebuf_write(ctx->out, "} %s_vtable;\n", name);
}

void
emit_role_decl(ASTNode *node, TranspilerCtx *ctx)
{
    const char *name = transpiler_decl_name_local(node);
    TranspilerHostedMethodView method_view;

    method_view = transpiler_hosted_method_view_from_decl(ctx, name, node);
    if (transpiler_hosted_method_view_missing_mir_metadata(&method_view)) {
        transpiler_set_mir_inventory_missing(
            ctx,
            "MIR-only C path missing method declaration metadata for role '%s'",
            name != NULL ? name : "(anonymous-role)");
        return;
    }
    if (!transpiler_require_hosted_method_view_rows(
            ctx,
            &method_view,
            "MIR-only C path has invalid method declaration metadata row for role '%s'",
            name != NULL ? name : "(anonymous-role)")) {
        return;
    }

    codebuf_write(ctx->out, "\n/* Role: %s */\n", name);
    emit_included_role_impls(node, ctx);
    if (ctx != NULL && ctx->backend_error != NULL)
        return;

    for (size_t i = 0; i < method_view.count; i++) {
        const MIRDeclMethod *method_meta =
            transpiler_hosted_method_view_metadata(&method_view, i);
        const MIRRoutine *mir_method =
            transpiler_mir_decl_method_routine(ctx, method_meta);
        ASTNode *method =
            transpiler_hosted_method_view_source_ast(&method_view, i);
        if (transpiler_hosted_method_view_missing_mir_method_row(&method_view, i)) {
            transpiler_set_mir_inventory_missing(
                ctx,
                "MIR-only C path has invalid method declaration metadata row for role '%s'",
                name != NULL ? name : "(anonymous-role)");
            return;
        }
        emit_role_method_impl(name, method_meta, mir_method, method, ctx);
        if (ctx != NULL && ctx->backend_error != NULL)
            return;
    }

    for (size_t i = 0; i < ast_role_impl_count(node); i++) {
        ASTNode *impl = ast_role_impl(node, i);

        if (impl == NULL)
            continue;

        if (impl->type == AST_IMPL_ABILITY) {
            emit_role_vtable_instance(name, impl, ctx);
            if (ctx != NULL && ctx->backend_error != NULL)
                return;

        } else if (impl->type == AST_OVERRIDE_FUNC) {
            ASTNode *func = ast_override_func_decl(impl);
            if (func == NULL || func->type != AST_FUNC_DECL)
                continue;

            const char *method_name = ast_declaration_name(func);
            char ret_type_buf[256];
            const char *ret_type = "void";
            if (ast_func_return_type(func) != NULL
                && pergyra_ast_type_to_c_copy_in_ctx(ctx, ast_func_return_type(func),
                    ret_type_buf, sizeof(ret_type_buf))) {
                ret_type = ret_type_buf;
            }

            codebuf_write(ctx->out, "\nstatic %s\n%s_%s(void *self",
                          ret_type, name, method_name);

            for (size_t k = 0; k < ast_func_param_count(func); k++) {
                FuncParam *p = ast_func_param(func, k);
                char pt[256];
                char surface_desc[256];
                if (p == NULL || p->name == NULL)
                    continue;
                if (strcmp(p->name, "self") == 0 && p->type == NULL)
                    continue;
                if (!transpiler_domain_nominal_surface_desc(surface_desc,
                        sizeof(surface_desc), "role override parameter",
                        name, method_name,
                        p != NULL ? p->name : NULL)) {
                    transpiler_domain_nominal_surface_desc_too_long(
                        ctx, "role override parameter");
                    return;
                }
                if (!transpiler_require_ast_c_type_copy(ctx,
                        p != NULL ? p->type : NULL,
                        surface_desc,
                        pt,
                        sizeof(pt))) {
                    return;
                }
                codebuf_write(ctx->out, ", %s %s", pt, p->name);
            }
            codebuf_write(ctx->out, ")\n{\n");

            ctx->indent++;
            if (ast_func_body(func) != NULL)
                emit_block(ast_func_body(func), ctx);
            ctx->indent--;

            codebuf_write(ctx->out, "}\n");
        }
    }

    emit_role_operator_aliases(node, ctx);
}

void
emit_party_decl(ASTNode *node, TranspilerCtx *ctx)
{
    const char *name = transpiler_decl_name_local(node);
    ASTNode *inventory_decl;

    if (name == NULL)
        return;
    inventory_decl = transpiler_find_named_decl_local(
        ctx, AST_PARTY_DECL, name);
    if (inventory_decl != NULL)
        node = inventory_decl;

    TranspilerHostedRoleSlotView role_view =
        transpiler_hosted_role_slot_view_from_decl(ctx, name, node);
    if (transpiler_hosted_role_slot_view_missing_mir_metadata(&role_view)) {
        transpiler_set_mir_inventory_missing(
            ctx,
            "MIR-only C path missing role-slot declaration metadata for party '%s'",
            name != NULL ? name : "(anonymous-party)");
        return;
    }
    TranspilerHostedSharedFieldView shared_view =
        transpiler_hosted_shared_field_view_from_decl(ctx, name, node);
    if (transpiler_hosted_shared_field_view_missing_mir_metadata(
            &shared_view)) {
        transpiler_set_mir_inventory_missing(
            ctx,
            "MIR-only C path missing shared-field declaration metadata for party '%s'",
            name != NULL ? name : "(anonymous-party)");
        return;
    }
    TranspilerHostedMethodView method_view =
        transpiler_hosted_method_view_from_decl(ctx, name, node);
    if (transpiler_hosted_method_view_missing_mir_metadata(&method_view)) {
        transpiler_set_mir_inventory_missing(
            ctx,
            "MIR-only C path missing method declaration metadata for party '%s'",
            name != NULL ? name : "(anonymous-party)");
        return;
    }
    if (!transpiler_require_hosted_method_view_rows(
            ctx,
            &method_view,
            "MIR-only C path has invalid method declaration metadata row for party '%s'",
            name != NULL ? name : "(anonymous-party)")) {
        return;
    }

    codebuf_write(ctx->out, "\n/* Party: %s */\n", name);
    codebuf_write(ctx->out, "typedef struct %s\n{\n", name);

    for (size_t i = 0; i < role_view.count; i++) {
        const char *slot_name;
        size_t ability_count;
        bool is_dyn;
        slot_name = transpiler_hosted_role_slot_view_name(&role_view, i);
        if (slot_name == NULL)
            continue;
        ability_count =
            transpiler_hosted_role_slot_view_required_ability_count(
                &role_view, i);
        is_dyn =
            transpiler_hosted_role_slot_view_is_dynamic(&role_view, i);
        codebuf_write(ctx->out, "    void *%s;\n", slot_name);
        for (size_t j = 0; j < ability_count; j++) {
            ASTNode *ab =
                transpiler_hosted_role_slot_view_required_ability(
                    &role_view, i, j);
            if (ab != NULL && ast_type_name(ab) != NULL) {
                char typedef_name[128];
                char *vtable_tag = render_ability_ref_vtable_tag_in_ctx(ctx, ab);
                ensure_ability_ref_vtable_decl(ab, ctx);
                if (!ability_ref_vtable_typedef_name(ab, typedef_name,
                        sizeof(typedef_name), ctx)) {
                    free(vtable_tag);
                    return;
                }
                if (vtable_tag == NULL) {
                    transpiler_set_backend_error_with_hints(ctx,
                        PGY_CODE_C_TYPE_UNSUPPORTED,
                        PGY_CAUSE_C_TYPE_UNSUPPORTED,
                        PGY_FIX_USE_LLVM_BACKEND_OR_EXTEND_TRANSPILER,
                        "cannot render ability vtable tag for party '%s' slot '%s'",
                        name != NULL ? name : "<party>",
                        slot_name != NULL ? slot_name : "<slot>");
                    return;
                }
                if (is_dyn) {
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

    for (size_t i = 0; i < shared_view.count; i++) {
        const char *shared_name =
            transpiler_hosted_shared_field_view_name(&shared_view, i);
        char ft[256];
        char surface_desc[256];
        if (!transpiler_domain_nominal_surface_desc(surface_desc,
                sizeof(surface_desc), "party shared field", name,
                shared_name, NULL)) {
            transpiler_domain_nominal_surface_desc_too_long(
                ctx, "party shared field");
            return;
        }
        if (!transpiler_require_ast_c_type_copy(
                ctx,
                transpiler_hosted_shared_field_view_type(&shared_view, i),
                surface_desc,
                ft,
                sizeof(ft))) {
            return;
        }
        codebuf_write(ctx->out, "    %s %s;\n", ft,
            shared_name != NULL ? shared_name : "field");
    }

    codebuf_write(ctx->out, "} %s;\n", name);

    for (size_t i = 0; i < method_view.count; i++) {
        const MIRDeclMethod *method_meta =
            transpiler_hosted_method_view_metadata(&method_view, i);
        ASTNode *method =
            transpiler_hosted_method_view_source_ast(&method_view, i);
        if (transpiler_hosted_method_view_missing_mir_method_row(&method_view, i)) {
            transpiler_set_mir_inventory_missing(
                ctx,
                "MIR-only C path has invalid method declaration metadata row for party '%s'",
                name != NULL ? name : "(anonymous-party)");
            return;
        }
        if (method_meta == NULL
            && (method == NULL || method->type != AST_FUNC_DECL)) {
            continue;
        }
        emit_hosted_method_forward_decl_from_metadata(name, method_meta,
            method, true, ctx->out, ctx);
    }

    transpiler_emit_hosted_methods_from_mir_or_error(name, "(anonymous-party)",
        "party", &method_view, ctx);

    for (size_t i = 0; i < role_view.count; i++) {
        size_t ability_count;
        const char *slot_name;
        if (!transpiler_hosted_role_slot_view_is_dynamic(&role_view, i))
            continue;
        slot_name = transpiler_hosted_role_slot_view_name(&role_view, i);
        if (slot_name == NULL)
            continue;
        ability_count =
            transpiler_hosted_role_slot_view_required_ability_count(
                &role_view, i);
        for (size_t j = 0; j < ability_count; j++) {
            ASTNode *ab =
                transpiler_hosted_role_slot_view_required_ability(
                    &role_view, i, j);
            if (ab == NULL || ast_type_name(ab) == NULL)
                continue;
            char typedef_name[128];
            char *vtable_tag = render_ability_ref_vtable_tag_in_ctx(ctx, ab);
            ensure_ability_ref_vtable_decl(ab, ctx);
            if (!ability_ref_vtable_typedef_name(ab, typedef_name,
                    sizeof(typedef_name), ctx)) {
                free(vtable_tag);
                return;
            }
            if (vtable_tag == NULL) {
                transpiler_set_backend_error_with_hints(ctx,
                    PGY_CODE_C_TYPE_UNSUPPORTED,
                    PGY_CAUSE_C_TYPE_UNSUPPORTED,
                    PGY_FIX_USE_LLVM_BACKEND_OR_EXTEND_TRANSPILER,
                    "cannot render ability vtable tag for party '%s' bind helper slot '%s'",
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
