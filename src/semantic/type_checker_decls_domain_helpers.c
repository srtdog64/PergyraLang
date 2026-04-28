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
            || !slot->data.domain_slot.is_binding) {
            continue;
        }

        if (seen == ordinal)
            return slot;
        seen++;
    }

    return NULL;
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

static ASTNode *
find_named_class_decl_local(ASTNode *program, const char *name)
{
    if (program == NULL || program->type != AST_PROGRAM || name == NULL)
        return NULL;

    for (size_t i = 0; i < program->data.program.count; i++) {
        ASTNode *stmt = program->data.program.statements[i];
        if (stmt == NULL || stmt->type != AST_CLASS_DECL
            || stmt->data.class_decl.name == NULL) {
            continue;
        }
        if (strcmp(stmt->data.class_decl.name, name) == 0)
            return stmt;
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
    const char *zone_name;

    (void) action_name;

    effect_slot = find_zone_effect_slot(zone, effect_slot_name);
    target_slot = find_zone_domain_slot(zone, target_slot_name);
    if (zone == NULL || effect_slot == NULL || target_slot == NULL || ctx == NULL)
        return false;

    zone_name = zone->data.zone_decl.name != NULL ? zone->data.zone_decl.name : "<zone>";

    effect_decl = find_domain_decl_by_name(ctx->program_root, AST_EFFECT_DECL,
        effect_slot->data.zone_layer_slot.layer_type);
    if (effect_decl == NULL)
        return false;

    target_count = count_bindable_domain_slots(effect_decl->data.effect_decl.slots,
        effect_decl->data.effect_decl.slot_count,
        effect_decl->data.effect_decl.refreshes,
        effect_decl->data.effect_decl.refresh_count);
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
            effect_decl->data.effect_decl.name != NULL
                ? effect_decl->data.effect_decl.name
                : "<unknown>",
            (unsigned long long) target_count,
            effect_slot_name != NULL ? effect_slot_name : "<effect-slot>",
            effect_decl->data.effect_decl.name != NULL
                ? effect_decl->data.effect_decl.name
                : "<unknown>",
            (unsigned long long) target_count,
            effect_decl->data.effect_decl.name != NULL
                ? effect_decl->data.effect_decl.name
                : "<unknown>");
        return true;
    }

    decl_target = find_nth_bindable_domain_slot(effect_decl->data.effect_decl.slots,
        effect_decl->data.effect_decl.slot_count,
        effect_decl->data.effect_decl.refreshes,
        effect_decl->data.effect_decl.refresh_count, 0);
    if (decl_target == NULL)
        return false;

    target_type = domain_resolve_slot_type(target_slot, ctx);
    decl_target_type = domain_resolve_slot_type(decl_target, ctx);
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
            effect_decl->data.effect_decl.name != NULL
                ? effect_decl->data.effect_decl.name
                : "<unknown>",
            decl_target_type != NULL && decl_target_type->name != NULL
                ? decl_target_type->name
                : "<unknown>",
            effect_slot_name != NULL ? effect_slot_name : "<effect-slot>",
            zone->data.zone_decl.name != NULL ? zone->data.zone_decl.name : "<zone>",
            effect_slot_name != NULL ? effect_slot_name : "<effect-slot>",
            target_slot_name != NULL ? target_slot_name : "<target-slot>",
            target_slot_name != NULL ? target_slot_name : "<unknown>",
            effect_slot_name != NULL ? effect_slot_name : "<effect-slot>",
            effect_slot_name != NULL ? effect_slot_name : "<effect-slot>",
            target_slot_name != NULL ? target_slot_name : "<unknown>",
            effect_decl->data.effect_decl.name != NULL
                ? effect_decl->data.effect_decl.name
                : "<unknown>",
            decl_target->data.domain_slot.slot_name != NULL
                ? decl_target->data.domain_slot.slot_name
                : "<target-slot>",
            decl_target_type != NULL && decl_target_type->name != NULL
                ? decl_target_type->name
                : "<unknown>",
            target_type != NULL && target_type->name != NULL ? target_type->name : "<unknown>",
            effect_decl->data.effect_decl.name != NULL
                ? effect_decl->data.effect_decl.name
                : "<unknown>",
            decl_target_type != NULL && decl_target_type->name != NULL
                ? decl_target_type->name
                : "<unknown>",
            effect_decl->data.effect_decl.name != NULL
                ? effect_decl->data.effect_decl.name
                : "<unknown>",
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
        return slot->data.domain_slot.is_subject;

    if (kind == RELATION_ENDPOINT_OBJECT) {
        if (slot->data.domain_slot.is_subject)
            return false;
        if (slot->data.domain_slot.is_tobject)
            return false;  /* tobject slot uses "tobject" kind, not "object" */
        decl = find_named_class_decl_local(ctx->program_root,
            slot_type != NULL ? slot_type->name : NULL);
        return decl != NULL
            && decl->data.class_decl.is_struct
            && decl->data.class_decl.nominal_kind == NOMINAL_DECL_OBJECT;
    }

    if (kind == RELATION_ENDPOINT_TOBJECT) {
        if (slot->data.domain_slot.is_subject)
            return false;
        if (!slot->data.domain_slot.is_tobject)
            return false;
        decl = find_named_class_decl_local(ctx->program_root,
            slot_type != NULL ? slot_type->name : NULL);
        return decl != NULL
            && decl->data.class_decl.is_struct
            && decl->data.class_decl.nominal_kind == NOMINAL_DECL_TOBJECT;
    }

    if (kind == RELATION_ENDPOINT_CLASS) {
        if (slot->data.domain_slot.is_subject || slot->data.domain_slot.is_tobject)
            return false;
        decl = find_named_class_decl_local(ctx->program_root,
            slot_type != NULL ? slot_type->name : NULL);
        if (decl == NULL)
            return true;
        return decl->data.class_decl.nominal_kind != NOMINAL_DECL_OBJECT
            && decl->data.class_decl.nominal_kind != NOMINAL_DECL_TOBJECT;
    }

    named_type = domain_resolve_named_type_ref(named_type_ref, ctx);
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
    RelationEndpointKind between_left_kind;
    RelationEndpointKind between_right_kind;
    ASTNode *between_left_type;
    ASTNode *between_right_type;
    const char *zone_name;

    (void) action_name;

    relation_slot = find_zone_relation_slot(zone, relation_slot_name);
    left_slot = find_zone_domain_slot(zone, left_slot_name);
    right_slot = find_zone_domain_slot(zone, right_slot_name);
    if (zone == NULL || relation_slot == NULL || left_slot == NULL || right_slot == NULL
        || ctx == NULL)
        return false;

    zone_name = zone->data.zone_decl.name != NULL ? zone->data.zone_decl.name : "<zone>";

    relation_decl = find_domain_decl_by_name(ctx->program_root, AST_RELATION_DECL,
        relation_slot->data.zone_layer_slot.layer_type);
    if (relation_decl == NULL)
        return false;

    between_left_kind = relation_decl->data.relation_decl.between_left_kind;
    between_right_kind = relation_decl->data.relation_decl.between_right_kind;
    between_left_type = relation_decl->data.relation_decl.between_left_type;
    between_right_type = relation_decl->data.relation_decl.between_right_type;
    left_type = domain_resolve_slot_type(left_slot, ctx);
    right_type = domain_resolve_slot_type(right_slot, ctx);

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
                relation_decl->data.relation_decl.name != NULL
                    ? relation_decl->data.relation_decl.name
                    : "<unknown>",
                relation_slot_name != NULL ? relation_slot_name : "<relation-slot>",
                relation_decl->data.relation_decl.name != NULL
                    ? relation_decl->data.relation_decl.name
                    : "<unknown>",
                relation_decl->data.relation_decl.name != NULL
                    ? relation_decl->data.relation_decl.name
                    : "<unknown>",
                relation_endpoint_kind_label(between_left_kind),
                left_slot_name != NULL ? left_slot_name : "<unknown>",
                left_type != NULL && left_type->name != NULL ? left_type->name : "<unknown>",
                relation_decl->data.relation_decl.name != NULL
                    ? relation_decl->data.relation_decl.name
                    : "<unknown>",
                relation_decl->data.relation_decl.name != NULL
                    ? relation_decl->data.relation_decl.name
                    : "<unknown>",
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
                relation_decl->data.relation_decl.name != NULL
                    ? relation_decl->data.relation_decl.name
                    : "<unknown>",
                relation_slot_name != NULL ? relation_slot_name : "<relation-slot>",
                relation_decl->data.relation_decl.name != NULL
                    ? relation_decl->data.relation_decl.name
                    : "<unknown>",
                relation_decl->data.relation_decl.name != NULL
                    ? relation_decl->data.relation_decl.name
                    : "<unknown>",
                relation_endpoint_kind_label(between_right_kind),
                right_slot_name != NULL ? right_slot_name : "<unknown>",
                right_type != NULL && right_type->name != NULL ? right_type->name : "<unknown>",
                relation_decl->data.relation_decl.name != NULL
                    ? relation_decl->data.relation_decl.name
                    : "<unknown>",
                relation_decl->data.relation_decl.name != NULL
                    ? relation_decl->data.relation_decl.name
                    : "<unknown>",
                right_type != NULL && right_type->name != NULL ? right_type->name : "<unknown>");
        }
        return true;
    }

    endpoint_count = count_bindable_domain_slots(relation_decl->data.relation_decl.slots,
        relation_decl->data.relation_decl.slot_count,
        relation_decl->data.relation_decl.refreshes,
        relation_decl->data.relation_decl.refresh_count);
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
            relation_decl->data.relation_decl.name != NULL
                ? relation_decl->data.relation_decl.name
                : "<unknown>",
            (unsigned long long) endpoint_count,
            relation_slot_name != NULL ? relation_slot_name : "<relation-slot>",
            relation_decl->data.relation_decl.name != NULL
                ? relation_decl->data.relation_decl.name
                : "<unknown>",
            (unsigned long long) endpoint_count,
            relation_decl->data.relation_decl.name != NULL
                ? relation_decl->data.relation_decl.name
                : "<unknown>");
        return true;
    }

    decl_left = find_nth_bindable_domain_slot(relation_decl->data.relation_decl.slots,
        relation_decl->data.relation_decl.slot_count,
        relation_decl->data.relation_decl.refreshes,
        relation_decl->data.relation_decl.refresh_count, 0);
    decl_right = find_nth_bindable_domain_slot(relation_decl->data.relation_decl.slots,
        relation_decl->data.relation_decl.slot_count,
        relation_decl->data.relation_decl.refreshes,
        relation_decl->data.relation_decl.refresh_count, 1);
    if (decl_left == NULL || decl_right == NULL)
        return false;

    decl_left_type = domain_resolve_slot_type(decl_left, ctx);
    decl_right_type = domain_resolve_slot_type(decl_right, ctx);

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
            relation_decl->data.relation_decl.name != NULL
                ? relation_decl->data.relation_decl.name
                : "<unknown>",
            decl_left_type != NULL && decl_left_type->name != NULL
                ? decl_left_type->name
                : "<unknown>",
            relation_slot_name != NULL ? relation_slot_name : "<relation-slot>",
            zone->data.zone_decl.name != NULL ? zone->data.zone_decl.name : "<zone>",
            left_slot_name != NULL ? left_slot_name : "<left-slot>",
            relation_decl->data.relation_decl.name != NULL
                ? relation_decl->data.relation_decl.name
                : "<unknown>",
            relation_slot_name != NULL ? relation_slot_name : "<relation-slot>",
            relation_decl->data.relation_decl.name != NULL
                ? relation_decl->data.relation_decl.name
                : "<unknown>",
            relation_decl->data.relation_decl.name != NULL
                ? relation_decl->data.relation_decl.name
                : "<unknown>",
            decl_left->data.domain_slot.slot_name != NULL
                ? decl_left->data.domain_slot.slot_name
                : "<left-slot>",
            decl_left_type != NULL && decl_left_type->name != NULL
                ? decl_left_type->name
                : "<unknown>",
            left_type != NULL && left_type->name != NULL ? left_type->name : "<unknown>",
            relation_decl->data.relation_decl.name != NULL
                ? relation_decl->data.relation_decl.name
                : "<unknown>",
            decl_left_type != NULL && decl_left_type->name != NULL
                ? decl_left_type->name
                : "<unknown>",
            relation_decl->data.relation_decl.name != NULL
                ? relation_decl->data.relation_decl.name
                : "<unknown>",
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
            relation_decl->data.relation_decl.name != NULL
                ? relation_decl->data.relation_decl.name
                : "<unknown>",
            decl_right_type != NULL && decl_right_type->name != NULL
                ? decl_right_type->name
                : "<unknown>",
            relation_slot_name != NULL ? relation_slot_name : "<relation-slot>",
            zone->data.zone_decl.name != NULL ? zone->data.zone_decl.name : "<zone>",
            relation_decl->data.relation_decl.name != NULL
                ? relation_decl->data.relation_decl.name
                : "<unknown>",
            right_slot_name != NULL ? right_slot_name : "<right-slot>",
            relation_slot_name != NULL ? relation_slot_name : "<relation-slot>",
            relation_decl->data.relation_decl.name != NULL
                ? relation_decl->data.relation_decl.name
                : "<unknown>",
            relation_slot_name != NULL ? relation_slot_name : "<relation-slot>",
            right_slot_name != NULL ? right_slot_name : "<unknown>",
            relation_decl->data.relation_decl.name != NULL
                ? relation_decl->data.relation_decl.name
                : "<unknown>",
            decl_right->data.domain_slot.slot_name != NULL
                ? decl_right->data.domain_slot.slot_name
                : "<right-slot>",
            decl_right_type != NULL && decl_right_type->name != NULL
                ? decl_right_type->name
                : "<unknown>",
            right_type != NULL && right_type->name != NULL ? right_type->name : "<unknown>",
            relation_decl->data.relation_decl.name != NULL
                ? relation_decl->data.relation_decl.name
                : "<unknown>",
            decl_right_type != NULL && decl_right_type->name != NULL
                ? decl_right_type->name
                : "<unknown>",
            relation_decl->data.relation_decl.name != NULL
                ? relation_decl->data.relation_decl.name
                : "<unknown>",
            right_type != NULL && right_type->name != NULL ? right_type->name : "<unknown>");
    }

    return true;
}
