#include "rir.h"
#include "rir_internal.h"

#include <stdlib.h>
#include <string.h>

#include "dir.h"
#include "../common/string_compat.h"

static void
rir_state_mark_error(RIRScope *scope, RIRStateSummary *summary, RIROpKind op_kind)
{
    if (summary == NULL)
        return;
    summary->has_transition_error = true;
    summary->final_state = RIR_STATE_INVALID;
    summary->last_op_name = rir_op_kind_name(op_kind);
    if (scope != NULL)
        scope->has_state_errors = true;
}

void
rir_apply_op_to_summary(RIRScope *scope, RIRStateSummary *summary, const RIROp *op)
{
    bool projection_like = false;
    bool authority_like = false;
    bool handoff_like = false;

    if (summary == NULL || op == NULL)
        return;

    summary->last_op_name = rir_op_kind_name(op->kind);
    projection_like = summary->resource_kind == RIR_RESOURCE_PROJECTION_OBJECT
        || summary->resource_kind == RIR_RESOURCE_PROJECTION_TOBJECT;
    authority_like = summary->resource_kind == RIR_RESOURCE_AUTHORITY_HANDLE
        || summary->resource_kind == RIR_RESOURCE_CAPABILITY_TOKEN;
    handoff_like = summary->resource_kind == RIR_RESOURCE_ZONE_HANDLE
        || summary->resource_kind == RIR_RESOURCE_WORLD_HANDLE;

    switch (op->kind) {
        case RIR_OP_CLAIM:
            if (handoff_like
                && (summary->final_state == RIR_STATE_HANDOFF_PENDING
                    || summary->final_state == RIR_STATE_HANDED_OFF
                    || summary->final_state == RIR_STATE_OWNED)) {
                summary->final_state = RIR_STATE_HANDED_OFF;
                return;
            }
            if (authority_like) {
                summary->final_state = RIR_STATE_AUTHORIZED;
                return;
            }
            summary->final_state = RIR_STATE_OWNED;
            return;

        case RIR_OP_READ:
            if (authority_like) {
                if (summary->final_state == RIR_STATE_AUTHORIZED)
                    return;
                rir_state_mark_error(scope, summary, op->kind);
                return;
            }
            if (summary->final_state == RIR_STATE_OWNED
                || summary->final_state == RIR_STATE_BORROWED_READ
                || summary->final_state == RIR_STATE_BORROWED_WRITE
                || summary->final_state == RIR_STATE_SYNCED
                || summary->final_state == RIR_STATE_DIRTY
                || summary->final_state == RIR_STATE_PUBLISHED) {
                return;
            }
            rir_state_mark_error(scope, summary, op->kind);
            return;

        case RIR_OP_WRITE:
            if (projection_like) {
                summary->final_state = RIR_STATE_DIRTY;
                return;
            }
            if (summary->final_state == RIR_STATE_OWNED
                || summary->final_state == RIR_STATE_BORROWED_WRITE
                || summary->final_state == RIR_STATE_DIRTY
                || summary->final_state == RIR_STATE_SYNCED) {
                summary->final_state = RIR_STATE_OWNED;
                return;
            }
            rir_state_mark_error(scope, summary, op->kind);
            return;

        case RIR_OP_RELEASE:
            if (authority_like) {
                summary->final_state = RIR_STATE_AUTHORITY_LOST;
                return;
            }
            if (projection_like) {
                summary->final_state = RIR_STATE_STALE;
                return;
            }
            if (handoff_like) {
                summary->final_state = RIR_STATE_HANDED_OFF;
                return;
            }
            if (summary->final_state == RIR_STATE_OWNED
                || summary->final_state == RIR_STATE_BORROWED_READ
                || summary->final_state == RIR_STATE_BORROWED_WRITE
                || summary->final_state == RIR_STATE_SYNCED
                || summary->final_state == RIR_STATE_DIRTY
                || summary->final_state == RIR_STATE_PUBLISHED
                || summary->final_state == RIR_STATE_REMOTE_PENDING) {
                summary->final_state = RIR_STATE_RELEASED;
                return;
            }
            rir_state_mark_error(scope, summary, op->kind);
            return;

        case RIR_OP_MOVE:
            if (authority_like) {
                summary->final_state = RIR_STATE_AUTHORITY_LOST;
                return;
            }
            if (projection_like) {
                summary->final_state = RIR_STATE_STALE;
                return;
            }
            if (handoff_like) {
                summary->final_state = RIR_STATE_HANDOFF_PENDING;
                return;
            }
            if (summary->final_state == RIR_STATE_OWNED
                || summary->final_state == RIR_STATE_BORROWED_READ
                || summary->final_state == RIR_STATE_BORROWED_WRITE
                || summary->final_state == RIR_STATE_SYNCED
                || summary->final_state == RIR_STATE_DIRTY
                || summary->final_state == RIR_STATE_PUBLISHED) {
                summary->final_state = RIR_STATE_MOVED;
                return;
            }
            rir_state_mark_error(scope, summary, op->kind);
            return;

        case RIR_OP_BORROW_READ:
            if (authority_like) {
                if (summary->final_state == RIR_STATE_AUTHORIZED)
                    return;
                rir_state_mark_error(scope, summary, op->kind);
                return;
            }
            if (summary->final_state == RIR_STATE_OWNED) {
                summary->final_state = RIR_STATE_BORROWED_READ;
                return;
            }
            rir_state_mark_error(scope, summary, op->kind);
            return;

        case RIR_OP_BORROW_WRITE:
            if (projection_like) {
                summary->final_state = RIR_STATE_DIRTY;
                return;
            }
            if (summary->final_state == RIR_STATE_OWNED) {
                summary->final_state = RIR_STATE_BORROWED_WRITE;
                return;
            }
            rir_state_mark_error(scope, summary, op->kind);
            return;

        case RIR_OP_PROJECT_REFRESH:
            summary->final_state = RIR_STATE_SYNCED;
            return;

        case RIR_OP_PROJECT_PUBLISH:
            summary->final_state = RIR_STATE_PUBLISHED;
            return;

        case RIR_OP_ATTACH_EFFECT:
        case RIR_OP_LINK_RELATION:
            summary->final_state = RIR_STATE_SYNCED;
            return;

        case RIR_OP_DETACH_EFFECT:
        case RIR_OP_UNLINK_RELATION:
            summary->final_state = RIR_STATE_DETACHED;
            return;

        case RIR_OP_AUTHORIZE:
            if (authority_like) {
                summary->final_state = RIR_STATE_AUTHORIZED;
                return;
            }
            return;

        case RIR_OP_AWAIT_REMOTE:
            if (summary->resource_kind == RIR_RESOURCE_REMOTE_FUTURE_HANDLE)
                summary->final_state = RIR_STATE_REMOTE_PENDING;
            else
                rir_state_mark_error(scope, summary, op->kind);
            return;

        case RIR_OP_ABORT_INTENT:
        case RIR_OP_COMPENSATE_INTENT_STEP:
            if (authority_like) {
                summary->final_state = RIR_STATE_AUTHORITY_LOST;
                return;
            }
            if (projection_like) {
                summary->final_state = RIR_STATE_STALE;
                return;
            }
            if (summary->resource_kind == RIR_RESOURCE_EFFECT_INSTANCE
                || summary->resource_kind == RIR_RESOURCE_RELATION_INSTANCE) {
                summary->final_state = RIR_STATE_COMPENSATED;
                return;
            }
            if (handoff_like) {
                summary->final_state = RIR_STATE_HANDED_OFF;
                return;
            }
            return;

        default:
            return;
    }
}

