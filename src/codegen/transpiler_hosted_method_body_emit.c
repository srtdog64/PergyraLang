#include "transpiler_hosted_method_body_emit.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>

#include "../compiler/mir.h"
#include "../parser/ast_api.h"
#include "../semantic/diag_codes.h"
#include "transpiler_inventory_view.h"
#include "transpiler_mir_func_emit.h"

static bool
transpiler_hosted_method_emit_name(char *out,
                                   size_t out_size,
                                   const char *host_name,
                                   const char *method_name)
{
    int written;

    if (out == NULL || out_size == 0 || host_name == NULL
        || method_name == NULL) {
        return false;
    }
    written = snprintf(out, out_size, "%s_%s", host_name, method_name);
    return written >= 0 && (size_t)written < out_size;
}

void
transpiler_emit_hosted_methods_from_mir_or_error(
    const char *host_name,
    const char *anonymous_host_name,
    const char *host_kind,
    const TranspilerHostedMethodView *method_view,
    TranspilerCtx *ctx)
{
    size_t method_count = method_view != NULL ? method_view->count : 0;

    if (transpiler_hosted_method_view_missing_mir_metadata(method_view)) {
        transpiler_set_mir_inventory_missing(
            ctx,
            "MIR-only C path missing declaration metadata for %s methods '%s'",
            host_kind != NULL ? host_kind : "host",
            host_name != NULL ? host_name : anonymous_host_name);
        return;
    }
    if (!transpiler_require_hosted_method_view_rows(
            ctx,
            method_view,
            "MIR-only C path has invalid method declaration metadata row for hosted method '%s'",
            host_name != NULL ? host_name : anonymous_host_name)) {
        return;
    }

    for (size_t i = 0; i < method_count; i++) {
        const MIRDeclMethod *method_meta =
            transpiler_hosted_method_view_metadata(method_view, i);
        ASTNode *method = NULL;
        const MIRRoutine *mir_method = NULL;
        const char *method_name = NULL;
        char emitted_name[256];

        method_name = transpiler_mir_decl_method_name(method_meta);

        mir_method = transpiler_mir_decl_method_routine(ctx, method_meta);
        if (method_meta == NULL && transpiler_active_has_mir(ctx)) {
            transpiler_set_mir_inventory_missing(
                ctx,
                "MIR-only C path missing method body metadata row for %s '%s'",
                host_kind != NULL ? host_kind : "host",
                host_name != NULL ? host_name : anonymous_host_name);
            return;
        }
        if (method == NULL && mir_method != NULL)
            method = transpiler_mir_routine_source_ast_of_type(
                mir_method, MIR_SCOPE_METHOD, AST_FUNC_DECL);
        if (method_name == NULL && method != NULL)
            method_name = ast_declaration_name(method);
        if (method == NULL || method->type != AST_FUNC_DECL) {
            if (transpiler_active_has_mir(ctx)) {
                transpiler_set_mir_inventory_missing(
                    ctx,
                    "MIR-only C path missing method body source metadata for %s method '%s.%s'",
                    host_kind != NULL ? host_kind : "host",
                    host_name != NULL ? host_name : anonymous_host_name,
                    method_name != NULL ? method_name : "(anonymous)");
                return;
            }
            continue;
        }

        if (mir_method == NULL) {
            transpiler_set_mir_inventory_missing(
                ctx,
                "MIR-only C path missing routine for %s method '%s.%s'",
                host_kind != NULL ? host_kind : "host",
                host_name != NULL ? host_name : anonymous_host_name,
                method_name != NULL ? method_name : "(anonymous)");
            return;
        }

        if (!transpiler_hosted_method_emit_name(
                emitted_name, sizeof(emitted_name), host_name, method_name)) {
            transpiler_set_backend_error_with_hints(ctx,
                PGY_CODE_C_TYPE_UNSUPPORTED,
                PGY_CAUSE_C_TYPE_UNSUPPORTED,
                PGY_FIX_USE_LLVM_BACKEND_OR_EXTEND_TRANSPILER,
                "C backend: hosted method symbol name is too long for %s.%s",
                host_name != NULL ? host_name : "(anonymous)",
                method_name != NULL ? method_name : "(anonymous)");
            return;
        }

        emit_func_decl_from_mir_named(method, mir_method, emitted_name,
                                      ctx->out, ctx);
        if (ctx != NULL && ctx->backend_error != NULL)
            return;
    }
}
