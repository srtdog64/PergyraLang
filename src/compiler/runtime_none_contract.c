/*
 * Copyright (c) 2025 Pergyra Language Project
 * All rights reserved.
 */

#include "runtime_none_contract.h"

#include <stdio.h>

#include "parser/ast_api.h"

typedef struct RuntimeNoneScan
{
    const char *surface;
    uint32_t    line;
    uint32_t    column;
} RuntimeNoneScan;

static bool runtime_none_scan_node(const ASTNode *node, RuntimeNoneScan *scan);

static bool
runtime_none_record(RuntimeNoneScan *scan, const ASTNode *node, const char *surface)
{
    if (scan == NULL || scan->surface != NULL)
        return false;
    scan->surface = surface;
    scan->line = node != NULL ? node->line : 0;
    scan->column = node != NULL ? node->column : 0;
    return false;
}

static bool
runtime_none_scan_list(ASTNode *const *items, size_t count, RuntimeNoneScan *scan)
{
    for (size_t i = 0; i < count; i++) {
        if (!runtime_none_scan_node(items[i], scan))
            return false;
    }
    return true;
}

static bool
runtime_none_scan_params(FuncParam *const *params, size_t count, RuntimeNoneScan *scan)
{
    for (size_t i = 0; i < count; i++) {
        if (params[i] != NULL && !runtime_none_scan_node(params[i]->type, scan))
            return false;
        if (params[i] != NULL && !runtime_none_scan_node(params[i]->default_value, scan))
            return false;
    }
    return true;
}

static bool
runtime_none_scan_relation_decl(const ASTNode *node, RuntimeNoneScan *scan)
{
    size_t count = 0;
    ASTNode **items = ast_relation_slots(node, &count);
    if (!runtime_none_scan_list(items, count, scan))
        return false;
    items = ast_relation_refreshes(node, &count);
    if (!runtime_none_scan_list(items, count, scan))
        return false;
    items = ast_relation_shared_fields(node, &count);
    if (!runtime_none_scan_list(items, count, scan))
        return false;
    items = ast_relation_methods(node, &count);
    return runtime_none_scan_list(items, count, scan);
}

static bool
runtime_none_scan_effect_decl(const ASTNode *node, RuntimeNoneScan *scan)
{
    size_t count = 0;
    ASTNode **items = ast_effect_slots(node, &count);
    if (!runtime_none_scan_list(items, count, scan))
        return false;
    items = ast_effect_refreshes(node, &count);
    if (!runtime_none_scan_list(items, count, scan))
        return false;
    items = ast_effect_shared_fields(node, &count);
    if (!runtime_none_scan_list(items, count, scan))
        return false;
    items = ast_effect_methods(node, &count);
    return runtime_none_scan_list(items, count, scan);
}

