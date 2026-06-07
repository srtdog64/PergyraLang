#ifdef PGY_LLVM_ENABLED
#include "llvm_internal.h"

static bool
llvm_decl_mir_routine_has_instructions(const MIRRoutine *routine)
{
    if (routine == NULL)
        return false;
    if (routine->block_count > 0 && routine->blocks == NULL)
        return true;
    for (size_t i = 0; i < routine->block_count; i++) {
        if (routine->blocks[i].instruction_count > 0)
            return true;
    }
    return false;
}

static ASTNode *
llvm_decl_function_from_routine(const MIRRoutine *routine)
{
    return llvm_mir_routine_source_ast_of_type(
        routine, MIR_SCOPE_FUNCTION, AST_FUNC_DECL);
}

static bool
llvm_decl_require_function_source_ast(LLVMGenCtx *ctx,
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
        && llvm_decl_mir_routine_has_instructions(routine)) {
        const char *routine_name = llvm_mir_routine_name(routine);
        llvm_set_mir_inventory_missing(ctx,
            "MIR-only LLVM path missing function source declaration metadata for routine '%s'",
            routine_name != NULL ? routine_name : "(anonymous)");
        return false;
    }
    return true;
}

static bool
llvm_decl_function_is_generic(const MIRRoutine *routine, ASTNode *func_decl)
{
    GenericParams *generic_params;

    if (llvm_mir_routine_has_signature(routine))
        return llvm_mir_routine_generic_param_count(routine) > 0;
    if (func_decl == NULL)
        return false;
    generic_params = ast_declaration_generic_params(func_decl);
    return ast_generic_param_count(generic_params) > 0;
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
        ASTNode *func_decl = NULL;
        if (!llvm_decl_require_function_source_ast(ctx, routine, &func_decl))
            return false;
        if (func_decl == NULL)
            continue;
        if (llvm_decl_function_is_generic(routine, func_decl)) {
            if (!llvm_register_generic_template_decl(ctx, func_decl))
                return false;
            continue;
        }
        if (llvm_lookup_function(ctx, ast_declaration_name(func_decl)) == NULL)
            llvm_forward_declare_func_from_mir(routine, func_decl, ctx);
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
        ASTNode *func_decl = NULL;
        if (!llvm_decl_require_function_source_ast(ctx, routine, &func_decl))
            return false;
        if (func_decl == NULL
            || llvm_decl_function_is_generic(routine, func_decl)) {
            continue;
        }
        if (llvm_decl_mir_routine_has_instructions(routine))
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
        ASTNode *func_decl = NULL;
        if (!llvm_decl_require_function_source_ast(ctx, routine, &func_decl))
            return false;
        if (func_decl == NULL
            || llvm_decl_function_is_generic(routine, func_decl)) {
            continue;
        }
        if (llvm_decl_mir_routine_has_instructions(routine))
            continue;
        llvm_set_mir_inventory_missing(ctx,
            "MIR-only LLVM path missing routine for function '%s'",
            ast_declaration_name(func_decl) != NULL
                ? ast_declaration_name(func_decl)
                : "(anonymous)");
        return false;
    }
    return true;
}

#endif /* PGY_LLVM_ENABLED */
