/*
 * Copyright (c) 2026 Pergyra Language Project
 * AIR RIR boundary and authority evidence owner.
 */

#include "air_internal.h"

static bool
air_append_rir_boundary_evidence(AIRProgram *air,
                                 AIRBoundaryNode *boundary,
                                 size_t boundary_index,
                                 const char *scope_name,
                                 bool retain_intent_authority_provider,
                                 char **error_message)
{
    if (air_boundary_has_summary_flag(boundary, AIR_EVIDENCE_RIR_BOUNDARY)) {
        /* Action authority can be proved by an intent-local Authorize op
         * even when a zone scope supplied the first structural boundary
         * fact.  Retain both named providers so the authority evidence stays
         * paired with the scope that owns the exact operation. */
        if ((!boundary->authority_from_action
             && !retain_intent_authority_provider)
            || air_boundary_has_evidence_kind_provider(
                   air, boundary_index, AIR_EVIDENCE_RIR_BOUNDARY,
                   scope_name)) {
            return true;
        }
        if (!air_append_evidence_node(air,
                                      AIR_EVIDENCE_RIR_BOUNDARY,
                                      boundary_index,
                                      scope_name,
                                      boundary->source_name,
                                      error_message)) {
            return false;
        }
        if (!air_increment_evidence_summary_count(
                air, AIR_EVIDENCE_RIR_BOUNDARY)) {
            air_set_error(error_message,
                          "AIR RIR boundary evidence counter overflow");
            return false;
        }
        return true;
    }
    if (!air_assign_first_owned_name(air,
                                     &boundary->rir_boundary_evidence_scope,
                                     scope_name,
                                     error_message,
                                     "RIR boundary")) {
        return false;
    }
    if (!air_append_evidence_node(air,
                                  AIR_EVIDENCE_RIR_BOUNDARY,
                                  boundary_index,
                                  scope_name,
                                  boundary->source_name,
                                  error_message)) {
        return false;
    }
    air_boundary_mark_summary_flag(boundary, AIR_EVIDENCE_RIR_BOUNDARY);
    if (!air_increment_evidence_summary_count(
            air,
            AIR_EVIDENCE_RIR_BOUNDARY)) {
        air_set_error(error_message,
                      "AIR RIR boundary evidence counter overflow");
        return false;
    }
    return true;
}

static bool
air_append_rir_authority_evidence(AIRProgram *air,
                                  AIRBoundaryNode *boundary,
                                  size_t boundary_index,
                                  const char *scope_name,
                                  const char *authority_name,
                                  char **error_message)
{
    if (air_boundary_has_evidence_kind_subject(air,
                                               boundary_index,
                                               AIR_EVIDENCE_RIR_AUTHORITY,
                                               authority_name)) {
        return true;
    }
    if (!air_assign_first_owned_name(air,
                                     &boundary->rir_authority_evidence_name,
                                     authority_name,
                                     error_message,
                                     "RIR authority")) {
        return false;
    }
    if (!air_append_evidence_node(air,
                                  AIR_EVIDENCE_RIR_AUTHORITY,
                                  boundary_index,
                                  scope_name,
                                  authority_name,
                                  error_message)) {
        return false;
    }
    air_boundary_mark_summary_flag(boundary, AIR_EVIDENCE_RIR_AUTHORITY);
    if (!air_increment_evidence_summary_count(
            air,
            AIR_EVIDENCE_RIR_AUTHORITY)) {
        air_set_error(error_message,
                      "AIR RIR authority evidence counter overflow");
        return false;
    }
    return true;
}

static bool
air_collect_rir_scope_fact_authority(AIRProgram *air,
                                     AIRBoundaryNode *boundary,
                                     size_t boundary_index,
                                     const RIRScope *scope,
                                     const char *scope_name,
                                     char **error_message)
{
    for (size_t i = 0; i < rir_scope_fact_count(scope); i++) {
        const RIRFact *fact = rir_scope_fact_at(scope, i);
        if (fact == NULL || fact->kind != RIR_FACT_AUTHORITY)
            continue;
        if (!air_boundary_declares_authority_name(boundary, fact->name))
            continue;
        if (!air_append_rir_authority_evidence(air,
                                               boundary,
                                               boundary_index,
                                               scope_name,
                                               fact->name,
                                               error_message)) {
            return false;
        }
    }
    return true;
}

