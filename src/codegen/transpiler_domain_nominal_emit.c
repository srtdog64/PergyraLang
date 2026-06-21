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
#include "transpiler_domain_role_ability_names.h"
#include "transpiler_domain_role_methods_emit.h"
#include "transpiler_func_forward_metadata.h"
#include "transpiler_host_self_policy.h"
#include "transpiler_hosted_method_body_emit.h"
#include "transpiler_role_ability_helpers.h"
#include "transpiler_type_require.h"
#include "transpiler_type_render.h"
#include "../compiler/mir_decl_headers.h"

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
                                  const MIRDeclMethod *method_meta,
                                  ASTNode *method,
                                  TranspilerCtx *ctx)
{
    const char *method_name;
    const char *ret_type = "void";
    char ret_type_storage[128];
    ASTNode *return_type;
    const char *return_type_name;
    size_t param_count;

    if (ctx != NULL && ctx->backend_error != NULL)
        return;
    if (role_name == NULL || included_role_name == NULL
        || (method_meta == NULL
            && (method == NULL || method->type != AST_FUNC_DECL
                || ast_declaration_name(method) == NULL))) {
        return;
    }

    method_name = method_meta != NULL
        ? transpiler_mir_decl_method_name(method_meta)
        : ast_declaration_name(method);
    if (transpiler_active_has_mir(ctx) && method_name == NULL) {
        transpiler_set_mir_inventory_missing(
            ctx,
            "MIR-only C path missing included role method name metadata for '%s'",
            included_role_name != NULL ? included_role_name : "(anonymous-role)");
        return;
    }
    if (transpiler_active_has_mir(ctx) && method_meta == NULL) {
        transpiler_set_mir_inventory_missing(
            ctx,
            "MIR-only C path missing included role method metadata for '%s.%s'",
            included_role_name != NULL ? included_role_name : "(anonymous-role)",
            method_name != NULL ? method_name : "(anonymous)");
        return;
    }
    return_type_name = method_meta != NULL
        ? transpiler_mir_decl_method_return_type_name(method_meta)
        : NULL;
    return_type = method_meta != NULL
        ? transpiler_mir_decl_method_return_type(method_meta)
        : ast_func_return_type(method);
    if (!transpiler_mir_decl_method_metadata_complete_for(ctx,
            method_meta,
            included_role_name,
            method_name,
            TRANSPILER_MIR_DECL_METHOD_REQUIRE_ALL_TYPE_NAMES,
            "MIR-only C path missing included role method return type-name metadata for '%s.%s'",
            "MIR-only C path missing included role method parameter type-name metadata for '%s.%s'")) {
        return;
    }
    if (return_type_name != NULL) {
        if (!transpiler_require_type_name_c_type_copy(
                ctx, return_type_name, "included role method return",
                ret_type_storage, sizeof(ret_type_storage))) {
            return;
        }
        ret_type = ret_type_storage;
    } else if (return_type != NULL) {
        if (pergyra_ast_type_to_c_copy_in_ctx(ctx, return_type,
                ret_type_storage, sizeof(ret_type_storage))) {
            ret_type = ret_type_storage;
        }
    }

    codebuf_write(ctx->out, "\nstatic %s\n%s_%s(void *_raw_self",
                  ret_type, role_name, method_name);
    param_count = method_meta != NULL
        ? transpiler_mir_decl_method_param_count(method_meta)
        : ast_func_param_count(method);
    for (size_t i = 0; i < param_count; i++) {
        FuncParam *param = method_meta != NULL
            ? transpiler_mir_decl_method_param(method_meta, i)
            : ast_func_param(method, i);
        const char *param_type_name = method_meta != NULL
            ? transpiler_mir_decl_method_param_type_name(method_meta, i)
            : NULL;
        char param_type[256];
        char *param_type_name_owned = NULL;
        bool pointer_param = false;
        char surface_desc[256];
        if (param == NULL) {
            if (method_meta != NULL) {
                transpiler_set_mir_inventory_missing(
                    ctx,
                    "MIR-only C path missing included role method parameter metadata for '%s.%s'",
                    included_role_name != NULL
                        ? included_role_name : "(anonymous-role)",
                    method_name != NULL ? method_name : "(anonymous)");
                return;
            }
            continue;
        }
        if (param->name == NULL)
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
        if (param_type_name != NULL) {
            if (!transpiler_require_type_name_c_type_copy(
                    ctx, param_type_name, surface_desc,
                    param_type, sizeof(param_type))) {
                return;
            }
        } else {
            if (!transpiler_require_ast_c_type_copy(
                    ctx, param->type, surface_desc,
                    param_type, sizeof(param_type))) {
                return;
            }
        }
        if (param_type_name == NULL && param->type != NULL)
            param_type_name_owned = render_type_name_in_ctx(ctx, param->type);
        pointer_param = (param_type_name != NULL
                && is_pointer_self_host_type_name(ctx, param_type_name))
            || (param_type_name_owned != NULL
                && is_pointer_self_host_type_name(ctx, param_type_name_owned));
        codebuf_write(ctx->out, ", %s%s %s",
                      param_type, pointer_param ? " *" : "", param->name);
        free(param_type_name_owned);
    }
    codebuf_write(ctx->out, ")\n{\n    ");
    if (strcmp(ret_type, "void") != 0) {
        codebuf_write(ctx->out, "return ");
    }
    codebuf_write(ctx->out, "%s_%s(_raw_self",
                  included_role_name, method_name);
    for (size_t i = 0; i < param_count; i++) {
        FuncParam *param = method_meta != NULL
            ? transpiler_mir_decl_method_param(method_meta, i)
            : ast_func_param(method, i);
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
    bool mir_active = transpiler_active_has_mir(ctx);
    const MIRDeclHeader *owner_role_header = NULL;
    size_t include_count;

    if (mir_active) {
        owner_role_header = transpiler_active_decl_header_of_type(
            ctx, AST_ROLE_DECL, owner_role_name);
        if (owner_role_header == NULL) {
            transpiler_set_mir_inventory_missing(
                ctx,
                "MIR-only C path missing role include metadata header for role '%s'",
                owner_role_name != NULL ? owner_role_name : "(anonymous-role)");
            return;
        }
    }

    include_count = mir_active
        ? mir_decl_header_role_include_count(owner_role_header)
        : ast_role_include_count(role);

    for (size_t i = 0; i < include_count; i++) {
        const MIRDeclRoleInclude *include_meta = NULL;
        ASTNode *include_stmt = NULL;
        const char *role_name = NULL;
        ASTNode *included_role;
        const MIRDeclHeader *included_role_header = NULL;

        if (mir_active) {
            include_meta = mir_decl_header_role_include(owner_role_header, i);
            role_name = mir_decl_role_include_name(include_meta);
            if (role_name == NULL) {
                transpiler_set_mir_inventory_missing(
                    ctx,
                    "MIR-only C path missing included role name metadata for role '%s'",
                    owner_role_name != NULL ? owner_role_name : "(anonymous-role)");
                return;
            }

            included_role_header = transpiler_active_decl_header_of_type(
                ctx, AST_ROLE_DECL, role_name);
            if (included_role_header == NULL) {
                transpiler_set_mir_inventory_missing(
                    ctx,
                    "MIR-only C path missing role impl metadata header for included role '%s'",
                    role_name);
                return;
            }

            for (size_t j = 0;
                 j < mir_decl_header_role_impl_count(included_role_header);
                 j++) {
                const MIRDeclRoleImpl *impl_meta =
                    mir_decl_header_role_impl(included_role_header, j);
                const MIRAbilityRef *ability_ref =
                    mir_decl_role_impl_ability_ref(impl_meta);
                const char *ability_name =
                    mir_ability_ref_base_name(ability_ref);
                bool owner_has_ability = false;

                if (ability_name == NULL) {
                    transpiler_set_mir_inventory_missing(
                        ctx,
                        "MIR-only C path missing included role impl ability-ref metadata for role '%s'",
                        role_name);
                    return;
                }

                for (size_t owner_i = 0;
                     owner_i < mir_decl_header_role_impl_count(owner_role_header);
                     owner_i++) {
                    const MIRDeclRoleImpl *owner_impl =
                        mir_decl_header_role_impl(owner_role_header, owner_i);
                    const MIRAbilityRef *owner_ref =
                        mir_decl_role_impl_ability_ref(owner_impl);
                    const char *owner_ability =
                        mir_ability_ref_base_name(owner_ref);
                    if (owner_ability != NULL
                        && strcmp(owner_ability, ability_name) == 0) {
                        owner_has_ability = true;
                        break;
                    }
                }
                if (owner_has_ability)
                    continue;

                for (size_t k = 0;
                     k < mir_decl_role_impl_method_count(impl_meta);
                     k++) {
                    const MIRDeclMethod *method_meta =
                        mir_decl_header_role_impl_method(
                            included_role_header, impl_meta, k);
                    const char *method_name =
                        transpiler_mir_decl_method_name(method_meta);
                    bool owner_has_method = false;

                    if (method_meta == NULL || method_name == NULL) {
                        transpiler_set_mir_inventory_missing(
                            ctx,
                            "MIR-only C path missing included role method metadata for '%s'",
                            role_name);
                        return;
                    }

                    for (size_t owner_m = 0;
                         owner_m < mir_decl_header_method_count(owner_role_header);
                         owner_m++) {
                        const MIRDeclMethod *owner_method =
                            mir_decl_header_method(owner_role_header, owner_m);
                        const char *owner_method_name =
                            transpiler_mir_decl_method_name(owner_method);
                        if (owner_method_name != NULL
                            && strcmp(owner_method_name, method_name) == 0) {
                            owner_has_method = true;
                            break;
                        }
                    }
                    if (owner_has_method)
                        continue;

                    emit_included_role_method_wrapper(
                        owner_role_name, role_name, method_meta, NULL, ctx);
                    if (ctx != NULL && ctx->backend_error != NULL)
                        return;
                }

                emit_role_vtable_instance(owner_role_name,
                    role_name, NULL, included_role_header, impl_meta, ctx);
                if (ctx != NULL && ctx->backend_error != NULL)
                    return;
            }
            continue;
        }

        include_stmt = ast_role_include(role, i);
        role_name = ast_include_role_name(include_stmt);
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
                    NULL,
                    method,
                    ctx);
                if (ctx != NULL && ctx->backend_error != NULL)
                    return;
            }

            emit_role_vtable_instance(owner_role_name,
                transpiler_decl_name_local(included_role), impl,
                NULL, NULL, ctx);
            if (ctx != NULL && ctx->backend_error != NULL)
                return;
        }
    }
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
        emit_role_method_impl(name, method_meta, mir_method, NULL, ctx);
        if (ctx != NULL && ctx->backend_error != NULL)
            return;
    }

    {
        size_t role_impl_index = 0;
        for (size_t i = 0; i < ast_role_impl_count(node); i++) {
            ASTNode *impl = ast_role_impl(node, i);

            if (impl == NULL)
                continue;

            if (impl->type == AST_IMPL_ABILITY) {
                const MIRDeclRoleImpl *impl_meta = NULL;
                if (transpiler_active_has_mir(ctx)) {
                    impl_meta = mir_decl_header_role_impl(
                        method_view.decl_header, role_impl_index);
                }
                role_impl_index++;
                emit_role_vtable_instance(
                    name, name, impl, method_view.decl_header, impl_meta, ctx);
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
                    && pergyra_ast_type_to_c_copy_in_ctx(ctx,
                        ast_func_return_type(func), ret_type_buf,
                        sizeof(ret_type_buf))) {
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
            const MIRAbilityRef *ability_ref =
                transpiler_hosted_role_slot_view_required_ability_ref(
                    &role_view, i, j);
            if (ability_ref != NULL) {
                char typedef_name[128];
                char *vtable_tag =
                    render_mir_ability_ref_vtable_tag_in_ctx(ctx, ability_ref);
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
                if (!transpiler_role_ability_vtable_typedef_name(
                        typedef_name, sizeof(typedef_name), vtable_tag)) {
                    free(vtable_tag);
                    transpiler_set_backend_error_with_hints(ctx,
                        PGY_CODE_C_TYPE_UNSUPPORTED,
                        PGY_CAUSE_C_TYPE_UNSUPPORTED,
                        PGY_FIX_USE_LLVM_BACKEND_OR_EXTEND_TRANSPILER,
                        "C backend: ability vtable typedef name is too long");
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
        if (method_meta == NULL && transpiler_active_has_mir(ctx)) {
            transpiler_set_mir_inventory_missing(ctx,
                "MIR-only C path missing hosted method forward metadata row for party '%s'",
                name != NULL ? name : "(anonymous-party)");
            return;
        }
        if (method_meta == NULL) {
            continue;
        }
        emit_hosted_method_forward_decl_from_metadata(name, method_meta,
            NULL, true, ctx->out, ctx);
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
            const MIRAbilityRef *ability_ref =
                transpiler_hosted_role_slot_view_required_ability_ref(
                    &role_view, i, j);
            if (ability_ref == NULL)
                continue;
            char typedef_name[128];
            char *vtable_tag =
                render_mir_ability_ref_vtable_tag_in_ctx(ctx, ability_ref);
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
            if (!transpiler_role_ability_vtable_typedef_name(
                    typedef_name, sizeof(typedef_name), vtable_tag)) {
                free(vtable_tag);
                transpiler_set_backend_error_with_hints(ctx,
                    PGY_CODE_C_TYPE_UNSUPPORTED,
                    PGY_CAUSE_C_TYPE_UNSUPPORTED,
                    PGY_FIX_USE_LLVM_BACKEND_OR_EXTEND_TRANSPILER,
                    "C backend: ability vtable typedef name is too long");
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
