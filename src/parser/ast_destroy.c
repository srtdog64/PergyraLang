/*
 * Copyright (c) 2025 Pergyra Language Project
 * AST destruction implementation.
 */

#include "ast.h"
#include "ast_destroy_internal.h"
#include <stdlib.h>

void
ast_destroy_generic_params(GenericParams* params) {
    if (params == NULL) return;

    for (size_t i = 0; i < params->count; i++) {
        GenericParam* param = params->params[i];
        if (param == NULL) continue;
        free(param->name);
        ast_destroy(param->constraint);
        ast_destroy(param->default_type);
        free(param);
    }

    free(params->params);
    free(params);
}

void
ast_destroy_where_clause(WhereClause* clause) {
    if (clause == NULL) return;

    for (size_t i = 0; i < clause->count; i++) {
        TypeConstraint* constraint = clause->constraints[i];
        if (constraint == NULL) continue;
        free(constraint->type_param);
        for (size_t j = 0; j < constraint->bound_count; j++) {
            ast_destroy(constraint->bounds[j]);
        }
        free(constraint->bounds);
        free(constraint);
    }

    free(clause->constraints);
    free(clause);
}

void
ast_destroy_structured_comment(StructuredComment* comment) {
    while (comment != NULL) {
        StructuredComment* next = comment->next;
        for (size_t i = 0; i < comment->tag_count; i++) {
            if (comment->tags[i] == NULL) continue;
            free(comment->tags[i]->content);
            free(comment->tags[i]);
        }
        free(comment->tags);
        free(comment);
        comment = next;
    }
}

