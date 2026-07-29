#include "type_checker_internal.h"
#include "domain_projection_path_fact_internal.h"
#include "diag_codes.h"
#include "parser/ast_api.h"
#include "../common/string_compat.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

static void
domain_participant_role_fact_clear(PgyDomainParticipantRoleFact *fact)
{
    if (fact == NULL)
        return;
    free(fact->owner_name);
    free(fact->field_name);
    free(fact->field_type_name);
    memset(fact, 0, sizeof(*fact));
}

static void
domain_projection_member_assignment_fact_clear(
    PgyDomainProjectionMemberAssignmentFact *fact)
{
    if (fact == NULL)
        return;
    free(fact->owner_name);
    free(fact->projection_slot_name);
    free(fact->source_slot_name);
    free(fact->target_field_name);
    free(fact->target_field_type_name);
    free(fact->source_path);
    free(fact->source_leaf_type_name);
    pgy_domain_projection_path_segments_destroy(
        fact->source_path_segments,
        fact->source_path_segment_count);
    memset(fact, 0, sizeof(*fact));
}

void
pgy_domain_participant_role_facts_destroy(
    PgyDomainParticipantRoleFact *facts,
    size_t fact_count)
{
    for (size_t i = 0; facts != NULL && i < fact_count; i++)
        domain_participant_role_fact_clear(&facts[i]);
    free(facts);
}

void
pgy_domain_projection_member_assignment_facts_destroy(
    PgyDomainProjectionMemberAssignmentFact *facts,
    size_t fact_count)
{
    for (size_t i = 0; facts != NULL && i < fact_count; i++)
        domain_projection_member_assignment_fact_clear(&facts[i]);
    free(facts);
}

static const char *
domain_runtime_owner_name(ASTNode *owner_decl)
{
    if (owner_decl == NULL)
        return NULL;
    switch (owner_decl->type) {
    case AST_EFFECT_DECL:
        return ast_effect_name(owner_decl);
    case AST_RELATION_DECL:
        return ast_relation_name(owner_decl);
    case AST_ZONE_DECL:
        return ast_zone_name(owner_decl);
    default:
        return NULL;
    }
}

static const char *
domain_participant_role_label(PgyDomainParticipantRole role)
{
    switch (role) {
    case PGY_DOMAIN_PARTICIPANT_EFFECT_BEARER:
        return "effect bearer";
    case PGY_DOMAIN_PARTICIPANT_RELATION_SOURCE:
        return "relation source";
    case PGY_DOMAIN_PARTICIPANT_RELATION_TARGET:
        return "relation target";
    default:
        return "domain participant";
    }
}

static bool
domain_participant_role_facts_reserve(SemanticContext *ctx, size_t needed)
{
    size_t capacity;
    PgyDomainParticipantRoleFact *grown;

    if (ctx == NULL)
        return false;
    if (needed <= ctx->domain_participant_role_fact_capacity)
        return true;
    if (needed > SIZE_MAX / sizeof(*grown))
        return false;

    capacity = ctx->domain_participant_role_fact_capacity != 0
        ? ctx->domain_participant_role_fact_capacity
        : 8;
    while (capacity < needed) {
        if (capacity > SIZE_MAX / 2) {
            capacity = needed;
            break;
        }
        capacity *= 2;
    }
    if (capacity > SIZE_MAX / sizeof(*grown))
        return false;
    grown = realloc(ctx->domain_participant_role_facts,
                    capacity * sizeof(*grown));
    if (grown == NULL)
        return false;
    ctx->domain_participant_role_facts = grown;
    ctx->domain_participant_role_fact_capacity = capacity;
    return true;
}

static bool
domain_projection_member_assignment_facts_reserve(
    SemanticContext *ctx,
    size_t needed)
{
    size_t capacity;
    PgyDomainProjectionMemberAssignmentFact *grown;

    if (ctx == NULL)
        return false;
    if (needed <= ctx->domain_projection_member_assignment_fact_capacity)
        return true;
    if (needed > SIZE_MAX / sizeof(*grown))
        return false;

    capacity = ctx->domain_projection_member_assignment_fact_capacity != 0
        ? ctx->domain_projection_member_assignment_fact_capacity
        : 8;
    while (capacity < needed) {
        if (capacity > SIZE_MAX / 2) {
            capacity = needed;
            break;
        }
        capacity *= 2;
    }
    if (capacity > SIZE_MAX / sizeof(*grown))
        return false;
    grown = realloc(ctx->domain_projection_member_assignment_facts,
                    capacity * sizeof(*grown));
    if (grown == NULL)
        return false;
    ctx->domain_projection_member_assignment_facts = grown;
    ctx->domain_projection_member_assignment_fact_capacity = capacity;
    return true;
}

