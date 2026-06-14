#ifdef PGY_LLVM_ENABLED
#include "llvm_internal.h"
#include "llvm_mir_signature.h"

static bool
llvm_decl_function_routine_has_body_storage(const MIRRoutine *routine)
{
    if (routine == NULL)
        return false;
    return routine->block_count > 0 && routine->blocks != NULL;
}

static ASTNode *
llvm_decl_function_from_routine(const MIRRoutine *routine)
{
    return mir_routine_source_decl_of_type(
        routine, MIR_SCOPE_FUNCTION, AST_FUNC_DECL);
}

static bool
llvm_decl_require_function_source_decl(LLVMGenCtx *ctx,
                                       const MIRRoutine *routine,
                                       ASTNode **func_decl_out)
{
    ASTNode *func_decl = llvm_decl_function_from_routine(routine);

    if (func_decl_out != NULL)
        *func_decl_out = func_decl;
    if (func_decl != NULL)
        return true;
    if (routine != NULL
        && llvm_mir_routine_kind(routine) == MIR_SCOPE_FUNCTION
        && llvm_decl_function_routine_has_body_storage(routine)) {
        const char *routine_name = llvm_mir_routine_name(routine);
        llvm_set_mir_inventory_missing(ctx,
            "MIR-only LLVM path missing function source declaration for routine '%s'",
            routine_name != NULL ? routine_name : "(anonymous)");
        return false;
    }
    return true;
}

bool
llvm_forward_declare_function_routines_from_inventory(
    LLVMGenCtx *ctx,
    const LLVMMIRRoutineInventory *inventory)
{
    if (ctx == NULL || inventory == NULL)
        return false;

    for (size_t i = 0; i < inventory->count; i++) {
        const MIRRoutine *routine = llvm_routine_inventory_get(inventory, i);
        if (routine == NULL) {
            llvm_set_mir_inventory_missing(ctx,
                "MIR-only LLVM path has invalid function routine inventory row");
            return false;
        }
        if (llvm_mir_routine_kind(routine) != MIR_SCOPE_FUNCTION)
            continue;
        const char *routine_name = llvm_mir_routine_name(routine);
        if (routine_name == NULL) {
            llvm_set_mir_inventory_missing(ctx,
                "MIR-only LLVM path has unnamed function routine inventory row");
            return false;
        }
        if (llvm_mir_or_ast_function_is_generic(routine, NULL)) {
            ASTNode *func_decl = NULL;
            if (!llvm_decl_require_function_source_decl(ctx, routine,
                    &func_decl)) {
                return false;
            }
            if (func_decl == NULL)
                continue;
            if (!llvm_register_generic_template_decl(ctx, func_decl))
                return false;
            continue;
        }
        if (llvm_lookup_function(ctx, routine_name) == NULL)
            llvm_forward_declare_func_from_mir(routine, NULL, ctx);
        if (ctx->has_error)
            return false;
    }
    return true;
}

bool
llvm_emit_function_routines_from_inventory(
    LLVMGenCtx *ctx,
    const LLVMMIRRoutineInventory *inventory)
{
    if (ctx == NULL || inventory == NULL)
        return false;

    for (size_t i = 0; i < inventory->count; i++) {
        const MIRRoutine *routine = llvm_routine_inventory_get(inventory, i);
        if (routine == NULL) {
            llvm_set_mir_inventory_missing(ctx,
                "MIR-only LLVM path has invalid function routine inventory row");
            return false;
        }
        if (llvm_mir_routine_kind(routine) != MIR_SCOPE_FUNCTION)
            continue;
        if (llvm_mir_or_ast_function_is_generic(routine, NULL))
            continue;
        if (!llvm_decl_function_routine_has_body_storage(routine)) {
            const char *routine_name = llvm_mir_routine_name(routine);
            llvm_set_mir_inventory_missing(ctx,
                "MIR-only LLVM path missing routine for function '%s'",
                routine_name != NULL ? routine_name : "(anonymous)");
            return false;
        }
        llvm_emit_func_from_mir(routine, ctx);
        if (ctx->has_error)
            return false;
    }
    return true;
}

bool
llvm_validate_function_routine_bodies_from_inventory(
    LLVMGenCtx *ctx,
    const LLVMMIRRoutineInventory *inventory)
{
    if (ctx == NULL || inventory == NULL)
        return false;

    for (size_t i = 0; i < inventory->count; i++) {
        const MIRRoutine *routine = llvm_routine_inventory_get(inventory, i);
        if (routine == NULL) {
            llvm_set_mir_inventory_missing(ctx,
                "MIR-only LLVM path has invalid function routine inventory row");
            return false;
        }
        if (llvm_mir_routine_kind(routine) != MIR_SCOPE_FUNCTION)
            continue;
        const char *routine_name = llvm_mir_routine_name(routine);
        if (routine_name == NULL) {
            llvm_set_mir_inventory_missing(ctx,
                "MIR-only LLVM path has unnamed function routine inventory row");
            return false;
        }
        if (llvm_mir_or_ast_function_is_generic(routine, NULL))
            continue;
        if (llvm_decl_function_routine_has_body_storage(routine))
            continue;
        llvm_set_mir_inventory_missing(ctx,
            "MIR-only LLVM path missing routine for function '%s'",
            routine_name);
        return false;
    }
    return true;
}

#endif /* PGY_LLVM_ENABLED */
