/*
 * Copyright (c) 2026 Pergyra Language Project
 * C backend MIR resource-op statement emission owner.
 */

#include "transpiler_mir_resource_op_emit.h"

#include "transpiler_decl_lookup.h"
#include "transpiler_enum.h"
#include "transpiler_inventory_view.h"
#include "transpiler_mir_pin_emit.h"
#include "transpiler_mir_reason.h"
#include "transpiler_mir_resource_name_helpers.h"
#include "transpiler_mir_resource_hook_emit.h"
#include "transpiler_mir_ssa_lookup.h"
#include "transpiler_mir_ssa_utils.h"
#include "transpiler_symbols.h"
#include "transpiler_mir_ssa_contract.h"

TranspilerMIRInstEmitResult
transpiler_emit_mir_resource_op_inst(CodeBuf *buf,
                                     const MIRRoutine *mir_routine,
                                     const MIRBasicBlock *block,
                                     const MIRInstruction *inst,
                                     size_t inst_index,
                                     TranspilerCtx *ctx,
                                     TranspilerSSANameMap *ssa_map_out,
                                     char *reason,
                                     size_t reason_cap)
{
    TranspilerMIRResourceOp op;

    if (inst == NULL || inst->kind != MIR_INST_RESOURCE_OP)
        return TRANSPILE_MIR_INST_NOT_HANDLED;
    op = transpiler_mir_resource_op_lookup(inst->name);
    if (!transpiler_mir_seed_resource_alias_local(ssa_map_out, inst))
        return TRANSPILE_MIR_INST_FAILED;
    if (op == TRANS_MIR_RESOURCE_OP_WRITE
        && inst->expr0 != NULL) {
        ASTNode *value_expr = inst->expr0;
        char map_reason[256];
        if (value_expr != NULL) {
            if (!transpiler_expr_identifiers_mapped(
                    ctx, value_expr, (const TranspilerSSANameMap *)ssa_map_out,
                    transpiler_mir_routine_name(mir_routine),
                    map_reason, sizeof(map_reason))) {
                return TRANSPILE_MIR_INST_HANDLED;
            }
            if (!transpiler_seed_expr_identifier_mappings(
                    block, inst_index, value_expr, ssa_map_out)) {
                return TRANSPILE_MIR_INST_FAILED;
            }
        }
    }
    if (inst->arg1 != NULL
        && transpiler_resolve_ssa_name(
               (const TranspilerSSANameMap *)ssa_map_out, inst->arg1) == NULL) {
        const char *mapped_value = transpiler_find_prior_block_ssa_name(
            block, inst_index, inst->arg1);
        if (mapped_value == NULL)
            mapped_value = transpiler_find_block_exit_ssa_name(block, inst->arg1);
        if (mapped_value != NULL
            && !transpiler_ssa_name_map_set(ssa_map_out, inst->arg1, mapped_value)) {
            return TRANSPILE_MIR_INST_FAILED;
        }
    }
    if (op == TRANS_MIR_RESOURCE_OP_CLAIM
        && !mir_instruction_is_with_slot_claim(inst)) {
        return TRANSPILE_MIR_INST_HANDLED;
    }
    if (!transpiler_emit_mir_resource_hook(ctx, buf, ctx->indent, inst, "0", false)) {
        if (reason != NULL && reason_cap > 0) {
            transpiler_mir_reasonf(reason, reason_cap,
                     "MIR block %llu emission failed: unable to emit resource op '%s'",
                     (unsigned long long) block->id,
                     inst->name != NULL ? inst->name : "<op>");
        }
        return TRANSPILE_MIR_INST_FAILED;
    }
    return TRANSPILE_MIR_INST_HANDLED;
}
