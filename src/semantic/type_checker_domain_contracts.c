/*
 * Copyright (c) 2025 Pergyra Language Project
 * All rights reserved.
 *
 * Zone relation/effect contract validation.  This owner keeps lifecycle
 * contract diagnostics separate from domain lookup/state helpers so semantic
 * declaration code does not rebuild include-order coupling.
 */

#include "type_checker_internal.h"
#include "diag_codes.h"
#include "parser/ast_api.h"

static ASTNode *
find_nth_bindable_domain_slot(ASTNode **slots, size_t slot_count,
                              ASTNode **refreshes, size_t refresh_count,
                              size_t ordinal)
{
    size_t seen = 0;
    (void)refreshes;
    (void)refresh_count;

    for (size_t i = 0; i < slot_count; i++) {
        ASTNode *slot = slots[i];
        if (slot == NULL || slot->type != AST_DOMAIN_SLOT
            || !ast_domain_slot_is_binding(slot)) {
            continue;
        }

        if (seen == ordinal)
            return slot;
        seen++;
    }

    return NULL;
}

static const char *
relation_endpoint_kind_label(RelationEndpointKind kind)
{
    switch (kind) {
    case RELATION_ENDPOINT_SUBJECT:
        return "subject";
    case RELATION_ENDPOINT_OBJECT:
        return "object";
    case RELATION_ENDPOINT_TOBJECT:
        return "tobject";
    case RELATION_ENDPOINT_CLASS:
        return "class/value";
    case RELATION_ENDPOINT_NAMED:
        return "named";
    default:
        return "endpoint";
    }
}

