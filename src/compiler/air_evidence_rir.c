/*
 * Copyright (c) 2026 Pergyra Language Project
 * AIR RIR evidence collection owner.
 */

#include "air_internal.h"

#include "../semantic/semantic.h"

static bool
air_boundary_authority_matches(const AIRBoundaryNode *boundary, const char *authority_name)
{
    if (boundary == NULL || authority_name == NULL)
        return false;
    for (size_t i = 0; i < boundary->authority_name_count; i++) {
        if (air_name_matches(boundary->authority_names[i], authority_name))
            return true;
    }
    return false;
}

static const char *
air_rir_scope_provider_name(const RIRScope *scope)
{
    if (scope == NULL)
        return NULL;
    if (!air_name_is_empty(scope->name))
        return scope->name;
    if (!air_name_is_empty(scope->owner_name))
        return scope->owner_name;
    return NULL;
}

static bool
air_require_rir_scope_provider(const RIRScope *scope,
                               char **error_message)
{
    if (air_rir_scope_provider_name(scope) != NULL)
        return true;
    air_set_error(error_message,
                  "AIR RIR evidence requires scope name or owner provenance");
    return false;
}

static bool
air_rir_scope_matches_boundary(const RIRScope *scope, const AIRBoundaryNode *boundary)
{
    if (scope == NULL || boundary == NULL)
        return false;
    if (!(scope->kind == RIR_SCOPE_INTENT
          || scope->kind == RIR_SCOPE_ZONE
          || scope->kind == RIR_SCOPE_WORLD)) {
        return false;
    }
    if (boundary->kind == AIR_BOUNDARY_PARALLEL
        || boundary->kind == AIR_BOUNDARY_IO
        || boundary->kind == AIR_BOUNDARY_CHANNEL) {
        return air_name_matches(scope->name, boundary->source_name);
    }
    if (boundary->kind == AIR_BOUNDARY_WORLD) {
        return air_name_matches(scope->name, boundary->source_name)
            || air_name_matches(scope->name, boundary->owner_name)
            || air_name_matches(scope->owner_name, boundary->owner_name)
            || air_name_matches(scope->owner_name, boundary->source_name);
    }
    return air_name_matches(scope->name, boundary->source_name)
        || air_name_matches(scope->owner_name, boundary->owner_name)
        || air_name_matches(scope->owner_name, boundary->source_name);
}


static bool
air_rir_op_matches_boundary_ast(const RIROp *op, const AIRBoundaryNode *boundary)
{
    if (op == NULL || boundary == NULL)
        return false;
    if (boundary->ast == NULL)
        return true;
    return op->ast == boundary->ast
        || air_ast_contains_node(boundary->ast, op->ast);
}

static bool
air_rir_io_op_matches_boundary(const RIROp *op, const AIRBoundaryNode *boundary)
{
    return op != NULL
        && boundary != NULL
        && boundary->kind == AIR_BOUNDARY_IO
        && op->kind == RIR_OP_IO
        && air_name_matches(op->subject, boundary->source_name)
        && air_rir_op_matches_boundary_ast(op, boundary);
}

static bool
air_rir_channel_op_matches_boundary(const RIROp *op, const AIRBoundaryNode *boundary)
{
    if (op == NULL || boundary == NULL || boundary->kind != AIR_BOUNDARY_CHANNEL)
        return false;
    if (air_name_matches(boundary->source_name, "channel-send"))
        return op->kind == RIR_OP_CHANNEL_SEND
            && air_rir_op_matches_boundary_ast(op, boundary);
    if (air_name_matches(boundary->source_name, "channel-recv"))
        return op->kind == RIR_OP_CHANNEL_RECV
            && air_rir_op_matches_boundary_ast(op, boundary);
    if (air_name_matches(boundary->source_name, "select"))
        return op->kind == RIR_OP_CHANNEL_SELECT
            && air_rir_op_matches_boundary_ast(op, boundary);
    return false;
}

