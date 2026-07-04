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
#include "codegen_mir_resource_name_helpers.h"
#include "transpiler_mir_resource_hook_emit.h"
#include "transpiler_mir_ssa_lookup.h"
#include "transpiler_mir_ssa_utils.h"
#include "transpiler_symbols.h"
#include "transpiler_mir_ssa_contract.h"

#include <string.h>

static void
transpiler_mir_register_with_slot_claim_fact(TranspilerCtx *ctx,
                                             const MIRInstruction *inst)
{
    const char *alias;
    const char *type_name;
    char inner_buf[128];
    bool is_secure;

    if (ctx == NULL || inst == NULL
        || !mir_instruction_is_with_slot_claim(inst)
        || inst->abi_type_name == NULL) {
        return;
    }
    alias = inst->slot_anchor != NULL ? inst->slot_anchor : inst->arg0;
    type_name = inst->abi_type_name;
    if (alias == NULL || alias[0] == '\0')
        return;
    if (lookup_typed_var(ctx, alias) != NULL
        && lookup_slot_type_copy(ctx, alias, inner_buf, sizeof(inner_buf))) {
        return;
    }
    is_secure = strncmp(type_name, "SecureSlot<", 11) == 0;
    if (!is_secure && strncmp(type_name, "Slot<", 5) != 0)
        return;
    if (!slot_inner_type_name_copy(type_name, inner_buf, sizeof(inner_buf))
        || inner_buf[0] == '\0') {
        return;
    }
    register_typed_var(ctx, alias, type_name);
    register_slot_var(ctx, alias, inner_buf, is_secure, false);
}

void
transpiler_register_mir_with_slot_claim_facts(TranspilerCtx *ctx,
                                              const MIRRoutine *routine)
{
    if (ctx == NULL || routine == NULL)
        return;
    for (size_t bi = 0; bi < routine->block_count; bi++) {
        const MIRBasicBlock *block = &routine->blocks[bi];
        if (block == NULL || !block->is_reachable || block->is_cleanup)
            continue;
        for (size_t ii = 0; ii < block->instruction_count; ii++) {
            const MIRInstruction *inst = &block->instructions[ii];
            if (inst->kind == MIR_INST_RESOURCE_OP
                && mir_instruction_is_with_slot_claim(inst)) {
                transpiler_mir_register_with_slot_claim_fact(ctx, inst);
            }
        }
    }
}

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
    if (op == TRANS_MIR_RESOURCE_OP_CLAIM)
        transpiler_mir_register_with_slot_claim_fact(ctx, inst);
    if (!transpiler_emit_mir_resource_hook(ctx, buf, ctx->indent, block, inst, "0", false)) {
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
