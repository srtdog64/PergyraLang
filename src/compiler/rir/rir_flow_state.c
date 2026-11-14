#include "rir_flow_state.h"

static bool
rir_is_handle_kind(RIRResourceKind kind)
{
    return kind == RIR_RESOURCE_RELATION_INSTANCE
           || kind == RIR_RESOURCE_EFFECT_INSTANCE
           || kind == RIR_RESOURCE_ZONE_HANDLE
           || kind == RIR_RESOURCE_WORLD_HANDLE;
}

static bool
rir_is_projection_kind(RIRResourceKind kind)
{
    return kind == RIR_RESOURCE_PROJECTION_OBJECT
           || kind == RIR_RESOURCE_PROJECTION_TOBJECT;
}

static bool
rir_is_authority_kind(RIRResourceKind kind)
{
    return kind == RIR_RESOURCE_AUTHORITY_HANDLE
           || kind == RIR_RESOURCE_CAPABILITY_TOKEN;
}

static bool
rir_is_handoff_kind(RIRResourceKind kind)
{
    return kind == RIR_RESOURCE_ZONE_HANDLE
           || kind == RIR_RESOURCE_WORLD_HANDLE;
}

static RIRResourceState
rir_merge_handle_states(RIRResourceKind kind,
                        RIRResourceState a,
                        RIRResourceState b,
                        bool *conflict)
{
    if (a == b)
        return a;
    if (a == RIR_STATE_UNINIT)
        return b;
    if (b == RIR_STATE_UNINIT)
        return a;
    if (a == RIR_STATE_INVALID || b == RIR_STATE_INVALID) {
        if (conflict != NULL)
            *conflict = true;
        return RIR_STATE_INVALID;
    }
    if ((a == RIR_STATE_RELEASED || a == RIR_STATE_MOVED)
        || (b == RIR_STATE_RELEASED || b == RIR_STATE_MOVED)) {
        if (conflict != NULL)
            *conflict = true;
        return RIR_STATE_INVALID;
    }

    if (rir_is_handoff_kind(kind)) {
        if (a == RIR_STATE_HANDOFF_PENDING || b == RIR_STATE_HANDOFF_PENDING)
            return RIR_STATE_HANDOFF_PENDING;
        if (a == RIR_STATE_HANDED_OFF && b == RIR_STATE_HANDED_OFF)
            return RIR_STATE_HANDED_OFF;
        if ((a == RIR_STATE_HANDED_OFF && b == RIR_STATE_OWNED)
            || (b == RIR_STATE_HANDED_OFF && a == RIR_STATE_OWNED))
            return RIR_STATE_HANDOFF_PENDING;
    }

    if (kind == RIR_RESOURCE_ZONE_HANDLE || kind == RIR_RESOURCE_WORLD_HANDLE) {
        if ((a == RIR_STATE_OWNED || a == RIR_STATE_BORROWED_READ || a == RIR_STATE_BORROWED_WRITE)
            && (b == RIR_STATE_OWNED || b == RIR_STATE_BORROWED_READ || b == RIR_STATE_BORROWED_WRITE)) {
            if (a == RIR_STATE_BORROWED_WRITE || b == RIR_STATE_BORROWED_WRITE)
                return RIR_STATE_BORROWED_WRITE;
            if (a == RIR_STATE_BORROWED_READ || b == RIR_STATE_BORROWED_READ)
                return RIR_STATE_BORROWED_READ;
            return RIR_STATE_OWNED;
        }
        if ((a == RIR_STATE_SYNCED || a == RIR_STATE_DIRTY)
            && (b == RIR_STATE_SYNCED || b == RIR_STATE_DIRTY))
            return RIR_STATE_DIRTY;
    } else {
        if ((a == RIR_STATE_DETACHED || a == RIR_STATE_SYNCED || a == RIR_STATE_DIRTY)
            && (b == RIR_STATE_DETACHED || b == RIR_STATE_SYNCED || b == RIR_STATE_DIRTY)) {
            if (a == RIR_STATE_DETACHED && b == RIR_STATE_DETACHED)
                return RIR_STATE_DETACHED;
            if (a == RIR_STATE_SYNCED && b == RIR_STATE_SYNCED)
                return RIR_STATE_SYNCED;
            return RIR_STATE_DIRTY;
        }
    }

    if (conflict != NULL)
        *conflict = true;
    return RIR_STATE_INVALID;
}

