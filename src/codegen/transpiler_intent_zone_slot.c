/*
 * Copyright (c) 2026 Pergyra Language Project
 * C backend intent zone-slot resolution helpers.
 */

#include "transpiler_intent_zone_slot.h"

#include <string.h>

#include "parser/ast_api.h"
#include "transpiler_context.h"
#include "transpiler_decl_lookup.h"
#include "transpiler_intent_context.h"

const char *
resolve_intent_zone_slot_name(TranspilerCtx *ctx, ASTNode *intent,
                              ASTNode *step, const char *alias)
{
    if (ctx == NULL || intent == NULL || step == NULL || alias == NULL
        || ast_intent_step_where_type(step) == NULL
        || ast_intent_step_where_type(step)->type != AST_TYPE
        || ast_type_name(ast_intent_step_where_type(step)) == NULL) {
        return "<unbound>";
    }
    return resolve_intent_zone_slot_name_for_zone(ctx, intent,
        ast_type_name(ast_intent_step_where_type(step)), alias);
}

const char *
resolve_intent_zone_slot_name_for_zone(TranspilerCtx *ctx, ASTNode *intent,
                                       const char *zone_type_name, const char *alias)
{
    return resolve_intent_zone_slot_name_for_zone_with_metadata(
        ctx, intent, zone_type_name, alias, NULL, NULL, 0);
}

const char *
resolve_intent_zone_slot_name_for_zone_with_metadata(
    TranspilerCtx *ctx,
    ASTNode *intent,
    const char *zone_type_name,
    const char *alias,
    const char **participant_aliases,
    const char **participant_types,
    size_t participant_count)
{
    ASTNode *zone_decl = NULL;
    const char *participant_type = NULL;
    const char *named_match = NULL;
    const char *typed_match = NULL;
    bool typed_match_is_ambiguous = false;
    TranspilerHostedDomainSlotView slot_view;

    if (ctx == NULL || intent == NULL || zone_type_name == NULL || alias == NULL) {
        return "<unbound>";
    }

    zone_decl = find_zone_decl_in_program_view(ctx, zone_type_name);
    participant_type = intent_zone_binding_type_name_with_metadata(
        intent, alias, participant_aliases, participant_types, participant_count);
    if (zone_decl == NULL)
        return "<unbound>";

    slot_view = transpiler_hosted_domain_slot_view_from_decl(ctx,
        zone_type_name, zone_decl);
    if (transpiler_hosted_domain_slot_view_missing_mir_metadata(&slot_view)) {
        transpiler_set_mir_inventory_missing(ctx,
            "MIR-only C path missing intent zone-slot metadata for '%s'",
            zone_type_name);
        return "<unbound>";
    }

    for (size_t i = 0; i < slot_view.count; i++) {
        const char *slot_name =
            transpiler_hosted_domain_slot_view_name(&slot_view, i);
        const char *slot_type_name =
            transpiler_hosted_domain_slot_view_type_name(&slot_view, i);
        if (!transpiler_hosted_domain_slot_view_is_subject_like(&slot_view, i)
            || slot_name == NULL) {
            continue;
        }
        if (strcmp(slot_name, alias) == 0) {
            named_match = slot_name;
            break;
        }
        if (participant_type != NULL
            && slot_type_name != NULL
            && strcmp(slot_type_name, participant_type) == 0) {
            if (typed_match != NULL)
                typed_match_is_ambiguous = true;
            else
                typed_match = slot_name;
        }
    }

    if (named_match != NULL)
        return named_match;
    if (typed_match != NULL && !typed_match_is_ambiguous)
        return typed_match;
    return "<unbound>";
}
