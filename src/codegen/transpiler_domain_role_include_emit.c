#include "transpiler_domain_role_include_emit.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#include "../compiler/mir_decl_headers.h"
#include "../parser/ast_api.h"
#include "../semantic/diag_codes.h"
#include "transpiler_decl_lookup.h"
#include "transpiler_domain_nominal_emit.h"
#include "transpiler_domain_role_ability_emit.h"
#include "transpiler_domain_role_ability_names.h"
#include "transpiler_domain_role_methods_emit.h"
#include "transpiler_host_self_policy.h"
#include "transpiler_role_ability_helpers.h"
#include "transpiler_type_render.h"
#include "transpiler_type_require.h"

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

void
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
