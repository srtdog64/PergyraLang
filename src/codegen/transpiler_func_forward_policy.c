/*
 * Copyright (c) 2025 Pergyra Language Project
 * All rights reserved.
 *
 * C backend function-forward declaration policy.
 */

#include <stdlib.h>
#include <string.h>

#include "transpiler.h"
#include "transpiler_context.h"
#include "transpiler_decl_lookup.h"
#include "transpiler_inventory_view.h"
#include "transpiler_mir_inventory_intent_collect.h"
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
transpiler_can_forward_declare_type_name_early(TranspilerCtx *ctx,
                                               const char *type_name)
{
    if (ctx == NULL || type_name == NULL)
        return true;
    if (transpiler_forward_type_name_is_allowed(type_name))
        return true;
    return find_class_decl(ctx, type_name) != NULL;
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
    return transpiler_can_forward_declare_type_name_early(ctx, name);
}

bool
transpiler_can_forward_declare_func_early(TranspilerCtx *ctx, ASTNode *func)
{
    const MIRRoutine *routine;
    bool routine_has_signature;

    if (ctx == NULL || func == NULL || func->type != AST_FUNC_DECL)
        return false;

    routine = transpiler_find_mir_function(ctx, func);
    routine_has_signature = transpiler_mir_routine_has_signature(routine);
    if (routine != NULL && transpiler_active_has_mir(ctx)
        && !routine_has_signature) {
        transpiler_set_mir_inventory_missing(ctx,
            "MIR-only C path missing function forward signature metadata for '%s'",
            ast_declaration_name(func) != NULL
                ? ast_declaration_name(func)
                : "(anonymous)");
        return false;
    }
    size_t generic_param_count = routine_has_signature
        ? transpiler_mir_routine_generic_param_count(routine)
        : ast_generic_param_count(ast_declaration_generic_params(func));
    if (generic_param_count > 0)
        return false;

    const char *return_type_name = routine_has_signature
        ? transpiler_mir_routine_return_type_name(routine)
        : NULL;
    if (return_type_name != NULL) {
        if (!transpiler_can_forward_declare_type_name_early(ctx,
                return_type_name)) {
            return false;
        }
    } else if (routine_has_signature
               && transpiler_mir_routine_return_type(routine) != NULL
               && transpiler_mir_routine_return_type(routine)->type
                   != AST_EVENT_HANDLER_TYPE) {
        transpiler_set_mir_inventory_missing(ctx,
            "MIR-only C path missing function forward return type-name metadata for '%s'",
            ast_declaration_name(func) != NULL
                ? ast_declaration_name(func)
                : "(anonymous)");
        return false;
    } else if (!transpiler_can_forward_declare_type_early(ctx,
            ast_func_return_type(func))) {
        return false;
    }

    size_t param_count = routine_has_signature
        ? transpiler_mir_routine_param_count(routine)
        : ast_func_param_count(func);
    for (size_t i = 0; i < param_count; i++) {
        FuncParam *p = routine_has_signature
            ? transpiler_mir_routine_param(routine, i)
            : ast_func_param(func, i);
        const char *param_type_name = routine_has_signature
            ? transpiler_mir_routine_param_type_name(routine, i)
            : NULL;
        if (p == NULL)
            continue;
        if (param_type_name != NULL) {
            if (!transpiler_can_forward_declare_type_name_early(ctx,
                    param_type_name)) {
                return false;
            }
            continue;
        }
        if (routine_has_signature
            && p->type != NULL
            && p->type->type != AST_EVENT_HANDLER_TYPE) {
            transpiler_set_mir_inventory_missing(ctx,
                "MIR-only C path missing function forward parameter type-name metadata for '%s'",
                ast_declaration_name(func) != NULL
                    ? ast_declaration_name(func)
                    : "(anonymous)");
            return false;
        }
        if (p->type == NULL)
            continue;
        if (!transpiler_can_forward_declare_type_early(ctx, p->type)) {
            return false;
        }
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
    if (transpiler_find_named_decl_local(ctx, AST_WORLD_DECL, name) != NULL)
        return false;
    return transpiler_has_known_nominal_type(ctx, name);
}

static bool
transpiler_can_forward_declare_type_name_after_zones(TranspilerCtx *ctx,
                                                     const char *type_name)
{
    if (ctx == NULL || type_name == NULL)
        return true;
    if (transpiler_can_forward_declare_type_name_early(ctx, type_name))
        return true;
    if (transpiler_find_named_decl_local(ctx, AST_WORLD_DECL, type_name) != NULL)
        return false;
    return transpiler_has_known_nominal_type(ctx, type_name);
}

bool
transpiler_can_forward_declare_func_after_zones(TranspilerCtx *ctx,
                                                ASTNode *func)
{
    const MIRRoutine *routine;
    bool routine_has_signature;

    if (ctx == NULL || func == NULL || func->type != AST_FUNC_DECL)
        return false;

    routine = transpiler_find_mir_function(ctx, func);
    routine_has_signature = transpiler_mir_routine_has_signature(routine);
    if (routine != NULL && transpiler_active_has_mir(ctx)
        && !routine_has_signature) {
        transpiler_set_mir_inventory_missing(ctx,
            "MIR-only C path missing function forward signature metadata for '%s'",
            ast_declaration_name(func) != NULL
                ? ast_declaration_name(func)
                : "(anonymous)");
        return false;
    }
    size_t generic_param_count = routine_has_signature
        ? transpiler_mir_routine_generic_param_count(routine)
        : ast_generic_param_count(ast_declaration_generic_params(func));
    if (generic_param_count > 0)
        return false;

    const char *return_type_name = routine_has_signature
        ? transpiler_mir_routine_return_type_name(routine)
        : NULL;
    if (return_type_name != NULL) {
        if (!transpiler_can_forward_declare_type_name_after_zones(ctx,
                return_type_name)) {
            return false;
        }
    } else if (routine_has_signature
               && transpiler_mir_routine_return_type(routine) != NULL
               && transpiler_mir_routine_return_type(routine)->type
                   != AST_EVENT_HANDLER_TYPE) {
        transpiler_set_mir_inventory_missing(ctx,
            "MIR-only C path missing function forward return type-name metadata for '%s'",
            ast_declaration_name(func) != NULL
                ? ast_declaration_name(func)
                : "(anonymous)");
        return false;
    } else if (!transpiler_can_forward_declare_type_after_zones(ctx,
            ast_func_return_type(func))) {
        return false;
    }

    size_t param_count = routine_has_signature
        ? transpiler_mir_routine_param_count(routine)
        : ast_func_param_count(func);
    for (size_t i = 0; i < param_count; i++) {
        FuncParam *p = routine_has_signature
            ? transpiler_mir_routine_param(routine, i)
            : ast_func_param(func, i);
        const char *param_type_name = routine_has_signature
            ? transpiler_mir_routine_param_type_name(routine, i)
            : NULL;
        if (p == NULL)
            continue;
        if (param_type_name != NULL) {
            if (!transpiler_can_forward_declare_type_name_after_zones(ctx,
                    param_type_name)) {
                return false;
            }
            continue;
        }
        if (routine_has_signature
            && p->type != NULL
            && p->type->type != AST_EVENT_HANDLER_TYPE) {
            transpiler_set_mir_inventory_missing(ctx,
                "MIR-only C path missing function forward parameter type-name metadata for '%s'",
                ast_declaration_name(func) != NULL
                    ? ast_declaration_name(func)
                    : "(anonymous)");
            return false;
        }
        if (p->type == NULL)
            continue;
        if (!transpiler_can_forward_declare_type_after_zones(ctx, p->type)) {
            return false;
        }
    }
    return true;
}
