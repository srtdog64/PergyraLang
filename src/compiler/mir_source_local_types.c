#include "mir_source_local_types.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "../common/string_compat.h"
#include "../parser/ast_api.h"
#include "mir_type_helpers.h"

void
mir_routine_source_local_type_names_clear(MIRRoutine *routine)
{
    if (routine == NULL)
        return;
    if (routine->source_local_types != NULL) {
        for (size_t i = 0; i < routine->source_local_type_count; i++) {
            free(routine->source_local_types[i].name);
            free(routine->source_local_types[i].type_name);
        }
    }
    free(routine->source_local_types);
    routine->source_local_types = NULL;
    routine->source_local_type_count = 0;
    routine->source_local_type_capacity = 0;
}

static bool
mir_source_local_type_append(MIRRoutine *routine,
                             const char *name,
                             ASTNode *type_node)
{
    if (routine == NULL || name == NULL || type_node == NULL
        || type_node->type != AST_TYPE) {
        return true;
    }
    for (size_t i = 0; i < routine->source_local_type_count; i++) {
        if (routine->source_local_types[i].name != NULL
            && strcmp(routine->source_local_types[i].name, name) == 0) {
            return true;
        }
    }
    if (routine->source_local_type_count
        == routine->source_local_type_capacity) {
        size_t next = routine->source_local_type_capacity == 0
            ? 8
            : routine->source_local_type_capacity * 2;
        if (next < routine->source_local_type_capacity
            || next > SIZE_MAX / sizeof(MIRSourceLocalType)) {
            return false;
        }
        MIRSourceLocalType *grown = realloc(routine->source_local_types,
            next * sizeof(MIRSourceLocalType));
        if (grown == NULL)
            return false;
        routine->source_local_types = grown;
        routine->source_local_type_capacity = next;
    }

    char *type_name = mir_render_type_name(type_node);
    if (type_name == NULL)
        return false;
    char *name_copy = pergyra_strdup(name);
    if (name_copy == NULL) {
        free(type_name);
        return false;
    }
    routine->source_local_types[routine->source_local_type_count].name =
        name_copy;
    routine->source_local_types[routine->source_local_type_count].type_name =
        type_name;
    routine->source_local_type_count++;
    return true;
}

static bool
mir_source_local_type_capture_node(MIRRoutine *routine, ASTNode *node)
{
    if (routine == NULL || node == NULL)
        return true;
    switch (node->type) {
    case AST_LET_DECL:
        return mir_source_local_type_append(routine,
            ast_let_name(node), ast_let_type(node));
    case AST_BLOCK: {
        size_t n = ast_block_statement_count(node);
        for (size_t i = 0; i < n; i++) {
            if (!mir_source_local_type_capture_node(routine,
                    ast_block_statement(node, i))) {
                return false;
            }
        }
        return true;
    }
    case AST_IF_STMT:
        return mir_source_local_type_capture_node(routine,
                   ast_if_then_branch(node))
            && mir_source_local_type_capture_node(routine,
                   ast_if_else_branch(node));
    case AST_WHILE_LOOP:
        return mir_source_local_type_capture_node(routine,
            ast_while_body(node));
    case AST_FOR_LOOP:
        return mir_source_local_type_capture_node(routine,
            ast_for_body(node));
    case AST_WITH_STMT:
        return mir_source_local_type_capture_node(routine,
            ast_with_body(node));
    case AST_MATCH_STMT: {
        size_t n = ast_match_case_count(node);
        for (size_t i = 0; i < n; i++) {
            ASTNode *c = ast_match_case_at(node, i);
            if (c != NULL && c->type == AST_MATCH_CASE
                && !mir_source_local_type_capture_node(routine,
                    ast_match_case_body(c))) {
                return false;
            }
        }
        return true;
    }
    default:
        return true;
    }
}

static const char *
mir_source_local_type_find_in_ast_node(ASTNode *node, const char *local_name)
{
    if (node == NULL || local_name == NULL)
        return NULL;
    switch (node->type) {
    case AST_LET_DECL: {
        const char *name = ast_let_name(node);
        if (name != NULL && strcmp(name, local_name) == 0) {
            ASTNode *type_node = ast_let_type(node);
            if (type_node != NULL && type_node->type == AST_TYPE)
                return ast_type_name(type_node);
        }
        return NULL;
    }
    case AST_BLOCK: {
        size_t n = ast_block_statement_count(node);
        for (size_t i = 0; i < n; i++) {
            const char *type_name = mir_source_local_type_find_in_ast_node(
                ast_block_statement(node, i), local_name);
            if (type_name != NULL)
                return type_name;
        }
        return NULL;
    }
    case AST_IF_STMT: {
        const char *type_name = mir_source_local_type_find_in_ast_node(
            ast_if_then_branch(node), local_name);
        if (type_name != NULL)
            return type_name;
        return mir_source_local_type_find_in_ast_node(
            ast_if_else_branch(node), local_name);
    }
    case AST_WHILE_LOOP:
        return mir_source_local_type_find_in_ast_node(
            ast_while_body(node), local_name);
    case AST_FOR_LOOP:
        return mir_source_local_type_find_in_ast_node(
            ast_for_body(node), local_name);
    case AST_WITH_STMT:
        return mir_source_local_type_find_in_ast_node(
            ast_with_body(node), local_name);
    case AST_MATCH_STMT: {
        size_t n = ast_match_case_count(node);
        for (size_t i = 0; i < n; i++) {
            ASTNode *match_case = ast_match_case_at(node, i);
            if (match_case == NULL || match_case->type != AST_MATCH_CASE)
                continue;
            const char *type_name = mir_source_local_type_find_in_ast_node(
                ast_match_case_body(match_case), local_name);
            if (type_name != NULL)
                return type_name;
        }
        return NULL;
    }
    default:
        return NULL;
    }
}

const char *
mir_source_local_type_name_in_ast(ASTNode *body, const char *local_name)
{
    return mir_source_local_type_find_in_ast_node(body, local_name);
}

bool
mir_routine_source_local_type_names_capture(MIRRoutine *routine)
{
    if (routine == NULL || routine->ast == NULL
        || routine->ast->type != AST_FUNC_DECL) {
        return true;
    }
    return mir_source_local_type_capture_node(routine,
        ast_func_body(routine->ast));
}
