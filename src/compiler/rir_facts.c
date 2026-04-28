#include "rir.h"
#include "rir_internal.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

ASTNode *g_rir_program_root = NULL;

ASTNode *
rir_find_domain_slot_in_owner(ASTNode *owner, const char *slot_name)
{
    ASTNode **slots = NULL;
    size_t slot_count = 0;

    if (owner == NULL || slot_name == NULL)
        return NULL;

    switch (owner->type) {
        case AST_ZONE_DECL:
            slots = owner->data.zone_decl.slots;
            slot_count = owner->data.zone_decl.slot_count;
            break;
        case AST_RELATION_DECL:
            slots = owner->data.relation_decl.slots;
            slot_count = owner->data.relation_decl.slot_count;
            break;
        case AST_EFFECT_DECL:
            slots = owner->data.effect_decl.slots;
            slot_count = owner->data.effect_decl.slot_count;
            break;
        default:
            return NULL;
    }

    for (size_t i = 0; i < slot_count; i++) {
        ASTNode *slot = slots[i];
        if (slot != NULL
            && slot->type == AST_DOMAIN_SLOT
            && slot->data.domain_slot.slot_name != NULL
            && strcmp(slot->data.domain_slot.slot_name, slot_name) == 0) {
            return slot;
        }
    }

    return NULL;
}

bool
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

char *
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

void
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

RIRStateSummary *
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

bool
scope_ensure_state_summary(RIRScope *scope, const RIRFact *fact)
{
    RIRStateSummary summary;
    if (scope == NULL || fact == NULL || fact->name == NULL)
        return true;
    if (fact->kind != RIR_FACT_RESOURCE
        && fact->kind != RIR_FACT_PROJECTION
        && fact->kind != RIR_FACT_AUTHORITY
        && fact->kind != RIR_FACT_CAPABILITY)
        return true;
    if (scope_find_state_summary(scope, fact->name) != NULL)
        return true;

    memset(&summary, 0, sizeof(summary));
    summary.name = fact->name;
    summary.slot_anchor = fact->slot_anchor;
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

const char *
rir_type_name(ASTNode *type_node)
{
    return type_name(type_node);
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

const char *
rir_expr_name(ASTNode *node)
{
    return expr_name(node);
}

static const char *
call_name(ASTNode *call)
{
    if (call == NULL || call->type != AST_CALL)
        return NULL;
    return expr_name(call->data.call.callee);
}

const char *
rir_call_name(ASTNode *call)
{
    return call_name(call);
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
        case RIR_RESOURCE_AUTHORITY_HANDLE:
        case RIR_RESOURCE_CAPABILITY_TOKEN:
            return RIR_STATE_AUTHORIZED;
        case RIR_RESOURCE_SUBJECT_SLOT:
        case RIR_RESOURCE_VESSEL_SLOT:
            return RIR_STATE_OWNED;
        case RIR_RESOURCE_OBJECT_SLOT:
        case RIR_RESOURCE_TOBJECT_SLOT:
            return RIR_STATE_UNINIT;
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

const char *
rir_rollback_policy_name(IntentRollbackPolicy policy)
{
    return rollback_policy_name(policy);
}

bool
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
    fact.slot_anchor = name;
    fact.arg0 = type_name(type_node);
    fact.resource_kind = kind;
    fact.state = state;
    fact.ast = ast;
    return scope_add_fact(scope, fact);
}

bool
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

bool
add_domain_slot_fact(RIRScope *scope, ASTNode *slot)
{
    RIRResourceKind kind;
    RIRResourceState state;

    if (scope == NULL || slot == NULL || slot->type != AST_DOMAIN_SLOT)
        return true;

    if (slot->data.domain_slot.is_subject) {
        kind = RIR_RESOURCE_SUBJECT_SLOT;
        state = RIR_STATE_OWNED;
    } else if (slot->data.domain_slot.is_vessel) {
        kind = RIR_RESOURCE_VESSEL_SLOT;
        state = RIR_STATE_OWNED;
    } else if (slot->data.domain_slot.is_tobject) {
        kind = RIR_RESOURCE_TOBJECT_SLOT;
        state = RIR_STATE_UNINIT;
    } else {
        kind = RIR_RESOURCE_OBJECT_SLOT;
        state = RIR_STATE_UNINIT;
    }

    return add_named_resource_fact(scope,
                                   slot->data.domain_slot.slot_name,
                                   type_name(slot->data.domain_slot.type),
                                   kind,
                                   state,
                                   slot);
}

bool
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
    fact.slot_anchor = target != NULL ? target : source;
    fact.arg0 = source;
    fact.arg1 = mode;
    fact.resource_kind = kind;
    fact.state = state;
    fact.ast = ast;
    return scope_add_fact(scope, fact);
}

bool
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
    fact.slot_anchor = name;
    fact.arg0 = type_name_value;
    fact.resource_kind = kind;
    fact.state = state;
    fact.ast = ast;
    return scope_add_fact(scope, fact);
}

bool
add_authority_fact(RIRScope *scope, const char *participant, const char *ability, ASTNode *ast)
{
    RIRFact fact;
    memset(&fact, 0, sizeof(fact));
    fact.kind = ability != NULL ? RIR_FACT_CAPABILITY : RIR_FACT_AUTHORITY;
    fact.name = participant;
    fact.slot_anchor = participant;
    fact.arg0 = ability;
    fact.resource_kind = ability != NULL
        ? RIR_RESOURCE_CAPABILITY_TOKEN
        : RIR_RESOURCE_AUTHORITY_HANDLE;
    fact.state = RIR_STATE_AUTHORIZED;
    fact.ast = ast;
    return scope_add_fact(scope, fact);
}

bool
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

bool
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
    op.slot_anchor = subject != NULL ? subject : arg0;
    op.arg0 = arg0;
    op.arg1 = arg1;
    op.ast = ast;
    return scope_add_op(scope, op);
}