RIRResourceState
rir_merge_states_for_kind(RIRResourceKind kind,
                          RIRResourceState a,
                          RIRResourceState b,
                          bool *conflict)
{
    if (rir_is_authority_kind(kind)) {
        if (a == b)
            return a;
        if (a == RIR_STATE_UNINIT)
            return b;
        if (b == RIR_STATE_UNINIT)
            return a;
        if (a == RIR_STATE_AUTHORITY_LOST || b == RIR_STATE_AUTHORITY_LOST)
            return RIR_STATE_AUTHORITY_LOST;
        if (a == RIR_STATE_AUTHORIZED && b == RIR_STATE_AUTHORIZED)
            return RIR_STATE_AUTHORIZED;
        if ((a == RIR_STATE_AUTHORIZED && b == RIR_STATE_AUTHORITY_LOST)
            || (b == RIR_STATE_AUTHORIZED && a == RIR_STATE_AUTHORITY_LOST))
            return RIR_STATE_AUTHORITY_LOST;
        if (conflict != NULL)
            *conflict = true;
        return RIR_STATE_INVALID;
    }
    if (rir_is_projection_kind(kind)) {
        if (a == b)
            return a;
        if (a == RIR_STATE_UNINIT)
            return b;
        if (b == RIR_STATE_UNINIT)
            return a;
        if (a == RIR_STATE_DETACHED || b == RIR_STATE_DETACHED) {
            if ((a == RIR_STATE_DETACHED
                 || a == RIR_STATE_SYNCED
                 || a == RIR_STATE_DIRTY
                 || a == RIR_STATE_PUBLISHED
                 || a == RIR_STATE_STALE)
                && (b == RIR_STATE_DETACHED
                    || b == RIR_STATE_SYNCED
                    || b == RIR_STATE_DIRTY
                    || b == RIR_STATE_PUBLISHED
                    || b == RIR_STATE_STALE)) {
                return RIR_STATE_DETACHED;
            }
        }
        if (a == RIR_STATE_STALE || b == RIR_STATE_STALE)
            return RIR_STATE_STALE;
        if (a == RIR_STATE_DETACHED && b == RIR_STATE_DETACHED)
            return RIR_STATE_DETACHED;
        if (a == RIR_STATE_PUBLISHED && b == RIR_STATE_PUBLISHED)
            return RIR_STATE_PUBLISHED;
        if (a == RIR_STATE_SYNCED && b == RIR_STATE_SYNCED)
            return RIR_STATE_SYNCED;
        if ((a == RIR_STATE_SYNCED || a == RIR_STATE_DIRTY || a == RIR_STATE_PUBLISHED || a == RIR_STATE_DETACHED)
            && (b == RIR_STATE_SYNCED || b == RIR_STATE_DIRTY || b == RIR_STATE_PUBLISHED || b == RIR_STATE_DETACHED))
            return RIR_STATE_STALE;
        if (conflict != NULL)
            *conflict = true;
        return RIR_STATE_INVALID;
    }
    if (rir_is_handle_kind(kind))
        return rir_merge_handle_states(kind, a, b, conflict);
    if (a == b)
        return a;
    if (a == RIR_STATE_UNINIT)
        return b;
    if (b == RIR_STATE_UNINIT)
        return a;
    if (a == RIR_STATE_INVALID || b == RIR_STATE_INVALID) {
        if (conflict != NULL)
            *conflict = true;
        return RIR_STATE_INVALID;
    }

    if ((a == RIR_STATE_RELEASED || a == RIR_STATE_MOVED)
        && (b == RIR_STATE_RELEASED || b == RIR_STATE_MOVED)) {
        if (a == b)
            return a;
        if (conflict != NULL)
            *conflict = true;
        return RIR_STATE_INVALID;
    }
    if ((a == RIR_STATE_RELEASED || a == RIR_STATE_MOVED)
        || (b == RIR_STATE_RELEASED || b == RIR_STATE_MOVED)) {
        if (conflict != NULL)
            *conflict = true;
        return RIR_STATE_INVALID;
    }

    if ((a == RIR_STATE_SYNCED || a == RIR_STATE_DIRTY || a == RIR_STATE_PUBLISHED || a == RIR_STATE_DETACHED)
        && (b == RIR_STATE_SYNCED || b == RIR_STATE_DIRTY || b == RIR_STATE_PUBLISHED || b == RIR_STATE_DETACHED))
        return RIR_STATE_DIRTY;

    if ((a == RIR_STATE_OWNED || a == RIR_STATE_BORROWED_READ || a == RIR_STATE_BORROWED_WRITE)
        && (b == RIR_STATE_OWNED || b == RIR_STATE_BORROWED_READ || b == RIR_STATE_BORROWED_WRITE)) {
        if (a == RIR_STATE_BORROWED_WRITE || b == RIR_STATE_BORROWED_WRITE)
            return RIR_STATE_BORROWED_WRITE;
        if (a == RIR_STATE_BORROWED_READ || b == RIR_STATE_BORROWED_READ)
            return RIR_STATE_BORROWED_READ;
        return RIR_STATE_OWNED;
    }

    if (a == RIR_STATE_REMOTE_PENDING && b == RIR_STATE_REMOTE_PENDING)
        return RIR_STATE_REMOTE_PENDING;
    if (a == RIR_STATE_MEASURED && b == RIR_STATE_MEASURED)
        return RIR_STATE_MEASURED;

    if (conflict != NULL)
        *conflict = true;
    return RIR_STATE_INVALID;
}
