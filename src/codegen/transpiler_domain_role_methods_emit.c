#include "transpiler_domain_role_methods_emit.h"

#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#include "../common/string_compat.h"
#include "../compiler/mir.h"
#include "../lexer/lexer.h"
#include "../parser/ast_api.h"
#include "../semantic/diag_codes.h"
#include "transpiler_context.h"
#include "transpiler_decl_lookup.h"
#include "transpiler_domain_role_ability_names.h"
#include "transpiler_generic_binding_query.h"
#include "transpiler_host_self_policy.h"
#include "transpiler_inventory_view.h"
#include "transpiler_mir_func_emit.h"
#include "transpiler_operator.h"
#include "transpiler_type_require.h"
#include "transpiler_type_render.h"
#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-function"
#endif
#include "transpiler_domain_role_ability_emit.h"
#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif

void
emit_role_method_impl(const char *role_name,
                      const MIRDeclMethod *method_meta,
                      const MIRRoutine *mir_method,
                      ASTNode *method,
                      TranspilerCtx *ctx)
{
    const char *method_name;

    if (ctx != NULL && ctx->backend_error != NULL)
        return;

    method_name = transpiler_mir_decl_method_name(method_meta);
    if (method_name == NULL && method != NULL)
        method_name = ast_declaration_name(method);
    if (transpiler_active_has_mir(ctx)
        && (role_name == NULL || method_name == NULL)) {
        transpiler_set_mir_inventory_missing(
            ctx,
            "MIR-only C path missing role method name metadata for '%s.%s'",
            role_name != NULL ? role_name : "(anonymous-role)",
            method_name != NULL ? method_name : "(anonymous)");
        return;
    }
    if (transpiler_active_has_mir(ctx) && method_meta == NULL) {
        transpiler_set_mir_inventory_missing(
            ctx,
            "MIR-only C path missing declaration metadata for role method '%s.%s'",
            role_name != NULL ? role_name : "(anonymous)",
            method_name != NULL ? method_name : "(anonymous)");
        return;
    }
    if (transpiler_active_has_mir(ctx) && mir_method == NULL) {
        transpiler_set_mir_inventory_missing(
            ctx,
            "MIR-only C path missing routine for role method '%s.%s'",
            role_name != NULL ? role_name : "(anonymous)",
            method_name != NULL ? method_name : "(anonymous)");
        return;
    }
    if (!transpiler_active_has_mir(ctx)
        && (method == NULL || method->type != AST_FUNC_DECL))
        return;
    if (mir_method != NULL) {
        char emitted_name[256];
        if (!transpiler_role_ability_host_method_name(
                emitted_name, sizeof(emitted_name), role_name, method_name)) {
            transpiler_set_backend_error_with_hints(ctx,
                PGY_CODE_C_TYPE_UNSUPPORTED,
                PGY_CAUSE_C_TYPE_UNSUPPORTED,
                PGY_FIX_USE_LLVM_BACKEND_OR_EXTEND_TRANSPILER,
                "C backend: role method symbol name is too long for '%s.%s'",
                role_name != NULL ? role_name : "(anonymous)",
                method_name != NULL ? method_name : "(anonymous)");
            return;
        }
        emit_func_decl_from_mir_named(NULL, mir_method, emitted_name,
                                      ctx->out, ctx);
        return;
    }
    if (!transpiler_active_has_mir(ctx))
        return;
    transpiler_set_mir_inventory_missing(
        ctx,
        "MIR-only C path missing routine for role method '%s.%s'",
        role_name != NULL ? role_name : "(anonymous)",
        method_name != NULL ? method_name : "(anonymous)");
}

