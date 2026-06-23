#include "transpiler_zone_specialization_emit.h"

#include "parser/ast_api.h"
#include "transpiler_context.h"
#include "transpiler_specialization_registry.h"

void
transpiler_emit_zone_required_specializations(
    TranspilerCtx *ctx,
    const TranspilerHostedDomainSlotView *slot_view,
    const TranspilerHostedSharedFieldView *shared_view,
    const TranspilerHostedMethodView *method_view)
{
    for (size_t i = 0; slot_view != NULL && i < slot_view->count; i++) {
        ensure_type_specializations_from_ast_to(ctx, ctx->out,
            transpiler_hosted_domain_slot_view_type(slot_view, i));
    }
    for (size_t i = 0; shared_view != NULL && i < shared_view->count; i++) {
        ensure_type_specializations_from_ast_to(ctx, ctx->out,
            transpiler_hosted_shared_field_view_type(shared_view, i));
    }
    for (size_t i = 0; method_view != NULL && i < method_view->count; i++) {
        const MIRDeclMethod *method_meta =
            transpiler_hosted_method_view_metadata(method_view, i);
        const MIRRoutine *mir_method =
            transpiler_mir_decl_method_routine(ctx, method_meta);
        if (method_meta == NULL || mir_method == NULL) {
            transpiler_set_mir_inventory_missing(ctx,
                "MIR-only C path missing method specialization routine for zone");
            return;
        }
        ensure_collection_specializations_from_mir_routine_to(ctx, ctx->out,
            mir_method);
    }
}
