#include "mir_decl_method_projection.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

static char *
mir_decl_projection_copy_string(const char *value)
{
    size_t len;
    char *copy;

    if (value == NULL)
        return NULL;
    len = strlen(value);
    copy = malloc(len + 1);
    if (copy == NULL)
        return NULL;
    memcpy(copy, value, len + 1);
    return copy;
}

void
mir_decl_method_projection_metadata_clear(MIRDeclMethod *meta)
{
    if (meta == NULL)
        return;
    if (meta->projection_write_root_names != NULL) {
        for (size_t i = 0; i < meta->projection_write_count; i++) {
            free(meta->projection_write_root_names[i]);
            free(meta->projection_write_member_names[i]);
        }
    }
    free(meta->projection_write_root_names);
    free(meta->projection_write_member_names);
    if (meta->projection_call_receiver_names != NULL) {
        for (size_t i = 0; i < meta->projection_call_count; i++) {
            free(meta->projection_call_receiver_names[i]);
            free(meta->projection_call_method_names[i]);
        }
    }
    free(meta->projection_call_receiver_names);
    free(meta->projection_call_method_names);
    meta->projection_write_root_names = NULL;
    meta->projection_write_member_names = NULL;
    meta->projection_write_count = 0;
    meta->projection_call_receiver_names = NULL;
    meta->projection_call_method_names = NULL;
    meta->projection_call_count = 0;
}

static bool
mir_decl_method_projection_append_write(MIRDeclMethod *meta,
                                        const char *root_name,
                                        const char *member_name)
{
    size_t next_count;
    char **roots;
    char **members;
    char *root_copy;
    char *member_copy = NULL;

    if (meta == NULL || root_name == NULL)
        return true;
    if (meta->projection_write_count == SIZE_MAX)
        return false;
    next_count = meta->projection_write_count + 1;
    roots = realloc(meta->projection_write_root_names,
        next_count * sizeof(char *));
    if (roots == NULL)
        return false;
    meta->projection_write_root_names = roots;
    members = realloc(meta->projection_write_member_names,
        next_count * sizeof(char *));
    if (members == NULL)
        return false;
    meta->projection_write_member_names = members;
    root_copy = mir_decl_projection_copy_string(root_name);
    if (root_copy == NULL)
        return false;
    if (member_name != NULL) {
        member_copy = mir_decl_projection_copy_string(member_name);
        if (member_copy == NULL) {
            free(root_copy);
            return false;
        }
    }
    meta->projection_write_root_names[meta->projection_write_count] =
        root_copy;
    meta->projection_write_member_names[meta->projection_write_count] =
        member_copy;
    meta->projection_write_count = next_count;
    return true;
}

static bool
mir_decl_method_projection_append_call(MIRDeclMethod *meta,
                                       const char *receiver_name,
                                       const char *method_name)
{
    size_t next_count;
    char **receivers;
    char **methods;
    char *receiver_copy;
    char *method_copy;

    if (meta == NULL || receiver_name == NULL || method_name == NULL)
        return true;
    if (meta->projection_call_count == SIZE_MAX)
        return false;
    next_count = meta->projection_call_count + 1;
    receivers = realloc(meta->projection_call_receiver_names,
        next_count * sizeof(char *));
    if (receivers == NULL)
        return false;
    meta->projection_call_receiver_names = receivers;
    methods = realloc(meta->projection_call_method_names,
        next_count * sizeof(char *));
    if (methods == NULL)
        return false;
    meta->projection_call_method_names = methods;
    receiver_copy = mir_decl_projection_copy_string(receiver_name);
    if (receiver_copy == NULL)
        return false;
    method_copy = mir_decl_projection_copy_string(method_name);
    if (method_copy == NULL) {
        free(receiver_copy);
        return false;
    }
    meta->projection_call_receiver_names[meta->projection_call_count] =
        receiver_copy;
    meta->projection_call_method_names[meta->projection_call_count] =
        method_copy;
    meta->projection_call_count = next_count;
    return true;
}

static bool
mir_decl_method_projection_capture_assignment(MIRDeclMethod *meta,
                                              ASTNode *target)
{
    ASTNode *cursor = target;

    if (target == NULL)
        return true;
    if (target->type == AST_IDENTIFIER)
        return mir_decl_method_projection_append_write(
            meta, ast_identifier_name(target), NULL);

    while (cursor != NULL && cursor->type == AST_MEMBER_ACCESS) {
        ASTNode *object = ast_member_object(cursor);
        if (object != NULL && object->type == AST_IDENTIFIER) {
            return mir_decl_method_projection_append_write(
                meta,
                ast_identifier_name(object),
                ast_member_name(cursor));
        }
        cursor = object;
    }
    return true;
}

static bool
mir_decl_method_projection_capture_node(MIRDeclMethod *meta,
                                        ASTNode *node,
                                        int depth)
{
    if (meta == NULL || node == NULL || depth > 8)
        return true;

    switch (node->type) {
    case AST_BLOCK:
        for (size_t i = 0; i < ast_block_statement_count(node); i++) {
            if (!mir_decl_method_projection_capture_node(
                    meta, ast_block_statement(node, i), depth + 1))
                return false;
        }
        break;
    case AST_IF_STMT:
        return mir_decl_method_projection_capture_node(
                meta, ast_if_then_branch(node), depth + 1)
            && mir_decl_method_projection_capture_node(
                meta, ast_if_else_branch(node), depth + 1);
    case AST_FOR_LOOP:
        return mir_decl_method_projection_capture_node(
            meta, ast_for_body(node), depth + 1);
    case AST_WHILE_LOOP:
        return mir_decl_method_projection_capture_node(
            meta, ast_while_body(node), depth + 1);
    case AST_MATCH_STMT:
        for (size_t i = 0; i < ast_match_case_count(node); i++) {
            if (!mir_decl_method_projection_capture_node(
                    meta, ast_match_case_at(node, i), depth + 1))
                return false;
        }
        return mir_decl_method_projection_capture_node(
            meta, ast_match_default_body(node), depth + 1);
    case AST_MATCH_CASE:
        return mir_decl_method_projection_capture_node(
            meta, ast_match_case_body(node), depth + 1);
    case AST_SELECT_STMT:
        for (size_t i = 0; i < ast_select_case_count(node); i++) {
            if (!mir_decl_method_projection_capture_node(
                    meta, ast_select_case(node, i), depth + 1))
                return false;
        }
        return mir_decl_method_projection_capture_node(
            meta, ast_select_default_case(node), depth + 1);
    case AST_ASYNC_BLOCK:
        for (size_t i = 0; i < ast_async_block_statement_count(node); i++) {
            if (!mir_decl_method_projection_capture_node(
                    meta, ast_async_block_statement(node, i), depth + 1))
                return false;
        }
        break;
    case AST_ASSIGNMENT:
        return mir_decl_method_projection_capture_assignment(
            meta, ast_assignment_target(node));
    case AST_CALL:
        if (ast_call_callee(node) != NULL
            && ast_call_callee(node)->type == AST_MEMBER_ACCESS) {
            ASTNode *object = ast_member_object(ast_call_callee(node));
            if (object != NULL && object->type == AST_IDENTIFIER) {
                return mir_decl_method_projection_append_call(
                    meta,
                    ast_identifier_name(object),
                    ast_member_name(ast_call_callee(node)));
            }
        }
        break;
    default:
        break;
    }
    return true;
}

bool
mir_decl_method_projection_metadata_capture(MIRDeclMethod *meta,
                                            ASTNode *method_body)
{
    return mir_decl_method_projection_capture_node(meta, method_body, 0);
}
