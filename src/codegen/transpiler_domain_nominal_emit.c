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
#include "transpiler_domain_role_include_emit.h"
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
