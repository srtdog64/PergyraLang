/*
 * Copyright (c) 2025 Pergyra Language Project
 * All rights reserved.
 *
 * Domain helpers: counting / locating / contract-checking routines shared
 * across declaration kinds (subject, object, zone, world, relation, effect,
 * projection).  These helpers live in their own translation unit so future
 * declaration kinds can depend on them without hidden include-order coupling.
 * See docs/101_semantic_split_template.md (5-A slice).
 */

#include "type_checker_internal.h"
#include "type_checker_decls_a_helpers_internal.h"
#include "type_checker_intent_helpers_internal.h"
#include "type_checker_visibility.h"
#include "diag_codes.h"
#include "../common/string_compat.h"

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

Type *
domain_resolve_type_ref(ASTNode *type_ref, SemanticContext *ctx)
{
    return semantic_type_resolution_lookup_or_materialize(ctx, type_ref);
}

Type *
domain_resolve_slot_type(ASTNode *slot, SemanticContext *ctx)
{
    ASTNode *type_ref;
    if (slot == NULL || slot->type != AST_DOMAIN_SLOT)
        return TYPE_UNKNOWN;
    type_ref = slot->data.domain_slot.type;
    return domain_resolve_type_ref(type_ref, ctx);
}

Type *
domain_resolve_shared_type(ASTNode *shared, SemanticContext *ctx)
{
    ASTNode *type_ref;
    if (shared == NULL || shared->type != AST_PARTY_SHARED)
        return TYPE_UNKNOWN;
    type_ref = shared->data.party_shared.type;
    return domain_resolve_type_ref(type_ref, ctx);
}

Type *
domain_resolve_named_type_ref(ASTNode *type_ref, SemanticContext *ctx)
{
    return domain_resolve_type_ref(type_ref, ctx);
}

/* Intent and world declaration bodies are owned by type_checker_intent_decl.c
 * and type_checker_world_decl.c. See docs/101_semantic_split_template.md. */

size_t
count_subject_domain_slots(ASTNode **slots, size_t slot_count)
{
    size_t subject_count = 0;
    for (size_t i = 0; i < slot_count; i++) {
        ASTNode *slot = slots[i];
        if (slot != NULL && slot->type == AST_DOMAIN_SLOT
            && slot->data.domain_slot.is_subject) {
            subject_count++;
        }
    }
    return subject_count;
}

size_t
count_object_domain_slots(ASTNode **slots, size_t slot_count)
{
    size_t object_count = 0;
    for (size_t i = 0; i < slot_count; i++) {
        ASTNode *slot = slots[i];
        if (slot != NULL && slot->type == AST_DOMAIN_SLOT
            && !slot->data.domain_slot.is_subject) {
            object_count++;
        }
    }
    return object_count;
}

size_t
count_bindable_domain_slots(ASTNode **slots, size_t slot_count,
                            ASTNode **refreshes, size_t refresh_count)
{
    size_t bindable_count = 0;
    (void)refreshes;
    (void)refresh_count;

    for (size_t i = 0; i < slot_count; i++) {
        ASTNode *slot = slots[i];
        if (slot == NULL || slot->type != AST_DOMAIN_SLOT
            || !slot->data.domain_slot.is_binding) {
            continue;
        }
        bindable_count++;
    }

    return bindable_count;
}

ASTNode *
find_domain_slot_local(ASTNode **slots, size_t slot_count, const char *slot_name)
{
    if (slots == NULL || slot_name == NULL)
        return NULL;

    for (size_t i = 0; i < slot_count; i++) {
        ASTNode *slot = slots[i];
        if (slot != NULL
            && slot->type == AST_DOMAIN_SLOT
            && slot->data.domain_slot.slot_name != NULL
            && strcmp(slot->data.domain_slot.slot_name, slot_name) == 0) {
            return slot;
        }
    }
    return NULL;
}

