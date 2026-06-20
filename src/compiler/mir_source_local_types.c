#include "mir_source_local_types.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "../common/string_compat.h"
#include "mir_decl_headers.h"
#include "mir_source_local_expr_types.h"
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
mir_source_local_type_append_name(const MIRProgram *program,
                                  MIRRoutine *routine,
                                  const char *name,
                                  const char *type_name)
{
    const char *effective_type_name = type_name;

    if (routine == NULL || name == NULL || type_name == NULL
        || type_name[0] == '\0') {
        return true;
    }
    if (program != NULL) {
        const char *alias_target =
            mir_decl_header_resolve_type_alias_target_type_name(
                program, type_name);
        if (alias_target != NULL)
            effective_type_name = alias_target;
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

    char *type_name_copy = mir_capture_type_name(NULL, effective_type_name);
    if (type_name_copy == NULL)
        return false;
    char *name_copy = pergyra_strdup(name);
    if (name_copy == NULL) {
        free(type_name_copy);
        return false;
    }
    routine->source_local_types[routine->source_local_type_count].name =
        name_copy;
    routine->source_local_types[routine->source_local_type_count].type_name =
        type_name_copy;
    routine->source_local_type_count++;
    return true;
}

static bool
mir_source_local_type_append(const MIRProgram *program,
                             MIRRoutine *routine,
                             const char *name,
                             ASTNode *type_node)
{
    char *rendered;
    bool ok;

    if (routine == NULL || name == NULL || type_node == NULL
        || type_node->type != AST_TYPE) {
        return true;
    }

    rendered = mir_capture_type_name(type_node, NULL);
    if (rendered == NULL)
        return false;
    ok = mir_source_local_type_append_name(program, routine, name, rendered);
    free(rendered);
    return ok;
}

static bool
mir_source_local_type_capture_node(const MIRProgram *program,
                                   MIRRoutine *routine,
                                   ASTNode *node)
{
    if (routine == NULL || node == NULL)
        return true;
    switch (node->type) {
    case AST_LET_DECL: {
        ASTNode *type_node = ast_let_type(node);
        MIRSourceLocalTypeScratch scratch = { 0 };
        if (type_node != NULL)
            return mir_source_local_type_append(program, routine,
                ast_let_name(node), type_node);
        return mir_source_local_type_append_name(program, routine,
            ast_let_name(node),
            mir_source_local_expr_type_name(program, routine, &scratch,
                ast_let_initializer(node)));
    }
    case AST_BLOCK: {
        size_t n = ast_block_statement_count(node);
        for (size_t i = 0; i < n; i++) {
            if (!mir_source_local_type_capture_node(program, routine,
                    ast_block_statement(node, i))) {
                return false;
            }
        }
        return true;
    }
    case AST_IF_STMT:
        return mir_source_local_type_capture_node(program, routine,
                   ast_if_then_branch(node))
            && mir_source_local_type_capture_node(program, routine,
                   ast_if_else_branch(node));
    case AST_WHILE_LOOP:
        return mir_source_local_type_capture_node(program, routine,
            ast_while_body(node));
    case AST_FOR_LOOP: {
        MIRSourceLocalTypeScratch scratch = { 0 };
        const char *loop_type = mir_source_local_for_loop_variable_type_name(
            program, routine, &scratch, node);
        bool ok = true;
        if (loop_type != NULL)
            ok = mir_source_local_type_append_name(program, routine,
                ast_for_variable(node), loop_type);
        return ok && mir_source_local_type_capture_node(program, routine,
            ast_for_body(node));
    }
    case AST_WITH_STMT: {
        const char *alias = ast_with_alias(node);
        char *claim_type = mir_claim_abi_type_name_from_ast(node);
        bool ok = true;
        if (alias != NULL && claim_type != NULL)
            ok = mir_source_local_type_append_name(program, routine, alias,
                claim_type);
        free(claim_type);
        return ok && mir_source_local_type_capture_node(program, routine,
            ast_with_body(node));
    }
    case AST_MATCH_STMT: {
        size_t n = ast_match_case_count(node);
        for (size_t i = 0; i < n; i++) {
            ASTNode *c = ast_match_case_at(node, i);
            if (c != NULL && c->type == AST_MATCH_CASE
                && !mir_source_local_type_capture_node(program, routine,
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
mir_routine_source_local_type_names_capture(const MIRProgram *program,
                                            MIRRoutine *routine)
{
    if (routine == NULL || routine->ast == NULL
        || routine->ast->type != AST_FUNC_DECL) {
        return true;
    }
    return mir_source_local_type_capture_node(program, routine,
        ast_func_body(routine->ast));
}
