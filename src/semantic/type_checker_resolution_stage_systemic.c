#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

#include "type_checker_internal.h"

static char *
stage_systemic_strdup_fmt(const char *fmt, ...)
{
    va_list ap, ap2;
    int len;
    char *buf;

    va_start(ap, fmt);
    va_copy(ap2, ap);
    len = vsnprintf(NULL, 0, fmt, ap);
    va_end(ap);
    if (len < 0) {
        va_end(ap2);
        return NULL;
    }

    buf = malloc((size_t)len + 1);
    if (buf != NULL)
        vsnprintf(buf, (size_t)len + 1, fmt, ap2);
    va_end(ap2);
    return buf;
}

void
semantic_stage_party_decl(ASTNode *decl, SemanticContext *ctx)
{
    if (decl == NULL || decl->type != AST_PARTY_DECL || ctx == NULL)
        return;

    semantic_stage_generic_contract_nodes(
        decl->data.party_decl.generic_params,
        NULL,
        ctx,
        decl,
        "party",
        decl->data.party_decl.name);
    (void)semantic_stage_resolve_type_quiet(
        decl->data.party_decl.extends,
        ctx,
        decl,
        decl->data.party_decl.name,
        "party extends lookup");
    for (size_t i = 0; i < decl->data.party_decl.shared_count; i++) {
        ASTNode *field = decl->data.party_decl.shared_fields[i];
        if (field == NULL || field->type != AST_PARTY_SHARED)
            continue;
        (void)semantic_stage_resolve_type_quiet(
            field->data.party_shared.type,
            ctx,
            field,
            field->data.party_shared.name,
            "party shared field type lookup");
    }
    for (size_t i = 0; i < decl->data.party_decl.role_count; i++) {
        ASTNode *role_slot = decl->data.party_decl.role_slots[i];
        char *consumer_name;
        if (role_slot == NULL || role_slot->type != AST_ROLE_SLOT)
            continue;
        consumer_name = stage_systemic_strdup_fmt(
            "party %s.%s",
            decl->data.party_decl.name != NULL
                ? decl->data.party_decl.name : "<party>",
            role_slot->data.role_slot.slot_name != NULL
                ? role_slot->data.role_slot.slot_name : "<role-slot>");
        if (consumer_name == NULL)
            continue;
        semantic_stage_required_abilities(
            role_slot->data.role_slot.required_abilities,
            role_slot->data.role_slot.ability_count,
            ctx,
            role_slot,
            consumer_name,
            "party role slot ability consumer lookup");
        free(consumer_name);
    }
    semantic_stage_method_array(
        decl->data.party_decl.methods,
        decl->data.party_decl.method_count,
        ctx,
        decl->data.party_decl.name);
}

void
semantic_stage_roster_decl(ASTNode *decl, SemanticContext *ctx)
{
    if (decl == NULL || decl->type != AST_ROSTER_DECL || ctx == NULL)
        return;

    semantic_stage_generic_contract_nodes(
        decl->data.roster_decl.generic_params,
        NULL,
        ctx,
        decl,
        "roster",
        decl->data.roster_decl.name);
    for (size_t i = 0; i < decl->data.roster_decl.party_count; i++) {
        ASTNode *slot = decl->data.roster_decl.party_slots[i];
        if (slot == NULL || slot->type != AST_SYSTEMIC_SLOT)
            continue;
        (void)semantic_stage_named_decl_quiet(
            ctx,
            AST_PARTY_DECL,
            slot->data.roster_slot.party_type);
    }
    for (size_t i = 0; i < decl->data.roster_decl.shared_count; i++) {
        ASTNode *field = decl->data.roster_decl.shared_fields[i];
        if (field == NULL || field->type != AST_PARTY_SHARED)
            continue;
        (void)semantic_stage_resolve_type_quiet(
            field->data.party_shared.type,
            ctx,
            field,
            field->data.party_shared.name,
            "roster shared field type lookup");
    }
    semantic_stage_method_array(
        decl->data.roster_decl.methods,
        decl->data.roster_decl.method_count,
        ctx,
        decl->data.roster_decl.name);
}