static bool
domain_participant_role_identity_exists(
    const SemanticContext *ctx,
    uint32_t program_syntax_id,
    uint32_t owner_syntax_id,
    PgyDomainParticipantRole role)
{
    if (ctx == NULL)
        return false;
    for (size_t i = 0; i < ctx->domain_participant_role_fact_count; i++) {
        const PgyDomainParticipantRoleFact *fact =
            &ctx->domain_participant_role_facts[i];
        if (fact->program_syntax_id == program_syntax_id
            && fact->owner_syntax_id == owner_syntax_id
            && fact->role == role) {
            return true;
        }
    }
    return false;
}

static bool
domain_projection_member_assignment_identity_exists(
    const SemanticContext *ctx,
    uint32_t program_syntax_id,
    uint32_t owner_syntax_id,
    uint32_t directive_syntax_id,
    uint32_t projection_slot_syntax_id,
    uint32_t target_field_syntax_id)
{
    if (ctx == NULL)
        return false;
    for (size_t i = 0;
         i < ctx->domain_projection_member_assignment_fact_count;
         i++) {
        const PgyDomainProjectionMemberAssignmentFact *fact =
            &ctx->domain_projection_member_assignment_facts[i];
        if (fact->program_syntax_id == program_syntax_id
            && fact->owner_syntax_id == owner_syntax_id
            && fact->directive_syntax_id == directive_syntax_id
            && fact->projection_slot_syntax_id == projection_slot_syntax_id
            && fact->target_field_syntax_id == target_field_syntax_id) {
            return true;
        }
    }
    return false;
}

static bool
domain_record_participant_roles(SemanticContext *ctx,
                                ASTNode *owner_decl,
                                ASTNode **slots,
                                size_t slot_count,
                                const PgyDomainParticipantRole *roles,
                                size_t role_count)
{
    ASTNode **bindable_slots = NULL;
    PgyDomainParticipantRoleFact *pending = NULL;
    const char *owner_name = domain_runtime_owner_name(owner_decl);
    uint32_t program_syntax_id;
    uint32_t owner_syntax_id;
    size_t bindable_count = 0;
    bool ok = false;

    if (ctx == NULL || owner_decl == NULL || roles == NULL || role_count == 0)
        return false;

    program_syntax_id = semantic_program_syntax_id(ctx);
    owner_syntax_id = ast_node_stable_id(owner_decl);
    bindable_slots = calloc(role_count, sizeof(*bindable_slots));
    pending = calloc(role_count, sizeof(*pending));
    if (bindable_slots == NULL || pending == NULL)
        goto oom;

    for (size_t i = 0; i < slot_count; i++) {
        ASTNode *slot = slots != NULL ? slots[i] : NULL;
        if (slot == NULL || slot->type != AST_DOMAIN_SLOT
            || !ast_domain_slot_is_binding(slot)) {
            continue;
        }
        if (bindable_count >= role_count) {
            semantic_error(ctx, owner_decl,
                "Semantic participant role prepass received a non-exact bindable slot cardinality");
            goto cleanup;
        }
        bindable_slots[bindable_count++] = slot;
    }
    if (bindable_count != role_count) {
        semantic_error(ctx, owner_decl,
            "Semantic participant role prepass received a non-exact bindable slot cardinality");
        goto cleanup;
    }
    if (program_syntax_id == 0 || owner_syntax_id == 0 || owner_name == NULL) {
        semantic_error(ctx, owner_decl,
            "Semantic participant role fact is missing stable program/owner identity");
        goto cleanup;
    }

    for (size_t i = 0; i < role_count; i++) {
        ASTNode *slot = bindable_slots[i];
        Type *slot_type = domain_lookup_slot_type_metadata(slot, ctx);
        const char *slot_name = ast_domain_slot_name(slot);
        const char *slot_type_name = type_name_or_unknown(slot_type);
        uint32_t field_syntax_id = ast_node_stable_id(slot);

        if (field_syntax_id == 0 || slot_name == NULL
            || slot_type == NULL || slot_type == TYPE_UNKNOWN
            || slot_type_name == NULL) {
            semantic_error(ctx, owner_decl,
                "Semantic %s fact is missing exact field identity or type",
                domain_participant_role_label(roles[i]));
            goto cleanup;
        }
        if (domain_participant_role_identity_exists(ctx,
                program_syntax_id, owner_syntax_id, roles[i])) {
            semantic_error(ctx, owner_decl,
                "Duplicate semantic %s identity for '%s'",
                domain_participant_role_label(roles[i]), owner_name);
            goto cleanup;
        }

        pending[i].program_syntax_id = program_syntax_id;
        pending[i].owner_syntax_id = owner_syntax_id;
        pending[i].role = roles[i];
        pending[i].field_syntax_id = field_syntax_id;
        pending[i].owner_name = pergyra_strdup(owner_name);
        pending[i].field_name = pergyra_strdup(slot_name);
        pending[i].field_type_name = pergyra_strdup(slot_type_name);
        if (pending[i].owner_name == NULL || pending[i].field_name == NULL
            || pending[i].field_type_name == NULL) {
            goto oom;
        }
    }

    if (role_count > SIZE_MAX - ctx->domain_participant_role_fact_count
        || !domain_participant_role_facts_reserve(ctx,
            ctx->domain_participant_role_fact_count + role_count)) {
        goto oom;
    }
    memcpy(&ctx->domain_participant_role_facts[
               ctx->domain_participant_role_fact_count],
           pending,
           role_count * sizeof(*pending));
    ctx->domain_participant_role_fact_count += role_count;
    memset(pending, 0, role_count * sizeof(*pending));
    ok = true;
    goto cleanup;

oom:
    semantic_error(ctx, owner_decl,
        "Semantic participant role fact allocation failed closed");
cleanup:
    pgy_domain_participant_role_facts_destroy(pending, role_count);
    free(bindable_slots);
    return ok;
}

