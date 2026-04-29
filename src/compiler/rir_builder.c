#include "rir.h"
#include "rir_internal.h"

#include <stdlib.h>
#include <string.h>

#include "../common/string_compat.h"
#include "hir.h"

#define type_name rir_type_name
#define expr_name rir_expr_name
#define call_name rir_call_name
#define rollback_policy_name rir_rollback_policy_name

static bool
rir_call_name_is_io_boundary(const char *name)
{
    static const char *io_names[] = {
        "FileOpen",
        "FileRead",
        "FileWrite",
        "FileClose",
        "ReadFile",
        "WriteFile",
        "Input",
        "ReadLine",
        "Now",
        "Sleep",
    };
    if (name == NULL)
        return false;
    for (size_t i = 0; i < sizeof(io_names) / sizeof(io_names[0]); i++) {
        if (strcmp(name, io_names[i]) == 0)
            return true;
    }
    return false;
}

static bool
rir_walk_call(RIRScope *scope, ASTNode *node)
{
    const char *name;
    bool member_slot_op_handled = false;
    if (node == NULL || node->type != AST_CALL)
        return true;

    if (node->data.call.callee != NULL
        && node->data.call.callee->type == AST_MEMBER_ACCESS) {
        ASTNode *member = node->data.call.callee;
        ASTNode *receiver = member->data.member.object;
        const char *method = member->data.member.name;
        const char *receiver_name = expr_name(receiver);

        if (receiver_name != NULL && method != NULL) {
            if (strcmp(method, "Read") == 0) {
                if (!add_op(scope, RIR_OP_READ, receiver_name, NULL, NULL, node))
                    return false;
                member_slot_op_handled = true;
            } else if (strcmp(method, "Write") == 0) {
                const char *value_name = node->data.call.arg_count >= 1
                    ? expr_name(node->data.call.arguments[0])
                    : NULL;
                if (!add_op(scope, RIR_OP_WRITE, receiver_name, value_name, NULL, node))
                    return false;
                member_slot_op_handled = true;
            } else if (strcmp(method, "Release") == 0) {
                if (!add_op(scope, RIR_OP_RELEASE, receiver_name, NULL, NULL, node))
                    return false;
                member_slot_op_handled = true;
            } else if (strcmp(method, "Move") == 0) {
                if (!add_op(scope, RIR_OP_MOVE, receiver_name, NULL, NULL, node))
                    return false;
                member_slot_op_handled = true;
            }
        }
    }

    name = member_slot_op_handled ? NULL : call_name(node);
    if (name != NULL) {
        ASTNode **args = node->data.call.arguments;
        size_t argc = node->data.call.arg_count;

        if (rir_call_name_is_io_boundary(name)) {
            const char *first_arg = argc >= 1 ? expr_name(args[0]) : NULL;
            if (!add_op(scope, RIR_OP_IO, name, first_arg, NULL, node))
                return false;
        } else if (strcmp(name, "Read") == 0 && argc >= 1) {
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
        } else if (strcmp(name, "ToTObject") == 0 && argc >= 2) {
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

bool
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

        case AST_SPAWN_EXPR:
            if (!add_op(scope, RIR_OP_SPAWN, "spawn", NULL, NULL, node))
                return false;
            if (!rir_walk_node(scope, node->data.spawn_expr.function))
                return false;
            for (size_t i = 0; i < node->data.spawn_expr.arg_count; i++) {
                if (!rir_walk_node(scope, node->data.spawn_expr.arguments[i]))
                    return false;
            }
            return true;

        case AST_CHANNEL_SEND:
            if (!add_op(scope,
                        RIR_OP_CHANNEL_SEND,
                        expr_name(node->data.channel_send.channel),
                        expr_name(node->data.channel_send.value),
                        NULL,
                        node))
                return false;
            if (!rir_walk_node(scope, node->data.channel_send.channel))
                return false;
            return rir_walk_node(scope, node->data.channel_send.value);

        case AST_CHANNEL_RECV:
            if (!add_op(scope,
                        RIR_OP_CHANNEL_RECV,
                        expr_name(node->data.channel_recv.channel),
                        NULL,
                        NULL,
                        node))
                return false;
            return rir_walk_node(scope, node->data.channel_recv.channel);

        case AST_SELECT_STMT:
            if (!add_op(scope, RIR_OP_CHANNEL_SELECT, "select", NULL, NULL, node))
                return false;
            for (size_t i = 0; i < node->data.select_stmt.case_count; i++) {
                if (!rir_walk_node(scope, node->data.select_stmt.cases[i]))
                    return false;
            }
            return rir_walk_node(scope, node->data.select_stmt.default_case);

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
            if (!add_op(scope, RIR_OP_PARALLEL, "parallel", NULL, NULL, node))
                return false;
            for (size_t i = 0; i < node->data.parallel.task_count; i++) {
                if (!rir_walk_node(scope, node->data.parallel.tasks[i]))
                    return false;
            }
            return true;

        case AST_ASYNC_BLOCK:
            if (!add_op(scope, RIR_OP_ASYNC, "async", NULL, NULL, node))
                return false;
            for (size_t i = 0; i < node->data.async_block.statement_count; i++) {
                if (!rir_walk_node(scope, node->data.async_block.statements[i]))
                    return false;
            }
            return true;

        case AST_TASK_GROUP:
            if (!add_op(scope, RIR_OP_TASK_GROUP, "task-group", NULL, NULL, node))
                return false;
            for (size_t i = 0; i < node->data.task_group.task_count; i++) {
                if (!rir_walk_node(scope, node->data.task_group.tasks[i]))
                    return false;
            }
            return true;

        default:
            return true;
    }
}

bool
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
    if (!rir_normalize_scope_shared(&scope)) {
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
        for (size_t i = 0; i < node->data.zone_decl.slot_count; i++) {
            if (!add_domain_slot_fact(&scope, node->data.zone_decl.slots[i]))
                goto oom;
        }
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
                ASTNode *ability_ref = auth->data.zone_authority.required_abilities[j];
                const char *ability_name = (ability_ref != NULL && ability_ref->type == AST_TYPE)
                    ? ability_ref->data.type.name : NULL;
                if (ability_name == NULL)
                    continue;
                if (!add_authority_fact(&scope,
                                        auth->data.zone_authority.subject_slot_name,
                                        ability_name,
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
                        apply->data.zone_apply.participant_slot_name,
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
                        detach->data.zone_detach.participant_slot_name,
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
        for (size_t i = 0; i < node->data.relation_decl.slot_count; i++) {
            if (!add_domain_slot_fact(&scope, node->data.relation_decl.slots[i]))
                goto oom;
        }
        refreshes = node->data.relation_decl.refreshes;
        refresh_count = node->data.relation_decl.refresh_count;
        methods = node->data.relation_decl.methods;
        method_count = node->data.relation_decl.method_count;
    } else if (node->type == AST_EFFECT_DECL) {
        for (size_t i = 0; i < node->data.effect_decl.slot_count; i++) {
            if (!add_domain_slot_fact(&scope, node->data.effect_decl.slots[i]))
                goto oom;
        }
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
        ASTNode *target_slot = rir_find_domain_slot_in_owner(
            node, refresh->data.zone_refresh.object_slot_name);
        bool tobject =
            refresh->data.zone_refresh.requires_dto
            || (refresh->data.zone_refresh.derive_target_kind
                && target_slot != NULL
                && target_slot->data.domain_slot.is_tobject);

        if (!add_projection_fact(&scope,
                                 refresh->data.zone_refresh.object_slot_name,
                                 refresh->data.zone_refresh.source_slot_name,
                                 tobject ? "publish" : (refresh->data.zone_refresh.derive_target_kind ? "bind" : "refresh"),
                                 tobject ? RIR_STATE_PUBLISHED
                                     : (refresh->data.zone_refresh.derive_target_kind ? RIR_STATE_DIRTY : RIR_STATE_SYNCED),
                                 tobject ? RIR_RESOURCE_PROJECTION_TOBJECT : RIR_RESOURCE_PROJECTION_OBJECT,
                                 refresh))
            goto oom;
        if (!add_op(&scope,
                    tobject ? RIR_OP_PROJECT_PUBLISH : RIR_OP_PROJECT_REFRESH,
                    refresh->data.zone_refresh.object_slot_name,
                    refresh->data.zone_refresh.source_slot_name,
                    refresh->data.zone_refresh.participant_slot_name,
                    refresh))
            goto oom;
    }

    if (!rir_normalize_scope_shared(&scope))
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