void
emit_role_vtable_instance(const char *role_name,
                          const char *metadata_role_name,
                          ASTNode *impl,
                          TranspilerCtx *ctx)
{
    ASTNode *ability_ref = ast_impl_ability_ref(impl);
    const char *ability_name = ast_impl_ability_name(impl);
    char typedef_name[128];
    char *vtable_tag = NULL;
    if (ctx != NULL && ctx->backend_error != NULL)
        return;
    if (ability_name == NULL) {
        if (transpiler_active_has_mir(ctx)) {
            transpiler_set_mir_inventory_missing(
                ctx,
                "MIR-only C path missing role vtable ability name metadata for role '%s'",
                role_name != NULL ? role_name : "(anonymous-role)");
        }
        return;
    }
    if (ast_impl_ability_method_count(impl) == 0)
        return;

    ensure_ability_ref_vtable_decl(ability_ref, ctx);
    if (ctx != NULL && ctx->backend_error != NULL)
        return;
    if (!ability_ref_vtable_typedef_name(
            ability_ref, typedef_name, sizeof(typedef_name), ctx))
        return;
    vtable_tag = render_effective_ability_ref_vtable_tag(
        find_ability_decl(ctx, ability_name),
        ability_ref,
        ctx);
    if (vtable_tag == NULL) {
        transpiler_set_backend_error_with_hints(ctx,
            PGY_CODE_C_TYPE_UNSUPPORTED,
            PGY_CAUSE_C_TYPE_UNSUPPORTED,
            PGY_FIX_USE_LLVM_BACKEND_OR_EXTEND_TRANSPILER,
            "cannot render ability vtable tag for role '%s' impl ability '%s'",
            role_name != NULL ? role_name : "<role>",
            ability_name != NULL ? ability_name : "<ability>");
        return;
    }
    codebuf_write(ctx->out,
        "\nstatic const %s %s_%s_vtable_instance __attribute__((unused)) = {\n",
        typedef_name, role_name, vtable_tag);

    for (size_t j = 0; j < ast_impl_ability_method_count(impl); j++) {
        ASTNode *method = ast_impl_ability_method(impl, j);
        const MIRDeclMethod *method_meta = NULL;
        const char *method_name;
        if (transpiler_active_has_mir(ctx)) {
            const char *ast_method_name = method != NULL
                ? ast_declaration_name(method) : NULL;
            const char *lookup_role_name = metadata_role_name != NULL
                ? metadata_role_name : role_name;
            method_meta = transpiler_find_host_method_metadata_in_context(
                ctx, lookup_role_name, ast_method_name);
            if (method_meta == NULL) {
                transpiler_set_mir_inventory_missing(
                    ctx,
                    "MIR-only C path missing role vtable method metadata for role '%s'",
                    lookup_role_name != NULL ? lookup_role_name : "(anonymous-role)");
                return;
            }
        }
        method_name = method_meta != NULL
            ? transpiler_mir_decl_method_name(method_meta)
            : ast_declaration_name(method);
        if (method_meta == NULL
            && (method == NULL || method->type != AST_FUNC_DECL)) {
            transpiler_set_mir_inventory_missing(
                ctx,
                "MIR-only C path missing role vtable method source metadata for role '%s'",
                role_name != NULL ? role_name : "(anonymous-role)");
            return;
        }
        if (method_name == NULL) {
            transpiler_set_mir_inventory_missing(
                ctx,
                "MIR-only C path missing role vtable method name metadata for role '%s'",
                role_name != NULL ? role_name : "(anonymous-role)");
            return;
        }
        codebuf_write(ctx->out, "    .%s = %s_%s,\n",
                      method_name,
                      role_name, method_name);
    }

    codebuf_write(ctx->out, "};\n");
    free(vtable_tag);
}

