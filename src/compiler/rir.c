#include "rir.h"

#include <stdlib.h>
#include <string.h>

#include "../common/string_compat.h"

static bool rir_walk_node(RIRScope *scope, ASTNode *node);
static bool rir_normalize_scope(RIRScope *scope);

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
    return RIR_RESOURCE_UNKNOWN;
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
    if (!rir_walk_node(&scope, func->data.func_decl.body)) {
        free(scope.facts);
        free(scope.ops);
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

    for (size_t i = 0; i < node->data.intent_decl.step_count; i++) {
        ASTNode *step = node->data.intent_decl.steps[i];
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

void
rir_destroy(RIRProgram *rir)
{
    if (rir == NULL)
        return;
    for (size_t i = 0; i < rir->scope_count; i++) {
        free(rir->scopes[i].facts);
        free(rir->scopes[i].ops);
        free(rir->scopes[i].state_summaries);
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
    }
}