ASTNode *
find_zone_domain_slot(ASTNode *zone, const char *slot_name)
{
    if (zone == NULL || slot_name == NULL)
        return NULL;

    return find_domain_slot_local(zone->data.zone_decl.slots,
        zone->data.zone_decl.slot_count, slot_name);
}

ASTNode *
find_domain_decl_by_name(ASTNode *program,
                         ASTNodeType decl_type,
                         const char *name)
{
    if (program == NULL || program->type != AST_PROGRAM || name == NULL)
        return NULL;

    for (size_t i = 0; i < program->data.program.count; i++) {
        ASTNode *stmt = program->data.program.statements[i];
        if (stmt == NULL || stmt->type != decl_type)
            continue;

        const char *decl_name = NULL;
        if (decl_type == AST_RELATION_DECL)
            decl_name = stmt->data.relation_decl.name;
        else if (decl_type == AST_EFFECT_DECL)
            decl_name = stmt->data.effect_decl.name;
        else if (decl_type == AST_ZONE_DECL)
            decl_name = stmt->data.zone_decl.name;
        else if (decl_type == AST_WORLD_DECL)
            decl_name = stmt->data.world_decl.name;
        else if (decl_type == AST_ROSTER_DECL)
            decl_name = stmt->data.roster_decl.name;

        if (decl_name != NULL && strcmp(decl_name, name) == 0)
            return stmt;
    }

    return NULL;
}

ASTNode *
find_zone_effect_slot(ASTNode *zone, const char *slot_name)
{
    if (zone == NULL || slot_name == NULL)
        return NULL;

    for (size_t i = 0; i < zone->data.zone_decl.layer_slot_count; i++) {
        ASTNode *slot = zone->data.zone_decl.layer_slots[i];
        if (slot != NULL
            && slot->type == AST_ZONE_LAYER_SLOT
            && !slot->data.zone_layer_slot.is_relation
            && slot->data.zone_layer_slot.slot_name != NULL
            && strcmp(slot->data.zone_layer_slot.slot_name, slot_name) == 0) {
            return slot;
        }
    }
    return NULL;
}

ASTNode *
find_zone_relation_slot(ASTNode *zone, const char *slot_name)
{
    if (zone == NULL || slot_name == NULL)
        return NULL;

    for (size_t i = 0; i < zone->data.zone_decl.layer_slot_count; i++) {
        ASTNode *slot = zone->data.zone_decl.layer_slots[i];
        if (slot != NULL
            && slot->type == AST_ZONE_LAYER_SLOT
            && slot->data.zone_layer_slot.is_relation
            && slot->data.zone_layer_slot.slot_name != NULL
            && strcmp(slot->data.zone_layer_slot.slot_name, slot_name) == 0) {
            return slot;
        }
    }
    return NULL;
}

ASTNode *
find_zone_authority(ASTNode *zone, const char *slot_name)
{
    if (zone == NULL || slot_name == NULL)
        return NULL;

    for (size_t i = 0; i < zone->data.zone_decl.authority_count; i++) {
        ASTNode *authority = zone->data.zone_decl.authorities[i];
        if (authority != NULL
            && authority->type == AST_ZONE_AUTHORITY
            && authority->data.zone_authority.subject_slot_name != NULL
            && strcmp(authority->data.zone_authority.subject_slot_name, slot_name) == 0) {
            return authority;
        }
    }

    return NULL;
}

static bool
domain_name_segment_equals(const char *name, const char *alias)
{
    size_t name_len;
    size_t alias_len;

    if (name == NULL || alias == NULL)
        return false;
    name_len = strlen(name);
    alias_len = strlen(alias);
    while (name_len > 0 && isspace((unsigned char)name[name_len - 1]))
        name_len--;
    while (alias_len > 0 && isspace((unsigned char)alias[alias_len - 1]))
        alias_len--;
    return name_len == alias_len && strncmp(name, alias, name_len) == 0;
}