static bool
domain_collect_participant_roles_from_node(ASTNode *node,
                                           SemanticContext *ctx)
{
    ASTNode **slots;
    size_t slot_count;
    size_t bindable_count;

    if (node == NULL || ctx == NULL)
        return true;
    if (node->type == AST_EFFECT_DECL) {
        static const PgyDomainParticipantRole roles[] = {
            PGY_DOMAIN_PARTICIPANT_EFFECT_BEARER
        };
        slots = ast_effect_slots(node, &slot_count);
        bindable_count = count_bindable_domain_slots(
            slots, slot_count, NULL, 0);
        if (bindable_count == 1)
            return domain_record_participant_roles(
                ctx, node, slots, slot_count, roles, 1);
        return true;
    }
    if (node->type == AST_RELATION_DECL) {
        static const PgyDomainParticipantRole roles[] = {
            PGY_DOMAIN_PARTICIPANT_RELATION_SOURCE,
            PGY_DOMAIN_PARTICIPANT_RELATION_TARGET
        };
        slots = ast_relation_slots(node, &slot_count);
        bindable_count = count_bindable_domain_slots(
            slots, slot_count, NULL, 0);
        if (bindable_count == 2)
            return domain_record_participant_roles(
                ctx, node, slots, slot_count, roles, 2);
        return true;
    }
    if (node->type == AST_NAMESPACE_DECL) {
        bool ok = true;
        for (size_t i = 0; i < ast_namespace_statement_count(node); i++) {
            ok = domain_collect_participant_roles_from_node(
                ast_namespace_statement(node, i), ctx) && ok;
        }
        return ok;
    }
    return true;
}

bool
semantic_collect_domain_participant_role_facts(ASTNode *program,
                                               SemanticContext *ctx)
{
    bool ok = true;

    if (program == NULL || program->type != AST_PROGRAM || ctx == NULL)
        return false;
    for (size_t i = 0; i < ast_program_statement_count(program); i++) {
        ok = domain_collect_participant_roles_from_node(
            ast_program_statement(program, i), ctx) && ok;
    }
    return ok;
}

