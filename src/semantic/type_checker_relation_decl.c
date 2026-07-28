#include "type_checker_internal.h"
#include "diag_codes.h"

bool
type_check_relation_decl(ASTNode *node, SemanticContext *ctx)
{
    ASTNode *saved_relation = ctx->current_relation;
    ASTNode **slots;
    ASTNode **refreshes;
    ASTNode **shared_fields;
    ASTNode **methods;
    size_t slot_count;
    size_t refresh_count;
    size_t shared_count;
    size_t method_count;
    slots = ast_relation_slots(node, &slot_count);
    refreshes = ast_relation_refreshes(node, &refresh_count);
    shared_fields = ast_relation_shared_fields(node, &shared_count);
    methods = ast_relation_methods(node, &method_count);
    ctx->current_relation = node;

    bool ok = type_check_overlay_decl_common(node, ctx,
        ast_relation_name(node),
        SYMBOL_RELATION,
        shared_fields,
        shared_count,
        methods,
        method_count,
        "relation");

    ok = type_check_domain_slots(slots, slot_count, ctx, "Relation") && ok;
    ok = type_check_domain_slot_initializers(slots, slot_count, ctx,
        "relation") && ok;
    for (size_t i = 0; i < refresh_count; i++) {
        ASTNode *refresh = refreshes[i];
        if (refresh == NULL)
            continue;
        const char *action_name =
            ast_zone_refresh_derives_target_kind(refresh) ? "bind"
            : (ast_zone_refresh_requires_dto(refresh) ? "publish" : "refresh");
        ok = type_check_projection_contract(slots, slot_count, node, "Relation",
            ast_relation_name(node), refresh,
            ast_zone_refresh_object_slot_name(refresh),
            ast_zone_refresh_source_slot_name(refresh), ctx,
            action_name) && ok;
    }
    size_t endpoint_count = count_bindable_domain_slots(
        slots,
        slot_count,
        refreshes,
        refresh_count);
    if (endpoint_count == 0
        && !(ast_relation_between_left_kind(node) != RELATION_ENDPOINT_NAMED
            || ast_relation_between_right_kind(node) != RELATION_ENDPOINT_NAMED
            || ast_relation_between_left_type(node) != NULL
            || ast_relation_between_right_type(node) != NULL)) {
        semantic_warning(ctx, node,
            "Relation '%s' should declare at least one endpoint slot.\n"
            "Reason:\n"
            "- declaration propagation edge is relation '%s' -> endpoint slot\n"
            "- relation contracts need a concrete endpoint surface to bind/link against\n"
            "- without an endpoint slot, relation and projection propagation stay underspecified\n"
            "- branch/join or zone/world propagation cannot anchor a stable relation edge without at least one endpoint slot\n"
            "Fix:\n"
            "- use 'for name: Type'\n"
            "- or declare a bindable subject/object endpoint slot",
            ast_relation_name(node),
            ast_relation_name(node));
    } else if (endpoint_count > 2) {
        semantic_warning(ctx, node,
            "Relation '%s' currently declares more than two endpoint slots.\n"
            "Reason:\n"
            "- relation is currently treated as a small edge contract\n"
            "- more than two endpoints usually mean party/zone coordination rather than a single relation edge\n"
            "Fix:\n"
            "- reduce the relation to two endpoints\n"
            "- or move the coordination shape into party/zone",
            ast_relation_name(node));
    }
    /* Validate 'between' clause if present */
    RelationEndpointKind blk = ast_relation_between_left_kind(node);
    RelationEndpointKind brk = ast_relation_between_right_kind(node);
    ASTNode *blt = ast_relation_between_left_type(node);
    ASTNode *brt = ast_relation_between_right_type(node);
    if (blk != RELATION_ENDPOINT_NAMED || brk != RELATION_ENDPOINT_NAMED
        || blt != NULL || brt != NULL) {
        /* At least one side must be 'subject' */
        bool left_is_subject = (blk == RELATION_ENDPOINT_SUBJECT);
        bool right_is_subject = (brk == RELATION_ENDPOINT_SUBJECT);
        if (!left_is_subject && !right_is_subject) {
            semantic_error_with_hints(ctx, PGY_CODE_SEM_ZONE_CONTRACT_INVALID, PGY_CAUSE_ZONE_CONTRACT, PGY_FIX_ALIGN_ZONE_SLOT_OR_STATE_NAMING, node,
                "Relation '%s' must declare at least one subject endpoint in 'between'.\n"
                "Reason:\n"
                "- relation edges are anchored around at least one active subject endpoint\n"
                "- current endpoint kinds are %d and %d, so the contract has no active anchor\n"
                "Fix:\n"
                "- change one side of 'between' to subject\n"
                "- or model this shape as effect/object projection instead",
                ast_relation_name(node), (int)blk, (int)brk);
            ok = false;
        }
    }

    ctx->current_relation = saved_relation;
    return ok && !ctx->has_error;
}
