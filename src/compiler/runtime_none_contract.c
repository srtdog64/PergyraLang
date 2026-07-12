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
runtime_none_scan_func_params(const ASTNode *func_decl, RuntimeNoneScan *scan)
{
    size_t count = ast_func_param_count(func_decl);

    for (size_t i = 0; i < count; i++) {
        FuncParam *param = ast_func_param(func_decl, i);
        if (param != NULL && !runtime_none_scan_node(param->type, scan))
            return false;
        if (param != NULL && !runtime_none_scan_node(param->default_value, scan))
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
            {
                size_t statement_count = 0;
                ASTNode **statements =
                    ast_program_statements(node, &statement_count);
                return runtime_none_scan_list(statements, statement_count, scan);
            }
        case AST_FUNC_DECL:
            if (node->is_async_decl)
                return runtime_none_record(scan, node, "async-func");
            if (!runtime_none_scan_func_params(node, scan))
                return false;
            if (!runtime_none_scan_node(ast_func_return_type(node), scan))
                return false;
            if (!runtime_none_scan_node(ast_func_body(node), scan))
                return false;
            {
                size_t ability_count = 0;
                ASTNode **abilities =
                    ast_func_required_abilities(node, &ability_count);
                return runtime_none_scan_list(abilities, ability_count, scan);
            }
        case AST_CLASS_DECL:
            {
                size_t field_count = 0;
                ClassField **fields = ast_class_fields(node, &field_count);
                for (size_t i = 0; i < field_count; i++) {
                    if (fields != NULL && fields[i] != NULL &&
                        !runtime_none_scan_node(fields[i]->type, scan))
                        return false;
                }
            }
            {
                size_t method_count = 0;
                ASTNode **methods = ast_class_methods(node, &method_count);
                return runtime_none_scan_list(methods, method_count, scan);
            }
        case AST_EXTERN_BLOCK:
            {
                size_t extern_count = 0;
                ASTNode **extern_decls =
                    ast_extern_block_declarations(node, &extern_count);
                return runtime_none_scan_list(extern_decls, extern_count, scan);
            }
        case AST_LET_DECL:
            return runtime_none_scan_node(ast_let_type(node), scan) &&
                   runtime_none_scan_node(ast_let_initializer(node), scan);
        case AST_LET_DESTRUCTURE:
            return runtime_none_scan_node(
                ast_let_destructure_initializer(node), scan);
        case AST_TYPE_ALIAS:
            return runtime_none_scan_node(ast_type_alias_target_type(node), scan);
        case AST_WITH_STMT:
            return runtime_none_scan_node(ast_with_slot_type(node), scan) &&
                   runtime_none_scan_node(ast_with_body(node), scan);
        case AST_BLOCK:
            {
                size_t statement_count = 0;
                ASTNode **statements = ast_block_statements(node, &statement_count);
                if (ast_block_is_pin_block(node))
                    return runtime_none_record(scan, node, "pin");
                return runtime_none_scan_list(statements, statement_count, scan);
            }
        case AST_FOR_LOOP:
            return runtime_none_scan_node(ast_for_range_start(node), scan) &&
                   runtime_none_scan_node(ast_for_range_end(node), scan) &&
                   runtime_none_scan_node(ast_for_iterable(node), scan) &&
                   runtime_none_scan_node(ast_for_body(node), scan);
        case AST_WHILE_LOOP:
            return runtime_none_scan_node(ast_while_condition(node), scan) &&
                   runtime_none_scan_node(ast_while_body(node), scan);
        case AST_IF_STMT:
            return runtime_none_scan_node(ast_if_condition(node), scan) &&
                   runtime_none_scan_node(ast_if_then_branch(node), scan) &&
                   runtime_none_scan_node(ast_if_else_branch(node), scan);
        case AST_RETURN:
            return runtime_none_scan_node(ast_return_value(node), scan);
        case AST_MATCH_STMT:
            if (!runtime_none_scan_node(ast_match_subject(node), scan))
                return false;
            if (!runtime_none_scan_list(ast_match_cases(node, NULL),
                                        ast_match_case_count(node),
                                        scan))
                return false;
            return runtime_none_scan_node(ast_match_default_body(node), scan);
        case AST_MATCH_CASE:
            if (!runtime_none_scan_node(ast_match_case_pattern(node), scan))
                return false;
            if (!runtime_none_scan_list(ast_match_case_patterns(node, NULL),
                                        ast_match_case_pattern_count(node),
                                        scan))
                return false;
            return runtime_none_scan_node(ast_match_case_guard(node), scan) &&
                   runtime_none_scan_node(ast_match_case_body(node), scan);
        case AST_BINARY:
            return runtime_none_scan_node(ast_binary_left(node), scan) &&
                   runtime_none_scan_node(ast_binary_right(node), scan);
        case AST_UNARY:
            return runtime_none_scan_node(ast_unary_operand(node), scan);
        case AST_CALL:
            {
                size_t arg_count = 0;
                ASTNode **args = ast_call_arguments(node, &arg_count);
                if (!runtime_none_scan_node(ast_call_callee(node), scan))
                    return false;
                return runtime_none_scan_list(args, arg_count, scan);
            }
        case AST_MEMBER_ACCESS:
            return runtime_none_scan_node(ast_member_object(node), scan);
        case AST_ARRAY_ACCESS:
            return runtime_none_scan_node(ast_array_access_array(node), scan) &&
                   runtime_none_scan_node(ast_array_access_index(node), scan);
        case AST_ARRAY_LITERAL:
            for (size_t i = 0; i < ast_array_literal_count(node); i++) {
                if (!runtime_none_scan_node(ast_array_literal_element(node, i), scan))
                    return false;
            }
            return true;
        case AST_TUPLE_LITERAL:
            for (size_t i = 0; i < ast_tuple_literal_count(node); i++) {
                if (!runtime_none_scan_node(ast_tuple_literal_element(node, i), scan))
                    return false;
            }
            return true;
        case AST_ASSIGNMENT:
            return runtime_none_scan_node(ast_assignment_target(node), scan) &&
                   runtime_none_scan_node(ast_assignment_value(node), scan);
        case AST_TYPE:
            for (size_t i = 0; i < ast_type_tuple_element_count(node); i++) {
                if (!runtime_none_scan_node(ast_type_tuple_element(node, i), scan))
                    return false;
            }
            return true;
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
            return runtime_none_scan_node(ast_role_parallel_block(node), scan);
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
            return runtime_none_scan_node(ast_party_extends(node), scan);
        case AST_PARTY_SHARED:
            return runtime_none_scan_node(ast_party_shared_type(node), scan) &&
                   runtime_none_scan_node(
                       ast_party_shared_initializer(node), scan);
        case AST_PARTY_INSTANCE:
            for (size_t i = 0; i < ast_party_instance_assignment_count(node); i++) {
                if (!runtime_none_scan_node(
                        ast_party_instance_assignment_value(node, i), scan))
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
            if (ast_lambda_is_async(node))
                return runtime_none_record(scan, node, "async-lambda");
            return runtime_none_scan_list(ast_lambda_params(node, NULL),
                                          ast_lambda_param_count(node),
                                          scan) &&
                   runtime_none_scan_node(ast_lambda_body(node), scan) &&
                   runtime_none_scan_node(ast_lambda_return_type(node), scan);
        case AST_NAMESPACE_DECL:
            return runtime_none_scan_list(
                ast_namespace_statements(node, NULL),
                ast_namespace_statement_count(node),
                scan);
        case AST_UNSAFE_BLOCK:
            return runtime_none_scan_node(ast_unsafe_block_body(node), scan);
        case AST_TRANSACTION_BLOCK:
            return runtime_none_scan_node(ast_transaction_block_body(node), scan);
        case AST_DEFER_STMT:
            return runtime_none_scan_node(ast_defer_body(node), scan);
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
