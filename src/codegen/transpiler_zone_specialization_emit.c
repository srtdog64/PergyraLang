#include "transpiler_zone_specialization_emit.h"

#include "parser/ast_api.h"
#include "transpiler_specialization_helpers.h"

void
transpiler_emit_zone_required_specializations(
    TranspilerCtx *ctx,
    ASTNode **slots,
    size_t slot_count,
    ASTNode **shared_fields,
    size_t shared_count,
    const TranspilerHostedMethodView *method_view)
{
    for (size_t i = 0; i < slot_count; i++) {
        ASTNode *slot = slots[i];
        if (slot != NULL)
            ensure_type_specializations_from_ast_to(ctx, ctx->out,
                ast_domain_slot_type(slot));
    }
    for (size_t i = 0; i < shared_count; i++) {
        ASTNode *shared = shared_fields[i];
        if (shared != NULL)
            ensure_type_specializations_from_ast_to(ctx, ctx->out,
                ast_party_shared_type(shared));
    }
    for (size_t i = 0; method_view != NULL && i < method_view->count; i++) {
        ASTNode *method =
            transpiler_hosted_method_view_source_ast(method_view, i);
        ensure_collection_specializations_from_stmt_to(ctx, ctx->out, method);
    }
}
