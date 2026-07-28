/* LLVM last-consumer lookup for MIR-owned domain runtime assignments. */

#ifdef PGY_LLVM_ENABLED

#include "llvm_domain_runtime_facts.h"

#include <string.h>

#include "llvm_internal_api.h"
#include "llvm_inventory_decl_lookup.h"
#include "../compiler/mir_decl_headers.h"
#include "../parser/ast_api.h"

static bool
llvm_domain_runtime_text_equal(const char *left, const char *right)
{
    return left != NULL && right != NULL && strcmp(left, right) == 0;
}

static uint32_t
llvm_domain_runtime_program_epoch(const MIRProgram *mir)
{
    if (mir == NULL)
        return 0;
    if (mir->domain_participant_role_fact_count > 0
        && mir->domain_participant_role_facts != NULL) {
        return mir->domain_participant_role_facts[0].program_syntax_id;
    }
    if (mir->domain_projection_member_assignment_fact_count > 0
        && mir->domain_projection_member_assignment_facts != NULL) {
        return mir->domain_projection_member_assignment_facts[0]
            .program_syntax_id;
    }
    return 0;
}

static bool
llvm_domain_runtime_require_decl_identity(LLVMGenCtx *ctx,
                                          const char *decl_name,
                                          uint32_t decl_syntax_id,
                                          const char *consumer_label)
{
    const MIRDeclHeader *header;
    ASTNode *decl;

    if (ctx == NULL || ctx->mir == NULL || decl_name == NULL
        || decl_syntax_id == 0)
        return false;
    header = mir_find_decl_header(ctx->mir, decl_name);
    decl = header != NULL
        ? llvm_find_decl_in_active_inventory(ctx, header->ast_type, decl_name)
        : NULL;
    if (decl == NULL || ast_node_stable_id(decl) != decl_syntax_id) {
        llvm_set_mir_inventory_missing(ctx,
            "LLVM domain runtime %s declaration id %u is foreign to '%s'",
            consumer_label != NULL ? consumer_label : "consumer",
            (unsigned)decl_syntax_id, decl_name);
        return false;
    }
    return true;
}

const MIRDeclField *
llvm_domain_runtime_require_exact_field(LLVMGenCtx *ctx,
                                        const char *owner_name,
                                        uint32_t field_syntax_id,
                                        const char *field_name,
                                        const char *field_type_name,
                                        const char *consumer_label)
{
    const MIRDeclHeader *header;

    if (ctx == NULL || ctx->mir == NULL || owner_name == NULL
        || field_syntax_id == 0 || field_name == NULL) {
        if (ctx != NULL) {
            llvm_set_mir_inventory_missing(ctx,
                "LLVM domain runtime %s field identity is incomplete",
                consumer_label != NULL ? consumer_label : "consumer");
        }
        return NULL;
    }
    header = mir_find_decl_header(ctx->mir, owner_name);
    if (header == NULL) {
        llvm_set_mir_inventory_missing(ctx,
            "LLVM domain runtime %s owner '%s' is foreign to MIR",
            consumer_label != NULL ? consumer_label : "consumer",
            owner_name);
        return NULL;
    }
    for (size_t i = 0; i < mir_decl_header_field_count(header); i++) {
        const MIRDeclField *field = mir_decl_header_field(header, i);
        const char *candidate_name = mir_decl_field_name(field);
        const char *candidate_type = mir_decl_field_type_name(field);

        if (mir_decl_field_source_syntax_id(field) != field_syntax_id)
            continue;
        if (!llvm_domain_runtime_text_equal(candidate_name, field_name)
            || (field_type_name != NULL
                && !llvm_domain_runtime_text_equal(candidate_type,
                                                   field_type_name))) {
            llvm_set_mir_inventory_missing(ctx,
                "LLVM domain runtime %s field identity disagrees with MIR for '%s.%s'",
                consumer_label != NULL ? consumer_label : "consumer",
                owner_name, field_name);
            return NULL;
        }
        return field;
    }
    llvm_set_mir_inventory_missing(ctx,
        "LLVM domain runtime %s field id %u is foreign to '%s.%s'",
        consumer_label != NULL ? consumer_label : "consumer",
        (unsigned)field_syntax_id, owner_name, field_name);
    return NULL;
}