static bool
domain_slot_name_matches_alias(const char *slot_name, const char *alias)
{
    const char *tail;

    if (domain_name_segment_equals(slot_name, alias))
        return true;

    tail = strrchr(slot_name, '.');
    if (tail != NULL && domain_name_segment_equals(tail + 1, alias))
        return true;
    tail = strrchr(slot_name, ':');
    if (tail != NULL && domain_name_segment_equals(tail + 1, alias))
        return true;
    tail = strrchr(slot_name, '/');
    return tail != NULL && domain_name_segment_equals(tail + 1, alias);
}

ASTNode *
resolve_zone_subject_slot_for_participant(ASTNode *zone,
                                          SemanticContext *ctx,
                                          const char *participant_alias,
                                          const char *participant_type_name,
                                          bool *ambiguous_out)
{
    ASTNode *typed_match = NULL;

    if (ambiguous_out != NULL)
        *ambiguous_out = false;
    if (zone == NULL || zone->type != AST_ZONE_DECL || ctx == NULL
        || participant_alias == NULL || participant_type_name == NULL) {
        return NULL;
    }

    for (size_t i = 0; i < zone->data.zone_decl.slot_count; i++) {
        ASTNode *slot = zone->data.zone_decl.slots[i];
        Type *slot_type;
        bool direct_name_match;
        bool type_name_match;
        if (slot == NULL || slot->type != AST_DOMAIN_SLOT
            || !slot->data.domain_slot.is_subject
            || slot->data.domain_slot.slot_name == NULL
            || slot->data.domain_slot.type == NULL) {
            continue;
        }
        direct_name_match = domain_slot_name_matches_alias(
            slot->data.domain_slot.slot_name, participant_alias);
        if (direct_name_match) {
            if (ambiguous_out != NULL)
                *ambiguous_out = false;
            return slot;
        }
        slot_type = domain_resolve_slot_type(slot, ctx);
        type_name_match = slot_type != NULL && slot_type->name != NULL
            && strcmp(slot_type->name, participant_type_name) == 0;
        if (!type_name_match) {
            continue;
        }
        if (typed_match != NULL) {
            if (ambiguous_out != NULL)
                *ambiguous_out = true;
        } else {
            typed_match = slot;
        }
    }

    if (ambiguous_out != NULL && *ambiguous_out)
        return NULL;
    return typed_match;
}

ASTNode *
find_zone_state(ASTNode *zone, const char *state_name)
{
    if (zone == NULL || state_name == NULL)
        return NULL;

    for (size_t i = 0; i < zone->data.zone_decl.state_count; i++) {
        ASTNode *state = zone->data.zone_decl.states[i];
        if (state != NULL
            && state->type == AST_ZONE_STATE
            && state->data.zone_state.state_name != NULL
            && strcmp(state->data.zone_state.state_name, state_name) == 0) {
            return state;
        }
    }

    return NULL;
}

bool
resolve_zone_effect_state(ASTNode *zone,
                          ASTNode *site,
                          const char *state_name,
                          SemanticContext *ctx,
                          const char *action_name,
                          const char **effect_slot_name,
                          const char **target_slot_name)
{
    ASTNode *state = find_zone_state(zone, state_name);

    if (effect_slot_name != NULL)
        *effect_slot_name = NULL;
    if (target_slot_name != NULL)
        *target_slot_name = NULL;

    if (state == NULL) {
        semantic_error_with_hints(ctx, PGY_CODE_SEM_ZONE_CONTRACT_INVALID, PGY_CAUSE_ZONE_CONTRACT, PGY_FIX_ALIGN_ZONE_SLOT_OR_STATE_NAMING, site,
            "Zone %s references unknown state '%s'",
            action_name,
            state_name != NULL ? state_name : "<unknown>");
        return false;
    }
    if (state->data.zone_state.is_relation) {
        semantic_error_with_hints(ctx, PGY_CODE_SEM_ZONE_CONTRACT_INVALID, PGY_CAUSE_ZONE_CONTRACT, PGY_FIX_ALIGN_ZONE_SLOT_OR_STATE_NAMING, site,
            "Zone %s state '%s' is a relation state and cannot be used as an effect state",
            action_name,
            state_name != NULL ? state_name : "<unknown>");
        return false;
    }

    if (effect_slot_name != NULL)
        *effect_slot_name = state->data.zone_state.layer_slot_name;
    if (target_slot_name != NULL)
        *target_slot_name = state->data.zone_state.left_or_target_slot_name;
    return true;
}

