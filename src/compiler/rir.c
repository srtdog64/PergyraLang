#include "rir.h"

#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

#include "hir.h"
#include "../common/string_compat.h"

static bool rir_walk_node(RIRScope *scope, ASTNode *node);
static bool rir_walk_block_node(RIRScope *scope, ASTNode *node);
static bool rir_normalize_scope(RIRScope *scope);
static bool add_named_resource_fact(RIRScope *scope,
                                    const char *name,
                                    const char *type_name_value,
                                    RIRResourceKind kind,
                                    RIRResourceState state,
                                    ASTNode *ast);
static ASTNode *g_rir_program_root = NULL;

static bool
append_scope(RIRProgram *rir, RIRScope scope)
{
    RIRScope *grown = realloc(rir->scopes, (rir->scope_count + 1) * sizeof(RIRScope));
    if (grown == NULL)
        return false;
    grown[rir->scope_count] = scope;
    rir->scopes = grown;
    rir->scope_count++;
    return true;
}

static char *
rir_strdup_fmt(const char *fmt, ...)
{
    va_list args;
    va_list copy;
    int length;
    char *result;

    va_start(args, fmt);
    va_copy(copy, args);
    length = vsnprintf(NULL, 0, fmt, copy);
    va_end(copy);
    if (length < 0) {
        va_end(args);
        return NULL;
    }

    result = malloc((size_t)length + 1);
    if (result == NULL) {
        va_end(args);
        return NULL;
    }
    vsnprintf(result, (size_t)length + 1, fmt, args);
    va_end(args);
    return result;
}

static bool
scope_add_fact(RIRScope *scope, RIRFact fact)
{
    RIRFact *grown = realloc(scope->facts, (scope->fact_count + 1) * sizeof(RIRFact));
    if (grown == NULL)
        return false;
    grown[scope->fact_count] = fact;
    scope->facts = grown;
    scope->fact_count++;
    return true;
}

static bool
scope_add_op(RIRScope *scope, RIROp op)
{
    RIROp *grown = realloc(scope->ops, (scope->op_count + 1) * sizeof(RIROp));
    if (grown == NULL)
        return false;
    grown[scope->op_count] = op;
    scope->ops = grown;
    scope->op_count++;
    return true;
}

static void
rir_free_flow_blocks(RIRScope *scope)
{
    if (scope == NULL)
        return;
    for (size_t i = 0; i < scope->flow_block_count; i++)
        free(scope->flow_blocks[i].facts);
    free(scope->flow_blocks);
    scope->flow_blocks = NULL;
    scope->flow_block_count = 0;
    scope->has_flow_sensitive_merge = false;
}

static bool
scope_add_state_summary(RIRScope *scope, RIRStateSummary summary)
{
    RIRStateSummary *grown = realloc(scope->state_summaries,
                                     (scope->state_summary_count + 1) * sizeof(RIRStateSummary));
    if (grown == NULL)
        return false;
    grown[scope->state_summary_count] = summary;
    scope->state_summaries = grown;
    scope->state_summary_count++;
    return true;
}

static RIRStateSummary *
scope_find_state_summary(RIRScope *scope, const char *name)
{
    if (scope == NULL || name == NULL)
        return NULL;

    for (size_t i = 0; i < scope->state_summary_count; i++) {
        if (scope->state_summaries[i].name != NULL
            && strcmp(scope->state_summaries[i].name, name) == 0) {
            return &scope->state_summaries[i];
        }
    }
    return NULL;
}

static bool
scope_ensure_state_summary(RIRScope *scope, const RIRFact *fact)
{
    RIRStateSummary summary;
    if (scope == NULL || fact == NULL || fact->name == NULL)
        return true;
    if (fact->kind != RIR_FACT_RESOURCE && fact->kind != RIR_FACT_PROJECTION)
        return true;
    if (scope_find_state_summary(scope, fact->name) != NULL)
        return true;

    memset(&summary, 0, sizeof(summary));
    summary.name = fact->name;
    summary.origin_kind = fact->kind;
    summary.resource_kind = fact->resource_kind;
    summary.initial_state = fact->state;
    summary.final_state = fact->state;
    summary.ast = fact->ast;
    return scope_add_state_summary(scope, summary);
}

static const char *
type_name(ASTNode *type_node)
{
    if (type_node == NULL)
        return NULL;
    if (type_node->type == AST_TYPE)
        return type_node->data.type.name;
    return NULL;
}

static RIRResourceKind
rir_nominal_kind_from_name(const char *name)
{
    if (g_rir_program_root == NULL || name == NULL || g_rir_program_root->type != AST_PROGRAM)
        return RIR_RESOURCE_UNKNOWN;

    for (size_t i = 0; i < g_rir_program_root->data.program.count; i++) {
        ASTNode *node = g_rir_program_root->data.program.statements[i];
        if (node == NULL)
            continue;
        switch (node->type) {
            case AST_RELATION_DECL:
                if (node->data.relation_decl.name != NULL
                    && strcmp(node->data.relation_decl.name, name) == 0)
                    return RIR_RESOURCE_RELATION_INSTANCE;
                break;
            case AST_EFFECT_DECL:
                if (node->data.effect_decl.name != NULL
                    && strcmp(node->data.effect_decl.name, name) == 0)
                    return RIR_RESOURCE_EFFECT_INSTANCE;
                break;
            case AST_ZONE_DECL:
                if (node->data.zone_decl.name != NULL
                    && strcmp(node->data.zone_decl.name, name) == 0)
                    return RIR_RESOURCE_ZONE_HANDLE;
                break;
            case AST_WORLD_DECL:
                if (node->data.world_decl.name != NULL
                    && strcmp(node->data.world_decl.name, name) == 0)
                    return RIR_RESOURCE_WORLD_HANDLE;
                break;
            default:
                break;
        }
    }
    return RIR_RESOURCE_UNKNOWN;
}

static const char *
expr_name(ASTNode *node)
{
    if (node == NULL)
        return NULL;

    switch (node->type) {
        case AST_IDENTIFIER:
            return node->data.identifier.name;
        case AST_MEMBER_ACCESS:
            return node->data.member.name;
        case AST_TYPE:
            return node->data.type.name;
        default:
            return NULL;
    }
}

static const char *
call_name(ASTNode *call)
{
    if (call == NULL || call->type != AST_CALL)
        return NULL;
    return expr_name(call->data.call.callee);
}

static RIRResourceKind
resource_kind_from_type(ASTNode *type_node)
{
    const char *name = type_name(type_node);
    if (name == NULL)
        return RIR_RESOURCE_UNKNOWN;
    if (strcmp(name, "Slot") == 0)
        return RIR_RESOURCE_LOCAL_SLOT;
    if (strcmp(name, "SecureSlot") == 0)
        return RIR_RESOURCE_SECURE_SLOT;
    if (strcmp(name, "DeviceSlot") == 0)
        return RIR_RESOURCE_DEVICE_SLOT;
    if (strcmp(name, "QubitSlot") == 0)
        return RIR_RESOURCE_QUBIT_HANDLE;
    if (strcmp(name, "RemoteFuture") == 0)
        return RIR_RESOURCE_REMOTE_FUTURE_HANDLE;
    return rir_nominal_kind_from_name(name);
}