ASTNode *
semantic_domain_participant_role_slot(SemanticContext *ctx,
                                      ASTNode *owner_decl,
                                      PgyDomainParticipantRole role,
                                      ASTNode *site)
{
    const PgyDomainParticipantRoleFact *matched = NULL;
    ASTNode **slots = NULL;
    size_t slot_count = 0;
    uint32_t program_syntax_id;
    uint32_t owner_syntax_id;

    if (ctx == NULL || owner_decl == NULL)
        return NULL;
    program_syntax_id = semantic_program_syntax_id(ctx);
    owner_syntax_id = ast_node_stable_id(owner_decl);
    for (size_t i = 0; i < ctx->domain_participant_role_fact_count; i++) {
        const PgyDomainParticipantRoleFact *fact =
            &ctx->domain_participant_role_facts[i];
        if (fact->program_syntax_id == program_syntax_id
            && fact->owner_syntax_id == owner_syntax_id
            && fact->role == role) {
            if (matched != NULL) {
                semantic_error(ctx, site,
                    "Duplicate semantic %s fact reached its exact consumer",
                    domain_participant_role_label(role));
                return NULL;
            }
            matched = fact;
        }
    }
    if (matched == NULL) {
        semantic_error(ctx, site,
            "Missing semantic %s fact for domain contract",
            domain_participant_role_label(role));
        return NULL;
    }

    if (owner_decl->type == AST_EFFECT_DECL)
        slots = ast_effect_slots(owner_decl, &slot_count);
    else if (owner_decl->type == AST_RELATION_DECL)
        slots = ast_relation_slots(owner_decl, &slot_count);
    else {
        semantic_error(ctx, site,
            "Semantic participant role fact has an invalid owner declaration");
        return NULL;
    }

    for (size_t i = 0; i < slot_count; i++) {
        ASTNode *slot = slots != NULL ? slots[i] : NULL;
        Type *slot_type;
        const char *slot_name;
        const char *slot_type_name;

        if (slot == NULL
            || ast_node_stable_id(slot) != matched->field_syntax_id) {
            continue;
        }
        slot_type = domain_lookup_slot_type_metadata(slot, ctx);
        slot_name = ast_domain_slot_name(slot);
        slot_type_name = type_name_or_unknown(slot_type);
        if (slot_name == NULL || slot_type == NULL || slot_type == TYPE_UNKNOWN
            || slot_type_name == NULL || matched->field_name == NULL
            || matched->field_type_name == NULL
            || strcmp(slot_name, matched->field_name) != 0
            || strcmp(slot_type_name, matched->field_type_name) != 0) {
            semantic_error(ctx, site,
                "Semantic %s fact failed exact field identity/type join",
                domain_participant_role_label(role));
            return NULL;
        }
        return slot;
    }

    semantic_error(ctx, site,
        "Semantic %s fact does not join an owner field stable identity",
        domain_participant_role_label(role));
    return NULL;
}

