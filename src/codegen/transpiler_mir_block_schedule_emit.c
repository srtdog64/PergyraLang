/*
 * Copyright (c) 2026 Pergyra Language Project
 * MIR block scheduling/prepass helpers for C emission.
 */

#include "transpiler_mir_block_schedule_emit.h"

#include <stdlib.h>

#include "transpiler_mir_pin_emit.h"
#include "transpiler_mir_reason.h"
#include "transpiler_mir_resource_name_helpers.h"
#include "transpiler_mir_resource_hook_emit.h"

bool
transpiler_mir_block_has_source_order_metadata(const MIRBasicBlock *block)
{
    if (block == NULL)
        return false;

    for (size_t i = 0; i < block->instruction_count; i++) {
        if (mir_instruction_has_source_statement_order(&block->instructions[i]))
            return true;
    }
    return false;
}

static bool
transpiler_mir_inst_should_precede(const MIRInstruction *left,
                                   size_t left_original_index,
                                   const MIRInstruction *right,
                                   size_t right_original_index)
{
    if (left == NULL || right == NULL)
        return left_original_index < right_original_index;

    int source_order =
        mir_instruction_source_statement_order_compare(left, right);
    if (source_order != 0)
        return source_order < 0;

    return left_original_index < right_original_index;
}

bool
transpiler_mir_block_build_source_order(const MIRBasicBlock *block,
                                        size_t **inst_order_out,
                                        char *reason,
                                        size_t reason_cap)
{
    size_t *inst_order;
    size_t order_count = 0;

    if (inst_order_out != NULL)
        *inst_order_out = NULL;
    if (block == NULL || inst_order_out == NULL)
        return false;

    inst_order = calloc(block->instruction_count, sizeof(size_t));
    if (inst_order == NULL) {
        free(inst_order);
        if (reason != NULL && reason_cap > 0) {
            transpiler_mir_reasonf(reason, reason_cap,
                     "MIR block %llu emission failed: unable to allocate source-order schedule",
                     (unsigned long long) block->id);
        }
        return false;
    }

    for (size_t inst_idx = 0; inst_idx < block->instruction_count; inst_idx++) {
        inst_order[order_count++] = inst_idx;
    }

    for (size_t i = 1; i < order_count; i++) {
        size_t current = inst_order[i];
        size_t j = i;
        while (j > 0) {
            size_t previous = inst_order[j - 1];
            if (transpiler_mir_inst_should_precede(&block->instructions[previous],
                                                   previous,
                                                   &block->instructions[current],
                                                   current)) {
                break;
            }
            inst_order[j] = previous;
            j--;
        }
        inst_order[j] = current;
    }

    *inst_order_out = inst_order;
    return true;
}

bool
transpiler_emit_mir_claim_prepass(CodeBuf *buf,
                                  const MIRBasicBlock *block,
                                  TranspilerCtx *ctx,
                                  char *reason,
                                  size_t reason_cap)
{
    if (buf == NULL || block == NULL || ctx == NULL)
        return false;

    for (size_t i = 0; i < block->instruction_count; i++) {
        const MIRInstruction *inst = &block->instructions[i];
        if (inst->kind != MIR_INST_RESOURCE_OP)
            continue;
        TranspilerMIRResourceOp op =
            transpiler_mir_resource_op_lookup(inst->name);
        if (op != TRANS_MIR_RESOURCE_OP_CLAIM)
            continue;
        if (!transpiler_mir_block_has_local_def_for_anchor(
                block, inst->slot_anchor != NULL ? inst->slot_anchor : inst->arg0)) {
            continue;
        }
        if (!transpiler_emit_mir_resource_hook(ctx, buf, ctx->indent,
                                               block, inst, "0", false)) {
            if (reason != NULL && reason_cap > 0) {
                transpiler_mir_reasonf(reason, reason_cap,
                         "MIR block %zu emission failed: unable to emit claim op for '%s'",
                         block->id,
                         inst->slot_anchor != NULL ? inst->slot_anchor : "<slot>");
            }
            return false;
        }
    }
    return true;
}
