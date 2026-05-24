/*
 * Copyright (c) 2026 Pergyra Language Project
 * Assignment target path formatting and borrowed-boundary root lookup.
 */

#include "type_checker_ownership_support_internal.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

#include "../common/string_compat.h"

typedef struct SemanticAssignmentPathOwner {
    SemanticContext *ctx;
    char *(*dup)(struct SemanticAssignmentPathOwner owner, const char *s);
    char *(*vfmt)(struct SemanticAssignmentPathOwner owner, const char *fmt,
                  va_list args);
    void (*release)(struct SemanticAssignmentPathOwner owner, char *path);
} SemanticAssignmentPathOwner;

static char *
semantic_assignment_path_scratch_dup(SemanticAssignmentPathOwner owner,
                                     const char *s)
{
    return pgy_arena_strdup(&owner.ctx->scratch_arena, s);
}

static char *
semantic_assignment_path_scratch_vfmt(SemanticAssignmentPathOwner owner,
                                      const char *fmt,
                                      va_list args)
{
    return pgy_arena_vfmt(&owner.ctx->scratch_arena, fmt, args);
}

static void
semantic_assignment_path_scratch_release(SemanticAssignmentPathOwner owner,
                                         char *path)
{
    (void)owner;
    (void)path;
}

static SemanticAssignmentPathOwner
semantic_assignment_path_scratch_owner(SemanticContext *ctx)
{
    SemanticAssignmentPathOwner owner = {
        .ctx = ctx,
        .dup = semantic_assignment_path_scratch_dup,
        .vfmt = semantic_assignment_path_scratch_vfmt,
        .release = semantic_assignment_path_scratch_release,
    };
    return owner;
}

static char *
semantic_assignment_path_dup(SemanticAssignmentPathOwner owner,
                             const char *s)
{
    return owner.dup(owner, s);
}

static char *
semantic_assignment_path_fmt(SemanticAssignmentPathOwner owner,
                             const char *fmt, ...)
{
    va_list args;
    char *result;

    va_start(args, fmt);
    result = owner.vfmt(owner, fmt, args);
    va_end(args);
    return result;
}

static void
semantic_assignment_path_release(SemanticAssignmentPathOwner owner,
                                 char *path)
{
    owner.release(owner, path);
}

static char *
semantic_assignment_target_path_impl_owned(ASTNode *expr,
                                           SemanticAssignmentPathOwner owner)
{
    char *base = NULL;
    char index_buf[32];

    if (expr == NULL)
        return semantic_assignment_path_dup(owner, "<target>");

    switch (expr->type) {
    case AST_IDENTIFIER:
        return ast_identifier_name(expr) != NULL
            ? semantic_assignment_path_dup(owner, ast_identifier_name(expr))
            : semantic_assignment_path_dup(owner, "<target>");
    case AST_MEMBER_ACCESS:
    {
        ASTNode *object_node = ast_member_object(expr);
        const char *member_name = ast_member_name(expr);
        if (member_name == NULL)
            return semantic_assignment_path_dup(owner, "<target>");
        base = semantic_assignment_target_path_impl_owned(object_node, owner);
        if (base == NULL)
            return semantic_assignment_path_fmt(owner, "<target>.%s",
                                                member_name);
        {
            char *result = semantic_assignment_path_fmt(
                owner, "%s.%s", base, member_name);
            semantic_assignment_path_release(owner, base);
            return result != NULL
                ? result
                : semantic_assignment_path_dup(owner, "<target>");
        }
    }
    case AST_ARRAY_ACCESS:
    {
        ASTNode *array_node = ast_array_access_array(expr);
        ASTNode *index_node = ast_array_access_index(expr);
        base = semantic_assignment_target_path_impl_owned(array_node, owner);
        if (index_node != NULL && index_node->type == AST_NUMBER) {
            snprintf(index_buf, sizeof(index_buf), "%g",
                     ast_number_value(index_node));
        } else if (index_node != NULL
                   && index_node->type == AST_IDENTIFIER
                   && ast_identifier_name(index_node) != NULL) {
            snprintf(index_buf, sizeof(index_buf), "%s",
                     ast_identifier_name(index_node));
        } else {
            snprintf(index_buf, sizeof(index_buf), "?");
        }
        if (base == NULL)
            return semantic_assignment_path_fmt(
                owner, "<target>[%s]", index_buf);
        {
            char *result = semantic_assignment_path_fmt(
                owner, "%s[%s]", base, index_buf);
            semantic_assignment_path_release(owner, base);
            return result != NULL
                ? result
                : semantic_assignment_path_dup(owner, "<target>");
        }
    }
    default:
        return semantic_assignment_path_dup(owner, "<target>");
    }
}

const char *
semantic_assignment_target_path_scratch(ASTNode *expr, SemanticContext *ctx)
{
    if (ctx == NULL)
        return "<target>";
    return semantic_assignment_target_path_impl_owned(
        expr,
        semantic_assignment_path_scratch_owner(ctx));
}

const char *
semantic_borrowed_boundary_root_name(ASTNode *expr, SemanticContext *ctx)
{
    if (expr == NULL || ctx == NULL)
        return NULL;

    switch (expr->type) {
    case AST_IDENTIFIER:
        return identifier_is_borrowed_boundary_param(expr, ctx)
            ? ast_identifier_name(expr)
            : NULL;
    case AST_MEMBER_ACCESS:
        return semantic_borrowed_boundary_root_name(
            ast_member_object(expr), ctx);
    case AST_ARRAY_ACCESS:
        return semantic_borrowed_boundary_root_name(
            ast_array_access_array(expr), ctx);
    default:
        return NULL;
    }
}