void
emit_role_operator_aliases(ASTNode *role, TranspilerCtx *ctx)
{
    const char *role_name;
    const char *for_type;
    PgyTokenType ops[] = {
        TOKEN_PLUS, TOKEN_MINUS, TOKEN_STAR, TOKEN_SLASH, TOKEN_PERCENT,
        TOKEN_EQUAL, TOKEN_NOT_EQUAL, TOKEN_LESS, TOKEN_LESS_EQUAL,
        TOKEN_GREATER, TOKEN_GREATER_EQUAL
    };

    if (ctx != NULL && ctx->backend_error != NULL)
        return;
    if (role == NULL || role->type != AST_ROLE_DECL) {
        return;
    }

    role_name = transpiler_decl_name_local(role);
    if (role_name == NULL) {
        if (transpiler_active_has_mir(ctx)) {
            transpiler_set_mir_inventory_missing(
                ctx,
                "MIR-only C path missing role declaration name metadata");
        }
        return;
    }

    for_type = transpiler_role_subject_type_name_local(role);
    if (for_type == NULL) {
        if (transpiler_active_has_mir(ctx)) {
            transpiler_set_mir_inventory_missing(
                ctx,
                "MIR-only C path missing role subject type metadata for role '%s'",
                role_name);
        }
        return;
    }

    for (size_t i = 0; i < sizeof(ops) / sizeof(ops[0]); i++) {
        PgyTokenType op = ops[i];
        const char *suffix = operator_overload_suffix(op);
        const MIRDeclMethod *method_meta =
            find_role_operator_method_metadata(ctx, role, op, 0);
        ASTNode *method = NULL;
        const char *method_name =
            transpiler_mir_decl_method_name(method_meta);
        char fn_name[256];
        if (method_meta == NULL) {
            method = find_role_operator_method_decl(ctx, role, op, 0);
            if (method != NULL && !transpiler_active_has_mir(ctx)) {
                method_name = ast_declaration_name(method);
            } else if (method != NULL) {
                transpiler_set_mir_inventory_missing(
                    ctx,
                    "MIR-only C path missing role operator method metadata for role '%s'",
                    role_name != NULL ? role_name : "(anonymous-role)");
                return;
            } else {
                method_name = NULL;
            }
        }
        if (method_meta != NULL && method_name == NULL) {
            transpiler_set_mir_inventory_missing(
                ctx,
                "MIR-only C path missing role operator method name metadata for role '%s'",
                role_name != NULL ? role_name : "(anonymous-role)");
            return;
        }
        if (suffix == NULL
            || (method_meta == NULL
                && (method == NULL || method->type != AST_FUNC_DECL))) {
            continue;
        }
        if (method_name == NULL) {
            transpiler_set_mir_inventory_missing(
                ctx,
                "C backend role operator method name metadata is missing for role '%s'",
                role_name);
            return;
        }
        if (!transpiler_role_operator_alias_name(fn_name, sizeof(fn_name),
                suffix, for_type)) {
            transpiler_set_backend_error_with_hints(ctx,
                PGY_CODE_C_TYPE_UNSUPPORTED,
                PGY_CAUSE_C_TYPE_UNSUPPORTED,
                PGY_FIX_USE_LLVM_BACKEND_OR_EXTEND_TRANSPILER,
                "C backend: role operator alias name is too long for role '%s'",
                role_name != NULL ? role_name : "(anonymous)");
            return;
        }
        if (transpiler_callable_decl_exists_local(ctx, fn_name))
            continue;

        FuncParam *rhs_param = NULL;
        const char *rhs_param_type_name = NULL;
        size_t rhs_param_count = 0;
        size_t param_count = method_meta != NULL
            ? transpiler_mir_decl_method_param_count(method_meta)
            : ast_func_param_count(method);
        for (size_t j = 0; j < param_count; j++) {
            FuncParam *p = method_meta != NULL
                ? transpiler_mir_decl_method_param(method_meta, j)
                : ast_func_param(method, j);
            if (p != NULL && p->name != NULL
                && !(p->type == NULL && strcmp(p->name, "self") == 0)) {
                rhs_param = p;
                rhs_param_type_name = method_meta != NULL
                    ? transpiler_mir_decl_method_param_type_name(method_meta, j)
                    : NULL;
                rhs_param_count++;
            }
        }
        if (rhs_param_count != 1)
            continue;

        const char *ret_type = "void";
        char ret_type_storage[128];
        char lhs_type_storage[128];
        char rhs_type_storage[128];
        const char *lhs_type = NULL;
        if (transpiler_require_ast_c_type_copy(
            ctx,
            transpiler_role_subject_type_node_local(role),
            "role operator lhs type",
            lhs_type_storage,
            sizeof(lhs_type_storage))) {
            lhs_type = lhs_type_storage;
        }
        const char *rhs_type = NULL;
        const char *rhs_name = (rhs_param != NULL && rhs_param->name != NULL)
            ? rhs_param->name : "rhs";
        char surface_desc[256];

        if (!transpiler_mir_decl_method_metadata_complete_for(ctx,
                method_meta,
                role_name,
                method_name,
                TRANSPILER_MIR_DECL_METHOD_REQUIRE_ALL_TYPE_NAMES,
                "MIR-only C path missing role operator return type-name metadata for '%s.%s'",
                "MIR-only C path missing role operator parameter type-name metadata for '%s.%s'")) {
            return;
        }
        {
            const char *return_type_name = method_meta != NULL
                ? transpiler_mir_decl_method_return_type_name(method_meta)
                : NULL;
            ASTNode *return_type = method_meta != NULL
                ? transpiler_mir_decl_method_return_type(method_meta)
                : ast_func_return_type(method);
            if (return_type_name != NULL) {
                if (transpiler_require_type_name_c_type_copy(ctx,
                        return_type_name,
                        "role operator return type",
                    ret_type_storage,
                    sizeof(ret_type_storage))) {
                    ret_type = ret_type_storage;
                }
            } else if (return_type != NULL
                && pergyra_ast_type_to_c_copy_in_ctx(ctx,
                    return_type, ret_type_storage,
                    sizeof(ret_type_storage))) {
                ret_type = ret_type_storage;
            }
        }
        if (!transpiler_role_ability_surface_desc(surface_desc,
                sizeof(surface_desc), "role operator parameter",
                role_name, method_name, rhs_name)) {
            transpiler_set_backend_error_with_hints(ctx,
                PGY_CODE_C_TYPE_UNSUPPORTED,
                PGY_CAUSE_C_TYPE_UNSUPPORTED,
                PGY_FIX_USE_LLVM_BACKEND_OR_EXTEND_TRANSPILER,
                "C backend: role operator parameter diagnostic is too long");
            return;
        }
        if (rhs_param_type_name != NULL
            && transpiler_require_type_name_c_type_copy(
                ctx, rhs_param_type_name, surface_desc,
                rhs_type_storage, sizeof(rhs_type_storage))) {
            rhs_type = rhs_type_storage;
        } else if (rhs_param_type_name == NULL
            && transpiler_require_ast_c_type_copy(
                ctx,
                rhs_param != NULL ? rhs_param->type : NULL,
                surface_desc,
                rhs_type_storage,
                sizeof(rhs_type_storage))) {
            rhs_type = rhs_type_storage;
        }
        if (lhs_type == NULL || rhs_type == NULL)
            return;

        codebuf_write(ctx->out,
            "\nstatic %s\noperator_%s_%s(%s lhs, %s %s)\n{\n",
            ret_type, suffix, for_type, lhs_type, rhs_type, rhs_name);
        codebuf_write(ctx->out, "    %s lhs_copy = lhs;\n", lhs_type);
        if (strcmp(ret_type, "void") == 0) {
            codebuf_write(ctx->out, "    %s_%s(&lhs_copy, %s);\n",
                role_name, method_name, rhs_name);
        } else {
            codebuf_write(ctx->out, "    return %s_%s(&lhs_copy, %s);\n",
                role_name, method_name, rhs_name);
        }
        codebuf_write(ctx->out, "}\n");
    }
}