bool
semantic_record_domain_projection_member_assignment(
    SemanticContext *ctx,
    ASTNode *owner_decl,
    ASTNode *directive,
    ASTNode *projection_slot,
    ASTNode *source_slot,
    ASTNode *target_decl,
    uint32_t target_field_syntax_id,
    const char *target_field_name,
    Type *target_field_type,
    ASTNode *source_decl,
    bool explicit_map,
    const char *source_path,
    Type *source_leaf_type,
    PgyDomainProjectionPathSegmentFact *source_path_segments,
    size_t source_path_segment_count)
{
    PgyDomainProjectionMemberAssignmentFact pending = {0};
    const char *owner_name = domain_runtime_owner_name(owner_decl);
    const char *projection_slot_name = ast_domain_slot_name(projection_slot);
    const char *source_slot_name = ast_domain_slot_name(source_slot);
    const char *target_type_name = type_name_or_unknown(target_field_type);
    const char *source_leaf_type_name = type_name_or_unknown(source_leaf_type);
    uint32_t program_syntax_id = ctx != NULL
        ? semantic_program_syntax_id(ctx)
        : 0;
    uint32_t owner_syntax_id = ast_node_stable_id(owner_decl);
    uint32_t directive_syntax_id = ast_node_stable_id(directive);
    uint32_t projection_slot_syntax_id = ast_node_stable_id(projection_slot);
    uint32_t source_slot_syntax_id = ast_node_stable_id(source_slot);
    uint32_t target_decl_syntax_id = ast_node_stable_id(target_decl);
    uint32_t source_decl_syntax_id = ast_node_stable_id(source_decl);
    bool ok = false;

    if (ctx == NULL) {
        pgy_domain_projection_path_segments_destroy(
            source_path_segments, source_path_segment_count);
        return false;
    }
    if (directive == NULL || directive->type != AST_ZONE_REFRESH
        || program_syntax_id == 0 || owner_syntax_id == 0
        || directive_syntax_id == 0 || projection_slot_syntax_id == 0
        || source_slot_syntax_id == 0 || target_decl_syntax_id == 0
        || target_field_syntax_id == 0 || source_decl_syntax_id == 0
        || owner_name == NULL || projection_slot_name == NULL
        || source_slot_name == NULL || target_field_name == NULL
        || target_field_type == NULL || target_field_type == TYPE_UNKNOWN
        || source_leaf_type == NULL || source_leaf_type == TYPE_UNKNOWN
        || target_type_name == NULL || source_leaf_type_name == NULL
        || source_path == NULL || source_path_segment_count == 0
        || source_path_segments == NULL) {
        semantic_error(ctx, directive,
            "Semantic projection member assignment is missing exact identity, path, or type");
        goto cleanup;
    }
    for (size_t i = 0; i < source_path_segment_count; i++) {
        if (source_path_segments[i].field_syntax_id == 0
            || source_path_segments[i].field_name == NULL
            || source_path_segments[i].field_type_name == NULL) {
            semantic_error(ctx, directive,
                "Semantic projection member assignment path segment is incomplete");
            goto cleanup;
        }
    }
    if (strcmp(source_path_segments[source_path_segment_count - 1]
                   .field_type_name,
               source_leaf_type_name) != 0) {
        semantic_error(ctx, directive,
            "Semantic projection member assignment leaf type disagrees with its exact path");
        goto cleanup;
    }
    if (domain_projection_member_assignment_identity_exists(ctx,
            program_syntax_id, owner_syntax_id, directive_syntax_id,
            projection_slot_syntax_id, target_field_syntax_id)) {
        semantic_error(ctx, directive,
            "Duplicate semantic projection member assignment identity for '%s.%s'",
            projection_slot_name, target_field_name);
        goto cleanup;
    }

    pending.program_syntax_id = program_syntax_id;
    pending.owner_syntax_id = owner_syntax_id;
    pending.directive_syntax_id = directive_syntax_id;
    pending.operation = domain_projection_operation(directive);
    pending.projection_slot_syntax_id = projection_slot_syntax_id;
    pending.source_slot_syntax_id = source_slot_syntax_id;
    pending.target_decl_syntax_id = target_decl_syntax_id;
    pending.target_field_syntax_id = target_field_syntax_id;
    pending.source_decl_syntax_id = source_decl_syntax_id;
    pending.explicit_map = explicit_map;
    pending.owner_name = pergyra_strdup(owner_name);
    pending.projection_slot_name = pergyra_strdup(projection_slot_name);
    pending.source_slot_name = pergyra_strdup(source_slot_name);
    pending.target_field_name = pergyra_strdup(target_field_name);
    pending.target_field_type_name = pergyra_strdup(target_type_name);
    pending.source_path = pergyra_strdup(source_path);
    pending.source_leaf_type_name = pergyra_strdup(source_leaf_type_name);
    pending.source_path_segments = source_path_segments;
    pending.source_path_segment_count = source_path_segment_count;
    source_path_segments = NULL;
    source_path_segment_count = 0;

    if (pending.owner_name == NULL || pending.projection_slot_name == NULL
        || pending.source_slot_name == NULL || pending.target_field_name == NULL
        || pending.target_field_type_name == NULL || pending.source_path == NULL
        || pending.source_leaf_type_name == NULL) {
        semantic_error(ctx, directive,
            "Semantic projection member assignment allocation failed closed");
        goto cleanup;
    }
    if (ctx->domain_projection_member_assignment_fact_count == SIZE_MAX
        || !domain_projection_member_assignment_facts_reserve(ctx,
            ctx->domain_projection_member_assignment_fact_count + 1)) {
        semantic_error(ctx, directive,
            "Semantic projection member assignment allocation failed closed");
        goto cleanup;
    }
    ctx->domain_projection_member_assignment_facts[
        ctx->domain_projection_member_assignment_fact_count++] = pending;
    memset(&pending, 0, sizeof(pending));
    ok = true;

cleanup:
    domain_projection_member_assignment_fact_clear(&pending);
    pgy_domain_projection_path_segments_destroy(
        source_path_segments, source_path_segment_count);
    return ok;
}