static bool
runtime_none_scan_node(const ASTNode *node, RuntimeNoneScan *scan)
{
    if (node == NULL)
        return true;

    switch (node->type) {
        case AST_PARALLEL_BLOCK:
            return runtime_none_record(scan, node, "parallel");
        case AST_ASYNC_BLOCK:
            return runtime_none_record(scan, node, "async");
        case AST_SPAWN_EXPR:
            return runtime_none_record(scan, node, "spawn");
        case AST_AWAIT_EXPR:
            return runtime_none_record(scan, node, "await");
        case AST_TASK_GROUP:
            return runtime_none_record(scan, node, "task-group");
        case AST_CHANNEL_SEND:
        case AST_CHANNEL_RECV:
        case AST_CHANNEL_TYPE:
            return runtime_none_record(scan, node, "channel");
        case AST_FUTURE_TYPE:
            return runtime_none_record(scan, node, "future");
        case AST_SELECT_STMT:
            return runtime_none_record(scan, node, "select");
        case AST_INTENT_DECL:
        case AST_INTENT_INVOLVES:
        case AST_INTENT_VALUE:
        case AST_INTENT_STEP:
            return runtime_none_record(scan, node, "intent");
        case AST_WORLD_DECL:
        case AST_WORLD_SYSTEMIC:
        case AST_WORLD_ZONE:
        case AST_WORLD_ACTIVATE:
        case AST_WORLD_DEACTIVATE:
        case AST_WORLD_MAINTAIN:
        case AST_WORLD_STATE:
            return runtime_none_record(scan, node, "world");
        case AST_ZONE_DECL:
        case AST_ZONE_LAYER_SLOT:
        case AST_ZONE_APPLY:
        case AST_ZONE_LINK:
        case AST_ZONE_DETACH:
        case AST_ZONE_UNLINK:
        case AST_ZONE_REFRESH:
        case AST_ZONE_MAINTAIN_EFFECT:
        case AST_ZONE_MAINTAIN_RELATION:
        case AST_ZONE_MAINTAIN_STATE:
        case AST_ZONE_AUTHORITY:
        case AST_ZONE_STATE:
            return runtime_none_record(scan, node, "zone");
        case AST_EVENT_DECL:
        case AST_EVENT_SUBSCRIBE:
        case AST_EVENT_UNSUBSCRIBE:
        case AST_EVENT_INVOKE:
        case AST_EVENT_HANDLER_TYPE:
            return runtime_none_record(scan, node, "event");
        default:
            break;
    }

    switch (node->type) {
        case AST_PROGRAM:
            return runtime_none_scan_list(node->data.program.statements,
                                          node->data.program.count,
                                          scan);
        case AST_FUNC_DECL:
            if (node->is_async_decl)
                return runtime_none_record(scan, node, "async-func");
            if (!runtime_none_scan_params(node->data.func_decl.params,
                                          node->data.func_decl.param_count,
                                          scan))
                return false;
            if (!runtime_none_scan_node(node->data.func_decl.return_type, scan))
                return false;
            if (!runtime_none_scan_node(node->data.func_decl.body, scan))
                return false;
            return runtime_none_scan_list(node->data.func_decl.required_abilities,
                                          node->data.func_decl.required_ability_count,
                                          scan);
        case AST_CLASS_DECL:
            for (size_t i = 0; i < node->data.class_decl.field_count; i++) {
                if (node->data.class_decl.fields[i] != NULL &&
                    !runtime_none_scan_node(node->data.class_decl.fields[i]->type, scan))
                    return false;
            }
            return runtime_none_scan_list(node->data.class_decl.methods,
                                          node->data.class_decl.method_count,
                                          scan);
        case AST_EXTERN_BLOCK:
            return runtime_none_scan_list(node->data.extern_block.declarations,
                                          node->data.extern_block.count,
                                          scan);
        case AST_LET_DECL:
            return runtime_none_scan_node(node->data.let_decl.type, scan) &&
                   runtime_none_scan_node(node->data.let_decl.initializer, scan);
        case AST_LET_DESTRUCTURE:
            return runtime_none_scan_node(node->data.let_destructure.initializer, scan);
        case AST_TYPE_ALIAS:
            return runtime_none_scan_node(node->data.type_alias.target_type, scan);
        case AST_WITH_STMT:
            return runtime_none_scan_node(node->data.with_stmt.slot_type, scan) &&
                   runtime_none_scan_node(node->data.with_stmt.body, scan);
        case AST_BLOCK:
            return runtime_none_scan_list(node->data.block.statements,
                                          node->data.block.count,
                                          scan);
        case AST_FOR_LOOP:
            return runtime_none_scan_node(node->data.for_loop.range_start, scan) &&
                   runtime_none_scan_node(node->data.for_loop.range_end, scan) &&
                   runtime_none_scan_node(node->data.for_loop.iterable, scan) &&
                   runtime_none_scan_node(node->data.for_loop.body, scan);
        case AST_WHILE_LOOP:
            return runtime_none_scan_node(node->data.while_loop.condition, scan) &&
                   runtime_none_scan_node(node->data.while_loop.body, scan);
        case AST_IF_STMT:
            return runtime_none_scan_node(node->data.if_stmt.condition, scan) &&
                   runtime_none_scan_node(node->data.if_stmt.then_branch, scan) &&
                   runtime_none_scan_node(node->data.if_stmt.else_branch, scan);
        case AST_RETURN:
            return runtime_none_scan_node(node->data.return_stmt.value, scan);
        case AST_MATCH_STMT:
            if (!runtime_none_scan_node(node->data.match_stmt.subject, scan))
                return false;
            if (!runtime_none_scan_list(node->data.match_stmt.cases,
                                        node->data.match_stmt.case_count,
                                        scan))
                return false;
            return runtime_none_scan_node(node->data.match_stmt.default_body, scan);
        case AST_MATCH_CASE:
            if (!runtime_none_scan_node(node->data.match_case.pattern, scan))
                return false;
            if (!runtime_none_scan_list(node->data.match_case.patterns,
                                        node->data.match_case.pattern_count,
                                        scan))
                return false;
            return runtime_none_scan_node(node->data.match_case.guard, scan) &&
                   runtime_none_scan_node(node->data.match_case.body, scan);
        case AST_BINARY:
            return runtime_none_scan_node(node->data.binary.left, scan) &&
                   runtime_none_scan_node(node->data.binary.right, scan);
        case AST_UNARY:
            return runtime_none_scan_node(node->data.unary.operand, scan);
        case AST_CALL:
            if (!runtime_none_scan_node(node->data.call.callee, scan))
                return false;
            return runtime_none_scan_list(node->data.call.arguments,
                                          node->data.call.arg_count,
                                          scan);
        case AST_MEMBER_ACCESS:
            return runtime_none_scan_node(node->data.member.object, scan);
        case AST_ARRAY_ACCESS:
            return runtime_none_scan_node(node->data.array_access.array, scan) &&
                   runtime_none_scan_node(node->data.array_access.index, scan);
        case AST_ARRAY_LITERAL:
            return runtime_none_scan_list(node->data.array_literal.elements,
                                          node->data.array_literal.count,
                                          scan);
        case AST_TUPLE_LITERAL:
            return runtime_none_scan_list(node->data.tuple_literal.elements,
                                          node->data.tuple_literal.count,
                                          scan);
        case AST_ASSIGNMENT:
            return runtime_none_scan_node(node->data.assignment.target, scan) &&
                   runtime_none_scan_node(node->data.assignment.value, scan);
        case AST_TYPE:
            return runtime_none_scan_list(node->data.type.tuple_elements,
                                          node->data.type.tuple_element_count,
                                          scan);
        case AST_ROLE_DECL:
            if (!runtime_none_scan_node(ast_role_for_type(node), scan))
                return false;
            for (size_t i = 0; i < ast_role_include_count(node); i++) {
                if (!runtime_none_scan_node(ast_role_include(node, i), scan))
                    return false;
            }
            for (size_t i = 0; i < ast_role_impl_count(node); i++) {
                if (!runtime_none_scan_node(ast_role_impl(node, i), scan))
                    return false;
            }
            return runtime_none_scan_node(node->data.role_decl.parallel_block, scan);
        case AST_IMPL_ABILITY:
            if (!runtime_none_scan_node(ast_impl_ability_ref(node), scan))
                return false;
            for (size_t i = 0; i < ast_impl_ability_method_count(node); i++) {
                if (!runtime_none_scan_node(ast_impl_ability_method(node, i), scan))
                    return false;
            }
            return true;
        case AST_PARTY_DECL:
            for (size_t i = 0; i < ast_party_role_count(node); i++) {
                if (!runtime_none_scan_node(ast_party_role(node, i), scan))
                    return false;
            }
            for (size_t i = 0; i < ast_party_shared_count(node); i++) {
                if (!runtime_none_scan_node(ast_party_shared(node, i), scan))
                    return false;
            }
            for (size_t i = 0; i < ast_party_method_count(node); i++) {
                if (!runtime_none_scan_node(ast_party_method(node, i), scan))
                    return false;
            }
            return runtime_none_scan_node(node->data.party_decl.extends, scan);
        case AST_PARTY_SHARED:
            return runtime_none_scan_node(node->data.party_shared.type, scan) &&
                   runtime_none_scan_node(node->data.party_shared.initializer, scan);
        case AST_PARTY_INSTANCE:
            for (size_t i = 0; i < node->data.party_instance.assignment_count; i++) {
                if (!runtime_none_scan_node(node->data.party_instance.assignments[i].value,
                                            scan))
                    return false;
            }
            return true;
        case AST_ROSTER_DECL:
            for (size_t i = 0; i < ast_roster_party_count(node); i++) {
                if (!runtime_none_scan_node(ast_roster_party(node, i), scan))
                    return false;
            }
            for (size_t i = 0; i < ast_roster_shared_count(node); i++) {
                if (!runtime_none_scan_node(ast_roster_shared(node, i), scan))
                    return false;
            }
            for (size_t i = 0; i < ast_roster_method_count(node); i++) {
                if (!runtime_none_scan_node(ast_roster_method(node, i), scan))
                    return false;
            }
            return true;
        case AST_RELATION_DECL:
            return runtime_none_scan_relation_decl(node, scan);
        case AST_EFFECT_DECL:
            return runtime_none_scan_effect_decl(node, scan);
        case AST_LAMBDA_EXPR:
            if (node->data.lambda_expr.is_async)
                return runtime_none_record(scan, node, "async-lambda");
            return runtime_none_scan_list(node->data.lambda_expr.params,
                                          node->data.lambda_expr.param_count,
                                          scan) &&
                   runtime_none_scan_node(node->data.lambda_expr.body, scan) &&
                   runtime_none_scan_node(node->data.lambda_expr.return_type, scan);
        case AST_NAMESPACE_DECL:
            return runtime_none_scan_list(node->data.namespace_decl.statements,
                                          node->data.namespace_decl.count,
                                          scan);
        case AST_UNSAFE_BLOCK:
            return runtime_none_scan_node(node->data.unsafe_block.body, scan);
        case AST_DEFER_STMT:
            return runtime_none_scan_node(node->data.defer_stmt.body, scan);
        default:
            return true;
    }
}