void
rir_apply_op_to_state(RIRResourceKind resource_kind,
                      RIRResourceState *state,
                      bool *had_error,
                      RIROpKind op_kind)
{
    RIRResourceState current;
    bool projection_like;
    bool authority_like;
    bool handoff_like;
    if (state == NULL)
        return;
    current = *state;
    projection_like = resource_kind == RIR_RESOURCE_PROJECTION_OBJECT
        || resource_kind == RIR_RESOURCE_PROJECTION_TOBJECT;
    authority_like = resource_kind == RIR_RESOURCE_AUTHORITY_HANDLE
        || resource_kind == RIR_RESOURCE_CAPABILITY_TOKEN;
    handoff_like = resource_kind == RIR_RESOURCE_ZONE_HANDLE
        || resource_kind == RIR_RESOURCE_WORLD_HANDLE;
    switch (op_kind) {
        case RIR_OP_CLAIM:
            if (handoff_like
                && (current == RIR_STATE_HANDOFF_PENDING
                    || current == RIR_STATE_HANDED_OFF
                    || current == RIR_STATE_OWNED)) {
                *state = RIR_STATE_HANDED_OFF;
                return;
            }
            if (authority_like) {
                *state = RIR_STATE_AUTHORIZED;
                return;
            }
            *state = RIR_STATE_OWNED;
            return;
        case RIR_OP_READ:
            if (authority_like) {
                if (current == RIR_STATE_AUTHORIZED)
                    return;
                *state = RIR_STATE_INVALID;
                if (had_error != NULL)
                    *had_error = true;
                return;
            }
            if (current == RIR_STATE_OWNED
                || current == RIR_STATE_BORROWED_READ
                || current == RIR_STATE_BORROWED_WRITE
                || current == RIR_STATE_SYNCED
                || current == RIR_STATE_DIRTY
                || current == RIR_STATE_PUBLISHED)
                return;
            *state = RIR_STATE_INVALID;
            if (had_error != NULL)
                *had_error = true;
            return;
        case RIR_OP_WRITE:
            if (projection_like) {
                *state = RIR_STATE_DIRTY;
                return;
            }
            if (current == RIR_STATE_OWNED
                || current == RIR_STATE_BORROWED_WRITE
                || current == RIR_STATE_DIRTY
                || current == RIR_STATE_SYNCED) {
                *state = RIR_STATE_OWNED;
                return;
            }
            *state = RIR_STATE_INVALID;
            if (had_error != NULL)
                *had_error = true;
            return;
        case RIR_OP_RELEASE:
            if (authority_like) {
                *state = RIR_STATE_AUTHORITY_LOST;
                return;
            }
            if (projection_like) {
                *state = RIR_STATE_STALE;
                return;
            }
            if (handoff_like) {
                *state = RIR_STATE_HANDED_OFF;
                return;
            }
            if (current == RIR_STATE_OWNED
                || current == RIR_STATE_BORROWED_READ
                || current == RIR_STATE_BORROWED_WRITE
                || current == RIR_STATE_SYNCED
                || current == RIR_STATE_DIRTY
                || current == RIR_STATE_PUBLISHED
                || current == RIR_STATE_REMOTE_PENDING) {
                *state = RIR_STATE_RELEASED;
                return;
            }
            *state = RIR_STATE_INVALID;
            if (had_error != NULL)
                *had_error = true;
            return;
        case RIR_OP_MOVE:
            if (authority_like) {
                *state = RIR_STATE_AUTHORITY_LOST;
                return;
            }
            if (projection_like) {
                *state = RIR_STATE_STALE;
                return;
            }
            if (handoff_like) {
                *state = RIR_STATE_HANDOFF_PENDING;
                return;
            }
            if (current == RIR_STATE_OWNED
                || current == RIR_STATE_BORROWED_READ
                || current == RIR_STATE_BORROWED_WRITE
                || current == RIR_STATE_SYNCED
                || current == RIR_STATE_DIRTY
                || current == RIR_STATE_PUBLISHED) {
                *state = RIR_STATE_MOVED;
                return;
            }
            *state = RIR_STATE_INVALID;
            if (had_error != NULL)
                *had_error = true;
            return;
        case RIR_OP_BORROW_READ:
            if (authority_like) {
                if (current == RIR_STATE_AUTHORIZED)
                    return;
                *state = RIR_STATE_INVALID;
                if (had_error != NULL)
                    *had_error = true;
                return;
            }
            if (current == RIR_STATE_OWNED) {
                *state = RIR_STATE_BORROWED_READ;
                return;
            }
            *state = RIR_STATE_INVALID;
            if (had_error != NULL)
                *had_error = true;
            return;
        case RIR_OP_BORROW_WRITE:
            if (projection_like) {
                *state = RIR_STATE_DIRTY;
                return;
            }
            if (current == RIR_STATE_OWNED) {
                *state = RIR_STATE_BORROWED_WRITE;
                return;
            }
            *state = RIR_STATE_INVALID;
            if (had_error != NULL)
                *had_error = true;
            return;
        case RIR_OP_PROJECT_REFRESH:
            *state = RIR_STATE_SYNCED;
            return;
        case RIR_OP_PROJECT_PUBLISH:
            *state = RIR_STATE_PUBLISHED;
            return;
        case RIR_OP_ATTACH_EFFECT:
        case RIR_OP_LINK_RELATION:
            *state = RIR_STATE_SYNCED;
            return;
        case RIR_OP_DETACH_EFFECT:
        case RIR_OP_UNLINK_RELATION:
            *state = RIR_STATE_DETACHED;
            return;
        case RIR_OP_AUTHORIZE:
            if (authority_like) {
                *state = RIR_STATE_AUTHORIZED;
                return;
            }
            return;
        case RIR_OP_AWAIT_REMOTE:
            if (resource_kind == RIR_RESOURCE_REMOTE_FUTURE_HANDLE) {
                *state = RIR_STATE_REMOTE_PENDING;
                return;
            }
            *state = RIR_STATE_INVALID;
            if (had_error != NULL)
                *had_error = true;
            return;
        case RIR_OP_ABORT_INTENT:
        case RIR_OP_COMPENSATE_INTENT_STEP:
            if (authority_like) {
                *state = RIR_STATE_AUTHORITY_LOST;
                return;
            }
            if (projection_like) {
                *state = RIR_STATE_STALE;
                return;
            }
            if (resource_kind == RIR_RESOURCE_EFFECT_INSTANCE
                || resource_kind == RIR_RESOURCE_RELATION_INSTANCE) {
                *state = RIR_STATE_COMPENSATED;
                return;
            }
            if (handoff_like) {
                *state = RIR_STATE_HANDED_OFF;
                return;
            }
            return;
        default:
            return;
    }
}