bool
resolve_zone_relation_state(ASTNode *zone,
                            ASTNode *site,
                            const char *state_name,
                            SemanticContext *ctx,
                            const char *action_name,
                            const char **relation_slot_name,
                            const char **left_slot_name,
                            const char **right_slot_name)
{
    ASTNode *state = find_zone_state(zone, state_name);

    if (relation_slot_name != NULL)
        *relation_slot_name = NULL;
    if (left_slot_name != NULL)
        *left_slot_name = NULL;
    if (right_slot_name != NULL)
        *right_slot_name = NULL;

    if (state == NULL) {
        semantic_error_with_hints(ctx, PGY_CODE_SEM_ZONE_CONTRACT_INVALID, PGY_CAUSE_ZONE_CONTRACT, PGY_FIX_ALIGN_ZONE_SLOT_OR_STATE_NAMING, site,
            "Zone %s references unknown state '%s'",
            action_name,
            state_name != NULL ? state_name : "<unknown>");
        return false;
    }
    if (!state->data.zone_state.is_relation) {
        semantic_error_with_hints(ctx, PGY_CODE_SEM_ZONE_CONTRACT_INVALID, PGY_CAUSE_ZONE_CONTRACT, PGY_FIX_ALIGN_ZONE_SLOT_OR_STATE_NAMING, site,
            "Zone %s state '%s' is an effect state and cannot be used as a relation state",
            action_name,
            state_name != NULL ? state_name : "<unknown>");
        return false;
    }

    if (relation_slot_name != NULL)
        *relation_slot_name = state->data.zone_state.layer_slot_name;
    if (left_slot_name != NULL)
        *left_slot_name = state->data.zone_state.left_or_target_slot_name;
    if (right_slot_name != NULL)
        *right_slot_name = state->data.zone_state.right_slot_name;
    return true;
}

bool
type_check_zone_participant_authority(ASTNode *zone,
                                ASTNode *site,
                                const char *participant_slot_name,
                                SemanticContext *ctx,
                                const char *action_name)
{
    ASTNode *participant_slot;
    ASTNode *authority;

    if (zone == NULL || ctx == NULL || participant_slot_name == NULL)
        return false;

    participant_slot = find_zone_domain_slot(zone, participant_slot_name);
    if (participant_slot == NULL) {
        semantic_error_with_hints(ctx, PGY_CODE_SEM_ZONE_CONTRACT_INVALID, PGY_CAUSE_ZONE_CONTRACT, PGY_FIX_ALIGN_ZONE_SLOT_OR_STATE_NAMING, site,
            "Zone %s references unknown authority subject slot '%s'",
            action_name, participant_slot_name);
        return true;
    }

    if (!participant_slot->data.domain_slot.is_subject) {
        semantic_error_with_hints(ctx, PGY_CODE_SEM_ZONE_CONTRACT_INVALID, PGY_CAUSE_ZONE_CONTRACT, PGY_FIX_ALIGN_ZONE_SLOT_OR_STATE_NAMING, site,
            "Zone %s authority '%s' must be a subject slot",
            action_name, participant_slot_name);
        return true;
    }

    authority = find_zone_authority(zone, participant_slot_name);
    if (zone->data.zone_decl.authority_count > 0 && authority == NULL) {
        semantic_error_with_hints(ctx, PGY_CODE_SEM_ZONE_CONTRACT_INVALID, PGY_CAUSE_ZONE_CONTRACT, PGY_FIX_ALIGN_ZONE_SLOT_OR_STATE_NAMING, site,
            "Zone %s participant '%s' is not declared in zone authority set",
            action_name, participant_slot_name);
        return true;
    }

    return true;
}
