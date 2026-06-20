#ifdef PGY_LLVM_ENABLED

#include "llvm_mir_bind_emit.h"

#include "llvm_internal_api.h"
#include "llvm_stmt_bind.h"

bool
llvm_mir_emit_bind_statement(const MIRInstruction *inst, LLVMGenCtx *ctx)
{
    if (inst == NULL || ctx == NULL)
        return false;
    if (inst->arg0 == NULL || inst->slot_anchor == NULL
        || inst->arg1 == NULL
        || !llvm_emit_bind_statement_parts(ctx, inst->arg0,
            inst->slot_anchor, inst->arg1, NULL)) {
        if (!ctx->has_error) {
            llvm_set_mir_topology_invalid(ctx,
                "LLVM MIR bind statement missing MIR bind facts");
        }
        return false;
    }
    return true;
}

#endif
