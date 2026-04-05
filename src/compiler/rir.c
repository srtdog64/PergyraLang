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

#include "rir_flow.inc"
#include "rir_builder.inc"
#include "rir_public.inc"