bool
type_check_zone_effect_contract(ASTNode *zone,
                                ASTNode *apply_like,
                                const char *effect_slot_name,
                                const char *target_slot_name,
                                SemanticContext *ctx,
                                const char *action_name)
{
    ASTNode *effect_slot;
    ASTNode *effect_decl;
    ASTNode *target_slot;
    ASTNode *decl_target;
    Type *target_type;
    Type *decl_target_type;
    size_t target_count;
    size_t effect_slot_count;
    size_t effect_refresh_count;
    ASTNode **effect_slots;
    ASTNode **effect_refreshes;
    const char *zone_name;
    const char *effect_name;

    (void) action_name;

    effect_slot = find_zone_effect_slot(zone, effect_slot_name);
    target_slot = find_zone_domain_slot(zone, target_slot_name);
    if (zone == NULL || effect_slot == NULL || target_slot == NULL || ctx == NULL)
        return false;

    zone_name = ast_zone_name(zone) != NULL ? ast_zone_name(zone) : "<zone>";

    effect_decl = semantic_find_effect_decl_by_name(ctx,
        ast_zone_layer_slot_layer_type(effect_slot));
    if (effect_decl == NULL)
        return false;
    effect_name = ast_effect_name(effect_decl) != NULL
        ? ast_effect_name(effect_decl)
        : "<unknown>";
    effect_slots = ast_effect_slots(effect_decl, &effect_slot_count);
    effect_refreshes = ast_effect_refreshes(effect_decl, &effect_refresh_count);

    target_count = count_bindable_domain_slots(effect_slots,
        effect_slot_count,
        effect_refreshes,
        effect_refresh_count);
    if (target_count != 1) {
        semantic_error_with_hints(ctx, PGY_CODE_SEM_ZONE_CONTRACT_INVALID, PGY_CAUSE_ZONE_CONTRACT, PGY_FIX_ALIGN_ZONE_SLOT_OR_STATE_NAMING, apply_like,
            "Zone %s requires effect '%s' to declare exactly one bindable target slot, found %llu.\n"
            "Reason:\n"
            "- zone effect slot '%s' binds effect '%s'\n"
            "- lifecycle propagation needs one concrete target path at the declaration boundary\n"
            "- current declaration exposes %llu bindable target paths, so target provenance is ambiguous\n"
            "Fix:\n"
            "- keep exactly one bindable target slot in effect '%s'\n"
            "- or split the effect contract if it truly targets multiple subjects",
            zone_name,
            effect_name,
            (unsigned long long) target_count,
            effect_slot_name != NULL ? effect_slot_name : "<effect-slot>",
            effect_name,
            (unsigned long long) target_count,
            effect_name);
        return true;
    }

    decl_target = find_nth_bindable_domain_slot(effect_slots,
        effect_slot_count,
        effect_refreshes,
        effect_refresh_count, 0);
    if (decl_target == NULL)
        return false;

    target_type = domain_lookup_slot_type_metadata(target_slot, ctx);
    decl_target_type = domain_lookup_slot_type_metadata(decl_target, ctx);
    if (!type_is_assignable(target_type, decl_target_type)) {
        semantic_error_with_hints(ctx, PGY_CODE_SEM_ZONE_CONTRACT_INVALID, PGY_CAUSE_ZONE_CONTRACT, PGY_FIX_ALIGN_ZONE_SLOT_OR_STATE_NAMING, apply_like,
            "Zone %s target slot '%s' has type '%s' but effect '%s' expects target type '%s'.\n"
            "Contract source:\n"
            "- effect slot '%s' in zone contract '%s'\n"
            "- propagation edge is effect slot '%s' -> target slot '%s'\n"
            "Reason:\n"
            "- zone slot '%s' is bound to effect slot '%s'\n"
            "- propagation edge is effect slot '%s' -> target slot '%s'\n"
            "- effect '%s' declares target slot '%s' with required type '%s'\n"
            "- actual bound target type is '%s'\n"
            "Fix:\n"
            "- bind effect '%s' to a zone slot of type '%s'\n"
            "- or change effect '%s' so its target contract accepts '%s'",
            zone_name,
            target_slot_name != NULL ? target_slot_name : "<unknown>",
            target_type != NULL && target_type->name != NULL ? target_type->name : "<unknown>",
            effect_name,
            decl_target_type != NULL && decl_target_type->name != NULL
                ? decl_target_type->name
                : "<unknown>",
            effect_slot_name != NULL ? effect_slot_name : "<effect-slot>",
            zone_name,
            effect_slot_name != NULL ? effect_slot_name : "<effect-slot>",
            target_slot_name != NULL ? target_slot_name : "<target-slot>",
            target_slot_name != NULL ? target_slot_name : "<unknown>",
            effect_slot_name != NULL ? effect_slot_name : "<effect-slot>",
            effect_slot_name != NULL ? effect_slot_name : "<effect-slot>",
            target_slot_name != NULL ? target_slot_name : "<unknown>",
            effect_name,
            ast_domain_slot_name(decl_target) != NULL
                ? ast_domain_slot_name(decl_target)
                : "<target-slot>",
            decl_target_type != NULL && decl_target_type->name != NULL
                ? decl_target_type->name
                : "<unknown>",
            target_type != NULL && target_type->name != NULL ? target_type->name : "<unknown>",
            effect_name,
            decl_target_type != NULL && decl_target_type->name != NULL
                ? decl_target_type->name
                : "<unknown>",
            effect_name,
            target_type != NULL && target_type->name != NULL ? target_type->name : "<unknown>");
    }

    return true;
}

static bool
relation_slot_matches_between_kind(ASTNode *slot,
                                   Type *slot_type,
                                   RelationEndpointKind kind,
                                   ASTNode *named_type_ref,
                                   SemanticContext *ctx)
{
    ASTNode *decl;
    Type *named_type;

    if (slot == NULL)
        return false;

    if (kind == RELATION_ENDPOINT_SUBJECT)
        return ast_domain_slot_is_subject(slot);

    if (kind == RELATION_ENDPOINT_OBJECT) {
        if (ast_domain_slot_is_subject(slot))
            return false;
        if (ast_domain_slot_is_tobject(slot))
            return false;  /* tobject slot uses "tobject" kind, not "object" */
        decl = semantic_find_class_decl_by_name(ctx,
            slot_type != NULL ? slot_type->name : NULL);
        return decl != NULL
            && ast_class_is_struct(decl)
            && ast_class_nominal_kind(decl) == NOMINAL_DECL_OBJECT;
    }

    if (kind == RELATION_ENDPOINT_TOBJECT) {
        if (ast_domain_slot_is_subject(slot))
            return false;
        if (!ast_domain_slot_is_tobject(slot))
            return false;
        decl = semantic_find_class_decl_by_name(ctx,
            slot_type != NULL ? slot_type->name : NULL);
        return decl != NULL
            && ast_class_is_struct(decl)
            && ast_class_nominal_kind(decl) == NOMINAL_DECL_TOBJECT;
    }

    if (kind == RELATION_ENDPOINT_CLASS) {
        if (ast_domain_slot_is_subject(slot) || ast_domain_slot_is_tobject(slot))
            return false;
        decl = semantic_find_class_decl_by_name(ctx,
            slot_type != NULL ? slot_type->name : NULL);
        if (decl == NULL)
            return true;
        return ast_class_nominal_kind(decl) != NOMINAL_DECL_OBJECT
            && ast_class_nominal_kind(decl) != NOMINAL_DECL_TOBJECT;
    }

    named_type = domain_lookup_named_type_metadata(named_type_ref, ctx);
    return slot_type != NULL
        && named_type != NULL
        && type_equals(slot_type, named_type);
}