static bool
air_collect_rir_scope_op_authority(AIRProgram *air,
                                   AIRBoundaryNode *boundary,
                                   size_t boundary_index,
                                   const RIRScope *scope,
                                   const char *scope_name,
                                   char **error_message)
{
    for (size_t i = 0; i < rir_scope_op_count(scope); i++) {
        const RIROp *op = rir_scope_op_at(scope, i);
        if (op == NULL || op->kind != RIR_OP_AUTHORIZE)
            continue;
        if (scope->kind == RIR_SCOPE_INTENT
            && boundary->ast != NULL
            && op->ast != boundary->ast
            && !air_ast_contains_node(boundary->ast, op->ast)) {
            continue;
        }
        if (!air_boundary_declares_authority_name(boundary, op->subject))
            continue;
        if (!air_append_rir_authority_evidence(air,
                                               boundary,
                                               boundary_index,
                                               scope_name,
                                               op->subject,
                                               error_message)) {
            return false;
        }
    }
    return true;
}

static void
air_collect_rir_scope_capture_evidence(AIRBoundaryNode *boundary,
                                       const RIRScope *scope)
{
    bool resource_capture_boundary;

    if (boundary == NULL || scope == NULL)
        return;

    resource_capture_boundary = boundary->kind == AIR_BOUNDARY_PARALLEL;

    for (size_t i = 0; i < rir_scope_op_count(scope); i++) {
        const RIROp *op = rir_scope_op_at(scope, i);
        if (op == NULL)
            continue;

        if (air_rir_parallel_op_matches_boundary(op, boundary)) {
            if (op->kind == RIR_OP_AWAIT_LOCAL)
                boundary->has_rir_await_local_evidence = true;
            if (op->kind == RIR_OP_SPAWN)
                boundary->has_rir_movability_requirement_evidence = true;
            if (op->kind == RIR_OP_PARALLEL)
                boundary->has_rir_deterministic_fork_join_evidence = true;
        }

        if ((op->kind == RIR_OP_CHANNEL_SEND
             || op->kind == RIR_OP_CHANNEL_RECV
             || op->kind == RIR_OP_CHANNEL_SELECT)
            && boundary->kind == AIR_BOUNDARY_CHANNEL) {
            boundary->has_rir_raw_channel_capture_evidence = true;
        }

        if ((op->kind == RIR_OP_BORROW_READ
             || op->kind == RIR_OP_BORROW_WRITE)
            && resource_capture_boundary
            && air_ast_contains_node(boundary->ast, op->ast)) {
            boundary->has_rir_live_view_capture_evidence = true;
        }

        if ((op->kind == RIR_OP_CLAIM
             || op->kind == RIR_OP_READ
             || op->kind == RIR_OP_WRITE
             || op->kind == RIR_OP_RELEASE
             || op->kind == RIR_OP_MOVE)
            && resource_capture_boundary
            && air_ast_contains_node(boundary->ast, op->ast)) {
            boundary->has_rir_raw_slot_capture_evidence = true;
        }
    }
}

bool
air_collect_rir_scope_boundary_evidence(AIRProgram *air,
                                        const RIRScope *scope,
                                        char **error_message)
{
    const char *scope_name;

    if (air == NULL || scope == NULL)
        return true;
    scope_name = air_rir_scope_provider_name(scope);
    for (size_t i = 0; i < air_boundary_node_count(air); i++) {
        AIRBoundaryNode *boundary = air_boundary_node_mut_at(air, i);
        if (boundary == NULL)
            continue;

        if (!air_rir_scope_provides_boundary_evidence(scope, boundary))
            continue;
        if (!air_require_rir_scope_provider(scope, error_message))
            return false;
        if (!air_append_rir_boundary_evidence(air,
                                              boundary,
                                              i,
                                              scope_name,
                                              scope->kind == RIR_SCOPE_INTENT
                                                  && boundary->authority_required,
                                              error_message)) {
            return false;
        }
        if (boundary->kind == AIR_BOUNDARY_ZONE)
            boundary->has_rir_zone_pin_evidence = true;
        if (!air_collect_rir_scope_fact_authority(air,
                                                  boundary,
                                                  i,
                                                  scope,
                                                  scope_name,
                                                  error_message)) {
            return false;
        }
        if (!air_collect_rir_scope_op_authority(air,
                                                boundary,
                                                i,
                                                scope,
                                                scope_name,
                                                error_message)) {
            return false;
        }
        air_collect_rir_scope_capture_evidence(boundary, scope);
    }
    return true;
}
