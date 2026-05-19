#ifdef PGY_LLVM_ENABLED
#include "llvm_internal.h"

static bool
llvm_decl_mir_routine_has_instructions(const MIRRoutine *routine)
{
    if (routine == NULL)
        return false;
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
llvm_decl_function_is_generic(ASTNode *func_decl)
{
    GenericParams *generic_params;

    if (func_decl == NULL)
        return false;
    generic_params = ast_func_generic_params(func_decl);
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
        const MIRRoutine *routine = &inventory->routines[i];
        ASTNode *func_decl = llvm_decl_function_from_routine(routine);
        if (func_decl == NULL)
            continue;
        if (llvm_decl_function_is_generic(func_decl)) {
            if (!llvm_register_generic_template_decl(ctx, func_decl))
                return false;
            continue;
        }
        if (llvm_lookup_function(ctx, ast_declaration_name(func_decl)) == NULL)
            llvm_forward_declare_func(func_decl, ctx);
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
        const MIRRoutine *routine = &inventory->routines[i];
        ASTNode *func_decl = llvm_decl_function_from_routine(routine);
        if (func_decl == NULL || llvm_decl_function_is_generic(func_decl))
            continue;
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
        const MIRRoutine *routine = &inventory->routines[i];
        ASTNode *func_decl = llvm_decl_function_from_routine(routine);
        if (func_decl == NULL || llvm_decl_function_is_generic(func_decl))
            continue;
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