static MIRDomainTopologyKind
llvm_domain_runtime_topology_kind(PgyDomainProjectionOperation operation)
{
    switch (operation) {
    case PGY_DOMAIN_PROJECTION_REFRESH:
        return MIR_DOMAIN_TOPOLOGY_PROJECTION_REFRESH;
    case PGY_DOMAIN_PROJECTION_PUBLISH:
        return MIR_DOMAIN_TOPOLOGY_PROJECTION_PUBLISH;
    case PGY_DOMAIN_PROJECTION_BIND:
        return MIR_DOMAIN_TOPOLOGY_PROJECTION_BIND;
    default:
        return (MIRDomainTopologyKind)-1;
    }
}

static bool
llvm_domain_runtime_projection_topology_matches(
    LLVMGenCtx *ctx,
    const PgyDomainProjectionMemberAssignmentFact *fact)
{
    MIRDomainTopologyKind expected_kind;

    if (ctx == NULL || ctx->mir == NULL || fact == NULL)
        return false;
    expected_kind = llvm_domain_runtime_topology_kind(fact->operation);
    for (size_t i = 0; i < ctx->mir->domain_topology_row_count; i++) {
        const MIRDomainTopologyRow *row = &ctx->mir->domain_topology_rows[i];

        if (row->source_syntax_id != fact->directive_syntax_id)
            continue;
        if (row->owner_source_syntax_id != fact->owner_syntax_id
            || row->kind != expected_kind
            || !llvm_domain_runtime_text_equal(row->owner_name,
                                               fact->owner_name)
            || row->projection_slot_source_syntax_id
                != fact->projection_slot_syntax_id
            || !llvm_domain_runtime_text_equal(row->projection_slot_name,
                                               fact->projection_slot_name)
            || row->source_slot_source_syntax_id
                != fact->source_slot_syntax_id
            || !llvm_domain_runtime_text_equal(row->source_slot_name,
                                               fact->source_slot_name)) {
            llvm_set_mir_inventory_missing(ctx,
                "LLVM domain runtime directive id %u is foreign to its MIR topology row",
                (unsigned)fact->directive_syntax_id);
            return false;
        }
        return true;
    }
    llvm_set_mir_inventory_missing(ctx,
        "LLVM domain runtime directive id %u has no MIR topology owner",
        (unsigned)fact->directive_syntax_id);
    return false;
}

