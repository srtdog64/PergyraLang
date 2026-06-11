#include "transpiler_zone_methods_emit.h"

#include <stddef.h>

#include "../parser/ast_api.h"
#include "transpiler_func_forward_metadata.h"
#include "transpiler_hosted_method_body_emit.h"

void
transpiler_emit_zone_hosted_methods_bridge(
    const char *name,
    const TranspilerHostedMethodView *method_view,
    TranspilerCtx *ctx)
{
    if (!transpiler_require_hosted_method_view_rows(
            ctx,
            method_view,
            "MIR-only C path has invalid method declaration metadata row for zone '%s'",
            name != NULL ? name : "(anonymous-zone)")) {
        return;
    }

    for (size_t i = 0; method_view != NULL && i < method_view->count; i++) {
        const MIRDeclMethod *method_meta =
            transpiler_hosted_method_view_metadata(method_view, i);
        ASTNode *method =
            transpiler_hosted_method_view_source_ast(method_view, i);
        if (method_meta == NULL && transpiler_active_has_mir(ctx)) {
            transpiler_set_mir_inventory_missing(ctx,
                "MIR-only C path missing hosted method forward metadata row for zone '%s'",
                name != NULL ? name : "(anonymous-zone)");
            return;
        }
        if (method_meta == NULL
            && (method == NULL || method->type != AST_FUNC_DECL)) {
            continue;
        }
        emit_hosted_method_forward_decl_from_metadata(name, method_meta,
            method, true, ctx->out, ctx);
    }

    transpiler_emit_hosted_methods_from_mir_or_error(name, "(anonymous-zone)",
        "zone", method_view, ctx);
}