static bool
air_rir_parallel_op_matches_boundary(const RIROp *op, const AIRBoundaryNode *boundary)
{
    if (op == NULL || boundary == NULL || boundary->kind != AIR_BOUNDARY_PARALLEL)
        return false;
    if (air_name_matches(boundary->source_name, "await"))
        return op->kind == RIR_OP_AWAIT_REMOTE
            && air_rir_op_matches_boundary_ast(op, boundary);
    if (air_name_matches(boundary->source_name, "spawn"))
        return op->kind == RIR_OP_SPAWN
            && air_rir_op_matches_boundary_ast(op, boundary);
    if (air_name_matches(boundary->source_name, "async"))
        return op->kind == RIR_OP_ASYNC
            && air_rir_op_matches_boundary_ast(op, boundary);
    if (air_name_matches(boundary->source_name, "task-group"))
        return op->kind == RIR_OP_TASK_GROUP
            && air_rir_op_matches_boundary_ast(op, boundary);
    if (air_name_matches(boundary->source_name, "parallel"))
        return op->kind == RIR_OP_PARALLEL
            && air_rir_op_matches_boundary_ast(op, boundary);
    return false;
}

static bool
air_rir_scope_provides_boundary_evidence(const RIRScope *scope,
                                         const AIRBoundaryNode *boundary)
{
    if (scope != NULL && boundary != NULL
        && boundary->kind == AIR_BOUNDARY_PARALLEL) {
        if (boundary->ast == NULL)
            return false;
        for (size_t i = 0; i < scope->op_count; i++) {
            if (air_rir_parallel_op_matches_boundary(&scope->ops[i], boundary))
                return true;
        }
        return false;
    }

    if (scope != NULL && boundary != NULL
        && boundary->kind == AIR_BOUNDARY_IO) {
        if (boundary->ast == NULL)
            return false;
        for (size_t i = 0; i < scope->op_count; i++) {
            if (air_rir_io_op_matches_boundary(&scope->ops[i], boundary))
                return true;
        }
        return false;
    }

    if (scope != NULL && boundary != NULL
        && boundary->kind == AIR_BOUNDARY_CHANNEL) {
        if (boundary->ast == NULL)
            return false;
        for (size_t i = 0; i < scope->op_count; i++) {
            if (air_rir_channel_op_matches_boundary(&scope->ops[i], boundary))
                return true;
        }
        return false;
    }

    if (!air_rir_scope_matches_boundary(scope, boundary))
        return false;

    if (boundary->kind != AIR_BOUNDARY_WORLD)
        return true;

    for (size_t i = 0; i < scope->op_count; i++) {
        const RIROp *op = &scope->ops[i];
        if (op->kind == RIR_OP_CLAIM
            && air_name_matches(op->subject, boundary->source_name)
            && air_rir_op_matches_boundary_ast(op, boundary)) {
            return true;
        }
        if (op->kind == RIR_OP_MOVE
            && air_name_matches(op->arg0, boundary->source_name)
            && air_rir_op_matches_boundary_ast(op, boundary)) {
            return true;
        }
    }
    return false;
}

static bool
air_rir_name_or_anchor_matches(const char *name, const char *slot_anchor, const char *needle)
{
    return air_name_matches(name, needle) || air_name_matches(slot_anchor, needle);
}

static bool
air_rir_scope_has_propagation_state(const RIRScope *scope,
                                    const RIROp *op,
                                    RIRResourceKind resource_kind)
{
    if (scope == NULL || op == NULL)
        return false;
    for (size_t i = 0; i < scope->state_summary_count; i++) {
        const RIRStateSummary *summary = &scope->state_summaries[i];
        if (summary->resource_kind == resource_kind
            && air_rir_name_or_anchor_matches(summary->name, summary->slot_anchor, op->subject)) {
            return true;
        }
    }
    for (size_t i = 0; i < scope->fact_count; i++) {
        const RIRFact *fact = &scope->facts[i];
        if (fact->resource_kind == resource_kind
            && air_rir_name_or_anchor_matches(fact->name, fact->slot_anchor, op->subject)) {
            return true;
        }
    }
    return false;
}

static bool
air_collect_rir_propagation_evidence(AIRProgram *air,
                                     const RIRScope *scope,
                                     const RIROp *op,
                                     const char *scope_name,
                                     char **error_message)
{
    const bool effect_op = op->kind == RIR_OP_ATTACH_EFFECT
        || op->kind == RIR_OP_DETACH_EFFECT;
    const bool relation_op = op->kind == RIR_OP_LINK_RELATION
        || op->kind == RIR_OP_UNLINK_RELATION;
    RIRResourceKind resource_kind;
    AIREvidenceKind evidence_kind;

    if (!effect_op && !relation_op)
        return true;

    resource_kind = effect_op
        ? RIR_RESOURCE_EFFECT_INSTANCE
        : RIR_RESOURCE_RELATION_INSTANCE;
    evidence_kind = effect_op
        ? AIR_EVIDENCE_RIR_EFFECT_PROPAGATION
        : AIR_EVIDENCE_RIR_RELATION_PROPAGATION;

    if (effect_op)
        air->rir_effect_propagation_required_count++;
    else
        air->rir_relation_propagation_required_count++;

    if (!air_rir_scope_has_propagation_state(scope, op, resource_kind))
        return true;
    if (!air_require_rir_scope_provider(scope, error_message))
        return false;

    if (!air_append_evidence_node(air,
                                  evidence_kind,
                                  SIZE_MAX,
                                  scope_name,
                                  op->subject,
                                  error_message)) {
        return false;
    }
    if (effect_op)
        air->rir_effect_propagation_evidence_count++;
    else
        air->rir_relation_propagation_evidence_count++;
    return true;
}

