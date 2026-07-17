#include "rir.h"
#include "rir_internal.h"

#include <string.h>

#include "parser/ast_api.h"

ASTNode *
rir_find_domain_slot_in_owner(ASTNode *owner, const char *slot_name)
{
    ASTNode **slots = NULL;
    size_t slot_count = 0;

    if (owner == NULL || slot_name == NULL)
        return NULL;

    switch (owner->type) {
        case AST_ZONE_DECL:
            slots = ast_zone_slots(owner, &slot_count);
            break;
        case AST_RELATION_DECL:
            slots = ast_relation_slots(owner, &slot_count);
            break;
        case AST_EFFECT_DECL:
            slots = ast_effect_slots(owner, &slot_count);
            break;
        default:
            return NULL;
    }

    for (size_t i = 0; i < slot_count; i++) {
        ASTNode *slot = slots[i];
        const char *candidate_name = ast_domain_slot_name(slot);
        if (slot != NULL
            && slot->type == AST_DOMAIN_SLOT
            && candidate_name != NULL
            && strcmp(candidate_name, slot_name) == 0) {
            return slot;
        }
    }

    return NULL;
}

static const char *
type_name(ASTNode *type_node)
{
    if (type_node == NULL)
        return NULL;
    return ast_type_name(type_node);
}

static RIRResourceKind
rir_nominal_kind_from_name(ASTNode *program, const char *name)
{
    if (program == NULL || name == NULL || program->type != AST_PROGRAM)
        return RIR_RESOURCE_UNKNOWN;

    for (size_t i = 0; i < ast_program_statement_count(program); i++) {
        ASTNode *node = ast_program_statement(program, i);
        if (node == NULL)
            continue;
        switch (node->type) {
            case AST_RELATION_DECL:
                if (ast_relation_name(node) != NULL
                    && strcmp(ast_relation_name(node), name) == 0)
                    return RIR_RESOURCE_RELATION_INSTANCE;
                break;
            case AST_EFFECT_DECL:
                if (ast_effect_name(node) != NULL
                    && strcmp(ast_effect_name(node), name) == 0)
                    return RIR_RESOURCE_EFFECT_INSTANCE;
                break;
            case AST_ZONE_DECL:
                if (ast_zone_name(node) != NULL
                    && strcmp(ast_zone_name(node), name) == 0)
                    return RIR_RESOURCE_ZONE_HANDLE;
                break;
            case AST_WORLD_DECL:
                if (ast_world_name(node) != NULL
                    && strcmp(ast_world_name(node), name) == 0)
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
            return ast_identifier_name(node);
        case AST_MEMBER_ACCESS:
            return ast_member_name(node);
        case AST_TYPE:
            return ast_type_name(node);
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
    return expr_name(ast_call_callee(call));
}

const char *
rir_call_name(ASTNode *call)
{
    return call_name(call);
}

static RIRResourceKind
resource_kind_from_type(ASTNode *program_root, ASTNode *type_node)
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
    if (strcmp(name, "Future") == 0)
        return RIR_RESOURCE_LOCAL_FUTURE_HANDLE;
    if (strcmp(name, "RemoteFuture") == 0)
        return RIR_RESOURCE_REMOTE_FUTURE_HANDLE;
    return rir_nominal_kind_from_name(program_root, name);
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
        case RIR_RESOURCE_LOCAL_FUTURE_HANDLE:
        case RIR_RESOURCE_REMOTE_FUTURE_HANDLE:
            return RIR_STATE_REMOTE_PENDING;
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
                  uint32_t declaration_syntax_id,
                  ASTNode *ast)
{
    RIRResourceKind kind = resource_kind_from_type(scope->program_root,
                                                   type_node);
    if (kind == RIR_RESOURCE_UNKNOWN)
        return true;
    if ((kind == RIR_RESOURCE_LOCAL_FUTURE_HANDLE
         || kind == RIR_RESOURCE_REMOTE_FUTURE_HANDLE)
        && state == RIR_STATE_UNINIT) {
        state = RIR_STATE_REMOTE_PENDING;
    }

    RIRFact fact;
    memset(&fact, 0, sizeof(fact));
    fact.kind = RIR_FACT_RESOURCE;
    fact.name = name;
    fact.slot_anchor = name;
    fact.arg0 = type_name(type_node);
    fact.resource_kind = kind;
    fact.state = state;
    /* Capture the source identity at the RIR collection boundary.  Flow
     * enrichment consumes this typed field; it must not reopen fact->ast. */
    fact.declaration_syntax_id = declaration_syntax_id;
    fact.ast = ast;
    return scope_add_fact(scope, fact);
}

bool
add_param_resource_fact(RIRScope *scope, const char *name, ASTNode *type_node, ASTNode *ast)
{
    RIRResourceKind kind = resource_kind_from_type(scope->program_root,
                                                   type_node);
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

    if (ast_domain_slot_is_subject(slot)) {
        kind = RIR_RESOURCE_SUBJECT_SLOT;
        state = RIR_STATE_OWNED;
    } else if (ast_domain_slot_is_vessel(slot)) {
        kind = RIR_RESOURCE_VESSEL_SLOT;
        state = RIR_STATE_OWNED;
    } else if (ast_domain_slot_is_tobject(slot)) {
        kind = RIR_RESOURCE_TOBJECT_SLOT;
        state = RIR_STATE_UNINIT;
    } else {
        kind = RIR_RESOURCE_OBJECT_SLOT;
        state = RIR_STATE_UNINIT;
    }

    return add_named_resource_fact(scope,
                                   ast_domain_slot_name(slot),
                                   type_name(ast_domain_slot_type(slot)),
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
    /* RIR owns the initial source identity snapshot; downstream joins use the
     * copied ID and never derive it again from the borrowed AST pointer. */
    fact.declaration_syntax_id = ast != NULL ? ast_node_stable_id(ast) : 0;
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
    return add_op_with_machine_contact(scope, kind, subject, arg0, arg1,
                                        RIR_MACHINE_CONTACT_NONE, ast);
}

bool
add_op_with_machine_contact(RIRScope *scope,
                            RIROpKind kind,
                            const char *subject,
                            const char *arg0,
                            const char *arg1,
                            RIRMachineContactKind machine_contact_kind,
                            ASTNode *ast)
{
    RIROp op;
    memset(&op, 0, sizeof(op));
    op.kind = kind;
    op.subject = subject;
    op.slot_anchor = subject != NULL ? subject : arg0;
    op.arg0 = arg0;
    op.arg1 = arg1;
    op.machine_contact_kind = machine_contact_kind;
    op.ast = ast;
    return scope_add_op(scope, op);
}
