
bool
type_check_relation_decl(ASTNode *node, SemanticContext *ctx)
{
    ASTNode *saved_relation = ctx->current_relation;
    ctx->current_relation = node;

    bool ok = type_check_overlay_decl_common(node, ctx,
        node->data.relation_decl.name,
        SYMBOL_RELATION,
        node->data.relation_decl.shared_fields,
        node->data.relation_decl.shared_count,
        node->data.relation_decl.methods,
        node->data.relation_decl.method_count,
        "relation");

    ok = type_check_domain_slots(node->data.relation_decl.slots,
        node->data.relation_decl.slot_count, ctx, "Relation") && ok;
    ok = type_check_domain_slot_initializers(node->data.relation_decl.slots,
        node->data.relation_decl.slot_count, ctx, "relation") && ok;
    for (size_t i = 0; i < node->data.relation_decl.refresh_count; i++) {
        ASTNode *refresh = node->data.relation_decl.refreshes[i];
        const char *action_name =
            refresh->data.zone_refresh.derive_target_kind ? "bind"
            : (refresh->data.zone_refresh.requires_dto ? "publish" : "refresh");
        if (refresh == NULL)
            continue;
        ok = type_check_projection_contract(node->data.relation_decl.slots,
            node->data.relation_decl.slot_count, "Relation", refresh,
            refresh->data.zone_refresh.object_slot_name,
            refresh->data.zone_refresh.source_slot_name, ctx,
            action_name) && ok;
    }
    size_t endpoint_count = count_bindable_domain_slots(
        node->data.relation_decl.slots,
        node->data.relation_decl.slot_count,
        node->data.relation_decl.refreshes,
        node->data.relation_decl.refresh_count);
    if (endpoint_count == 0
        && !(node->data.relation_decl.between_left_kind != RELATION_ENDPOINT_NAMED
            || node->data.relation_decl.between_right_kind != RELATION_ENDPOINT_NAMED
            || node->data.relation_decl.between_left_type != NULL
            || node->data.relation_decl.between_right_type != NULL)) {
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
            node->data.relation_decl.name,
            node->data.relation_decl.name);
    } else if (endpoint_count > 2) {
        semantic_warning(ctx, node,
            "Relation '%s' currently declares more than two endpoint slots.\n"
            "Reason:\n"
            "- relation is currently treated as a small edge contract\n"
            "- more than two endpoints usually mean party/zone coordination rather than a single relation edge\n"
            "Fix:\n"
            "- reduce the relation to two endpoints\n"
            "- or move the coordination shape into party/zone",
            node->data.relation_decl.name);
    }
    /* Validate 'between' clause if present */
    RelationEndpointKind blk = node->data.relation_decl.between_left_kind;
    RelationEndpointKind brk = node->data.relation_decl.between_right_kind;
    ASTNode *blt = node->data.relation_decl.between_left_type;
    ASTNode *brt = node->data.relation_decl.between_right_type;
    if (blk != RELATION_ENDPOINT_NAMED || brk != RELATION_ENDPOINT_NAMED
        || blt != NULL || brt != NULL) {
        /* At least one side must be 'subject' */
        bool left_is_subject = (blk == RELATION_ENDPOINT_SUBJECT);
        bool right_is_subject = (brk == RELATION_ENDPOINT_SUBJECT);
        if (!left_is_subject && !right_is_subject) {
            semantic_error_with_hints(ctx, "PGY_SEM_ZONE_CONTRACT_INVALID", "semantic:zone_contract", "align-zone-slot-or-state-naming", node,
                "Relation '%s' must declare at least one subject endpoint in 'between'.\n"
                "Reason:\n"
                "- relation edges are anchored around at least one active subject endpoint\n"
                "- current endpoint kinds are %d and %d, so the contract has no active anchor\n"
                "Fix:\n"
                "- change one side of 'between' to subject\n"
                "- or model this shape as effect/object projection instead",
                node->data.relation_decl.name, (int)blk, (int)brk);
            ok = false;
        }
    }

    ctx->current_relation = saved_relation;
    return ok && !ctx->has_error;
}
