#include "transpiler_zone_specialization_emit.h"

#include "parser/ast_api.h"
#include "transpiler_specialization_registry.h"

void
transpiler_emit_zone_required_specializations(
    TranspilerCtx *ctx,
    ASTNode **slots,
    size_t slot_count,
    const TranspilerHostedSharedFieldView *shared_view,
    const TranspilerHostedMethodView *method_view)
{
    for (size_t i = 0; i < slot_count; i++) {
        ASTNode *slot = slots[i];
        if (slot != NULL)
            ensure_type_specializations_from_ast_to(ctx, ctx->out,
                ast_domain_slot_type(slot));
    }
    for (size_t i = 0; shared_view != NULL && i < shared_view->count; i++) {
        ensure_type_specializations_from_ast_to(ctx, ctx->out,
            transpiler_hosted_shared_field_view_type(shared_view, i));
    }
    for (size_t i = 0; method_view != NULL && i < method_view->count; i++) {
        ASTNode *method =
            transpiler_hosted_method_view_source_ast(method_view, i);
        ensure_collection_specializations_from_stmt_to(ctx, ctx->out, method);
    }
}