static bool
llvm_domain_runtime_projection_fact_ready(
    LLVMGenCtx *ctx,
    const PgyDomainProjectionMemberAssignmentFact *fact)
{
    const MIRDeclField *projection_slot;
    const MIRDeclField *source_slot;
    const char *target_owner_name;
    const char *source_owner_name;
    const char *path_cursor;

    if (ctx == NULL || ctx->mir == NULL || fact == NULL)
        return false;
    if (fact->program_syntax_id !=
            llvm_domain_runtime_program_epoch(ctx->mir)
        || fact->target_decl_syntax_id == 0
        || fact->source_decl_syntax_id == 0
        || fact->source_path_segments == NULL
        || fact->source_path_segment_count == 0) {
        llvm_set_mir_inventory_missing(ctx,
            "LLVM domain runtime projection fact has missing or foreign identity");
        return false;
    }
    if (!llvm_domain_runtime_projection_topology_matches(ctx, fact))
        return false;

    projection_slot = llvm_domain_runtime_require_exact_field(ctx,
        fact->owner_name, fact->projection_slot_syntax_id,
        fact->projection_slot_name, NULL, "projection-slot");
    source_slot = llvm_domain_runtime_require_exact_field(ctx,
        fact->owner_name, fact->source_slot_syntax_id,
        fact->source_slot_name, NULL, "source-slot");
    if (projection_slot == NULL || source_slot == NULL)
        return false;

    target_owner_name = mir_decl_field_type_name(projection_slot);
    source_owner_name = mir_decl_field_type_name(source_slot);
    if (target_owner_name == NULL || source_owner_name == NULL
        || !llvm_domain_runtime_require_decl_identity(ctx,
            target_owner_name, fact->target_decl_syntax_id,
            "projection-target")
        || !llvm_domain_runtime_require_decl_identity(ctx,
            source_owner_name, fact->source_decl_syntax_id,
            "projection-source")
        || llvm_domain_runtime_require_exact_field(ctx,
            target_owner_name, fact->target_field_syntax_id,
            fact->target_field_name, fact->target_field_type_name,
            "projection-target") == NULL) {
        return false;
    }

    path_cursor = fact->source_path;
    for (size_t i = 0; i < fact->source_path_segment_count; i++) {
        const PgyDomainProjectionPathSegmentFact *segment =
            &fact->source_path_segments[i];
        size_t segment_length;

        if (path_cursor == NULL || segment->field_name == NULL)
            return false;
        segment_length = strlen(segment->field_name);
        if (strncmp(path_cursor, segment->field_name, segment_length) != 0
            || (i + 1 < fact->source_path_segment_count
                ? path_cursor[segment_length] != '.'
                : path_cursor[segment_length] != '\0')) {
            llvm_set_mir_inventory_missing(ctx,
                "LLVM domain runtime source path text disagrees with exact segments");
            return false;
        }

        if (llvm_domain_runtime_require_exact_field(ctx,
                source_owner_name, segment->field_syntax_id,
                segment->field_name, segment->field_type_name,
                "projection-source-path") == NULL) {
            return false;
        }
        source_owner_name = segment->field_type_name;
        path_cursor += segment_length;
        if (i + 1 < fact->source_path_segment_count)
            path_cursor++;
    }
    return llvm_domain_runtime_text_equal(
        fact->source_path_segments[fact->source_path_segment_count - 1]
            .field_type_name,
        fact->source_leaf_type_name);
}

static bool
llvm_domain_runtime_projection_same_identity(
    const PgyDomainProjectionMemberAssignmentFact *left,
    const PgyDomainProjectionMemberAssignmentFact *right)
{
    return left != NULL && right != NULL
        && left->program_syntax_id == right->program_syntax_id
        && left->owner_syntax_id == right->owner_syntax_id
        && left->directive_syntax_id == right->directive_syntax_id;
}

bool
llvm_domain_runtime_projection_same_directive(
    const PgyDomainProjectionMemberAssignmentFact *anchor,
    const PgyDomainProjectionMemberAssignmentFact *candidate)
{
    return llvm_domain_runtime_projection_same_identity(anchor, candidate)
        && anchor->operation == candidate->operation
        && llvm_domain_runtime_text_equal(anchor->owner_name,
                                          candidate->owner_name)
        && anchor->projection_slot_syntax_id
            == candidate->projection_slot_syntax_id
        && llvm_domain_runtime_text_equal(anchor->projection_slot_name,
                                          candidate->projection_slot_name)
        && anchor->source_slot_syntax_id
            == candidate->source_slot_syntax_id
        && llvm_domain_runtime_text_equal(anchor->source_slot_name,
                                          candidate->source_slot_name)
        && anchor->target_decl_syntax_id == candidate->target_decl_syntax_id
        && anchor->source_decl_syntax_id == candidate->source_decl_syntax_id;
}

static bool
llvm_domain_runtime_projection_is_anchor(const MIRProgram *mir,
    size_t index, const char *owner_name)
{
    const PgyDomainProjectionMemberAssignmentFact *fact;

    if (mir == NULL || owner_name == NULL
        || index >= mir->domain_projection_member_assignment_fact_count) {
        return false;
    }
    fact = &mir->domain_projection_member_assignment_facts[index];
    if (!llvm_domain_runtime_text_equal(fact->owner_name, owner_name))
        return false;
    for (size_t i = 0; i < index; i++) {
        const PgyDomainProjectionMemberAssignmentFact *prior =
            &mir->domain_projection_member_assignment_facts[i];
        if (llvm_domain_runtime_projection_same_identity(fact, prior))
            return false;
    }
    return true;
}