static RIRResourceState
rir_default_state_for_kind(RIRResourceKind kind)
{
    switch (kind) {
        case RIR_RESOURCE_EFFECT_INSTANCE:
        case RIR_RESOURCE_RELATION_INSTANCE:
            return RIR_STATE_DETACHED;
        case RIR_RESOURCE_ZONE_HANDLE:
        case RIR_RESOURCE_WORLD_HANDLE:
            return RIR_STATE_OWNED;
        default:
            return RIR_STATE_UNINIT;
    }
}

static const char *
rollback_policy_name(IntentRollbackPolicy policy)
{
    switch (policy) {
        case INTENT_ROLLBACK_FULL: return "full";
        case INTENT_ROLLBACK_CURRENT: return "current";
        case INTENT_ROLLBACK_NONE: return "none";
        default: return "unknown";
    }
}

static bool
add_resource_fact(RIRScope *scope,
                  const char *name,
                  ASTNode *type_node,
                  RIRResourceState state,
                  ASTNode *ast)
{
    RIRResourceKind kind = resource_kind_from_type(type_node);
    if (kind == RIR_RESOURCE_UNKNOWN)
        return true;

    RIRFact fact;
    memset(&fact, 0, sizeof(fact));
    fact.kind = RIR_FACT_RESOURCE;
    fact.name = name;
    fact.arg0 = type_name(type_node);
    fact.resource_kind = kind;
    fact.state = state;
    fact.ast = ast;
    return scope_add_fact(scope, fact);
}

static bool
add_param_resource_fact(RIRScope *scope, const char *name, ASTNode *type_node, ASTNode *ast)
{
    RIRResourceKind kind = resource_kind_from_type(type_node);
    if (kind == RIR_RESOURCE_UNKNOWN)
        return true;
    return add_named_resource_fact(scope,
                                   name,
                                   type_name(type_node),
                                   kind,
                                   rir_default_state_for_kind(kind),
                                   ast);
}

static bool
add_projection_fact(RIRScope *scope,
                    const char *target,
                    const char *source,
                    const char *mode,
                    RIRResourceState state,
                    RIRResourceKind kind,
                    ASTNode *ast)
{
    RIRFact fact;
    memset(&fact, 0, sizeof(fact));
    fact.kind = RIR_FACT_PROJECTION;
    fact.name = target;
    fact.arg0 = source;
    fact.arg1 = mode;
    fact.resource_kind = kind;
    fact.state = state;
    fact.ast = ast;
    return scope_add_fact(scope, fact);
}

static bool
add_named_resource_fact(RIRScope *scope,
                        const char *name,
                        const char *type_name_value,
                        RIRResourceKind kind,
                        RIRResourceState state,
                        ASTNode *ast)
{
    RIRFact fact;
    memset(&fact, 0, sizeof(fact));
    fact.kind = RIR_FACT_RESOURCE;
    fact.name = name;
    fact.arg0 = type_name_value;
    fact.resource_kind = kind;
    fact.state = state;
    fact.ast = ast;
    return scope_add_fact(scope, fact);
}

static bool
add_authority_fact(RIRScope *scope, const char *actor, const char *ability, ASTNode *ast)
{
    RIRFact fact;
    memset(&fact, 0, sizeof(fact));
    fact.kind = ability != NULL ? RIR_FACT_CAPABILITY : RIR_FACT_AUTHORITY;
    fact.name = actor;
    fact.arg0 = ability;
    fact.ast = ast;
    return scope_add_fact(scope, fact);
}

static bool
add_intent_policy_fact(RIRScope *scope, const char *key, const char *value, ASTNode *ast)
{
    RIRFact fact;
    memset(&fact, 0, sizeof(fact));
    fact.kind = RIR_FACT_INTENT_POLICY;
    fact.name = key;
    fact.arg0 = value;
    fact.ast = ast;
    return scope_add_fact(scope, fact);
}

static bool
add_op(RIRScope *scope,
       RIROpKind kind,
       const char *subject,
       const char *arg0,
       const char *arg1,
       ASTNode *ast)
{
    RIROp op;
    memset(&op, 0, sizeof(op));
    op.kind = kind;
    op.subject = subject;
    op.arg0 = arg0;
    op.arg1 = arg1;
    op.ast = ast;
    return scope_add_op(scope, op);
}

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

static void
rir_apply_op_to_summary(RIRScope *scope, RIRStateSummary *summary, const RIROp *op)
{
    if (summary == NULL || op == NULL)
        return;

    summary->last_op_name = rir_op_kind_name(op->kind);

    switch (op->kind) {
        case RIR_OP_CLAIM:
            summary->final_state = RIR_STATE_OWNED;
            return;

        case RIR_OP_READ:
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
            if (summary->final_state == RIR_STATE_OWNED) {
                summary->final_state = RIR_STATE_BORROWED_READ;
                return;
            }
            rir_state_mark_error(scope, summary, op->kind);
            return;

        case RIR_OP_BORROW_WRITE:
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

        case RIR_OP_AWAIT_REMOTE:
            if (summary->resource_kind == RIR_RESOURCE_REMOTE_FUTURE_HANDLE)
                summary->final_state = RIR_STATE_REMOTE_PENDING;
            else
                rir_state_mark_error(scope, summary, op->kind);
            return;

        default:
            return;
    }
}

static void
rir_apply_op_to_state(RIRResourceKind resource_kind,
                      RIRResourceState *state,
                      bool *had_error,
                      RIROpKind op_kind)
{
    RIRResourceState current;
    if (state == NULL)
        return;
    current = *state;
    switch (op_kind) {
        case RIR_OP_CLAIM:
            *state = RIR_STATE_OWNED;
            return;
        case RIR_OP_READ:
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
            if (current == RIR_STATE_OWNED) {
                *state = RIR_STATE_BORROWED_READ;
                return;
            }
            *state = RIR_STATE_INVALID;
            if (had_error != NULL)
                *had_error = true;
            return;
        case RIR_OP_BORROW_WRITE:
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
        case RIR_OP_AWAIT_REMOTE:
            if (resource_kind == RIR_RESOURCE_REMOTE_FUTURE_HANDLE) {
                *state = RIR_STATE_REMOTE_PENDING;
                return;
            }
            *state = RIR_STATE_INVALID;
            if (had_error != NULL)
                *had_error = true;
            return;
        default:
            return;
    }
}

static bool
rir_is_handle_kind(RIRResourceKind kind)
{
    return kind == RIR_RESOURCE_RELATION_INSTANCE
           || kind == RIR_RESOURCE_EFFECT_INSTANCE
           || kind == RIR_RESOURCE_ZONE_HANDLE
           || kind == RIR_RESOURCE_WORLD_HANDLE;
}

