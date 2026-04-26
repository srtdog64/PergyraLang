#ifndef PGY_RIR_NAMES_H
#define PGY_RIR_NAMES_H

/* Public RIR vocabulary names used by validation and dump surfaces. */

const char *
rir_scope_kind_name(RIRScopeKind kind)
{
    switch (kind) {
        case RIR_SCOPE_FUNCTION: return "function";
        case RIR_SCOPE_METHOD: return "method";
        case RIR_SCOPE_INTENT: return "intent";
        case RIR_SCOPE_ZONE: return "zone";
        case RIR_SCOPE_RELATION: return "relation";
        case RIR_SCOPE_EFFECT: return "effect";
        case RIR_SCOPE_WORLD: return "world";
        default: return "unknown";
    }
}

const char *
rir_fact_kind_name(RIRFactKind kind)
{
    switch (kind) {
        case RIR_FACT_RESOURCE: return "resource";
        case RIR_FACT_PROJECTION: return "projection";
        case RIR_FACT_AUTHORITY: return "authority";
        case RIR_FACT_CAPABILITY: return "capability";
        case RIR_FACT_INTENT_POLICY: return "intent-policy";
        default: return "unknown";
    }
}

const char *
rir_resource_kind_name(RIRResourceKind kind)
{
    switch (kind) {
        case RIR_RESOURCE_UNKNOWN: return "unknown";
        case RIR_RESOURCE_LOCAL_SLOT: return "LocalSlot";
        case RIR_RESOURCE_SECURE_SLOT: return "SecureSlot";
        case RIR_RESOURCE_DEVICE_SLOT: return "DeviceSlot";
        case RIR_RESOURCE_AUTHORITY_HANDLE: return "AuthorityHandle";
        case RIR_RESOURCE_CAPABILITY_TOKEN: return "CapabilityToken";
        case RIR_RESOURCE_SUBJECT_SLOT: return "SubjectSlot";
        case RIR_RESOURCE_OBJECT_SLOT: return "ObjectSlot";
        case RIR_RESOURCE_TOBJECT_SLOT: return "TObjectSlot";
        case RIR_RESOURCE_VESSEL_SLOT: return "VesselSlot";
        case RIR_RESOURCE_QUBIT_HANDLE: return "QubitHandle";
        case RIR_RESOURCE_REMOTE_FUTURE_HANDLE: return "RemoteFutureHandle";
        case RIR_RESOURCE_PROJECTION_OBJECT: return "ProjectionObject";
        case RIR_RESOURCE_PROJECTION_TOBJECT: return "ProjectionTObject";
        case RIR_RESOURCE_EFFECT_INSTANCE: return "EffectInstance";
        case RIR_RESOURCE_RELATION_INSTANCE: return "RelationInstance";
        case RIR_RESOURCE_ZONE_HANDLE: return "ZoneHandle";
        case RIR_RESOURCE_WORLD_HANDLE: return "WorldHandle";
        default: return "unknown";
    }
}

const char *
rir_resource_state_name(RIRResourceState state)
{
    switch (state) {
        case RIR_STATE_UNINIT: return "Uninit";
        case RIR_STATE_OWNED: return "Owned";
        case RIR_STATE_BORROWED_READ: return "BorrowedRead";
        case RIR_STATE_BORROWED_WRITE: return "BorrowedWrite";
        case RIR_STATE_MOVED: return "Moved";
        case RIR_STATE_RELEASED: return "Released";
        case RIR_STATE_INVALID: return "Invalid";
        case RIR_STATE_MEASURED: return "Measured";
        case RIR_STATE_REMOTE_PENDING: return "RemotePending";
        case RIR_STATE_AUTHORIZED: return "Authorized";
        case RIR_STATE_AUTHORITY_LOST: return "AuthorityLost";
        case RIR_STATE_SYNCED: return "Synced";
        case RIR_STATE_DIRTY: return "Dirty";
        case RIR_STATE_STALE: return "Stale";
        case RIR_STATE_DETACHED: return "Detached";
        case RIR_STATE_PUBLISHED: return "Published";
        case RIR_STATE_HANDOFF_PENDING: return "HandoffPending";
        case RIR_STATE_HANDED_OFF: return "HandedOff";
        case RIR_STATE_COMPENSATED: return "Compensated";
        default: return "unknown";
    }
}

const char *
rir_op_kind_name(RIROpKind kind)
{
    switch (kind) {
        case RIR_OP_CLAIM: return "Claim";
        case RIR_OP_READ: return "Read";
        case RIR_OP_WRITE: return "Write";
        case RIR_OP_RELEASE: return "Release";
        case RIR_OP_MOVE: return "Move";
        case RIR_OP_BORROW_READ: return "BorrowRead";
        case RIR_OP_BORROW_WRITE: return "BorrowWrite";
        case RIR_OP_PROJECT_REFRESH: return "ProjectRefresh";
        case RIR_OP_PROJECT_PUBLISH: return "ProjectPublish";
        case RIR_OP_ATTACH_EFFECT: return "AttachEffect";
        case RIR_OP_DETACH_EFFECT: return "DetachEffect";
        case RIR_OP_LINK_RELATION: return "LinkRelation";
        case RIR_OP_UNLINK_RELATION: return "UnlinkRelation";
        case RIR_OP_AUTHORIZE: return "Authorize";
        case RIR_OP_AWAIT_REMOTE: return "AwaitRemote";
        case RIR_OP_COMMIT_INTENT: return "CommitIntent";
        case RIR_OP_ABORT_INTENT: return "AbortIntent";
        case RIR_OP_COMPENSATE_INTENT_STEP: return "CompensateIntentStep";
        default: return "unknown";
    }
}

#endif /* PGY_RIR_NAMES_H */