LLVMDomainRuntimeProjectionView
llvm_domain_runtime_projection_view(LLVMGenCtx *ctx,
                                    const char *owner_name)
{
    LLVMDomainRuntimeProjectionView view = {0};
    const MIRDeclHeader *owner_header;

    view.owner_name = owner_name;
    if (ctx == NULL || ctx->mir == NULL || owner_name == NULL
        || !ctx->mir->has_domain_runtime_facts
        || !ctx->mir->has_domain_topology) {
        if (ctx != NULL) {
            llvm_set_mir_inventory_missing(ctx,
                "LLVM domain runtime projection owner is missing MIR facts");
        }
        return view;
    }
    view.mir = ctx->mir;
    owner_header = mir_find_decl_header(ctx->mir, owner_name);
    if (owner_header == NULL) {
        llvm_set_mir_inventory_missing(ctx,
            "LLVM domain runtime projection owner '%s' is foreign to MIR",
            owner_name);
        return view;
    }
    for (size_t i = 0;
         i < ctx->mir->domain_projection_member_assignment_fact_count; i++) {
        const PgyDomainProjectionMemberAssignmentFact *fact =
            &ctx->mir->domain_projection_member_assignment_facts[i];
        if (!llvm_domain_runtime_text_equal(fact->owner_name, owner_name))
            continue;
        for (size_t j = 0; j < i; j++) {
            const PgyDomainProjectionMemberAssignmentFact *prior =
                &ctx->mir->domain_projection_member_assignment_facts[j];
            if (llvm_domain_runtime_projection_same_identity(fact, prior)
                && !llvm_domain_runtime_projection_same_directive(
                    fact, prior)) {
                llvm_set_mir_inventory_missing(ctx,
                    "LLVM domain runtime directive members disagree on exact source or target identity");
                return view;
            }
        }
        if (!llvm_domain_runtime_projection_fact_ready(ctx, fact))
            return view;
        if (llvm_domain_runtime_projection_is_anchor(ctx->mir, i,
                owner_name)) {
            view.directive_count++;
        }
    }
    if (view.directive_count !=
            mir_decl_header_zone_refresh_count(owner_header)) {
        llvm_set_mir_inventory_missing(ctx,
            "LLVM domain runtime projection directives are incomplete for '%s'",
            owner_name);
        return view;
    }
    view.valid = true;
    return view;
}

const PgyDomainProjectionMemberAssignmentFact *
llvm_domain_runtime_projection_anchor(
    const LLVMDomainRuntimeProjectionView *view,
    size_t directive_index)
{
    size_t seen = 0;

    if (view == NULL || !view->valid || view->mir == NULL
        || directive_index >= view->directive_count) {
        return NULL;
    }
    for (size_t i = 0;
         i < view->mir->domain_projection_member_assignment_fact_count; i++) {
        if (!llvm_domain_runtime_projection_is_anchor(view->mir, i,
                view->owner_name)) {
            continue;
        }
        if (seen == directive_index) {
            return &view->mir->domain_projection_member_assignment_facts[i];
        }
        seen++;
    }
    return NULL;
}

size_t
llvm_domain_runtime_projection_member_count(
    const LLVMDomainRuntimeProjectionView *view,
    const PgyDomainProjectionMemberAssignmentFact *anchor)
{
    size_t count = 0;

    if (view == NULL || !view->valid || view->mir == NULL || anchor == NULL)
        return 0;
    for (size_t i = 0;
         i < view->mir->domain_projection_member_assignment_fact_count; i++) {
        const PgyDomainProjectionMemberAssignmentFact *candidate =
            &view->mir->domain_projection_member_assignment_facts[i];
        if (llvm_domain_runtime_projection_same_directive(anchor, candidate))
            count++;
    }
    return count;
}

