/*
 * Copyright (c) 2025 Pergyra Language Project
 * All rights reserved.
 *
 * C backend function-forward declaration policy.
 */

#include <stdlib.h>
#include <string.h>

#include "transpiler.h"
#include "transpiler_decl_lookup.h"
#include "../parser/ast_api.h"

static int
transpiler_forward_allowed_type_compare(const void *key, const void *entry)
{
    const char *name = *(const char * const *)key;
    const char *allowed = *(const char * const *)entry;

    return strcmp(name, allowed);
}

static bool
transpiler_forward_type_name_is_allowed(const char *name)
{
    static const char *allowed_names[] = {
        "Array",
        "Bool",
        "Box",
        "Byte",
        "Channel",
        "Char",
        "DeviceSlot",
        "Float",
        "Future",
        "Int",
        "Option",
        "Qubit",
        "Rc",
        "RemoteFuture",
        "Result",
        "SecureSlot",
        "Slice",
        "Slot",
        "String",
        "Void",
        "Weak",
    };
    const char **match;

    if (name == NULL)
        return false;
    match = (const char **)bsearch(&name,
        allowed_names,
        sizeof(allowed_names) / sizeof(allowed_names[0]),
        sizeof(allowed_names[0]),
        transpiler_forward_allowed_type_compare);
    return match != NULL;
}

bool
transpiler_can_forward_declare_type_early(TranspilerCtx *ctx,
                                          ASTNode *type_node)
{
    const char *name;

    if (ctx == NULL || type_node == NULL)
        return true;
    if (type_node->type != AST_TYPE || ast_type_name(type_node) == NULL)
        return true;

    name = ast_type_name(type_node);
    if (transpiler_forward_type_name_is_allowed(name))
        return true;

    return find_class_decl(ctx, name) != NULL;
}

bool
transpiler_can_forward_declare_func_early(TranspilerCtx *ctx, ASTNode *func)
{
    if (ctx == NULL || func == NULL || func->type != AST_FUNC_DECL)
        return false;
    GenericParams *generic_params = ast_func_generic_params(func);
    if (ast_generic_param_count(generic_params) > 0)
        return false;
    if (!transpiler_can_forward_declare_type_early(ctx,
            ast_func_return_type(func)))
        return false;
    for (size_t i = 0; i < ast_func_param_count(func); i++) {
        FuncParam *p = ast_func_param(func, i);
        if (p == NULL || p->type == NULL)
            continue;
        if (!transpiler_can_forward_declare_type_early(ctx, p->type))
            return false;
    }
    return true;
}

static bool
transpiler_can_forward_declare_type_after_zones(TranspilerCtx *ctx,
                                                ASTNode *type_node)
{
    const char *name = NULL;

    if (transpiler_can_forward_declare_type_early(ctx, type_node))
        return true;
    if (ctx == NULL || type_node == NULL || type_node->type != AST_TYPE
        || ast_type_name(type_node) == NULL)
        return false;

    name = ast_type_name(type_node);
    if (find_world_decl(ctx, name) != NULL)
        return false;
    return transpiler_has_known_nominal_type(ctx, name);
}

bool
transpiler_can_forward_declare_func_after_zones(TranspilerCtx *ctx,
                                                ASTNode *func)
{
    if (ctx == NULL || func == NULL || func->type != AST_FUNC_DECL)
        return false;
    GenericParams *generic_params = ast_func_generic_params(func);
    if (ast_generic_param_count(generic_params) > 0)
        return false;
    if (!transpiler_can_forward_declare_type_after_zones(ctx,
            ast_func_return_type(func)))
        return false;
    for (size_t i = 0; i < ast_func_param_count(func); i++) {
        FuncParam *p = ast_func_param(func, i);
        if (p == NULL || p->type == NULL)
            continue;
        if (!transpiler_can_forward_declare_type_after_zones(ctx, p->type))
            return false;
    }
    return true;
}
