/*
 * Copyright (c) 2026 Pergyra Language Project
 *
 * A match subject is one language evaluation.  Native MIR historically kept
 * the source expression on each case edge, so C and LLVM could call a
 * side-effecting subject once per condition (and once more for a payload).
 * Normalize the source AST before semantic analysis:
 *
 *     match EXPR { ... }
 *
 * becomes
 *
 *     { let __pgy_match_subject_N = EXPR;
 *       match __pgy_match_subject_N { ... } }
 *
 * The existing local/MIR owners then carry one definition to both backends.
 */

#include "match_subject_single_evaluation_desugar.h"

#include "../parser/ast.h"
#include "../parser/ast_api.h"

#include <stddef.h>
#include <stdio.h>

#define PGY_MATCH_SUBJECT_PREFIX "__pgy_match_subject_"

static void match_subject_desugar_node(ASTNode *node, unsigned *counter);

static bool
match_subject_is_trivial(const ASTNode *subject)
{
    if (subject == NULL)
        return true;
    return subject->type == AST_IDENTIFIER
        || subject->type == AST_NUMBER
        || subject->type == AST_STRING
        || subject->type == AST_BOOLEAN;
}

static ASTNode *
match_subject_desugar_slot(ASTNode *match, unsigned *counter)
{
    ASTNode *subject = ast_match_subject(match);
    ASTNode *let;
    ASTNode *ident;
    ASTNode *block;
    char name[80];
    unsigned ordinal;

    if (match_subject_is_trivial(subject))
        return match;

    ordinal = counter != NULL ? (*counter)++ : 0;
    snprintf(name, sizeof(name), PGY_MATCH_SUBJECT_PREFIX "%u", ordinal);
    let = ast_create_let_declaration(name);
    ident = ast_create_identifier(name);
    block = ast_create_block();
    if (let == NULL || ident == NULL || block == NULL)
        return match;

    let->line = match->line;
    let->column = match->column;
    ident->line = match->line;
    ident->column = match->column;
    block->line = match->line;
    block->column = match->column;

    subject = ast_match_detach_subject(match);
    if (subject == NULL)
        return match;
    if (!ast_let_attach_initializer(let, subject)) {
        (void)ast_match_attach_subject(match, subject);
        return match;
    }
    if (!ast_match_attach_subject(match, ident))
        return match;

    ast_add_statement(block, let);
    ast_add_statement(block, match);
    return block;
}

static void
match_subject_desugar_statement_list(ASTNode **statements, size_t count,
                                     unsigned *counter)
{
    if (statements == NULL)
        return;
    for (size_t i = 0; i < count; i++) {
        ASTNode *statement = statements[i];
        if (statement == NULL)
            continue;
        match_subject_desugar_node(statement, counter);
        if (statement->type == AST_MATCH_STMT
            && !match_subject_is_trivial(ast_match_subject(statement))) {
            statements[i] = match_subject_desugar_slot(statement, counter);
        }
    }
}

static void
match_subject_desugar_node(ASTNode *node, unsigned *counter)
{
    ASTNode **methods = NULL;
    size_t method_count = 0;

    if (node == NULL)
        return;
    switch (node->type) {
    case AST_PROGRAM: {
        size_t count = 0;
        ASTNode **items = ast_program_statements(node, &count);
        match_subject_desugar_statement_list(items, count, counter);
        return;
    }
    case AST_BLOCK: {
        size_t count = 0;
        ASTNode **items = ast_block_statements(node, &count);
        match_subject_desugar_statement_list(items, count, counter);
        return;
    }
    case AST_ASYNC_BLOCK: {
        size_t count = 0;
        ASTNode **items = ast_async_block_statements(node, &count);
        match_subject_desugar_statement_list(items, count, counter);
        return;
    }
    case AST_PARALLEL_BLOCK: {
        size_t count = 0;
        ASTNode **items = ast_parallel_tasks(node, &count);
        match_subject_desugar_statement_list(items, count, counter);
        return;
    }
    case AST_EXTERN_BLOCK: {
        size_t count = 0;
        ASTNode **items = ast_extern_block_declarations(node, &count);
        match_subject_desugar_statement_list(items, count, counter);
        return;
    }
    case AST_NAMESPACE_DECL: {
        size_t count = 0;
        ASTNode **items = ast_namespace_statements(node, &count);
        match_subject_desugar_statement_list(items, count, counter);
        return;
    }
    case AST_FUNC_DECL:
        match_subject_desugar_node(ast_func_body(node), counter);
        return;
    case AST_FOR_LOOP:
        match_subject_desugar_node(ast_for_body(node), counter);
        return;
    case AST_WHILE_LOOP:
        match_subject_desugar_node(ast_while_body(node), counter);
        return;
    case AST_IF_STMT:
        match_subject_desugar_node(ast_if_then_branch(node), counter);
        match_subject_desugar_node(ast_if_else_branch(node), counter);
        return;
    case AST_WITH_STMT:
        match_subject_desugar_node(ast_with_body(node), counter);
        return;
    case AST_MATCH_STMT:
        for (size_t i = 0; i < ast_match_case_count(node); i++)
            match_subject_desugar_node(ast_match_case_at(node, i), counter);
        match_subject_desugar_node(ast_match_default_body(node), counter);
        return;
    case AST_MATCH_CASE:
        match_subject_desugar_node(ast_match_case_body(node), counter);
        return;
    case AST_SELECT_STMT:
        for (size_t i = 0; i < ast_select_case_count(node); i++)
            match_subject_desugar_node(ast_select_case(node, i), counter);
        match_subject_desugar_node(ast_select_default_case(node), counter);
        return;
    default:
        break;
    }

    switch (node->type) {
    case AST_CLASS_DECL:
        methods = ast_class_methods(node, &method_count);
        break;
    case AST_ENUM_DECL:
        methods = ast_enum_methods(node, &method_count);
        break;
    case AST_ABILITY_DECL:
        methods = ast_ability_methods(node, &method_count);
        break;
    case AST_IMPL_ABILITY:
        for (size_t i = 0; i < ast_impl_ability_method_count(node); i++)
            match_subject_desugar_node(ast_impl_ability_method(node, i), counter);
        return;
    case AST_PARTY_DECL:
        methods = ast_party_methods(node, &method_count);
        break;
    case AST_ROSTER_DECL:
        methods = ast_roster_methods(node, &method_count);
        break;
    case AST_WORLD_DECL:
        methods = ast_world_methods(node, &method_count);
        break;
    case AST_RELATION_DECL:
        methods = ast_relation_methods(node, &method_count);
        break;
    case AST_EFFECT_DECL:
        methods = ast_effect_methods(node, &method_count);
        break;
    case AST_ZONE_DECL:
        methods = ast_zone_methods(node, &method_count);
        break;
    default:
        return;
    }
    for (size_t i = 0; i < method_count; i++)
        match_subject_desugar_node(methods != NULL ? methods[i] : NULL, counter);
}

void
match_subject_single_evaluation_desugar_program(ASTNode *program)
{
    unsigned counter = 0;

    if (program != NULL)
        match_subject_desugar_node(program, &counter);
}
