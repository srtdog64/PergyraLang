#include "transpiler_world_frontier_inputs.h"

#include "../parser/ast.h"
#include "domain_frontier_policy.h"
#include "transpiler_context.h"
#include "transpiler_decl_lookup.h"
#include "transpiler_projection.h"

size_t
transpiler_frontier_zone_member_count(void *ctx, const char *zone_name)
{
    TranspilerCtx *transpiler_ctx = (TranspilerCtx *)ctx;
    ASTNode *zone_decl;
    size_t state_count;
    TranspilerHostedZoneStateView state_view;
    TranspilerHostedZoneLayerSlotView layer_view;

    if (transpiler_ctx == NULL || zone_name == NULL)
        return 0;

    zone_decl = NULL;
    if (!transpiler_active_has_mir(transpiler_ctx)) {
        zone_decl = transpiler_find_named_decl_local(
            transpiler_ctx, AST_ZONE_DECL, zone_name);
        if (zone_decl == NULL || zone_decl->type != AST_ZONE_DECL)
            return 0;
    }

    state_view = transpiler_hosted_zone_state_view_from_decl(
        transpiler_ctx, zone_name, zone_decl);
    if ((transpiler_active_has_mir(transpiler_ctx)
            && !state_view.uses_mir_metadata)
        || transpiler_hosted_zone_state_view_missing_mir_metadata(
            &state_view)) {
        transpiler_set_mir_inventory_missing(transpiler_ctx,
            "MIR-only C path missing embedded zone state metadata for world frontier '%s'",
            zone_name);
        return 0;
    }
    state_count = state_view.count;
    layer_view = transpiler_hosted_zone_layer_slot_view_from_decl(
        transpiler_ctx, zone_name, zone_decl);
    if ((transpiler_active_has_mir(transpiler_ctx)
            && !layer_view.uses_mir_metadata)
        || transpiler_hosted_zone_layer_slot_view_missing_mir_metadata(
            &layer_view)) {
        transpiler_set_mir_inventory_missing(transpiler_ctx,
            "MIR-only C path missing embedded zone layer-slot metadata for world frontier '%s'",
            zone_name);
        return 0;
    }

    return pgy_frontier_embedded_zone_member_count(
        state_count, layer_view.count);
}

const char *
transpiler_frontier_world_zone_type_name(void *ctx, size_t index)
{
    return transpiler_hosted_world_zone_slot_view_type_name(
        (const TranspilerHostedWorldZoneSlotView *)ctx,
        index);
}
