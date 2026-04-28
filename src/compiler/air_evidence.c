#include "air_internal.h"

#include <string.h>

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

static bool
air_hir_routine_matches_boundary(const HIRRoutine *routine,
                                 const AIRIntentNode *intent,
                                 const AIRBoundaryNode *boundary)
{
    if (routine == NULL || intent == NULL || boundary == NULL)
        return false;
    return air_name_matches(routine->owner_name, intent->intent_owner)
        || air_name_matches(routine->name, intent->step_name)
        || air_name_matches(routine->name, intent->intent_owner)
        || air_name_matches(routine->owner_name, boundary->source_name)
        || air_name_matches(routine->name, boundary->source_name);
}

bool
air_collect_hir_evidence(AIRProgram *air, const HIRProgram *hir, char **error_message)
{
    if (air == NULL || hir == NULL)
        return true;
    for (size_t i = 0; i < hir->routine_count; i++) {
        const HIRRoutine *routine = &hir->routines[i];
        for (size_t j = 0; j < air->boundary_count; j++) {
            AIRBoundaryNode *boundary = &air->boundaries[j];
            const AIRIntentNode *intent = &air->intents[boundary->intent_index];
            if (air_hir_routine_matches_boundary(routine, intent, boundary)) {
                const char *routine_name = routine->name != NULL
                    ? routine->name
                    : routine->owner_name;
                if (!air_assign_first_owned_name(air,
                                                 &boundary->hir_routine_evidence_name,
                                                 routine_name,
                                                 error_message,
                                                 "HIR routine")) {
                    return false;
                }
                boundary->has_hir_routine_evidence = true;
                air->hir_routine_evidence_count++;
            }
        }
    }
    return true;
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
air_rir_scope_provides_boundary_evidence(const RIRScope *scope,
                                         const AIRBoundaryNode *boundary)
{
    if (!air_rir_scope_matches_boundary(scope, boundary))
        return false;

    if (boundary->kind != AIR_BOUNDARY_WORLD)
        return true;

    for (size_t i = 0; i < scope->op_count; i++) {
        const RIROp *op = &scope->ops[i];
        if (op->kind == RIR_OP_CLAIM
            && air_name_matches(op->subject, boundary->source_name)) {
            return true;
        }
        if (op->kind == RIR_OP_MOVE
            && air_name_matches(op->arg0, boundary->source_name)) {
            return true;
        }
    }
    return false;
}

bool
air_collect_rir_evidence(AIRProgram *air, const RIRProgram *rir, char **error_message)
{
    if (air == NULL || rir == NULL)
        return true;
    for (size_t i = 0; i < rir->scope_count; i++) {
        const RIRScope *scope = &rir->scopes[i];
        const char *scope_name = scope->name != NULL ? scope->name : scope->owner_name;
        for (size_t j = 0; j < scope->fact_count; j++) {
            if (scope->facts[j].kind == RIR_FACT_AUTHORITY)
                air->rir_authority_evidence_count++;
        }
        for (size_t j = 0; j < scope->op_count; j++) {
            if (scope->ops[j].kind == RIR_OP_AUTHORIZE)
                air->rir_authority_evidence_count++;
        }
        for (size_t j = 0; j < air->boundary_count; j++) {
            AIRBoundaryNode *boundary = &air->boundaries[j];
            if (!air_rir_scope_provides_boundary_evidence(scope, boundary))
                continue;
            if (!air_assign_first_owned_name(air,
                                             &boundary->rir_boundary_evidence_scope,
                                             scope_name,
                                             error_message,
                                             "RIR boundary")) {
                return false;
            }
            boundary->has_rir_boundary_evidence = true;
            air->rir_boundary_evidence_count++;
            for (size_t k = 0; k < scope->fact_count; k++) {
                if (scope->facts[k].kind == RIR_FACT_AUTHORITY
                    && air_boundary_authority_matches(boundary, scope->facts[k].name)) {
                    if (!air_assign_first_owned_name(air,
                                                     &boundary->rir_authority_evidence_name,
                                                     scope->facts[k].name,
                                                     error_message,
                                                     "RIR authority")) {
                        return false;
                    }
                    boundary->has_rir_authority_evidence = true;
                    break;
                }
            }
            for (size_t k = 0; !boundary->has_rir_authority_evidence && k < scope->op_count; k++) {
                if (scope->ops[k].kind == RIR_OP_AUTHORIZE
                    && air_boundary_authority_matches(boundary, scope->ops[k].subject)) {
                    if (!air_assign_first_owned_name(air,
                                                     &boundary->rir_authority_evidence_name,
                                                     scope->ops[k].subject,
                                                     error_message,
                                                     "RIR authority")) {
                        return false;
                    }
                    boundary->has_rir_authority_evidence = true;
                    break;
                }
            }
        }
    }
    return true;
}
