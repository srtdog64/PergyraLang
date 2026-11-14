#ifdef PGY_LLVM_ENABLED

#include "llvm_mir_local_expected_type.h"

#include "llvm_internal_api.h"
#include "llvm_mir_local_type_lookup.h"
#include "llvm_mir_vars.h"

const char *
llvm_mir_local_expected_type_name(const MIRRoutine *routine,
                                  const MIRInstruction *inst,
                                  const char *base_name)
{
    const char *type_name;

    if (inst == NULL)
        return NULL;
    if (inst->abi_type_name != NULL && inst->abi_type_name[0] != '\0')
        return inst->abi_type_name;
    if (routine == NULL)
        return NULL;
    if (inst->arg0 != NULL) {
        type_name = mir_routine_source_local_type_name(routine, inst->arg0);
        if (type_name != NULL && type_name[0] != '\0')
            return type_name;
    }
    if (base_name != NULL) {
        type_name = mir_routine_source_local_type_name(routine, base_name);
        if (type_name != NULL && type_name[0] != '\0')
            return type_name;
    }
    if (inst->result_name != NULL) {
        char result_base[128];
        if (llvm_mir_base_name_from_versioned(inst->result_name, result_base,
                sizeof(result_base))) {
            type_name = mir_routine_source_local_type_name(routine,
                result_base);
            if (type_name != NULL && type_name[0] != '\0')
                return type_name;
        }
    }
    return NULL;
}

LLVMTypeRef
llvm_mir_local_infer_expr_type(const MIRRoutine *routine,
                               LLVMGenCtx *ctx,
                               const MIRInstruction *inst,
                               const char *base_name,
                               ASTNode *expr)
{
    const char *saved_expected_type_name;
    ASTNode *saved_expected_callable_type;
    const char *expected_type_name;
    LLVMTypeRef type;

    if (ctx == NULL || expr == NULL)
        return NULL;
    saved_expected_type_name = ctx->expected_type_name;
    saved_expected_callable_type = ctx->expected_callable_type;
    expected_type_name = llvm_mir_local_expected_type_name(routine, inst,
        base_name);
    if (expected_type_name != NULL && expected_type_name[0] != '\0')
        ctx->expected_type_name = expected_type_name;
    if (inst != NULL && inst->expr1 != NULL
        && inst->expr1->type == AST_EVENT_HANDLER_TYPE) {
        ctx->expected_callable_type = inst->expr1;
    }
    type = llvm_mir_local_array_access_type(routine, ctx, expr);
    if (type == NULL && expr->type != AST_ARRAY_ACCESS)
        type = llvm_stmt_infer_expr_type(ctx, expr);
    ctx->expected_callable_type = saved_expected_callable_type;
    ctx->expected_type_name = saved_expected_type_name;
    return type;
}

#endif
