/*
 * Copyright (c) 2026 Pergyra Language Project
 * Split AST accessor owner: AST_CALL and identifier-rename accessors.
 */

#include "ast_constructors_internal.h"
#include "../common/string_compat.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

GenericParams*
ast_call_generic_args(const ASTNode* node)
{
    if (node == NULL || node->type != AST_CALL)
        return NULL;
    return node->data.call.generic_args;
}

size_t
ast_call_generic_arg_count(const ASTNode* node)
{
    GenericParams *generic_args = ast_call_generic_args(node);
    return generic_args != NULL ? generic_args->count : 0;
}

GenericParam*
ast_call_generic_arg(const ASTNode* node, size_t index)
{
    GenericParams *generic_args = ast_call_generic_args(node);
    if (generic_args == NULL
        || generic_args->params == NULL
        || index >= generic_args->count) {
        return NULL;
    }
    return generic_args->params[index];
}

ASTNode*
ast_call_callee(const ASTNode* node)
{
    if (node == NULL || node->type != AST_CALL)
        return NULL;
    return node->data.call.callee;
}

uint32_t
ast_call_semantic_callee_decl_id(const ASTNode *node)
{
    return node != NULL && node->type == AST_CALL
        ? node->data.call.semantic_callee_decl_id
        : 0;
}

bool
ast_call_set_semantic_callee_decl_id(ASTNode *node, uint32_t decl_id)
{
    if (node == NULL || node->type != AST_CALL)
        return false;
    node->data.call.semantic_callee_decl_id = decl_id;
    return true;
}

bool
ast_call_semantic_callee_builtin_kind(const ASTNode *node,
                                      uint32_t *kind_out)
{
    if (kind_out != NULL)
        *kind_out = 0;
    if (node == NULL || node->type != AST_CALL
        || !node->data.call.semantic_callee_builtin_kind_set) {
        return false;
    }
    if (kind_out != NULL)
        *kind_out = node->data.call.semantic_callee_builtin_kind;
    return true;
}

bool
ast_call_set_semantic_callee_builtin_kind(ASTNode *node, uint32_t kind)
{
    if (node == NULL || node->type != AST_CALL)
        return false;
    node->data.call.semantic_callee_builtin_kind = kind;
    node->data.call.semantic_callee_builtin_kind_set = true;
    return true;
}

bool
ast_call_semantic_runtime_call_abi_id(
    const ASTNode *node, uint32_t *runtime_call_abi_id_out)
{
    if (runtime_call_abi_id_out != NULL)
        *runtime_call_abi_id_out = 0;
    if (node == NULL || node->type != AST_CALL
        || !node->data.call.semantic_runtime_call_abi_id_set
        || node->data.call.semantic_runtime_call_abi_id == 0) {
        return false;
    }
    if (runtime_call_abi_id_out != NULL) {
        *runtime_call_abi_id_out =
            node->data.call.semantic_runtime_call_abi_id;
    }
    return true;
}

bool
ast_call_set_semantic_runtime_call_abi_id(
    ASTNode *node, uint32_t runtime_call_abi_id)
{
    if (node == NULL || node->type != AST_CALL || runtime_call_abi_id == 0)
        return false;
    node->data.call.semantic_runtime_call_abi_id = runtime_call_abi_id;
    node->data.call.semantic_runtime_call_abi_id_set = true;
    return true;
}

bool
ast_call_uses_braced_initializer_syntax(const ASTNode *node)
{
    return node != NULL && node->type == AST_CALL
        && node->data.call.uses_braced_initializer_syntax;
}

bool
ast_call_mark_braced_initializer_syntax(ASTNode *node)
{
    if (node == NULL || node->type != AST_CALL)
        return false;
    node->data.call.uses_braced_initializer_syntax = true;
    return true;
}

size_t
ast_call_arg_count(const ASTNode* node)
{
    if (node == NULL || node->type != AST_CALL)
        return 0;
    return node->data.call.arg_count;
}

ASTNode**
ast_call_arguments(const ASTNode* node, size_t* count_out)
{
    if (count_out != NULL)
        *count_out = ast_call_arg_count(node);
    if (node == NULL || node->type != AST_CALL)
        return NULL;
    return node->data.call.arguments;
}

ASTNode*
ast_call_argument(const ASTNode* node, size_t index)
{
    if (node == NULL || node->type != AST_CALL
        || index >= node->data.call.arg_count) {
        return NULL;
    }
    return node->data.call.arguments[index];
}

const char*
ast_call_argument_name(const ASTNode* node, size_t index)
{
    if (node == NULL || node->type != AST_CALL
        || node->data.call.arg_names == NULL
        || index >= node->data.call.arg_count) {
        return NULL;
    }
    return node->data.call.arg_names[index];
}

bool
ast_call_has_named_arguments(const ASTNode* node)
{
    if (node == NULL || node->type != AST_CALL
        || node->data.call.arg_names == NULL) {
        return false;
    }
    for (size_t i = 0; i < node->data.call.arg_count; i++) {
        if (node->data.call.arg_names[i] != NULL)
            return true;
    }
    return false;
}

ASTNode*
ast_call_find_named_argument(const ASTNode* node, const char* field_name)
{
    if (node == NULL || node->type != AST_CALL
        || node->data.call.arg_names == NULL || field_name == NULL) {
        return NULL;
    }
    for (size_t i = 0; i < node->data.call.arg_count; i++) {
        const char* nm = node->data.call.arg_names[i];
        if (nm != NULL && strcmp(nm, field_name) == 0)
            return node->data.call.arguments[i];
    }
    return NULL;
}

bool
ast_replace_identifier_name_copy(ASTNode* node, const char* name)
{
    char *copy;

    if (node == NULL || node->type != AST_IDENTIFIER || name == NULL)
        return false;
    copy = pergyra_strdup(name);
    if (copy == NULL)
        return false;
    free(node->data.identifier.name);
    node->data.identifier.name = copy;
    return true;
}

void
ast_init_call_borrowed_view(ASTNode* node, ASTNode* callee,
                            ASTNode** arguments, size_t arg_count,
                            GenericParams* generic_args)
{
    if (node == NULL)
        return;
    memset(node, 0, sizeof(*node));
    node->type = AST_CALL;
    node->data.call.callee = callee;
    node->data.call.arguments = arguments;
    node->data.call.arg_count = arg_count;
    node->data.call.generic_args = generic_args;
}
