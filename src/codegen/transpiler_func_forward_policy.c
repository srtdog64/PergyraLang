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
#include "transpiler_mir_signature.h"
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

    if (ctx == NULL || func == NULL || func->type != AST_FUNC_DECL)
        return false;

    routine = transpiler_find_mir_function(ctx, func);
    if (transpiler_active_has_mir(ctx) && routine == NULL) {
        transpiler_set_mir_inventory_missing(ctx,
            "MIR-only C path missing function forward routine for '%s'",
            ast_declaration_name(func) != NULL
                ? ast_declaration_name(func)
                : "(anonymous)");
        return false;
    }
    if (!transpiler_mir_routine_signature_metadata_complete_for(ctx,
            routine,
            func,
            TRANSPILER_MIR_SIGNATURE_REQUIRE_ALL_TYPE_NAMES,
            "MIR-only C path missing function forward signature metadata for '%s'",
            "MIR-only C path missing function forward return type-name metadata for '%s'",
            "MIR-only C path missing function forward parameter type-name metadata for '%s'")) {
        return false;
    }
    if (transpiler_mir_or_ast_function_is_generic(routine, func))
        return false;
    if (routine == NULL)
        return false;

    const char *return_type_name =
        transpiler_mir_routine_return_type_name(routine);
    if (return_type_name != NULL) {
        if (!transpiler_can_forward_declare_type_name_early(ctx,
                return_type_name)) {
            return false;
        }
    } else if (!transpiler_can_forward_declare_type_early(
            ctx, transpiler_mir_routine_return_type(routine))) {
        return false;
    }

    size_t param_count = transpiler_mir_routine_param_count(routine);
    for (size_t i = 0; i < param_count; i++) {
        FuncParam *p = transpiler_mir_routine_param(routine, i);
        const char *param_type_name =
            transpiler_mir_routine_param_type_name(routine, i);
        if (p == NULL)
            continue;
        if (param_type_name != NULL) {
            if (!transpiler_can_forward_declare_type_name_early(ctx,
                    param_type_name)) {
                return false;
            }
            continue;
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
    if (transpiler_active_has_mir(ctx)) {
        if (transpiler_active_decl_header_of_type(
                ctx, AST_WORLD_DECL, name) != NULL) {
            return false;
        }
    } else if (transpiler_find_named_decl_local(
                   ctx, AST_WORLD_DECL, name) != NULL) {
        return false;
    }
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
    if (transpiler_active_has_mir(ctx)) {
        if (transpiler_active_decl_header_of_type(
                ctx, AST_WORLD_DECL, type_name) != NULL) {
            return false;
        }
    } else if (transpiler_find_named_decl_local(
                   ctx, AST_WORLD_DECL, type_name) != NULL) {
        return false;
    }
    return transpiler_has_known_nominal_type(ctx, type_name);
}

bool
transpiler_can_forward_declare_func_after_zones(TranspilerCtx *ctx,
                                                ASTNode *func)
{
    const MIRRoutine *routine;

    if (ctx == NULL || func == NULL || func->type != AST_FUNC_DECL)
        return false;

    routine = transpiler_find_mir_function(ctx, func);
    if (transpiler_active_has_mir(ctx) && routine == NULL) {
        transpiler_set_mir_inventory_missing(ctx,
            "MIR-only C path missing function forward routine for '%s'",
            ast_declaration_name(func) != NULL
                ? ast_declaration_name(func)
                : "(anonymous)");
        return false;
    }
    if (!transpiler_mir_routine_signature_metadata_complete_for(ctx,
            routine,
            func,
            TRANSPILER_MIR_SIGNATURE_REQUIRE_ALL_TYPE_NAMES,
            "MIR-only C path missing function forward signature metadata for '%s'",
            "MIR-only C path missing function forward return type-name metadata for '%s'",
            "MIR-only C path missing function forward parameter type-name metadata for '%s'")) {
        return false;
    }
    if (transpiler_mir_or_ast_function_is_generic(routine, func))
        return false;
    if (routine == NULL)
        return false;

    const char *return_type_name =
        transpiler_mir_routine_return_type_name(routine);
    if (return_type_name != NULL) {
        if (!transpiler_can_forward_declare_type_name_after_zones(ctx,
                return_type_name)) {
            return false;
        }
    } else if (!transpiler_can_forward_declare_type_after_zones(
            ctx, transpiler_mir_routine_return_type(routine))) {
        return false;
    }

    size_t param_count = transpiler_mir_routine_param_count(routine);
    for (size_t i = 0; i < param_count; i++) {
        FuncParam *p = transpiler_mir_routine_param(routine, i);
        const char *param_type_name =
            transpiler_mir_routine_param_type_name(routine, i);
        if (p == NULL)
            continue;
        if (param_type_name != NULL) {
            if (!transpiler_can_forward_declare_type_name_after_zones(ctx,
                    param_type_name)) {
                return false;
            }
            continue;
        }
        if (p->type == NULL)
            continue;
        if (!transpiler_can_forward_declare_type_after_zones(ctx, p->type)) {
            return false;
        }
    }
    return true;
}