bool
type_check_zone_relation_contract(ASTNode *zone,
                                  ASTNode *link_like,
                                  const char *relation_slot_name,
                                  const char *left_slot_name,
                                  const char *right_slot_name,
                                  SemanticContext *ctx,
                                  const char *action_name)
{
    ASTNode *relation_slot;
    ASTNode *relation_decl;
    ASTNode *left_slot;
    ASTNode *right_slot;
    ASTNode *decl_left;
    ASTNode *decl_right;
    Type *left_type;
    Type *right_type;
    Type *decl_left_type;
    Type *decl_right_type;
    size_t endpoint_count;
    size_t relation_slot_count;
    size_t relation_refresh_count;
    ASTNode **relation_slots;
    ASTNode **relation_refreshes;
    RelationEndpointKind between_left_kind;
    RelationEndpointKind between_right_kind;
    ASTNode *between_left_type;
    ASTNode *between_right_type;
    const char *zone_name;
    const char *relation_name;

    (void) action_name;

    relation_slot = find_zone_relation_slot(zone, relation_slot_name);
    left_slot = find_zone_domain_slot(zone, left_slot_name);
    right_slot = find_zone_domain_slot(zone, right_slot_name);
    if (zone == NULL || relation_slot == NULL || left_slot == NULL || right_slot == NULL
        || ctx == NULL)
        return false;

    zone_name = ast_zone_name(zone) != NULL ? ast_zone_name(zone) : "<zone>";

    relation_decl = semantic_find_relation_decl_by_name(ctx,
        ast_zone_layer_slot_layer_type(relation_slot));
    if (relation_decl == NULL)
        return false;
    relation_name = ast_relation_name(relation_decl) != NULL
        ? ast_relation_name(relation_decl)
        : "<unknown>";
    relation_slots = ast_relation_slots(relation_decl, &relation_slot_count);
    relation_refreshes = ast_relation_refreshes(relation_decl,
                                                &relation_refresh_count);

    between_left_kind = ast_relation_between_left_kind(relation_decl);
    between_right_kind = ast_relation_between_right_kind(relation_decl);
    between_left_type = ast_relation_between_left_type(relation_decl);
    between_right_type = ast_relation_between_right_type(relation_decl);
    left_type = domain_lookup_slot_type_metadata(left_slot, ctx);
    right_type = domain_lookup_slot_type_metadata(right_slot, ctx);

    if (between_left_kind != RELATION_ENDPOINT_NAMED || between_right_kind != RELATION_ENDPOINT_NAMED
        || between_left_type != NULL || between_right_type != NULL) {
        if (!relation_slot_matches_between_kind(left_slot, left_type, between_left_kind, between_left_type, ctx)) {
            semantic_error_with_hints(ctx, PGY_CODE_SEM_ZONE_CONTRACT_INVALID, PGY_CAUSE_ZONE_CONTRACT, PGY_FIX_ALIGN_ZONE_SLOT_OR_STATE_NAMING, link_like,
                "Zone %s left slot '%s' does not satisfy relation '%s' left-endpoint contract.\n"
                "Reason:\n"
                "- zone relation slot '%s' binds declaration '%s'\n"
                "- relation '%s' requires a %s-compatible left endpoint\n"
                "- actual left slot '%s' resolves to type '%s'\n"
                "Fix:\n"
                "- bind relation '%s' to a left slot that matches the declared endpoint kind\n"
                "- or change relation '%s' so its left endpoint contract accepts '%s'",
                zone_name,
                left_slot_name != NULL ? left_slot_name : "<unknown>",
                relation_name,
                relation_slot_name != NULL ? relation_slot_name : "<relation-slot>",
                relation_name,
                relation_name,
                relation_endpoint_kind_label(between_left_kind),
                left_slot_name != NULL ? left_slot_name : "<unknown>",
                left_type != NULL && left_type->name != NULL ? left_type->name : "<unknown>",
                relation_name,
                relation_name,
                left_type != NULL && left_type->name != NULL ? left_type->name : "<unknown>");
        }

        if (!relation_slot_matches_between_kind(right_slot, right_type, between_right_kind, between_right_type, ctx)) {
            semantic_error_with_hints(ctx, PGY_CODE_SEM_ZONE_CONTRACT_INVALID, PGY_CAUSE_ZONE_CONTRACT, PGY_FIX_ALIGN_ZONE_SLOT_OR_STATE_NAMING, link_like,
                "Zone %s right slot '%s' does not satisfy relation '%s' right-endpoint contract.\n"
                "Reason:\n"
                "- zone relation slot '%s' binds declaration '%s'\n"
                "- relation '%s' requires a %s-compatible right endpoint\n"
                "- actual right slot '%s' resolves to type '%s'\n"
                "Fix:\n"
                "- bind relation '%s' to a right slot that matches the declared endpoint kind\n"
                "- or change relation '%s' so its right endpoint contract accepts '%s'",
                zone_name,
                right_slot_name != NULL ? right_slot_name : "<unknown>",
                relation_name,
                relation_slot_name != NULL ? relation_slot_name : "<relation-slot>",
                relation_name,
                relation_name,
                relation_endpoint_kind_label(between_right_kind),
                right_slot_name != NULL ? right_slot_name : "<unknown>",
                right_type != NULL && right_type->name != NULL ? right_type->name : "<unknown>",
                relation_name,
                relation_name,
                right_type != NULL && right_type->name != NULL ? right_type->name : "<unknown>");
        }
        return true;
    }

    endpoint_count = count_bindable_domain_slots(relation_slots,
        relation_slot_count,
        relation_refreshes,
        relation_refresh_count);
    if (endpoint_count != 2) {
        semantic_error_with_hints(ctx, PGY_CODE_SEM_ZONE_CONTRACT_INVALID, PGY_CAUSE_ZONE_CONTRACT, PGY_FIX_ALIGN_ZONE_SLOT_OR_STATE_NAMING, link_like,
            "Zone %s requires relation '%s' to declare exactly two bindable endpoint slots, found %llu.\n"
            "Reason:\n"
            "- zone relation slot '%s' binds declaration '%s'\n"
            "- relation propagation needs one left endpoint and one right endpoint at declaration time\n"
            "- current declaration exposes %llu bindable endpoint paths, so edge provenance is ambiguous\n"
            "Fix:\n"
            "- keep exactly two bindable endpoint slots in relation '%s'\n"
            "- or split the relation contract if it truly models multiple independent edges",
            zone_name,
            relation_name,
            (unsigned long long) endpoint_count,
            relation_slot_name != NULL ? relation_slot_name : "<relation-slot>",
            relation_name,
            (unsigned long long) endpoint_count,
            relation_name);
        return true;
    }

    decl_left = find_nth_bindable_domain_slot(relation_slots,
        relation_slot_count,
        relation_refreshes,
        relation_refresh_count, 0);
    decl_right = find_nth_bindable_domain_slot(relation_slots,
        relation_slot_count,
        relation_refreshes,
        relation_refresh_count, 1);
    if (decl_left == NULL || decl_right == NULL)
        return false;

    decl_left_type = domain_lookup_slot_type_metadata(decl_left, ctx);
    decl_right_type = domain_lookup_slot_type_metadata(decl_right, ctx);

    if (!type_is_assignable(left_type, decl_left_type)) {
        semantic_error_with_hints(ctx, PGY_CODE_SEM_ZONE_CONTRACT_INVALID, PGY_CAUSE_ZONE_CONTRACT, PGY_FIX_ALIGN_ZONE_SLOT_OR_STATE_NAMING, link_like,
            "Zone %s left slot '%s' has type '%s' but relation '%s' expects left endpoint type '%s'.\n"
            "Contract source:\n"
            "- relation slot '%s' in zone contract '%s'\n"
            "- endpoint edge: left '%s' -> relation '%s'\n"
            "Reason:\n"
            "- zone relation slot '%s' binds declaration '%s'\n"
            "- relation '%s' declares left endpoint slot '%s' with required type '%s'\n"
            "- actual bound left slot type is '%s'\n"
            "Fix:\n"
            "- bind relation '%s' to a left slot of type '%s'\n"
            "- or change relation '%s' so its left endpoint contract accepts '%s'",
            zone_name,
            left_slot_name != NULL ? left_slot_name : "<unknown>",
            left_type != NULL && left_type->name != NULL ? left_type->name : "<unknown>",
            relation_name,
            decl_left_type != NULL && decl_left_type->name != NULL
                ? decl_left_type->name
                : "<unknown>",
            relation_slot_name != NULL ? relation_slot_name : "<relation-slot>",
            zone_name,
            left_slot_name != NULL ? left_slot_name : "<left-slot>",
            relation_name,
            relation_slot_name != NULL ? relation_slot_name : "<relation-slot>",
            relation_name,
            relation_name,
            ast_domain_slot_name(decl_left) != NULL
                ? ast_domain_slot_name(decl_left)
                : "<left-slot>",
            decl_left_type != NULL && decl_left_type->name != NULL
                ? decl_left_type->name
                : "<unknown>",
            left_type != NULL && left_type->name != NULL ? left_type->name : "<unknown>",
            relation_name,
            decl_left_type != NULL && decl_left_type->name != NULL
                ? decl_left_type->name
                : "<unknown>",
            relation_name,
            left_type != NULL && left_type->name != NULL ? left_type->name : "<unknown>");
    }

    if (!type_is_assignable(right_type, decl_right_type)) {
        semantic_error_with_hints(ctx, PGY_CODE_SEM_ZONE_CONTRACT_INVALID, PGY_CAUSE_ZONE_CONTRACT, PGY_FIX_ALIGN_ZONE_SLOT_OR_STATE_NAMING, link_like,
            "Zone %s right slot '%s' has type '%s' but relation '%s' expects right endpoint type '%s'.\n"
            "Contract source:\n"
            "- relation slot '%s' in zone contract '%s'\n"
            "- endpoint edge: relation '%s' -> right '%s'\n"
            "Reason:\n"
            "- zone relation slot '%s' binds declaration '%s'\n"
            "- propagation edge is relation slot '%s' -> right endpoint '%s'\n"
            "- relation '%s' declares right endpoint slot '%s' with required type '%s'\n"
            "- actual bound right slot type is '%s'\n"
            "Fix:\n"
            "- bind relation '%s' to a right slot of type '%s'\n"
            "- or change relation '%s' so its right endpoint contract accepts '%s'",
            zone_name,
            right_slot_name != NULL ? right_slot_name : "<unknown>",
            right_type != NULL && right_type->name != NULL ? right_type->name : "<unknown>",
            relation_name,
            decl_right_type != NULL && decl_right_type->name != NULL
                ? decl_right_type->name
                : "<unknown>",
            relation_slot_name != NULL ? relation_slot_name : "<relation-slot>",
            zone_name,
            relation_name,
            right_slot_name != NULL ? right_slot_name : "<right-slot>",
            relation_slot_name != NULL ? relation_slot_name : "<relation-slot>",
            relation_name,
            relation_slot_name != NULL ? relation_slot_name : "<relation-slot>",
            right_slot_name != NULL ? right_slot_name : "<unknown>",
            relation_name,
            ast_domain_slot_name(decl_right) != NULL
                ? ast_domain_slot_name(decl_right)
                : "<right-slot>",
            decl_right_type != NULL && decl_right_type->name != NULL
                ? decl_right_type->name
                : "<unknown>",
            right_type != NULL && right_type->name != NULL ? right_type->name : "<unknown>",
            relation_name,
            decl_right_type != NULL && decl_right_type->name != NULL
                ? decl_right_type->name
                : "<unknown>",
            relation_name,
            right_type != NULL && right_type->name != NULL ? right_type->name : "<unknown>");
    }

    return true;
}
