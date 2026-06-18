#include "module_normalizer_internal.h"

#include <stdlib.h>
#include <string.h>

#include "../common/string_compat.h"
#include "../parser/ast_api.h"

static const char *module_rename_scope_lookup(const ModuleRenameScope *scope,
                                              const char *name);

static const char *
module_rename_scope_lookup(const ModuleRenameScope *scope, const char *name)
{
    for (const ModuleRenameScope *s = scope; s != NULL; s = s->parent) {
        for (size_t i = 0; i < s->count; i++) {
            if (strcmp(s->entries[i].old_name, name) == 0)
                return s->entries[i].new_name;
        }
    }
    return NULL;
}

static void
normalize_generic_params(GenericParams *params,
                         ModuleRenameScope *scope,
                         ModuleShadowNames *shadow)
{
    if (params == NULL)
        return;
    size_t saved = shadow->count;
    size_t param_count = ast_generic_param_count(params);
    for (size_t i = 0; i < param_count; i++) {
        GenericParam *param = ast_generic_param_at(params, i);
        if (ast_generic_param_name(param) != NULL)
            module_shadow_push(shadow, ast_generic_param_name(param));
    }
    for (size_t i = 0; i < param_count; i++) {
        GenericParam *param = ast_generic_param_at(params, i);
        if (param == NULL)
            continue;
        module_normalizer_normalize_node_refs(ast_generic_param_constraint(param),
                                              scope, shadow);
        module_normalizer_normalize_node_refs(ast_generic_param_default_type(param),
                                              scope, shadow);
    }
    module_shadow_pop_to(shadow, saved);
}

static void
normalize_type_node(ASTNode *node,
                    ModuleRenameScope *scope,
                    ModuleShadowNames *shadow)
{
    const char *type_name;
    GenericParams *generic_args;

    if (node == NULL || node->type != AST_TYPE)
        return;

    type_name = ast_type_name(node);
    if (type_name != NULL
        && !module_shadow_contains(shadow, type_name)) {
        const char *replacement =
            module_rename_scope_lookup(scope, type_name);
        if (replacement != NULL && strcmp(type_name, replacement) != 0) {
            (void)ast_replace_type_name_copy(node, replacement);
        }
    }

    generic_args = ast_type_generic_args(node);
    if (generic_args != NULL) {
        size_t generic_count = ast_generic_param_count(generic_args);
        for (size_t i = 0; i < generic_count; i++) {
            GenericParam *arg = ast_generic_param_at(generic_args, i);
            if (arg == NULL)
                continue;
            module_normalizer_normalize_node_refs(ast_generic_param_constraint(arg),
                                                  scope, shadow);
            module_normalizer_normalize_node_refs(ast_generic_param_default_type(arg),
                                                  scope, shadow);
        }
    }
}

static void
normalize_call_args(ASTNode **args,
                    size_t count,
                    ModuleRenameScope *scope,
                    ModuleShadowNames *shadow)
{
    for (size_t i = 0; i < count; i++)
        module_normalizer_normalize_node_refs(args[i], scope, shadow);
}

