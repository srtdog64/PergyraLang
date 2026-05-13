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
    const char *party_name;
    ASTNode **shared_fields;
    ASTNode **methods;
    size_t role_count;
    size_t shared_count;
    size_t method_count;

    if (decl == NULL || decl->type != AST_PARTY_DECL || ctx == NULL)
        return;
    party_name = ast_party_name(decl);
    role_count = ast_party_role_count(decl);
    shared_fields = ast_party_shared_fields(decl, &shared_count);
    methods = ast_party_methods(decl, &method_count);

    semantic_stage_generic_contract_nodes(
        decl->data.party_decl.generic_params,
        NULL,
        ctx,
        decl,
        "party",
        party_name);
    (void)semantic_stage_resolve_type_quiet(
        decl->data.party_decl.extends,
        ctx,
        decl,
        party_name,
        "party extends lookup");
    for (size_t i = 0; i < shared_count; i++) {
        ASTNode *field = shared_fields[i];
        if (field == NULL || field->type != AST_PARTY_SHARED)
            continue;
        (void)semantic_stage_resolve_type_quiet(
            ast_party_shared_type(field),
            ctx,
            field,
            ast_party_shared_name(field),
            "party shared field type lookup");
    }
    for (size_t i = 0; i < role_count; i++) {
        ASTNode *role_slot = ast_party_role(decl, i);
        const char *slot_name = ast_role_slot_name(role_slot);
        size_t ability_count = ast_role_slot_required_ability_count(role_slot);
        ASTNode **abilities =
            ast_role_slot_required_abilities(role_slot, NULL);
        char *consumer_name;
        if (role_slot == NULL || role_slot->type != AST_ROLE_SLOT)
            continue;
        consumer_name = stage_systemic_strdup_fmt(
            "party %s.%s",
            party_name != NULL ? party_name : "<party>",
            slot_name != NULL ? slot_name : "<role-slot>");
        if (consumer_name == NULL)
            continue;
        semantic_stage_required_abilities(
            abilities,
            ability_count,
            ctx,
            role_slot,
            consumer_name,
            "party role slot ability consumer lookup");
        free(consumer_name);
    }
    semantic_stage_method_array(
        methods,
        method_count,
        ctx,
        party_name);
}

void
semantic_stage_roster_decl(ASTNode *decl, SemanticContext *ctx)
{
    const char *roster_name;
    ASTNode **shared_fields;
    ASTNode **methods;
    size_t party_count;
    size_t shared_count;
    size_t method_count;

    if (decl == NULL || decl->type != AST_ROSTER_DECL || ctx == NULL)
        return;
    roster_name = ast_roster_name(decl);
    party_count = ast_roster_party_count(decl);
    shared_fields = ast_roster_shared_fields(decl, &shared_count);
    methods = ast_roster_methods(decl, &method_count);

    semantic_stage_generic_contract_nodes(
        decl->data.roster_decl.generic_params,
        NULL,
        ctx,
        decl,
        "roster",
        roster_name);
    for (size_t i = 0; i < party_count; i++) {
        ASTNode *slot = ast_roster_party(decl, i);
        if (slot == NULL || slot->type != AST_SYSTEMIC_SLOT)
            continue;
        (void)semantic_stage_named_decl_quiet(
            ctx,
            AST_PARTY_DECL,
            ast_roster_slot_party_type(slot));
    }
    for (size_t i = 0; i < shared_count; i++) {
        ASTNode *field = shared_fields[i];
        if (field == NULL || field->type != AST_PARTY_SHARED)
            continue;
        (void)semantic_stage_resolve_type_quiet(
            ast_party_shared_type(field),
            ctx,
            field,
            ast_party_shared_name(field),
            "roster shared field type lookup");
    }
    semantic_stage_method_array(
        methods,
        method_count,
        ctx,
        roster_name);
}

void
semantic_stage_world_decl(ASTNode *decl, SemanticContext *ctx)
{
    ASTNode **rosters;
    ASTNode **zones;
    ASTNode **shared_fields;
    ASTNode **methods;
    size_t roster_count;
    size_t zone_count;
    size_t shared_count;
    size_t method_count;

    if (decl == NULL || decl->type != AST_WORLD_DECL || ctx == NULL)
        return;
    rosters = ast_world_rosters(decl, &roster_count);
    zones = ast_world_zones(decl, &zone_count);
    shared_fields = ast_world_shared_fields(decl, &shared_count);
    methods = ast_world_methods(decl, &method_count);

    ctx->current_world = decl;
    for (size_t i = 0; i < roster_count; i++) {
        ASTNode *roster = rosters[i];
        if (roster == NULL || roster->type != AST_WORLD_SYSTEMIC)
            continue;
        (void)semantic_stage_named_decl_quiet(
            ctx,
            AST_ROSTER_DECL,
            ast_world_roster_type_name(roster));
    }
    for (size_t i = 0; i < zone_count; i++) {
        ASTNode *zone = zones[i];
        if (zone == NULL || zone->type != AST_WORLD_ZONE)
            continue;
        (void)semantic_stage_named_decl_quiet(
            ctx,
            AST_ZONE_DECL,
            ast_world_zone_type_name(zone));
    }
    semantic_stage_world_local_contracts(decl, ctx);
    for (size_t i = 0; i < shared_count; i++) {
        ASTNode *field = shared_fields[i];
        if (field == NULL || field->type != AST_PARTY_SHARED)
            continue;
        if (semantic_type_resolution_lookup_metadata_type_ref(ctx,
                ast_party_shared_type(field)) == NULL) {
            (void)semantic_stage_resolve_type_quiet(
                ast_party_shared_type(field),
                ctx,
                field,
                ast_party_shared_name(field),
                "world shared field type lookup");
        }
    }
    semantic_stage_method_array(
        methods,
        method_count,
        ctx,
        ast_world_name(decl));
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
            ast_intent_step_name(step) != NULL
                ? ast_intent_step_name(step) : "<step>");
        if (step_consumer_name == NULL)
            continue;
        (void)semantic_stage_resolve_type_quiet(
            ast_intent_step_where_type(step),
            ctx,
            step,
            step_consumer_name,
            "intent step where-type lookup");
        semantic_stage_required_abilities(
            ast_intent_step_required_abilities(step, NULL),
            ast_intent_step_required_ability_count(step),
            ctx,
            step,
            step_consumer_name,
            "intent step ability consumer lookup");
        (void)semantic_stage_named_decl_quiet(
            ctx,
            AST_EFFECT_DECL,
            ast_intent_step_causes_effect(step));
        free(step_consumer_name);
    }
}