bool
air_collect_rir_evidence(AIRProgram *air, const RIRProgram *rir, char **error_message)
{
    if (air == NULL || rir == NULL)
        return true;
    air->has_rir_input = true;
    for (size_t i = 0; i < rir->scope_count; i++) {
        const RIRScope *scope = &rir->scopes[i];
        const char *scope_name = air_rir_scope_provider_name(scope);
        for (size_t j = 0; j < scope->fact_count; j++) {
            if (scope->facts[j].kind == RIR_FACT_AUTHORITY)
                air->rir_authority_evidence_count++;
        }
        for (size_t j = 0; j < scope->op_count; j++) {
            if (scope->ops[j].kind == RIR_OP_AUTHORIZE)
                air->rir_authority_evidence_count++;
            if (!air_collect_rir_propagation_evidence(air,
                                                      scope,
                                                      &scope->ops[j],
                                                      scope_name,
                                                      error_message)) {
                return false;
            }
        }
        for (size_t j = 0; j < air->boundary_count; j++) {
            AIRBoundaryNode *boundary = &air->boundaries[j];
            if (!air_rir_scope_provides_boundary_evidence(scope, boundary))
                continue;
            if (!air_require_rir_scope_provider(scope, error_message))
                return false;
            if (!boundary->has_rir_boundary_evidence) {
                if (!air_assign_first_owned_name(
                        air,
                        &boundary->rir_boundary_evidence_scope,
                        scope_name,
                        error_message,
                        "RIR boundary")) {
                    return false;
                }
                if (!air_append_evidence_node(air,
                                              AIR_EVIDENCE_RIR_BOUNDARY,
                                              j,
                                              scope_name,
                                              boundary->source_name,
                                              error_message)) {
                    return false;
                }
                boundary->has_rir_boundary_evidence = true;
                air->rir_boundary_evidence_count++;
            }
            for (size_t k = 0; k < scope->fact_count; k++) {
                if (scope->facts[k].kind == RIR_FACT_AUTHORITY
                    && air_boundary_authority_matches(boundary, scope->facts[k].name)) {
                    if (air_boundary_has_evidence_kind_subject(
                            air,
                            j,
                            AIR_EVIDENCE_RIR_AUTHORITY,
                            scope->facts[k].name)) {
                        continue;
                    }
                    if (!air_assign_first_owned_name(air,
                                                     &boundary->rir_authority_evidence_name,
                                                     scope->facts[k].name,
                                                     error_message,
                                                     "RIR authority")) {
                        return false;
                    }
                    if (!air_append_evidence_node(air,
                                                  AIR_EVIDENCE_RIR_AUTHORITY,
                                                  j,
                                                  scope_name,
                                                  scope->facts[k].name,
                                                  error_message)) {
                        return false;
                    }
                    boundary->has_rir_authority_evidence = true;
                }
            }
            for (size_t k = 0; k < scope->op_count; k++) {
                if (scope->ops[k].kind == RIR_OP_AUTHORIZE
                    && air_boundary_authority_matches(boundary, scope->ops[k].subject)) {
                    if (air_boundary_has_evidence_kind_subject(
                            air,
                            j,
                            AIR_EVIDENCE_RIR_AUTHORITY,
                            scope->ops[k].subject)) {
                        continue;
                    }
                    if (!air_assign_first_owned_name(air,
                                                     &boundary->rir_authority_evidence_name,
                                                     scope->ops[k].subject,
                                                     error_message,
                                                     "RIR authority")) {
                        return false;
                    }
                    if (!air_append_evidence_node(air,
                                                  AIR_EVIDENCE_RIR_AUTHORITY,
                                                  j,
                                                  scope_name,
                                                  scope->ops[k].subject,
                                                  error_message)) {
                        return false;
                    }
                    boundary->has_rir_authority_evidence = true;
                }
            }
        }
    }
    return true;
}