bool
runtime_none_validate_ast(const ASTNode *ast, char *message, size_t message_size)
{
    RuntimeNoneScan scan = {0};

    if (runtime_none_scan_node(ast, &scan))
        return true;

    if (message != NULL && message_size > 0) {
        if (scan.line > 0) {
            snprintf(message,
                     message_size,
                     "PGY_DRIVER_RUNTIME_NONE_UNSUPPORTED: --runtime=none rejects runtime-dependent surface '%s' at line %u, column %u. "
                     "Reason: no-runtime mode cannot lower scheduler, channel, intent, zone/world, or event runtime contracts yet. "
                     "Fix: use --runtime=default or remove that surface from the freestanding build.",
                     scan.surface != NULL ? scan.surface : "unknown",
                     scan.line,
                     scan.column);
        } else {
            snprintf(message,
                     message_size,
                     "PGY_DRIVER_RUNTIME_NONE_UNSUPPORTED: --runtime=none rejects runtime-dependent surface '%s'. "
                     "Reason: no-runtime mode cannot lower scheduler, channel, intent, zone/world, or event runtime contracts yet. "
                     "Fix: use --runtime=default or remove that surface from the freestanding build.",
                     scan.surface != NULL ? scan.surface : "unknown");
        }
    }
    return false;
}
