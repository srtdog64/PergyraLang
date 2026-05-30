/*
 * Copyright (c) 2026 Pergyra Language Project
 * AIR RIR scope and boundary matching owner.
 */

#include "air_internal.h"

const char *
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

bool
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
air_rir_scope_matches_boundary(const RIRScope *scope,
                               const AIRBoundaryNode *boundary)
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
air_rir_op_matches_boundary_ast(const RIROp *op,
                                const AIRBoundaryNode *boundary)
{
    if (op == NULL || boundary == NULL)
        return false;
    if (boundary->ast == NULL)
        return true;
    return op->ast == boundary->ast
        || air_ast_contains_node(boundary->ast, op->ast);
}

static bool
air_rir_io_op_matches_boundary(const RIROp *op,
                               const AIRBoundaryNode *boundary)
{
    return op != NULL
        && boundary != NULL
        && boundary->kind == AIR_BOUNDARY_IO
        && op->kind == RIR_OP_IO
        && air_name_matches(op->subject, boundary->source_name)
        && air_rir_op_matches_boundary_ast(op, boundary);
}

static bool
air_rir_channel_op_matches_boundary(const RIROp *op,
                                    const AIRBoundaryNode *boundary)
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

bool
air_rir_parallel_op_matches_boundary(const RIROp *op,
                                     const AIRBoundaryNode *boundary)
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

bool
air_rir_scope_provides_boundary_evidence(const RIRScope *scope,
                                         const AIRBoundaryNode *boundary)
{
    if (scope != NULL && boundary != NULL
        && boundary->kind == AIR_BOUNDARY_PARALLEL) {
        if (boundary->ast == NULL)
            return false;
        for (size_t i = 0; i < rir_scope_op_count(scope); i++) {
            const RIROp *op = rir_scope_op_at(scope, i);
            if (air_rir_parallel_op_matches_boundary(op, boundary))
                return true;
        }
        return false;
    }

    if (scope != NULL && boundary != NULL
        && boundary->kind == AIR_BOUNDARY_IO) {
        if (boundary->ast == NULL)
            return false;
        for (size_t i = 0; i < rir_scope_op_count(scope); i++) {
            const RIROp *op = rir_scope_op_at(scope, i);
            if (air_rir_io_op_matches_boundary(op, boundary))
                return true;
        }
        return false;
    }

    if (scope != NULL && boundary != NULL
        && boundary->kind == AIR_BOUNDARY_CHANNEL) {
        if (boundary->ast == NULL)
            return false;
        for (size_t i = 0; i < rir_scope_op_count(scope); i++) {
            const RIROp *op = rir_scope_op_at(scope, i);
            if (air_rir_channel_op_matches_boundary(op, boundary))
                return true;
        }
        return false;
    }

    if (!air_rir_scope_matches_boundary(scope, boundary))
        return false;
    if (boundary->kind != AIR_BOUNDARY_WORLD)
        return true;

    for (size_t i = 0; i < rir_scope_op_count(scope); i++) {
        const RIROp *op = rir_scope_op_at(scope, i);
        if (op == NULL)
            continue;
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