static bool
rir_state_changed(RIRResourceState a, RIRResourceState b)
{
    return a != b;
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

static RIRResourceState
rir_merge_states_for_kind(RIRResourceKind kind,
                          RIRResourceState a,
                          RIRResourceState b,
                          bool *conflict)
{
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

static bool
rir_normalize_scope(RIRScope *scope)
{
    if (scope == NULL)
        return true;

    free(scope->state_summaries);
    scope->state_summaries = NULL;
    scope->state_summary_count = 0;
    scope->has_state_errors = false;

    for (size_t i = 0; i < scope->fact_count; i++) {
        if (!scope_ensure_state_summary(scope, &scope->facts[i]))
            return false;
    }

    for (size_t i = 0; i < scope->op_count; i++) {
        RIROp *op = &scope->ops[i];
        RIRStateSummary *summary = scope_find_state_summary(scope, op->subject);
        if (summary == NULL)
            continue;
        rir_apply_op_to_summary(scope, summary, op);
    }

    return true;
}

static RIRScopeKind
rir_scope_kind_from_hir(const HIRRoutine *routine)
{
    if (routine == NULL)
        return RIR_SCOPE_FUNCTION;
    if (routine->kind == HIR_TOPLEVEL_INTENT)
        return RIR_SCOPE_INTENT;
    if (routine->is_hosted || routine->is_action_like)
        return RIR_SCOPE_METHOD;
    return RIR_SCOPE_FUNCTION;
}

static RIRScope *
rir_find_matching_scope(RIRProgram *rir, const HIRRoutine *routine)
{
    RIRScopeKind wanted_kind;
    if (rir == NULL || routine == NULL || routine->name == NULL)
        return NULL;
    wanted_kind = rir_scope_kind_from_hir(routine);
    for (size_t i = 0; i < rir->scope_count; i++) {
        RIRScope *scope = &rir->scopes[i];
        if (scope->kind == wanted_kind
            && scope->name != NULL
            && strcmp(scope->name, routine->name) == 0) {
            return scope;
        }
    }
    return NULL;
}

static bool
rir_collect_block_ops(const HIRBasicBlock *block, RIROp **ops_out, size_t *op_count_out)
{
    RIRScope temp_scope;
    if (ops_out == NULL || op_count_out == NULL)
        return false;
    *ops_out = NULL;
    *op_count_out = 0;
    if (block == NULL)
        return true;
    memset(&temp_scope, 0, sizeof(temp_scope));
    for (size_t i = 0; i < block->statement_count; i++) {
        if (!rir_walk_block_node(&temp_scope, block->statements[i])) {
            free(temp_scope.ops);
            return false;
        }
    }
    if (!rir_walk_block_node(&temp_scope, block->terminator_condition)
        || !rir_walk_block_node(&temp_scope, block->terminator_value)) {
        free(temp_scope.ops);
        return false;
    }
    *ops_out = temp_scope.ops;
    *op_count_out = temp_scope.op_count;
    return true;
}

static bool
rir_prepare_flow_blocks(RIRScope *scope, const HIRRoutine *hir_routine)
{
    if (scope == NULL || hir_routine == NULL || !hir_routine->has_cfg)
        return true;
    rir_free_flow_blocks(scope);
    scope->flow_blocks = calloc(hir_routine->cfg.block_count, sizeof(RIRFlowBlock));
    if (scope->flow_blocks == NULL)
        return false;
    scope->flow_block_count = hir_routine->cfg.block_count;
    for (size_t i = 0; i < hir_routine->cfg.block_count; i++) {
        RIRFlowBlock *flow = &scope->flow_blocks[i];
        const HIRBasicBlock *hir_block = &hir_routine->cfg.blocks[i];
        flow->block_id = i;
        flow->is_reachable = hir_block->is_reachable;
        flow->is_join = hir_block->predecessor_count > 1;
        flow->fact_count = scope->state_summary_count;
        if (flow->fact_count == 0)
            continue;
        flow->facts = calloc(flow->fact_count, sizeof(RIRFlowFact));
        if (flow->facts == NULL)
            return false;
        for (size_t j = 0; j < flow->fact_count; j++) {
            flow->facts[j].name = scope->state_summaries[j].name;
            flow->facts[j].entry_state = RIR_STATE_UNINIT;
            flow->facts[j].exit_state = RIR_STATE_UNINIT;
            flow->facts[j].merged_from_join = false;
            flow->facts[j].widened_by_loop = false;
            flow->facts[j].entry_conflict = false;
            flow->facts[j].has_merge_conflict = false;
        }
    }
    return true;
}

static bool
rir_enrich_scope_with_hir_flow(RIRScope *scope, const HIRRoutine *hir_routine)
{
    RIROp **block_ops = NULL;
    size_t *block_op_counts = NULL;
    bool changed;
    size_t limit;

    if (scope == NULL || hir_routine == NULL || !hir_routine->has_cfg || scope->state_summary_count == 0)
        return true;
    if (!rir_prepare_flow_blocks(scope, hir_routine))
        return false;

    block_ops = calloc(hir_routine->cfg.block_count, sizeof(RIROp *));
    block_op_counts = calloc(hir_routine->cfg.block_count, sizeof(size_t));
    if (block_ops == NULL || block_op_counts == NULL)
        goto oom;

    for (size_t i = 0; i < hir_routine->cfg.block_count; i++) {
        if (!rir_collect_block_ops(&hir_routine->cfg.blocks[i], &block_ops[i], &block_op_counts[i]))
            goto oom;
    }

    limit = hir_routine->cfg.block_count * 4 + 1;
    do {
        changed = false;
        for (size_t order = 0; order < hir_routine->cfg.block_count; order++) {
            const HIRBasicBlock *hir_block = NULL;
            RIRFlowBlock *flow = NULL;
            size_t block_id = SIZE_MAX;

            for (size_t i = 0; i < hir_routine->cfg.block_count; i++) {
                if (hir_routine->cfg.blocks[i].rpo_index == order) {
                    block_id = i;
                    break;
                }
            }
            if (block_id == SIZE_MAX)
                continue;
            hir_block = &hir_routine->cfg.blocks[block_id];
            flow = &scope->flow_blocks[block_id];
            if (!hir_block->is_reachable || flow->facts == NULL)
                continue;

            for (size_t fact_i = 0; fact_i < scope->state_summary_count; fact_i++) {
                RIRResourceState merged = RIR_STATE_UNINIT;
                bool merge_conflict = false;
                bool reachable_pred = false;

                if (block_id == hir_routine->cfg.entry_block) {
                    merged = scope->state_summaries[fact_i].initial_state;
                } else {
                    for (size_t p = 0; p < hir_block->predecessor_count; p++) {
                        size_t pred = hir_block->predecessors[p];
                        if (pred >= scope->flow_block_count || !scope->flow_blocks[pred].is_reachable)
                            continue;
                        merged = rir_merge_states_for_kind(scope->state_summaries[fact_i].resource_kind,
                                                           merged,
                                                           scope->flow_blocks[pred].facts[fact_i].exit_state,
                                                           &merge_conflict);
                        reachable_pred = true;
                    }
                    if (!reachable_pred)
                        merged = scope->state_summaries[fact_i].initial_state;
                }

                if (rir_state_changed(flow->facts[fact_i].entry_state, merged)
                    || flow->facts[fact_i].entry_conflict != merge_conflict
                    || flow->facts[fact_i].widened_by_loop != (hir_block->is_loop_header
                                                               && hir_block->predecessor_count > 1)
                    || flow->facts[fact_i].merged_from_join != (hir_block->predecessor_count > 1)) {
                    changed = true;
                }
                flow->facts[fact_i].entry_state = merged;
                flow->facts[fact_i].merged_from_join = hir_block->predecessor_count > 1;
                flow->facts[fact_i].widened_by_loop = hir_block->is_loop_header
                                                      && hir_block->predecessor_count > 1;
                flow->facts[fact_i].entry_conflict = merge_conflict;
                if (flow->facts[fact_i].merged_from_join)
                    scope->has_flow_sensitive_merge = true;
                if (merge_conflict)
                    scope->has_state_errors = true;

                {
                    RIRResourceState exit_state = merged;
                    bool had_error = merge_conflict;
                    for (size_t op_i = 0; op_i < block_op_counts[block_id]; op_i++) {
                        const RIROp *op = &block_ops[block_id][op_i];
                        if (op->subject == NULL
                            || strcmp(op->subject, scope->state_summaries[fact_i].name) != 0)
                            continue;
                        rir_apply_op_to_state(scope->state_summaries[fact_i].resource_kind,
                                              &exit_state,
                                              &had_error,
                                              op->kind);
                    }
                    if (rir_state_changed(flow->facts[fact_i].exit_state, exit_state)
                        || flow->facts[fact_i].has_merge_conflict != had_error) {
                        changed = true;
                    }
                    flow->facts[fact_i].exit_state = exit_state;
                    flow->facts[fact_i].has_merge_conflict = had_error;
                }
            }
        }
    } while (changed && --limit > 0);

    if (limit == 0)
        scope->has_state_errors = true;

    for (size_t i = 0; i < hir_routine->cfg.block_count; i++)
        free(block_ops[i]);
    free(block_ops);
    free(block_op_counts);
    return true;

oom:
    if (block_ops != NULL) {
        for (size_t i = 0; i < hir_routine->cfg.block_count; i++)
            free(block_ops[i]);
    }
    free(block_ops);
    free(block_op_counts);
    rir_free_flow_blocks(scope);
    return false;
}

static bool
rir_walk_call(RIRScope *scope, ASTNode *node)
{
    const char *name;
    if (node == NULL || node->type != AST_CALL)
        return true;

    name = call_name(node);
    if (name != NULL) {
        ASTNode **args = node->data.call.arguments;
        size_t argc = node->data.call.arg_count;

        if (strcmp(name, "Read") == 0 && argc >= 1) {
            if (!add_op(scope, RIR_OP_READ, expr_name(args[0]), NULL, NULL, node))
                return false;
        } else if (strcmp(name, "Write") == 0 && argc >= 1) {
            if (!add_op(scope, RIR_OP_WRITE, expr_name(args[0]), argc >= 2 ? expr_name(args[1]) : NULL, NULL, node))
                return false;
        } else if (strcmp(name, "Release") == 0 && argc >= 1) {
            if (!add_op(scope, RIR_OP_RELEASE, expr_name(args[0]), NULL, NULL, node))
                return false;
        } else if (strcmp(name, "Move") == 0 && argc >= 1) {
            if (!add_op(scope, RIR_OP_MOVE, expr_name(args[0]), NULL, NULL, node))
                return false;
        } else if (strcmp(name, "ToObject") == 0 && argc >= 2) {
            if (!add_op(scope, RIR_OP_PROJECT_REFRESH, expr_name(args[1]), expr_name(args[0]), NULL, node))
                return false;
        } else if (strcmp(name, "ToDto") == 0 && argc >= 2) {
            if (!add_op(scope, RIR_OP_PROJECT_PUBLISH, expr_name(args[1]), expr_name(args[0]), NULL, node))
                return false;
        }
    }

    if (!rir_walk_node(scope, node->data.call.callee))
        return false;
    for (size_t i = 0; i < node->data.call.arg_count; i++) {
        if (!rir_walk_node(scope, node->data.call.arguments[i]))
            return false;
    }
    return true;
}

static bool
rir_walk_node(RIRScope *scope, ASTNode *node)
{
    if (scope == NULL || node == NULL)
        return true;

    switch (node->type) {
        case AST_BLOCK:
            for (size_t i = 0; i < node->data.block.count; i++) {
                if (!rir_walk_node(scope, node->data.block.statements[i]))
                    return false;
            }
            return true;

        case AST_LET_DECL: {
            const char *init_name = NULL;
            RIRResourceState state = RIR_STATE_UNINIT;
            if (node->data.let_decl.initializer != NULL
                && node->data.let_decl.initializer->type == AST_CALL) {
                init_name = call_name(node->data.let_decl.initializer);
            }

            if (init_name != NULL) {
                if (strcmp(init_name, "ClaimSlot") == 0) {
                    state = RIR_STATE_OWNED;
                    if (!add_op(scope, RIR_OP_CLAIM, node->data.let_decl.name, "Slot", NULL,
                                node->data.let_decl.initializer))
                        return false;
                } else if (strcmp(init_name, "ClaimSecureSlot") == 0) {
                    state = RIR_STATE_OWNED;
                    if (!add_op(scope, RIR_OP_CLAIM, node->data.let_decl.name, "SecureSlot", NULL,
                                node->data.let_decl.initializer))
                        return false;
                } else if (strcmp(init_name, "ClaimDeviceSlot") == 0) {
                    state = RIR_STATE_OWNED;
                    if (!add_op(scope, RIR_OP_CLAIM, node->data.let_decl.name, "DeviceSlot", NULL,
                                node->data.let_decl.initializer))
                        return false;
                } else if (strcmp(init_name, "ViewRead") == 0
                           && node->data.let_decl.initializer->data.call.arg_count >= 1) {
                    if (!add_op(scope,
                                RIR_OP_BORROW_READ,
                                expr_name(node->data.let_decl.initializer->data.call.arguments[0]),
                                node->data.let_decl.name,
                                NULL,
                                node->data.let_decl.initializer))
                        return false;
                } else if (strcmp(init_name, "ViewWrite") == 0
                           && node->data.let_decl.initializer->data.call.arg_count >= 1) {
                    if (!add_op(scope,
                                RIR_OP_BORROW_WRITE,
                                expr_name(node->data.let_decl.initializer->data.call.arguments[0]),
                                node->data.let_decl.name,
                                NULL,
                                node->data.let_decl.initializer))
                        return false;
                }
            }

            if (!add_resource_fact(scope,
                                   node->data.let_decl.name,
                                   node->data.let_decl.type,
                                   state,
                                   node))
                return false;
            return rir_walk_node(scope, node->data.let_decl.initializer);
        }

        case AST_WITH_STMT:
            if (!add_resource_fact(scope,
                                   node->data.with_stmt.alias,
                                   node->data.with_stmt.slot_type,
                                   RIR_STATE_OWNED,
                                   node))
                return false;
            if (!add_op(scope, RIR_OP_CLAIM,
                        node->data.with_stmt.alias,
                        node->data.with_stmt.is_secure ? "SecureSlot" : "Slot",
                        NULL,
                        node))
                return false;
            return rir_walk_node(scope, node->data.with_stmt.body);

        case AST_ASSIGNMENT:
            if (!rir_walk_node(scope, node->data.assignment.target))
                return false;
            return rir_walk_node(scope, node->data.assignment.value);

        case AST_AWAIT_EXPR:
            if (!add_op(scope, RIR_OP_AWAIT_REMOTE,
                        expr_name(node->data.await_expr.expression),
                        NULL, NULL, node))
                return false;
            return rir_walk_node(scope, node->data.await_expr.expression);

        case AST_CALL:
            return rir_walk_call(scope, node);

        case AST_IF_STMT:
            if (!rir_walk_node(scope, node->data.if_stmt.condition))
                return false;
            if (!rir_walk_node(scope, node->data.if_stmt.then_branch))
                return false;
            return rir_walk_node(scope, node->data.if_stmt.else_branch);

        case AST_FOR_LOOP:
            if (!rir_walk_node(scope, node->data.for_loop.range_start))
                return false;
            if (!rir_walk_node(scope, node->data.for_loop.range_end))
                return false;
            if (!rir_walk_node(scope, node->data.for_loop.iterable))
                return false;
            return rir_walk_node(scope, node->data.for_loop.body);

        case AST_WHILE_LOOP:
            if (!rir_walk_node(scope, node->data.while_loop.condition))
                return false;
            return rir_walk_node(scope, node->data.while_loop.body);

        case AST_RETURN:
            return rir_walk_node(scope, node->data.return_stmt.value);

        case AST_MATCH_STMT:
            if (!rir_walk_node(scope, node->data.match_stmt.subject))
                return false;
            for (size_t i = 0; i < node->data.match_stmt.case_count; i++) {
                if (!rir_walk_node(scope, node->data.match_stmt.cases[i]))
                    return false;
            }
            if (!rir_walk_node(scope, node->data.match_stmt.default_body))
                return false;
            return true;

        case AST_MATCH_CASE:
            if (!rir_walk_node(scope, node->data.match_case.pattern))
                return false;
            if (!rir_walk_node(scope, node->data.match_case.guard))
                return false;
            return rir_walk_node(scope, node->data.match_case.body);

        case AST_PARALLEL_BLOCK:
            for (size_t i = 0; i < node->data.parallel.task_count; i++) {
                if (!rir_walk_node(scope, node->data.parallel.tasks[i]))
                    return false;
            }
            return true;

        default:
            return true;
    }
}

static bool
rir_walk_block_node(RIRScope *scope, ASTNode *node)
{
    if (scope == NULL || node == NULL)
        return true;

    switch (node->type) {
        case AST_IF_STMT:
            return rir_walk_block_node(scope, node->data.if_stmt.condition);
        case AST_FOR_LOOP:
            return rir_walk_block_node(scope, node->data.for_loop.range_start)
                   && rir_walk_block_node(scope, node->data.for_loop.range_end)
                   && rir_walk_block_node(scope, node->data.for_loop.iterable);
        case AST_WHILE_LOOP:
            return rir_walk_block_node(scope, node->data.while_loop.condition);
        case AST_MATCH_STMT:
            if (!rir_walk_block_node(scope, node->data.match_stmt.subject))
                return false;
            for (size_t i = 0; i < node->data.match_stmt.case_count; i++) {
                ASTNode *match_case = node->data.match_stmt.cases[i];
                if (match_case == NULL)
                    continue;
                if (!rir_walk_block_node(scope, match_case->data.match_case.pattern)
                    || !rir_walk_block_node(scope, match_case->data.match_case.guard)) {
                    return false;
                }
            }
            return true;
        case AST_BLOCK:
            for (size_t i = 0; i < node->data.block.count; i++) {
                if (!rir_walk_block_node(scope, node->data.block.statements[i]))
                    return false;
            }
            return true;
        default:
            return rir_walk_node(scope, node);
    }
}

static bool
rir_collect_func_scope(RIRProgram *rir,
                       RIRScopeKind kind,
                       const char *owner_name,
                       ASTNode *func)
{
    RIRScope scope;
    memset(&scope, 0, sizeof(scope));
    scope.id = rir->scope_count;
    scope.kind = kind;
    scope.owner_name = owner_name;
    scope.name = func->data.func_decl.name;
    scope.ast = func;
    for (size_t i = 0; i < func->data.func_decl.param_count; i++) {
        FuncParam *param = func->data.func_decl.params[i];
        if (param == NULL)
            continue;
        if (!add_param_resource_fact(&scope, param->name, param->type, func))
            goto oom;
    }
    if (!rir_walk_node(&scope, func->data.func_decl.body)) {
oom:
        free(scope.facts);
        free(scope.ops);
        free(scope.state_summaries);
        return false;
    }
    if (!rir_normalize_scope(&scope)) {
        free(scope.facts);
        free(scope.ops);
        free(scope.state_summaries);
        return false;
    }
    return append_scope(rir, scope);
}

static bool
rir_collect_zone_like_scope(RIRProgram *rir, ASTNode *node, RIRScopeKind kind, const char *name)
{
    RIRScope scope;
    memset(&scope, 0, sizeof(scope));
    scope.id = rir->scope_count;
    scope.kind = kind;
    scope.name = name;
    scope.ast = node;

    ASTNode **refreshes = NULL;
    size_t refresh_count = 0;
    ASTNode **methods = NULL;
    size_t method_count = 0;

    if (node->type == AST_ZONE_DECL) {
        for (size_t i = 0; i < node->data.zone_decl.layer_slot_count; i++) {
            ASTNode *slot = node->data.zone_decl.layer_slots[i];
            if (!add_named_resource_fact(&scope,
                                         slot->data.zone_layer_slot.slot_name,
                                         slot->data.zone_layer_slot.layer_type,
                                         slot->data.zone_layer_slot.is_relation
                                             ? RIR_RESOURCE_RELATION_INSTANCE
                                             : RIR_RESOURCE_EFFECT_INSTANCE,
                                         RIR_STATE_DETACHED,
                                         slot))
                goto oom;
        }
        for (size_t i = 0; i < node->data.zone_decl.authority_count; i++) {
            ASTNode *auth = node->data.zone_decl.authorities[i];
            if (!add_authority_fact(&scope, auth->data.zone_authority.subject_slot_name, NULL, auth))
                goto oom;
            for (size_t j = 0; j < auth->data.zone_authority.ability_count; j++) {
                if (!add_authority_fact(&scope,
                                        auth->data.zone_authority.subject_slot_name,
                                        auth->data.zone_authority.required_abilities[j],
                                        auth))
                    goto oom;
            }
        }

        refreshes = node->data.zone_decl.refreshes;
        refresh_count = node->data.zone_decl.refresh_count;
        methods = node->data.zone_decl.methods;
        method_count = node->data.zone_decl.method_count;

        for (size_t i = 0; i < node->data.zone_decl.apply_count; i++) {
            ASTNode *apply = node->data.zone_decl.applies[i];
            if (!add_op(&scope, RIR_OP_ATTACH_EFFECT,
                        apply->data.zone_apply.effect_slot_name,
                        apply->data.zone_apply.target_slot_name,
                        apply->data.zone_apply.actor_slot_name,
                        apply))
                goto oom;
        }
        for (size_t i = 0; i < node->data.zone_decl.link_count; i++) {
            ASTNode *link = node->data.zone_decl.links[i];
            if (!add_op(&scope, RIR_OP_LINK_RELATION,
                        link->data.zone_link.relation_slot_name,
                        link->data.zone_link.left_slot_name,
                        link->data.zone_link.right_slot_name,
                        link))
                goto oom;
        }
        for (size_t i = 0; i < node->data.zone_decl.detach_count; i++) {
            ASTNode *detach = node->data.zone_decl.detaches[i];
            if (!add_op(&scope, RIR_OP_DETACH_EFFECT,
                        detach->data.zone_detach.effect_slot_name,
                        detach->data.zone_detach.target_slot_name,
                        detach->data.zone_detach.actor_slot_name,
                        detach))
                goto oom;
        }
        for (size_t i = 0; i < node->data.zone_decl.unlink_count; i++) {
            ASTNode *unlink = node->data.zone_decl.unlinks[i];
            if (!add_op(&scope, RIR_OP_UNLINK_RELATION,
                        unlink->data.zone_unlink.relation_slot_name,
                        unlink->data.zone_unlink.left_slot_name,
                        unlink->data.zone_unlink.right_slot_name,
                        unlink))
                goto oom;
        }
    } else if (node->type == AST_RELATION_DECL) {
        refreshes = node->data.relation_decl.refreshes;
        refresh_count = node->data.relation_decl.refresh_count;
        methods = node->data.relation_decl.methods;
        method_count = node->data.relation_decl.method_count;
    } else if (node->type == AST_EFFECT_DECL) {
        refreshes = node->data.effect_decl.refreshes;
        refresh_count = node->data.effect_decl.refresh_count;
        methods = node->data.effect_decl.methods;
        method_count = node->data.effect_decl.method_count;
    } else if (node->type == AST_WORLD_DECL) {
        for (size_t i = 0; i < node->data.world_decl.zone_count; i++) {
            ASTNode *slot = node->data.world_decl.zones[i];
            if (!add_named_resource_fact(&scope,
                                         slot->data.world_zone.slot_name,
                                         slot->data.world_zone.zone_type,
                                         RIR_RESOURCE_ZONE_HANDLE,
                                         RIR_STATE_OWNED,
                                         slot))
                goto oom;
        }
        methods = node->data.world_decl.methods;
        method_count = node->data.world_decl.method_count;
    }

    for (size_t i = 0; i < refresh_count; i++) {
        ASTNode *refresh = refreshes[i];
        bool dto = refresh->data.zone_refresh.requires_dto;
        if (!add_projection_fact(&scope,
                                 refresh->data.zone_refresh.object_slot_name,
                                 refresh->data.zone_refresh.source_slot_name,
                                 dto ? "publish" : (refresh->data.zone_refresh.infer_target_kind ? "bind" : "refresh"),
                                 dto ? RIR_STATE_PUBLISHED
                                     : (refresh->data.zone_refresh.infer_target_kind ? RIR_STATE_DIRTY : RIR_STATE_SYNCED),
                                 dto ? RIR_RESOURCE_PROJECTION_DTO : RIR_RESOURCE_PROJECTION_OBJECT,
                                 refresh))
            goto oom;
        if (!add_op(&scope,
                    dto ? RIR_OP_PROJECT_PUBLISH : RIR_OP_PROJECT_REFRESH,
                    refresh->data.zone_refresh.object_slot_name,
                    refresh->data.zone_refresh.source_slot_name,
                    refresh->data.zone_refresh.actor_slot_name,
                    refresh))
            goto oom;
    }

    if (!rir_normalize_scope(&scope))
        goto oom;

    if (!append_scope(rir, scope))
        goto oom_no_scope;

    for (size_t i = 0; i < method_count; i++) {
        if (!rir_collect_func_scope(rir, RIR_SCOPE_METHOD, name, methods[i]))
            return false;
    }
    return true;

oom:
    free(scope.facts);
    free(scope.ops);
    free(scope.state_summaries);
oom_no_scope:
    return false;
}

static bool
rir_collect_intent_scope(RIRProgram *rir, ASTNode *node)
{
    RIRScope scope;
    memset(&scope, 0, sizeof(scope));
    scope.id = rir->scope_count;
    scope.kind = RIR_SCOPE_INTENT;
    scope.name = node->data.intent_decl.name;
    scope.ast = node;

    if (!add_intent_policy_fact(&scope,
                                "concurrency",
                                node->data.intent_decl.is_concurrent ? "concurrent" : "exclusive",
                                node))
        goto oom;
    if (!add_intent_policy_fact(&scope,
                                "rollback",
                                rollback_policy_name(node->data.intent_decl.rollback_policy),
                                node))
        goto oom;

    for (size_t i = 0; i < node->data.intent_decl.involve_count; i++) {
        ASTNode *involves = node->data.intent_decl.involves[i];
        if (involves == NULL || involves->type != AST_INTENT_INVOLVES)
            continue;
        if (!add_param_resource_fact(&scope,
                                     involves->data.intent_involves.alias,
                                     involves->data.intent_involves.subject_type,
                                     involves))
            goto oom;
    }

    for (size_t i = 0; i < node->data.intent_decl.step_count; i++) {
        ASTNode *step = node->data.intent_decl.steps[i];
        if (step->data.intent_step.using_expr != NULL) {
            if (!add_op(&scope,
                        RIR_OP_READ,
                        expr_name(step->data.intent_step.using_expr),
                        step->data.intent_step.name,
                        step->data.intent_step.where_type != NULL
                            ? type_name(step->data.intent_step.where_type) : NULL,
                        step))
                goto oom;
        }
        if (step->data.intent_step.transfer_from_alias != NULL) {
            if (!add_op(&scope,
                        RIR_OP_MOVE,
                        step->data.intent_step.transfer_from_alias,
                        step->data.intent_step.transfer_to_alias,
                        step->data.intent_step.name,
                        step))
                goto oom;
        }
        if (step->data.intent_step.transfer_to_alias != NULL) {
            if (!add_op(&scope,
                        RIR_OP_CLAIM,
                        step->data.intent_step.transfer_to_alias,
                        step->data.intent_step.transfer_from_alias,
                        step->data.intent_step.name,
                        step))
                goto oom;
        }
        for (size_t j = 0; j < step->data.intent_step.required_ability_count; j++) {
            if (!add_authority_fact(&scope, step->data.intent_step.name,
                                    step->data.intent_step.required_abilities[j],
                                    step))
                goto oom;
        }
        for (size_t j = 0; j < step->data.intent_step.authorized_by_count; j++) {
            if (!add_op(&scope, RIR_OP_AUTHORIZE,
                        step->data.intent_step.authorized_by[j],
                        step->data.intent_step.name,
                        step->data.intent_step.where_type != NULL
                            ? type_name(step->data.intent_step.where_type) : NULL,
                        step))
                goto oom;
        }
        for (size_t j = 0; j < step->data.intent_step.on_expr_count; j++) {
            if (!rir_walk_node(&scope, step->data.intent_step.on_exprs[j]))
                goto oom;
        }
        for (size_t j = 0; j < step->data.intent_step.compensate_expr_count; j++) {
            const char *comp_name = expr_name(step->data.intent_step.compensate_exprs[j]);
            if (comp_name == NULL
                && step->data.intent_step.compensate_exprs[j] != NULL
                && step->data.intent_step.compensate_exprs[j]->type == AST_CALL) {
                comp_name = call_name(step->data.intent_step.compensate_exprs[j]);
            }
            if (!add_op(&scope, RIR_OP_COMPENSATE_INTENT_STEP,
                        step->data.intent_step.name,
                        comp_name,
                        NULL,
                        step->data.intent_step.compensate_exprs[j]))
                goto oom;
            if (!rir_walk_node(&scope, step->data.intent_step.compensate_exprs[j]))
                goto oom;
        }
    }

    if (node->data.intent_decl.rollback_policy != INTENT_ROLLBACK_NONE) {
        if (!add_op(&scope, RIR_OP_ABORT_INTENT,
                    node->data.intent_decl.name,
                    rollback_policy_name(node->data.intent_decl.rollback_policy),
                    NULL,
                    node))
            goto oom;
    }
    if (!add_op(&scope, RIR_OP_COMMIT_INTENT, node->data.intent_decl.name, NULL, NULL, node))
        goto oom;

    if (!rir_normalize_scope(&scope))
        goto oom;

    return append_scope(rir, scope);

oom:
    free(scope.facts);
    free(scope.ops);
    free(scope.state_summaries);
    return false;
}

RIRProgram *
rir_lower(ASTNode *annotated_ast, char **error_message)
{
    RIRProgram *rir;
    if (error_message != NULL)
        *error_message = NULL;
    if (annotated_ast == NULL || annotated_ast->type != AST_PROGRAM) {
        if (error_message != NULL)
            *error_message = pergyra_strdup("RIR lowering requires AST_PROGRAM");
        return NULL;
    }

    g_rir_program_root = annotated_ast;
    rir = calloc(1, sizeof(RIRProgram));
    if (rir == NULL) {
        if (error_message != NULL)
            *error_message = pergyra_strdup("out of memory");
        return NULL;
    }

    for (size_t i = 0; i < annotated_ast->data.program.count; i++) {
        ASTNode *node = annotated_ast->data.program.statements[i];
        bool ok = true;
        switch (node->type) {
            case AST_FUNC_DECL:
                ok = rir_collect_func_scope(rir,
                                            node->data.func_decl.is_action ? RIR_SCOPE_METHOD : RIR_SCOPE_FUNCTION,
                                            NULL,
                                            node);
                break;
            case AST_CLASS_DECL:
                for (size_t j = 0; ok && j < node->data.class_decl.method_count; j++) {
                    ok = rir_collect_func_scope(rir, RIR_SCOPE_METHOD,
                                                node->data.class_decl.name,
                                                node->data.class_decl.methods[j]);
                }
                break;
            case AST_ACTOR_DECL:
                for (size_t j = 0; ok && j < node->data.actor_decl.method_count; j++) {
                    ok = rir_collect_func_scope(rir, RIR_SCOPE_METHOD,
                                                node->data.actor_decl.name,
                                                node->data.actor_decl.methods[j]);
                }
                break;
            case AST_ZONE_DECL:
                ok = rir_collect_zone_like_scope(rir, node, RIR_SCOPE_ZONE, node->data.zone_decl.name);
                break;
            case AST_RELATION_DECL:
                ok = rir_collect_zone_like_scope(rir, node, RIR_SCOPE_RELATION, node->data.relation_decl.name);
                break;
            case AST_EFFECT_DECL:
                ok = rir_collect_zone_like_scope(rir, node, RIR_SCOPE_EFFECT, node->data.effect_decl.name);
                break;
            case AST_WORLD_DECL:
                ok = rir_collect_zone_like_scope(rir, node, RIR_SCOPE_WORLD, node->data.world_decl.name);
                break;
            case AST_INTENT_DECL:
                ok = rir_collect_intent_scope(rir, node);
                break;
            default:
                break;
        }
        if (!ok) {
            if (error_message != NULL)
                *error_message = pergyra_strdup("out of memory");
            rir_destroy(rir);
            return NULL;
        }
    }

    return rir;
}

bool
rir_enrich_with_hir_flow(RIRProgram *rir, const HIRProgram *hir, char **error_message)
{
    if (error_message != NULL)
        *error_message = NULL;
    if (rir == NULL || hir == NULL)
        return true;

    for (size_t i = 0; i < hir->routine_count; i++) {
        const HIRRoutine *hir_routine = &hir->routines[i];
        RIRScope *scope = rir_find_matching_scope(rir, hir_routine);
        if (scope == NULL)
            continue;
        if (!rir_enrich_scope_with_hir_flow(scope, hir_routine)) {
            if (error_message != NULL)
                *error_message = pergyra_strdup("out of memory");
            return false;
        }
    }

    return true;
}

void
rir_destroy(RIRProgram *rir)
{
    if (rir == NULL)
        return;
    for (size_t i = 0; i < rir->scope_count; i++) {
        free(rir->scopes[i].facts);
        free(rir->scopes[i].ops);
        free(rir->scopes[i].state_summaries);
        rir_free_flow_blocks(&rir->scopes[i]);
    }
    free(rir->scopes);
    free(rir);
}

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
        case RIR_RESOURCE_QUBIT_HANDLE: return "QubitHandle";
        case RIR_RESOURCE_REMOTE_FUTURE_HANDLE: return "RemoteFutureHandle";
        case RIR_RESOURCE_PROJECTION_OBJECT: return "ProjectionObject";
        case RIR_RESOURCE_PROJECTION_DTO: return "ProjectionDto";
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
        case RIR_STATE_SYNCED: return "Synced";
        case RIR_STATE_DIRTY: return "Dirty";
        case RIR_STATE_DETACHED: return "Detached";
        case RIR_STATE_PUBLISHED: return "Published";
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

bool
rir_validate(const RIRProgram *rir, char **error_message)
{
    if (error_message != NULL)
        *error_message = NULL;
    if (rir == NULL) {
        if (error_message != NULL)
            *error_message = pergyra_strdup("RIR program is null");
        return false;
    }

    for (size_t i = 0; i < rir->scope_count; i++) {
        const RIRScope *scope = &rir->scopes[i];
        for (size_t j = 0; j < scope->state_summary_count; j++) {
            const RIRStateSummary *summary = &scope->state_summaries[j];
            if (summary->name == NULL || summary->resource_kind == RIR_RESOURCE_UNKNOWN) {
                if (error_message != NULL) {
                    *error_message = rir_strdup_fmt(
                        "RIR scope '%s' has incomplete state summary[%zu]",
                        scope->name != NULL ? scope->name : "(anonymous)",
                        j);
                }
                return false;
            }
        }

        for (size_t j = 0; j < scope->flow_block_count; j++) {
            const RIRFlowBlock *block = &scope->flow_blocks[j];
            if (block->fact_count > 0 && block->facts == NULL) {
                if (error_message != NULL) {
                    *error_message = rir_strdup_fmt(
                        "RIR scope '%s' flow-block[%zu] has missing fact storage",
                        scope->name != NULL ? scope->name : "(anonymous)",
                        block->block_id);
                }
                return false;
            }
            for (size_t k = 0; k < block->fact_count; k++) {
                if (block->facts[k].name == NULL) {
                    if (error_message != NULL) {
                        *error_message = rir_strdup_fmt(
                            "RIR scope '%s' flow-block[%zu] has unnamed fact",
                            scope->name != NULL ? scope->name : "(anonymous)",
                            block->block_id);
                    }
                    return false;
                }
            }
        }

        if (scope->kind == RIR_SCOPE_INTENT) {
            bool has_commit = false;
            bool has_abort = false;
            for (size_t j = 0; j < scope->op_count; j++) {
                has_commit = has_commit || scope->ops[j].kind == RIR_OP_COMMIT_INTENT;
                has_abort = has_abort || scope->ops[j].kind == RIR_OP_ABORT_INTENT;
            }
            if (!has_commit || !has_abort) {
                if (error_message != NULL) {
                    *error_message = rir_strdup_fmt(
                        "RIR intent scope '%s' is missing commit/abort structure",
                        scope->name != NULL ? scope->name : "(anonymous)");
                }
                return false;
            }
        }
    }

    return true;
}

void
rir_dump(const RIRProgram *rir, FILE *out)
{
    if (out == NULL)
        out = stdout;
    if (rir == NULL) {
        fprintf(out, "RIR: (null)\n");
        return;
    }

    fprintf(out, "RIR Program\n  scopes: %zu\n", rir->scope_count);
    for (size_t i = 0; i < rir->scope_count; i++) {
        const RIRScope *scope = &rir->scopes[i];
        fprintf(out, "  scope[%02zu] %-8s %s%s%s facts=%zu ops=%zu\n",
                i,
                rir_scope_kind_name(scope->kind),
                scope->owner_name != NULL ? scope->owner_name : "",
                scope->owner_name != NULL ? "." : "",
                scope->name != NULL ? scope->name : "(anonymous)",
                scope->fact_count,
                scope->op_count);
        fprintf(out, "    normalize summaries=%zu state-errors=%s\n",
                scope->state_summary_count,
                scope->has_state_errors ? "yes" : "no");
        for (size_t j = 0; j < scope->fact_count; j++) {
            const RIRFact *fact = &scope->facts[j];
            fprintf(out, "    fact[%02zu] %-13s name=%s arg0=%s arg1=%s kind=%s state=%s\n",
                    j,
                    rir_fact_kind_name(fact->kind),
                    fact->name != NULL ? fact->name : "-",
                    fact->arg0 != NULL ? fact->arg0 : "-",
                    fact->arg1 != NULL ? fact->arg1 : "-",
                    rir_resource_kind_name(fact->resource_kind),
                    rir_resource_state_name(fact->state));
        }
        for (size_t j = 0; j < scope->op_count; j++) {
            const RIROp *op = &scope->ops[j];
            fprintf(out, "    op[%02zu] %-20s subject=%s arg0=%s arg1=%s\n",
                    j,
                    rir_op_kind_name(op->kind),
                    op->subject != NULL ? op->subject : "-",
                    op->arg0 != NULL ? op->arg0 : "-",
                    op->arg1 != NULL ? op->arg1 : "-");
        }
        for (size_t j = 0; j < scope->state_summary_count; j++) {
            const RIRStateSummary *summary = &scope->state_summaries[j];
            fprintf(out,
                    "    state[%02zu] %-13s name=%s kind=%s init=%s final=%s last-op=%s error=%s\n",
                    j,
                    rir_fact_kind_name(summary->origin_kind),
                    summary->name != NULL ? summary->name : "-",
                    rir_resource_kind_name(summary->resource_kind),
                    rir_resource_state_name(summary->initial_state),
                    rir_resource_state_name(summary->final_state),
                    summary->last_op_name != NULL ? summary->last_op_name : "-",
                    summary->has_transition_error ? "yes" : "no");
        }
        for (size_t j = 0; j < scope->flow_block_count; j++) {
            const RIRFlowBlock *block = &scope->flow_blocks[j];
            fprintf(out,
                    "    flow-block[%02zu] reachable=%s join=%s facts=%zu\n",
                    block->block_id,
                    block->is_reachable ? "yes" : "no",
                    block->is_join ? "yes" : "no",
                    block->fact_count);
            for (size_t k = 0; k < block->fact_count; k++) {
                const RIRFlowFact *fact = &block->facts[k];
                fprintf(out,
                        "      flow[%02zu] name=%s entry=%s exit=%s join=%s widened=%s entry-conflict=%s exit-conflict=%s\n",
                        k,
                        fact->name != NULL ? fact->name : "-",
                        rir_resource_state_name(fact->entry_state),
                        rir_resource_state_name(fact->exit_state),
                        fact->merged_from_join ? "yes" : "no",
                        fact->widened_by_loop ? "yes" : "no",
                        fact->entry_conflict ? "yes" : "no",
                        fact->has_merge_conflict ? "yes" : "no");
            }
        }
    }
}
