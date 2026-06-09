#include <string.h>

#include "mir.h"
#include "../parser/ast_api.h"

bool
mir_program_has_main_function(const MIRProgram *mir)
{
    return mir != NULL && mir->has_main_function;
}

const char *
mir_program_main_function_name(const MIRProgram *mir)
{
    return mir != NULL ? mir->main_function_name : NULL;
}

bool
mir_program_has_top_level_exec(const MIRProgram *mir)
{
    return mir != NULL && mir->has_top_level_exec;
}

void
mir_routine_inventory_from_program(const MIRProgram *mir,
                                   MIRRoutineInventory *inventory)
{
    if (inventory == NULL)
        return;
    inventory->routines = NULL;
    inventory->count = 0;
    if (mir != NULL) {
        inventory->routines = mir->routines;
        inventory->count = mir->routine_count;
    }
}

const MIRRoutine *
mir_routine_inventory_get(const MIRRoutineInventory *inventory, size_t index)
{
    if (inventory == NULL || inventory->routines == NULL
        || index >= inventory->count) {
        return NULL;
    }
    return &inventory->routines[index];
}

void
mir_decl_header_inventory_from_program(const MIRProgram *mir,
                                       MIRDeclHeaderInventory *inventory)
{
    if (inventory == NULL)
        return;
    inventory->headers = NULL;
    inventory->count = 0;
    if (mir != NULL) {
        inventory->headers = mir->decl_headers;
        inventory->count = mir->decl_header_count;
    }
}

const MIRDeclHeader *
mir_decl_header_inventory_get(const MIRDeclHeaderInventory *inventory,
                              size_t index)
{
    if (inventory == NULL || inventory->headers == NULL
        || index >= inventory->count) {
        return NULL;
    }
    return &inventory->headers[index];
}

ASTNode *
mir_routine_source_ast(const MIRRoutine *routine)
{
    return routine != NULL ? routine->ast : NULL;
}

MIRScopeKind
mir_routine_kind(const MIRRoutine *routine)
{
    return routine != NULL ? routine->kind : MIR_SCOPE_FUNCTION;
}

const char *
mir_routine_name(const MIRRoutine *routine)
{
    return routine != NULL ? routine->name : NULL;
}

const char *
mir_routine_owner_name(const MIRRoutine *routine)
{
    return routine != NULL ? routine->owner_name : NULL;
}

ASTNodeType
mir_routine_owner_ast_type(const MIRRoutine *routine)
{
    return routine != NULL ? routine->owner_ast_type : AST_PROGRAM;
}

bool
mir_routine_has_signature(const MIRRoutine *routine)
{
    return routine != NULL && routine->has_signature;
}

size_t
mir_routine_generic_param_count(const MIRRoutine *routine)
{
    return mir_routine_has_signature(routine)
        ? routine->generic_param_count
        : 0;
}

size_t
mir_routine_param_count(const MIRRoutine *routine)
{
    return mir_routine_has_signature(routine) ? routine->param_count : 0;
}

FuncParam *
mir_routine_param(const MIRRoutine *routine, size_t index)
{
    if (!mir_routine_has_signature(routine) || routine->params == NULL
        || index >= routine->param_count) {
        return NULL;
    }
    return routine->params[index];
}

const char *
mir_routine_param_type_name(const MIRRoutine *routine, size_t index)
{
    if (!mir_routine_has_signature(routine) || routine->param_type_names == NULL
        || index >= routine->param_count) {
        return NULL;
    }
    return routine->param_type_names[index];
}

ASTNode *
mir_routine_return_type(const MIRRoutine *routine)
{
    return mir_routine_has_signature(routine) ? routine->return_type : NULL;
}

const char *
mir_routine_return_type_name(const MIRRoutine *routine)
{
    return mir_routine_has_signature(routine) ? routine->return_type_name : NULL;
}

static const char *
mir_routine_source_local_walk(ASTNode *node, const char *local_name)
{
    if (node == NULL || local_name == NULL)
        return NULL;
    switch (node->type) {
    case AST_LET_DECL: {
        const char *name = ast_let_name(node);
        if (name != NULL && strcmp(name, local_name) == 0) {
            ASTNode *ty = ast_let_type(node);
            if (ty != NULL && ty->type == AST_TYPE)
                return ast_type_name(ty);
        }
        return NULL;
    }
    case AST_BLOCK: {
        size_t n = ast_block_statement_count(node);
        for (size_t i = 0; i < n; i++) {
            const char *t = mir_routine_source_local_walk(
                ast_block_statement(node, i), local_name);
            if (t != NULL)
                return t;
        }
        return NULL;
    }
    case AST_IF_STMT: {
        const char *t = mir_routine_source_local_walk(
            ast_if_then_branch(node), local_name);
        if (t != NULL)
            return t;
        return mir_routine_source_local_walk(
            ast_if_else_branch(node), local_name);
    }
    case AST_WHILE_LOOP:
        return mir_routine_source_local_walk(
            ast_while_body(node), local_name);
    case AST_FOR_LOOP:
        return mir_routine_source_local_walk(
            ast_for_body(node), local_name);
    case AST_WITH_STMT:
        return mir_routine_source_local_walk(
            ast_with_body(node), local_name);
    case AST_MATCH_STMT: {
        size_t n = ast_match_case_count(node);
        for (size_t i = 0; i < n; i++) {
            ASTNode *c = ast_match_case_at(node, i);
            if (c == NULL || c->type != AST_MATCH_CASE)
                continue;
            const char *t = mir_routine_source_local_walk(
                ast_match_case_body(c), local_name);
            if (t != NULL)
                return t;
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
    return mir_routine_source_local_walk(body, local_name);
}

const char *
mir_routine_source_local_type_name(const MIRRoutine *routine,
                                   const char *local_name)
{
    if (routine == NULL || local_name == NULL)
        return NULL;
    ASTNode *src = routine->ast;
    if (src == NULL)
        return NULL;
    ASTNode *body = NULL;
    if (src->type == AST_FUNC_DECL)
        body = ast_func_body(src);
    if (body == NULL)
        return NULL;
    return mir_routine_source_local_walk(body, local_name);
}

const char *
mir_routine_within_zone(const MIRRoutine *routine)
{
    return mir_routine_has_signature(routine) ? routine->within_zone : NULL;
}

void
mir_mutable_routine_inventory_from_program(
        MIRProgram *mir,
        MIRMutableRoutineInventory *inventory)
{
    if (inventory == NULL)
        return;
    inventory->routines = NULL;
    inventory->count = 0;
    if (mir != NULL) {
        inventory->routines = mir->routines;
        inventory->count = mir->routine_count;
    }
}

MIRRoutine *
mir_mutable_routine_inventory_get(
        const MIRMutableRoutineInventory *inventory,
        size_t index)
{
    if (inventory == NULL || inventory->routines == NULL
        || index >= inventory->count) {
        return NULL;
    }
    return &inventory->routines[index];
}
