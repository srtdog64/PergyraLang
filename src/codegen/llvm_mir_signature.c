/*
 * Copyright (c) 2026 Pergyra Language Project
 * LLVM MIR routine signature policy.
 */

#ifdef PGY_LLVM_ENABLED

#include "llvm_internal.h"
#include "llvm_mir_signature.h"

static const char *
llvm_signature_decl_name(ASTNode *func_decl)
{
    const char *name = func_decl != NULL
        ? ast_declaration_name(func_decl)
        : NULL;
    return name != NULL ? name : "(anonymous)";
}

bool
llvm_mir_routine_signature_metadata_complete_for(
    LLVMGenCtx *ctx,
    const MIRRoutine *routine,
    ASTNode *func_decl,
    unsigned requirements,
    const char *missing_signature_fmt,
    const char *missing_return_type_fmt,
    const char *missing_param_type_fmt)
{
    const char *func_name = llvm_signature_decl_name(func_decl);

    if (routine == NULL)
        return true;

    if (!llvm_mir_routine_has_signature(routine)) {
        llvm_set_mir_inventory_missing(ctx,
            missing_signature_fmt != NULL
                ? missing_signature_fmt
                : "MIR-only LLVM path missing function signature metadata for '%s'",
            func_name);
        return false;
    }

    if ((requirements & LLVM_MIR_SIGNATURE_REQUIRE_RETURN_TYPE_NAME) != 0
        && llvm_mir_routine_return_type_name(routine) == NULL) {
        ASTNode *return_type = llvm_mir_routine_return_type(routine);
        if (return_type != NULL
            && return_type->type != AST_EVENT_HANDLER_TYPE) {
            llvm_set_mir_inventory_missing(ctx,
                missing_return_type_fmt != NULL
                    ? missing_return_type_fmt
                    : "MIR-only LLVM path missing function return type-name metadata for '%s'",
                func_name);
            return false;
        }
    }

    if ((requirements & LLVM_MIR_SIGNATURE_REQUIRE_PARAM_TYPE_NAMES) != 0) {
        for (size_t i = 0; i < llvm_mir_routine_param_count(routine); i++) {
            FuncParam *param = llvm_mir_routine_param(routine, i);
            if (param == NULL
                || llvm_mir_routine_param_type_name(routine, i) != NULL) {
                continue;
            }
            if (param->type != NULL
                && param->type->type != AST_EVENT_HANDLER_TYPE) {
                llvm_set_mir_inventory_missing(ctx,
                    missing_param_type_fmt != NULL
                        ? missing_param_type_fmt
                        : "MIR-only LLVM path missing function parameter type-name metadata for '%s'",
                    func_name);
                return false;
            }
        }
    }

    return true;
}

bool
llvm_mir_routine_signature_metadata_complete(
    LLVMGenCtx *ctx,
    const MIRRoutine *routine,
    ASTNode *func_decl,
    const char *missing_signature_fmt,
    const char *missing_return_type_fmt,
    const char *missing_param_type_fmt)
{
    return llvm_mir_routine_signature_metadata_complete_for(ctx,
        routine,
        func_decl,
        LLVM_MIR_SIGNATURE_REQUIRE_ALL_TYPE_NAMES,
        missing_signature_fmt,
        missing_return_type_fmt,
        missing_param_type_fmt);
}

bool
llvm_mir_or_ast_function_is_generic(const MIRRoutine *routine,
                                    const ASTNode *func_decl)
{
    GenericParams *generic_params;

    if (llvm_mir_routine_has_signature(routine))
        return llvm_mir_routine_generic_param_count(routine) > 0;
    if (func_decl == NULL || func_decl->type != AST_FUNC_DECL)
        return false;
    generic_params = ast_declaration_generic_params((ASTNode *)func_decl);
    return ast_generic_param_count(generic_params) > 0;
}

#endif /* PGY_LLVM_ENABLED */
