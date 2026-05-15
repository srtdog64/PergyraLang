/*
 * Copyright (c) 2026 Pergyra Language Project
 * C backend intent zone-slot resolution helpers.
 */

#include "transpiler_intent_zone_slot.h"

#include <stdint.h>
#include <string.h>

#include "parser/ast_api.h"
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
    ASTNode *zone_decl = NULL;
    const char *participant_type = NULL;
    ASTNode *named_match = NULL;
    ASTNode *typed_match = NULL;

    if (ctx == NULL || intent == NULL || zone_type_name == NULL || alias == NULL) {
        return "<unbound>";
    }

    zone_decl = find_zone_decl_in_program_view(ctx, zone_type_name);
    participant_type = intent_participant_type_name(intent, alias);
    if (zone_decl == NULL)
        return "<unbound>";

    size_t slot_count = 0;
    ASTNode **slots = ast_zone_slots(zone_decl, &slot_count);
    for (size_t i = 0; i < slot_count; i++) {
        ASTNode *slot = slots[i];
        const char *slot_name = ast_domain_slot_name(slot);
        ASTNode *slot_type = ast_domain_slot_type(slot);
        if (slot == NULL || slot->type != AST_DOMAIN_SLOT
            || !ast_domain_slot_is_subject(slot)
            || slot_name == NULL) {
            continue;
        }
        if (strcmp(slot_name, alias) == 0) {
            named_match = slot;
            break;
        }
        if (participant_type != NULL
            && slot_type != NULL
            && slot_type->type == AST_TYPE
            && ast_type_name(slot_type) != NULL
            && strcmp(ast_type_name(slot_type), participant_type) == 0) {
            if (typed_match != NULL)
                typed_match = (ASTNode *)(uintptr_t)1;
            else
                typed_match = slot;
        }
    }

    if (named_match != NULL)
        return ast_domain_slot_name(named_match);
    if (typed_match != NULL && typed_match != (ASTNode *)(uintptr_t)1)
        return ast_domain_slot_name(typed_match);
    return "<unbound>";
}