void
semantic_stage_world_decl(ASTNode *decl, SemanticContext *ctx)
{
    if (decl == NULL || decl->type != AST_WORLD_DECL || ctx == NULL)
        return;

    ctx->current_world = decl;
    for (size_t i = 0; i < decl->data.world_decl.roster_count; i++) {
        ASTNode *roster = decl->data.world_decl.rosters[i];
        if (roster == NULL || roster->type != AST_WORLD_SYSTEMIC)
            continue;
        (void)semantic_stage_named_decl_quiet(
            ctx,
            AST_ROSTER_DECL,
            roster->data.world_roster.roster_type);
    }
    for (size_t i = 0; i < decl->data.world_decl.zone_count; i++) {
        ASTNode *zone = decl->data.world_decl.zones[i];
        if (zone == NULL || zone->type != AST_WORLD_ZONE)
            continue;
        (void)semantic_stage_named_decl_quiet(
            ctx,
            AST_ZONE_DECL,
            zone->data.world_zone.zone_type);
    }
    semantic_stage_world_local_contracts(decl, ctx);
    for (size_t i = 0; i < decl->data.world_decl.shared_count; i++) {
        ASTNode *field = decl->data.world_decl.shared_fields[i];
        if (field == NULL || field->type != AST_PARTY_SHARED)
            continue;
        if (semantic_type_resolution_lookup_metadata_type_ref(ctx,
                field->data.party_shared.type) == NULL) {
            (void)semantic_stage_resolve_type_quiet(
                field->data.party_shared.type,
                ctx,
                field,
                field->data.party_shared.name,
                "world shared field type lookup");
        }
    }
    semantic_stage_method_array(
        decl->data.world_decl.methods,
        decl->data.world_decl.method_count,
        ctx,
        decl->data.world_decl.name);
}

void
semantic_stage_intent_decl(ASTNode *decl, SemanticContext *ctx)
{
    if (decl == NULL || decl->type != AST_INTENT_DECL || ctx == NULL)
        return;

    for (size_t i = 0; i < decl->data.intent_decl.involve_count; i++) {
        ASTNode *binding = decl->data.intent_decl.involves[i];
        if (binding == NULL || binding->type != AST_INTENT_INVOLVES)
            continue;
        (void)semantic_stage_resolve_type_quiet(
            binding->data.intent_involves.subject_type,
            ctx,
            binding,
            binding->data.intent_involves.alias,
            "intent involves type lookup");
    }
    for (size_t i = 0; i < decl->data.intent_decl.value_count; i++) {
        ASTNode *binding = decl->data.intent_decl.values[i];
        if (binding == NULL || binding->type != AST_INTENT_VALUE)
            continue;
        (void)semantic_stage_resolve_type_quiet(
            binding->data.intent_value.value_type,
            ctx,
            binding,
            binding->data.intent_value.alias,
            "intent value type lookup");
    }
    (void)semantic_stage_resolve_type_quiet(
        decl->data.intent_decl.default_where_type,
        ctx,
        decl,
        decl->data.intent_decl.name,
        "intent default where-type lookup");
    for (size_t i = 0; i < decl->data.intent_decl.step_count; i++) {
        ASTNode *step = decl->data.intent_decl.steps[i];
        char *step_consumer_name;
        if (step == NULL || step->type != AST_INTENT_STEP)
            continue;
        step_consumer_name = stage_systemic_strdup_fmt(
            "intent %s.%s",
            decl->data.intent_decl.name != NULL
                ? decl->data.intent_decl.name : "<intent>",
            step->data.intent_step.name != NULL
                ? step->data.intent_step.name : "<step>");
        if (step_consumer_name == NULL)
            continue;
        (void)semantic_stage_resolve_type_quiet(
            step->data.intent_step.where_type,
            ctx,
            step,
            step_consumer_name,
            "intent step where-type lookup");
        semantic_stage_required_abilities(
            step->data.intent_step.required_abilities,
            step->data.intent_step.required_ability_count,
            ctx,
            step,
            step_consumer_name,
            "intent step ability consumer lookup");
        (void)semantic_stage_named_decl_quiet(
            ctx,
            AST_EFFECT_DECL,
            step->data.intent_step.causes_effect);
        free(step_consumer_name);
    }
}
