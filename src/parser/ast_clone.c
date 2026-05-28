/*
 * Copyright (c) 2025 Pergyra Language Project
 * AST clone helpers
 */

#include "ast.h"
#include "../common/string_compat.h"

#include <stdlib.h>

static Token
ast_clone_token(Token token)
{
    Token clone = token;

    clone.text = token.text != NULL ? pergyra_strdup(token.text) : NULL;
    return clone;
}

static ASTNode*
ast_clone_literal_node(ASTNode* node)
{
    ASTNode* clone = calloc(1, sizeof(ASTNode));

    if (clone == NULL)
        return NULL;
    clone->type = node->type;
    switch (node->type) {
        case AST_NUMBER:
            clone->data.number = node->data.number;
            break;
        case AST_STRING:
            clone->data.string.value = node->data.string.value != NULL
                ? pergyra_strdup(node->data.string.value)
                : NULL;
            break;
        case AST_BOOLEAN:
            clone->data.boolean = node->data.boolean;
            break;
        default:
            free(clone);
            return NULL;
    }
    return clone;
}

static GenericParams*
ast_clone_generic_params(GenericParams* params)
{
    GenericParams* clone;

    if (params == NULL)
        return NULL;

    clone = calloc(1, sizeof(GenericParams));
    if (clone == NULL)
        return NULL;

    clone->count = params->count;
    clone->capacity = params->count;
    if (params->count == 0)
        return clone;

    clone->params = calloc(params->count, sizeof(GenericParam*));
    if (clone->params == NULL) {
        free(clone);
        return NULL;
    }

    for (size_t i = 0; i < params->count; i++) {
        GenericParam* src = params->params[i];
        GenericParam* dst;

        if (src == NULL)
            continue;

        dst = calloc(1, sizeof(GenericParam));
        if (dst == NULL)
            continue;

        dst->name = pergyra_strdup(src->name);
        dst->constraint = ast_clone(src->constraint);
        dst->default_type = ast_clone(src->default_type);
        clone->params[i] = dst;
    }

    return clone;
}


ASTNode*
ast_clone(ASTNode* node)
{
    ASTNode* clone;

    if (node == NULL)
        return NULL;

    switch (node->type) {
        case AST_IDENTIFIER:
            clone = ast_create_identifier(node->data.identifier.name);
            break;
        case AST_NUMBER:
        case AST_STRING:
        case AST_BOOLEAN:
            clone = ast_clone_literal_node(node);
            break;
        case AST_BINARY:
            clone = ast_create_binary(ast_clone(node->data.binary.left),
                ast_clone_token(node->data.binary.op),
                ast_clone(node->data.binary.right));
            break;
        case AST_UNARY:
            clone = ast_create_unary(ast_clone_token(node->data.unary.op),
                ast_clone(node->data.unary.operand));
            break;
        case AST_MEMBER_ACCESS:
            clone = ast_create_member_access(ast_clone(node->data.member.object),
                node->data.member.name);
            break;
        case AST_ARRAY_ACCESS:
            clone = ast_create_array_access(ast_clone(node->data.array_access.array),
                ast_clone(node->data.array_access.index));
            break;
        case AST_TYPE:
            clone = ast_create_type(node->data.type.name);
            clone->data.type.generic_args =
                ast_clone_generic_params(node->data.type.generic_args);
            if (node->data.type.tuple_elements != NULL
                && node->data.type.tuple_element_count > 0) {
                size_t n = node->data.type.tuple_element_count;
                clone->data.type.tuple_elements = calloc(n, sizeof(ASTNode *));
                clone->data.type.tuple_element_count = n;
                for (size_t i = 0; i < n; i++)
                    clone->data.type.tuple_elements[i] =
                        ast_clone(node->data.type.tuple_elements[i]);
            }
            break;
        case AST_CHANNEL_TYPE:
            clone = ast_create_channel_type(
                ast_clone(node->data.channel_type.element_type));
            clone->data.channel_type.capacity =
                ast_clone(node->data.channel_type.capacity);
            break;
        case AST_FUTURE_TYPE:
            clone = ast_create_future_type(
                ast_clone(node->data.future_type.value_type));
            break;
        case AST_EVENT_HANDLER_TYPE:
            clone = ast_create_event_handler_type();
            break;
        default:
            clone = NULL;
            break;
    }

    if (clone != NULL) {
        clone->access = node->access;
        clone->has_explicit_access = node->has_explicit_access;
        clone->line = node->line;
        clone->column = node->column;
        clone->origin_path = node->origin_path != NULL
            ? pergyra_strdup(node->origin_path)
            : NULL;
    }

    return clone;
}