void
module_normalizer_normalize_node_refs(ASTNode *node,
                                      ModuleRenameScope *scope,
                                      ModuleShadowNames *shadow)
{
    if (node == NULL)
        return;

    if (module_normalizer_normalize_domain_ref_node(node, scope, shadow))
        return;

    switch (node->type) {
        case AST_PROGRAM:
        case AST_NAMESPACE_DECL:
        case AST_IMPORT_DECL:
        case AST_ENUM_DECL:
            return;

        case AST_IDENTIFIER:
            if (ast_identifier_name(node) != NULL
                && !module_shadow_contains(shadow, ast_identifier_name(node))) {
                const char *replacement =
                    module_rename_scope_lookup(scope, ast_identifier_name(node));
                if (replacement != NULL
                    && strcmp(ast_identifier_name(node), replacement) != 0) {
                    (void)ast_replace_identifier_name_copy(node, replacement);
                }
            }
            return;

        case AST_TYPE:
            normalize_type_node(node, scope, shadow);
            return;

        case AST_LET_DECL:
            module_normalizer_normalize_node_refs(ast_let_type(node), scope, shadow);
            module_normalizer_normalize_node_refs(ast_let_initializer(node), scope, shadow);
            return;

        case AST_FUNC_DECL: {
            GenericParams *generic_params =
                ast_declaration_generic_params(node);
            normalize_generic_params(generic_params, scope, shadow);
            size_t saved = shadow->count;
            size_t generic_count = ast_generic_param_count(generic_params);
            for (size_t i = 0; i < generic_count; i++) {
                GenericParam *gp = ast_generic_param_at(generic_params, i);
                if (ast_generic_param_name(gp) != NULL)
                    module_shadow_push(shadow, ast_generic_param_name(gp));
            }
            for (size_t i = 0; i < ast_func_param_count(node); i++) {
                FuncParam *param = ast_func_param(node, i);
                if (param == NULL)
                    continue;
                module_normalizer_normalize_node_refs(param->type, scope, shadow);
                module_normalizer_normalize_node_refs(param->default_value, scope, shadow);
                module_shadow_push(shadow, param->name);
            }
            module_normalizer_normalize_node_refs(ast_func_return_type(node), scope, shadow);
            module_normalizer_normalize_node_refs(ast_func_body(node), scope, shadow);
            module_shadow_pop_to(shadow, saved);
            return;
        }

        case AST_CLASS_DECL:
            normalize_generic_params(
                ast_declaration_generic_params(node), scope, shadow);
            size_t field_count = 0;
            ClassField **fields = ast_class_fields(node, &field_count);
            for (size_t i = 0; i < field_count; i++) {
                if (fields != NULL && fields[i] != NULL) {
                    module_normalizer_normalize_node_refs(
                        fields[i]->type, scope, shadow);
                }
            }
            size_t method_count = 0;
            ASTNode **methods = ast_class_methods(node, &method_count);
            for (size_t i = 0; i < method_count; i++)
                module_normalizer_normalize_node_refs(
                    methods != NULL ? methods[i] : NULL, scope, shadow);
            return;

        case AST_ABILITY_DECL:
            normalize_generic_params(
                ast_declaration_generic_params(node), scope, shadow);
            for (size_t i = 0; i < ast_ability_require_field_count(node); i++)
                module_normalizer_normalize_node_refs(
                    ast_ability_require_field(node, i), scope, shadow);
            for (size_t i = 0; i < ast_ability_method_count(node); i++)
                module_normalizer_normalize_node_refs(ast_ability_method(node, i), scope, shadow);
            return;

        case AST_ROLE_DECL:
            module_normalizer_normalize_node_refs(ast_role_for_type(node), scope, shadow);
            normalize_generic_params(
                ast_declaration_generic_params(node), scope, shadow);
            for (size_t i = 0; i < ast_role_include_count(node); i++)
                module_normalizer_normalize_node_refs(ast_role_include(node, i), scope, shadow);
            for (size_t i = 0; i < ast_role_impl_count(node); i++)
                module_normalizer_normalize_node_refs(ast_role_impl(node, i), scope, shadow);
            module_normalizer_normalize_node_refs(ast_role_parallel_block(node), scope, shadow);
            return;

        case AST_PARTY_DECL:
            module_normalizer_normalize_node_refs(
                ast_party_extends(node), scope, shadow);
            normalize_generic_params(
                ast_declaration_generic_params(node), scope, shadow);
            for (size_t i = 0; i < ast_party_role_count(node); i++)
                module_normalizer_normalize_node_refs(ast_party_role(node, i), scope, shadow);
            for (size_t i = 0; i < ast_party_shared_count(node); i++)
                module_normalizer_normalize_node_refs(ast_party_shared(node, i), scope, shadow);
            for (size_t i = 0; i < ast_party_method_count(node); i++)
                module_normalizer_normalize_node_refs(ast_party_method(node, i), scope, shadow);
            return;

        case AST_ROSTER_DECL:
            normalize_generic_params(
                ast_declaration_generic_params(node), scope, shadow);
            for (size_t i = 0; i < ast_roster_party_count(node); i++)
                module_normalizer_normalize_node_refs(ast_roster_party(node, i), scope, shadow);
            for (size_t i = 0; i < ast_roster_shared_count(node); i++)
                module_normalizer_normalize_node_refs(ast_roster_shared(node, i), scope, shadow);
            for (size_t i = 0; i < ast_roster_method_count(node); i++)
                module_normalizer_normalize_node_refs(ast_roster_method(node, i), scope, shadow);
            return;

        case AST_EVENT_DECL:
            for (size_t i = 0; i < ast_event_param_count(node); i++)
                module_normalizer_normalize_node_refs(ast_event_param(node, i), scope, shadow);
            module_normalizer_normalize_node_refs(ast_event_return_type(node), scope, shadow);
            return;

        case AST_REQUIRE_FIELD:
            module_normalizer_normalize_node_refs(ast_require_field_type(node), scope, shadow);
            return;

        case AST_PARTY_SHARED:
            module_normalizer_normalize_node_refs(
                ast_party_shared_type(node), scope, shadow);
            module_normalizer_normalize_node_refs(
                ast_party_shared_initializer(node), scope, shadow);
            return;

        case AST_SYSTEMIC_SLOT:
            /*
             * Intentional AST mutation seam: import normalization owns
             * renaming embedded roster slot party types through the AST
             * mutator API. Read-only consumers must use ast_roster_slot_*
             * accessors instead.
             */
            if (ast_roster_slot_party_type(node) != NULL) {
                const char *replacement =
                    module_rename_scope_lookup(scope,
                        ast_roster_slot_party_type(node));
                if (replacement != NULL
                    && strcmp(ast_roster_slot_party_type(node), replacement) != 0) {
                    (void)ast_roster_slot_replace_party_type(node, replacement);
                }
            }
            return;

        case AST_WORLD_SYSTEMIC:
            /*
             * Intentional AST mutation seam: import normalization owns
             * renaming embedded world roster type references through the AST
             * mutator API. Read-only consumers must use ast_world_roster_*
             * accessors instead.
             */
            if (ast_world_roster_type_name(node) != NULL) {
                const char *replacement =
                    module_rename_scope_lookup(scope,
                        ast_world_roster_type_name(node));
                if (replacement != NULL
                    && strcmp(ast_world_roster_type_name(node), replacement) != 0) {
                    (void)ast_world_roster_replace_type_name(node, replacement);
                }
            }
            module_normalizer_normalize_node_refs(
                ast_world_roster_initializer(node), scope, shadow);
            return;

        case AST_BLOCK:
        case AST_ASYNC_BLOCK: {
            size_t saved = shadow->count;
            ASTNode **stmts = node->type == AST_BLOCK
                ? ast_block_statements(node, NULL)
                : ast_async_block_statements(node, NULL);
            size_t count = node->type == AST_BLOCK
                ? ast_block_statement_count(node)
                : ast_async_block_statement_count(node);
            for (size_t i = 0; i < count; i++) {
                ASTNode *stmt = stmts[i];
                module_normalizer_normalize_node_refs(stmt, scope, shadow);
                if (stmt != NULL && stmt->type == AST_LET_DECL)
                    module_shadow_push(shadow, ast_let_name(stmt));
                if (stmt != NULL && stmt->type == AST_FOR_LOOP
                    && ast_for_variable(stmt) != NULL)
                    module_shadow_push(shadow, ast_for_variable(stmt));
            }
            module_shadow_pop_to(shadow, saved);
            return;
        }

        case AST_WITH_STMT: {
            module_normalizer_normalize_node_refs(ast_with_slot_type(node), scope, shadow);
            size_t saved = shadow->count;
            if (ast_with_alias(node) != NULL)
                module_shadow_push(shadow, ast_with_alias(node));
            module_normalizer_normalize_node_refs(ast_with_body(node), scope, shadow);
            module_shadow_pop_to(shadow, saved);
            return;
        }

        case AST_FOR_LOOP: {
            module_normalizer_normalize_node_refs(ast_for_range_start(node), scope, shadow);
            module_normalizer_normalize_node_refs(ast_for_range_end(node), scope, shadow);
            size_t saved = shadow->count;
            module_shadow_push(shadow, ast_for_variable(node));
            module_normalizer_normalize_node_refs(ast_for_body(node), scope, shadow);
            module_shadow_pop_to(shadow, saved);
            return;
        }

        case AST_WHILE_LOOP:
            module_normalizer_normalize_node_refs(ast_while_condition(node), scope, shadow);
            module_normalizer_normalize_node_refs(ast_while_body(node), scope, shadow);
            return;

        case AST_IF_STMT:
            module_normalizer_normalize_node_refs(ast_if_condition(node), scope, shadow);
            module_normalizer_normalize_node_refs(ast_if_then_branch(node), scope, shadow);
            module_normalizer_normalize_node_refs(ast_if_else_branch(node), scope, shadow);
            return;

        case AST_RETURN:
            module_normalizer_normalize_node_refs(ast_return_value(node), scope, shadow);
            return;

        case AST_BINARY:
            module_normalizer_normalize_node_refs(ast_binary_left(node), scope, shadow);
            module_normalizer_normalize_node_refs(ast_binary_right(node), scope, shadow);
            return;

        case AST_UNARY:
            module_normalizer_normalize_node_refs(ast_unary_operand(node), scope, shadow);
            return;

        case AST_CALL:
            {
                size_t arg_count = 0;
                ASTNode **args = ast_call_arguments(node, &arg_count);
                module_normalizer_normalize_node_refs(ast_call_callee(node), scope, shadow);
                normalize_call_args(args, arg_count, scope, shadow);
            }
            return;

        case AST_MEMBER_ACCESS:
            module_normalizer_normalize_node_refs(ast_member_object(node), scope, shadow);
            return;

        case AST_ARRAY_ACCESS:
            module_normalizer_normalize_node_refs(ast_array_access_array(node), scope, shadow);
            module_normalizer_normalize_node_refs(ast_array_access_index(node), scope, shadow);
            return;

        case AST_ARRAY_LITERAL:
            for (size_t i = 0; i < ast_array_literal_count(node); i++)
                module_normalizer_normalize_node_refs(ast_array_literal_element(node, i), scope, shadow);
            return;

        case AST_ASSIGNMENT:
            module_normalizer_normalize_node_refs(ast_assignment_target(node), scope, shadow);
            module_normalizer_normalize_node_refs(ast_assignment_value(node), scope, shadow);
            return;

        case AST_AWAIT_EXPR:
            module_normalizer_normalize_node_refs(ast_await_expression(node), scope, shadow);
            return;

        case AST_CHANNEL_SEND:
            module_normalizer_normalize_node_refs(ast_channel_send_channel(node), scope, shadow);
            module_normalizer_normalize_node_refs(ast_channel_send_value(node), scope, shadow);
            return;

        case AST_CHANNEL_RECV:
            module_normalizer_normalize_node_refs(ast_channel_recv_channel(node), scope, shadow);
            return;

        case AST_SELECT_STMT:
            normalize_call_args(ast_select_cases(node, NULL), ast_select_case_count(node), scope, shadow);
            module_normalizer_normalize_node_refs(ast_select_default_case(node), scope, shadow);
            return;

        case AST_MATCH_STMT:
            module_normalizer_normalize_node_refs(ast_match_subject(node), scope, shadow);
            normalize_call_args(ast_match_cases(node, NULL), ast_match_case_count(node), scope, shadow);
            module_normalizer_normalize_node_refs(ast_match_default_body(node), scope, shadow);
            return;

        case AST_MATCH_CASE:
            module_normalizer_normalize_node_refs(ast_match_case_pattern(node), scope, shadow);
            module_normalizer_normalize_node_refs(ast_match_case_guard(node), scope, shadow);
            module_normalizer_normalize_node_refs(ast_match_case_body(node), scope, shadow);
            return;

        case AST_EVENT_SUBSCRIBE:
        case AST_EVENT_UNSUBSCRIBE:
            module_normalizer_normalize_node_refs(ast_event_op_event(node), scope, shadow);
            module_normalizer_normalize_node_refs(ast_event_op_handler(node), scope, shadow);
            return;

        case AST_EVENT_INVOKE:
            module_normalizer_normalize_node_refs(ast_event_invoke_event(node), scope, shadow);
            normalize_call_args(ast_event_invoke_arguments(node, NULL), ast_event_invoke_arg_count(node), scope, shadow);
            return;

        case AST_UNSAFE_BLOCK:
            module_normalizer_normalize_node_refs(ast_unsafe_block_body(node), scope, shadow);
            return;
        case AST_TRANSACTION_BLOCK:
            module_normalizer_normalize_node_refs(ast_transaction_block_body(node), scope, shadow);
            return;

        case AST_DEFER_STMT:
            module_normalizer_normalize_node_refs(ast_defer_body(node), scope, shadow);
            return;

        case AST_NUMBER:
        case AST_STRING:
        case AST_BOOLEAN:
        case AST_BREAK:
        case AST_CONTINUE:
            return;

        default:
            return;
    }
}