void ast_destroy(ASTNode* node) {
    if (!node) return;

    if (ast_destroy_domain_node(node)) {
        free(node->origin_path);
        free(node);
        return;
    }
    
    switch (node->type) {
        case AST_PROGRAM:
            for (size_t i = 0; i < node->data.program.count; i++) {
                ast_destroy(node->data.program.statements[i]);
            }
            free(node->data.program.statements);
            break;
            
        case AST_FUNC_DECL:
            free(node->data.func_decl.name);
            for (size_t i = 0; i < node->data.func_decl.param_count; i++) {
                free(node->data.func_decl.params[i]->name);
                ast_destroy(node->data.func_decl.params[i]->type);
                ast_destroy(node->data.func_decl.params[i]->default_value);
                free(node->data.func_decl.params[i]);
            }
            free(node->data.func_decl.params);
            ast_destroy(node->data.func_decl.return_type);
            ast_destroy(node->data.func_decl.body);
            ast_destroy_generic_params(node->data.func_decl.generic_params);
            ast_destroy_where_clause(node->data.func_decl.where_clause);
            for (size_t i = 0; i < node->data.func_decl.required_ability_count; i++)
                ast_destroy(node->data.func_decl.required_abilities[i]);
            free(node->data.func_decl.required_abilities);
            free(node->data.func_decl.within_zone);
            free(node->data.func_decl.causes_effect);
            for (size_t i = 0; i < node->data.func_decl.authorized_by_count; i++)
                free(node->data.func_decl.authorized_by[i]);
            free(node->data.func_decl.authorized_by);
            ast_destroy_structured_comment(node->data.func_decl.doc_comment);
            break;
            
        case AST_CLASS_DECL:
            free(node->data.class_decl.name);
            for (size_t i = 0; i < node->data.class_decl.field_count; i++) {
                free(node->data.class_decl.fields[i]->name);
                ast_destroy(node->data.class_decl.fields[i]->type);
                ast_destroy(node->data.class_decl.fields[i]->default_value);
                free(node->data.class_decl.fields[i]);
            }
            free(node->data.class_decl.fields);
            for (size_t i = 0; i < node->data.class_decl.method_count; i++) {
                ast_destroy(node->data.class_decl.methods[i]);
            }
            free(node->data.class_decl.methods);
            for (size_t i = 0;
                 i < node->data.class_decl.field_destructure_count; i++) {
                ast_destroy(node->data.class_decl.field_destructures[i]);
            }
            free(node->data.class_decl.field_destructures);
            ast_destroy_generic_params(node->data.class_decl.generic_params);
            ast_destroy_where_clause(node->data.class_decl.where_clause);
            ast_destroy_structured_comment(node->data.class_decl.doc_comment);
            break;

        case AST_ENUM_DECL:
            free(node->data.enum_decl.name);
            for (size_t i = 0; i < node->data.enum_decl.variant_count; i++) {
                free(node->data.enum_decl.variants[i]);
                if (node->data.enum_decl.variant_params != NULL) {
                    size_t pc = node->data.enum_decl.variant_param_counts != NULL
                        ? node->data.enum_decl.variant_param_counts[i] : 0;
                    for (size_t p = 0; p < pc; p++)
                        ast_destroy(node->data.enum_decl.variant_params[i][p]);
                    free(node->data.enum_decl.variant_params[i]);
                }
            }
            free(node->data.enum_decl.variants);
            free(node->data.enum_decl.variant_params);
            free(node->data.enum_decl.variant_param_counts);
            for (size_t i = 0; i < node->data.enum_decl.method_count; i++)
                ast_destroy(node->data.enum_decl.methods[i]);
            free(node->data.enum_decl.methods);
            break;

        case AST_EXTERN_BLOCK:
            free(node->data.extern_block.abi);
            for (size_t i = 0; i < node->data.extern_block.count; i++) {
                ast_destroy(node->data.extern_block.declarations[i]);
            }
            free(node->data.extern_block.declarations);
            break;
            
        case AST_LET_DECL:
            free(node->data.let_decl.name);
            ast_destroy(node->data.let_decl.type);
            ast_destroy(node->data.let_decl.initializer);
            break;
        case AST_TYPE_ALIAS:
            free(node->data.type_alias.name);
            ast_destroy(node->data.type_alias.target_type);
            break;

        case AST_LET_DESTRUCTURE:
            for (size_t i = 0; i < node->data.let_destructure.name_count; i++)
                free(node->data.let_destructure.names[i]);
            free(node->data.let_destructure.names);
            ast_destroy(node->data.let_destructure.initializer);
            break;

        case AST_WITH_STMT:
            ast_destroy(node->data.with_stmt.slot_type);
            free(node->data.with_stmt.alias);
            ast_destroy(node->data.with_stmt.body);
            free(node->data.with_stmt.security_level);
            break;
            
        case AST_PARALLEL_BLOCK:
            for (size_t i = 0; i < node->data.parallel.task_count; i++) {
                ast_destroy(node->data.parallel.tasks[i]);
            }
            free(node->data.parallel.tasks);
            break;
            
        case AST_BLOCK:
            for (size_t i = 0; i < node->data.block.count; i++) {
                ast_destroy(node->data.block.statements[i]);
            }
            free(node->data.block.pin_source_name);
            free(node->data.block.pin_view_name);
            free(node->data.block.statements);
            break;
            
        case AST_FOR_LOOP:
            free(node->data.for_loop.label);
            free(node->data.for_loop.variable);
            ast_destroy(node->data.for_loop.range_start);
            ast_destroy(node->data.for_loop.range_end);
            ast_destroy(node->data.for_loop.iterable);
            ast_destroy(node->data.for_loop.body);
            break;
            
        case AST_WHILE_LOOP:
            free(node->data.while_loop.label);
            ast_destroy(node->data.while_loop.condition);
            ast_destroy(node->data.while_loop.body);
            break;

        case AST_BREAK:
            free(node->data.break_stmt.label);
            break;

        case AST_CONTINUE:
            free(node->data.continue_stmt.label);
            break;

        case AST_MATCH_STMT:
            ast_destroy(node->data.match_stmt.subject);
            for (size_t i = 0; i < node->data.match_stmt.case_count; i++)
                ast_destroy(node->data.match_stmt.cases[i]);
            free(node->data.match_stmt.cases);
            ast_destroy(node->data.match_stmt.default_body);
            break;

        case AST_MATCH_CASE:
            if (node->data.match_case.patterns != NULL) {
                for (size_t i = 0; i < node->data.match_case.pattern_count; i++)
                    ast_destroy(node->data.match_case.patterns[i]);
                free(node->data.match_case.patterns);
            } else {
                ast_destroy(node->data.match_case.pattern);
            }
            ast_destroy(node->data.match_case.guard);
            ast_destroy(node->data.match_case.body);
            break;

        case AST_IF_STMT:
            ast_destroy(node->data.if_stmt.condition);
            ast_destroy(node->data.if_stmt.then_branch);
            ast_destroy(node->data.if_stmt.else_branch);
            break;
            
        case AST_RETURN:
            ast_destroy(node->data.return_stmt.value);
            break;
            
        case AST_BINARY:
            ast_destroy(node->data.binary.left);
            ast_destroy(node->data.binary.right);
            break;
            
        case AST_UNARY:
            ast_destroy(node->data.unary.operand);
            break;
            
        case AST_CALL:
            ast_destroy(node->data.call.callee);
            for (size_t i = 0; i < node->data.call.arg_count; i++) {
                ast_destroy(node->data.call.arguments[i]);
                if (node->data.call.arg_names != NULL)
                    free(node->data.call.arg_names[i]);
            }
            free(node->data.call.arguments);
            free(node->data.call.arg_names);
            ast_destroy_generic_params(node->data.call.generic_args);
            break;
            
        case AST_MEMBER_ACCESS:
            ast_destroy(node->data.member.object);
            free(node->data.member.name);
            break;
            
        case AST_ARRAY_ACCESS:
            ast_destroy(node->data.array_access.array);
            ast_destroy(node->data.array_access.index);
            break;
            
        case AST_ASSIGNMENT:
            ast_destroy(node->data.assignment.target);
            ast_destroy(node->data.assignment.value);
            break;

        case AST_TUPLE_LITERAL:
            for (size_t i = 0; i < node->data.tuple_literal.count; i++)
                ast_destroy(node->data.tuple_literal.elements[i]);
            free(node->data.tuple_literal.elements);
            break;

        case AST_MAP_LITERAL:
            for (size_t i = 0; i < node->data.map_literal.count; i++) {
                ast_destroy(node->data.map_literal.keys[i]);
                ast_destroy(node->data.map_literal.values[i]);
            }
            free(node->data.map_literal.keys);
            free(node->data.map_literal.values);
            break;

        case AST_CAST:
            ast_destroy(node->data.cast.operand);
            free(node->data.cast.target_type);
            break;

        case AST_TYPE_TEST:
            ast_destroy(node->data.type_test.operand);
            free(node->data.type_test.target_type);
            break;

        case AST_STRING:
            free(node->data.string.value);
            break;
            
        case AST_IDENTIFIER:
            free(node->data.identifier.name);
            break;
            
        case AST_TYPE:
            free(node->data.type.name);
            ast_destroy_generic_params(node->data.type.generic_args);
            if (node->data.type.tuple_elements != NULL) {
                for (size_t i = 0; i < node->data.type.tuple_element_count; i++)
                    ast_destroy(node->data.type.tuple_elements[i]);
                free(node->data.type.tuple_elements);
            }
            break;

        case AST_ASYNC_BLOCK:
            for (size_t i = 0; i < node->data.async_block.statement_count; i++) {
                ast_destroy(node->data.async_block.statements[i]);
            }
            free(node->data.async_block.statements);
            break;

        case AST_AWAIT_EXPR:
            ast_destroy(node->data.await_expr.expression);
            break;

        case AST_CHANNEL_SEND:
            ast_destroy(node->data.channel_send.channel);
            ast_destroy(node->data.channel_send.value);
            break;

        case AST_CHANNEL_RECV:
            ast_destroy(node->data.channel_recv.channel);
            break;

        case AST_SELECT_STMT:
            for (size_t i = 0; i < node->data.select_stmt.case_count; i++) {
                ast_destroy(node->data.select_stmt.cases[i]);
            }
            free(node->data.select_stmt.cases);
            ast_destroy(node->data.select_stmt.default_case);
            break;

        case AST_SPAWN_EXPR:
            ast_destroy(node->data.spawn_expr.function);
            for (size_t i = 0; i < node->data.spawn_expr.arg_count; i++) {
                ast_destroy(node->data.spawn_expr.arguments[i]);
            }
            free(node->data.spawn_expr.arguments);
            break;

        case AST_CHANNEL_TYPE:
            ast_destroy(node->data.channel_type.element_type);
            ast_destroy(node->data.channel_type.capacity);
            break;

        case AST_FUTURE_TYPE:
            ast_destroy(node->data.future_type.value_type);
            break;

        case AST_TASK_GROUP:
            for (size_t i = 0; i < node->data.task_group.task_count; i++) {
                ast_destroy(node->data.task_group.tasks[i]);
            }
            free(node->data.task_group.tasks);
            break;

        case AST_LAMBDA_EXPR:
            for (size_t i = 0; i < node->data.lambda_expr.param_count; i++)
                ast_destroy(node->data.lambda_expr.params[i]);
            free(node->data.lambda_expr.params);
            ast_destroy(node->data.lambda_expr.body);
            ast_destroy(node->data.lambda_expr.return_type);
            break;

        case AST_IMPORT_DECL:
            free(node->data.import_decl.path);
            break;

        case AST_USE_DECL:
            free(node->data.use_decl.module_name);
            break;

        case AST_NAMESPACE_DECL:
            free(node->data.namespace_decl.name);
            for (size_t i = 0; i < node->data.namespace_decl.count; i++) {
                ast_destroy(node->data.namespace_decl.statements[i]);
            }
            free(node->data.namespace_decl.statements);
            break;

        case AST_UNSAFE_BLOCK:
            ast_destroy(node->data.unsafe_block.body);
            free(node->data.unsafe_block.capability);
            break;

        case AST_DEFER_STMT:
            ast_destroy(node->data.defer_stmt.body);
            break;

        case AST_BIND_STMT:
            free(node->data.bind_stmt.party_var);
            free(node->data.bind_stmt.slot_name);
            free(node->data.bind_stmt.role_name);
            break;

        default:
            break;
    }

    free(node->origin_path);
    free(node);
}