const PgyDomainProjectionMemberAssignmentFact *
llvm_domain_runtime_projection_member_at(
    const LLVMDomainRuntimeProjectionView *view,
    const PgyDomainProjectionMemberAssignmentFact *anchor,
    size_t member_index)
{
    size_t seen = 0;

    if (view == NULL || !view->valid || view->mir == NULL || anchor == NULL)
        return NULL;
    for (size_t i = 0;
         i < view->mir->domain_projection_member_assignment_fact_count; i++) {
        const PgyDomainProjectionMemberAssignmentFact *candidate =
            &view->mir->domain_projection_member_assignment_facts[i];
        if (!llvm_domain_runtime_projection_same_directive(anchor, candidate))
            continue;
        if (seen == member_index)
            return candidate;
        seen++;
    }
    return NULL;
}

const PgyDomainParticipantRoleFact *
llvm_domain_runtime_require_participant_role(
    LLVMGenCtx *ctx,
    const char *owner_name,
    PgyDomainParticipantRole role)
{
    const PgyDomainParticipantRoleFact *match = NULL;
    const MIRDeclHeader *owner_header;
    ASTNodeType expected_type;

    if (ctx == NULL || ctx->mir == NULL || owner_name == NULL
        || !ctx->mir->has_domain_runtime_facts) {
        if (ctx != NULL) {
            llvm_set_mir_inventory_missing(ctx,
                "LLVM domain participant role is missing MIR runtime facts");
        }
        return NULL;
    }
    if (role != PGY_DOMAIN_PARTICIPANT_EFFECT_BEARER
        && role != PGY_DOMAIN_PARTICIPANT_RELATION_SOURCE
        && role != PGY_DOMAIN_PARTICIPANT_RELATION_TARGET) {
        llvm_set_mir_inventory_missing(ctx,
            "LLVM domain participant role identity is invalid");
        return NULL;
    }
    expected_type = role == PGY_DOMAIN_PARTICIPANT_EFFECT_BEARER
        ? AST_EFFECT_DECL : AST_RELATION_DECL;
    owner_header = mir_find_decl_header_of_type(ctx->mir, expected_type,
        owner_name);
    if (owner_header == NULL) {
        llvm_set_mir_inventory_missing(ctx,
            "LLVM domain participant owner '%s' is foreign to MIR",
            owner_name);
        return NULL;
    }
    for (size_t i = 0;
         i < ctx->mir->domain_participant_role_fact_count; i++) {
        const PgyDomainParticipantRoleFact *fact =
            &ctx->mir->domain_participant_role_facts[i];
        if (fact->role != role
            || !llvm_domain_runtime_text_equal(fact->owner_name,
                                               owner_name)) {
            continue;
        }
        if (match != NULL) {
            llvm_set_mir_inventory_missing(ctx,
                "LLVM domain participant role is duplicated for '%s'",
                owner_name);
            return NULL;
        }
        match = fact;
    }
    if (match == NULL) {
        llvm_set_mir_inventory_missing(ctx,
            "LLVM domain participant role is missing for '%s'",
            owner_name);
        return NULL;
    }
    if (match->program_syntax_id !=
            llvm_domain_runtime_program_epoch(ctx->mir)) {
        llvm_set_mir_inventory_missing(ctx,
            "LLVM domain participant role belongs to a foreign program epoch");
        return NULL;
    }
    if (!llvm_domain_runtime_require_decl_identity(ctx, owner_name,
            match->owner_syntax_id, "participant-owner")
        || llvm_domain_runtime_require_exact_field(ctx,
            owner_name, match->field_syntax_id, match->field_name,
            match->field_type_name, "participant-role") == NULL) {
        return NULL;
    }
    for (size_t i = 0;
         i < ctx->mir->domain_projection_member_assignment_fact_count; i++) {
        const PgyDomainProjectionMemberAssignmentFact *projection =
            &ctx->mir->domain_projection_member_assignment_facts[i];
        if (llvm_domain_runtime_text_equal(projection->owner_name,
                                          owner_name)
            && projection->owner_syntax_id != match->owner_syntax_id) {
            llvm_set_mir_inventory_missing(ctx,
                "LLVM domain participant role owner id is foreign to projection facts for '%s'",
                owner_name);
            return NULL;
        }
    }
    return match;
}

#endif /* PGY_LLVM_ENABLED */
