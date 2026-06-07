/*
 * Copyright (c) 2026 Pergyra Language Project
 * LLVM backend early forward-declaration eligibility.
 */

#ifdef PGY_LLVM_ENABLED

#include "llvm_backend.h"
#include "llvm_backend_type_map_internal.h"
#include "llvm_internal.h"

#include <stdbool.h>

static bool
llvm_can_forward_declare_type_name_early(LLVMGenCtx *ctx,
                                         const char *type_name)
{
    PgyTypeKind kind;

    if (ctx == NULL || type_name == NULL)
        return true;

    kind = pgy_classify_type(type_name);
    if (pgy_kind_to_llvm(ctx, kind) != NULL)
        return true;

    switch (kind) {
    case PGY_TK_RESULT:
    case PGY_TK_OPTION:
    case PGY_TK_SLOT:
    case PGY_TK_SECURE_SLOT:
    case PGY_TK_DEVICE_SLOT:
    case PGY_TK_REMOTE_FUTURE:
    case PGY_TK_ARRAY:
    case PGY_TK_SLICE:
    case PGY_TK_CHANNEL:
    case PGY_TK_BOX:
    case PGY_TK_RC:
    case PGY_TK_WEAK:
    case PGY_TK_FUTURE:
        return true;
    case PGY_TK_UNKNOWN:
    case PGY_TK_CLASS:
        return llvm_lookup_class(ctx, type_name) != NULL;
    default:
        return true;
    }
}

static bool
llvm_can_forward_declare_type_early(LLVMGenCtx *ctx, ASTNode *type_node)
{
    if (ctx == NULL || type_node == NULL)
        return true;
    if (ast_type_name(type_node) == NULL)
        return true;

    return llvm_can_forward_declare_type_name_early(
        ctx, ast_type_name(type_node));
}

bool
llvm_can_forward_declare_func_early(LLVMGenCtx *ctx, ASTNode *func)
{
    const MIRRoutine *routine;
    bool routine_has_signature;

    if (ctx == NULL || func == NULL || func->type != AST_FUNC_DECL)
        return false;

    routine = llvm_active_function_routine_for_source_ast(ctx, func);
    routine_has_signature = llvm_mir_routine_has_signature(routine);
    if (routine != NULL && llvm_active_has_mir(ctx)
        && !routine_has_signature) {
        llvm_set_mir_inventory_missing(ctx,
            "MIR-only LLVM path missing function forward signature metadata for '%s'",
            ast_declaration_name(func) != NULL
                ? ast_declaration_name(func)
                : "(anonymous)");
        return false;
    }
    size_t generic_param_count = routine_has_signature
        ? llvm_mir_routine_generic_param_count(routine)
        : ast_generic_param_count(ast_declaration_generic_params(func));
    if (generic_param_count > 0)
        return false;

    const char *return_type_name = routine_has_signature
        ? llvm_mir_routine_return_type_name(routine)
        : NULL;
    if (return_type_name != NULL) {
        if (!llvm_can_forward_declare_type_name_early(ctx, return_type_name))
            return false;
    } else if (routine_has_signature
               && llvm_mir_routine_return_type(routine) != NULL
               && llvm_mir_routine_return_type(routine)->type
                   != AST_EVENT_HANDLER_TYPE) {
        llvm_set_mir_inventory_missing(ctx,
            "MIR-only LLVM path missing function forward return type-name metadata for '%s'",
            ast_declaration_name(func) != NULL
                ? ast_declaration_name(func)
                : "(anonymous)");
        return false;
    } else if (!llvm_can_forward_declare_type_early(ctx,
            ast_func_return_type(func))) {
        return false;
    }

    size_t param_count = routine_has_signature
        ? llvm_mir_routine_param_count(routine)
        : ast_func_param_count(func);
    for (size_t i = 0; i < param_count; i++) {
        FuncParam *param = routine_has_signature
            ? llvm_mir_routine_param(routine, i)
            : ast_func_param(func, i);
        const char *param_type_name = routine_has_signature
            ? llvm_mir_routine_param_type_name(routine, i)
            : NULL;
        if (param == NULL)
            continue;
        if (param_type_name != NULL) {
            if (!llvm_can_forward_declare_type_name_early(ctx,
                    param_type_name)) {
                return false;
            }
            continue;
        }
        if (routine_has_signature
            && param->type != NULL
            && param->type->type != AST_EVENT_HANDLER_TYPE) {
            llvm_set_mir_inventory_missing(ctx,
                "MIR-only LLVM path missing function forward parameter type-name metadata for '%s'",
                ast_declaration_name(func) != NULL
                    ? ast_declaration_name(func)
                    : "(anonymous)");
            return false;
        }
        if (param->type == NULL)
            continue;
        if (!llvm_can_forward_declare_type_early(ctx, param->type)) {
            return false;
        }
    }
    return true;
}

#endif /* PGY_LLVM_ENABLED */
