#include "mir.h"

#include <stdlib.h>
#include <string.h>

#include "../common/string_compat.h"

static bool
append_instruction(MIRBasicBlock *block, MIRInstruction inst)
{
    MIRInstruction *grown;
    if (block == NULL)
        return false;
    grown = realloc(block->instructions, (block->instruction_count + 1) * sizeof(MIRInstruction));
    if (grown == NULL)
        return false;
    grown[block->instruction_count] = inst;
    block->instructions = grown;
    block->instruction_count++;
    return true;
}

static bool
append_block(MIRRoutine *routine, MIRBasicBlock block)
{
    MIRBasicBlock *grown;
    if (routine == NULL)
        return false;
    grown = realloc(routine->blocks, (routine->block_count + 1) * sizeof(MIRBasicBlock));
    if (grown == NULL)
        return false;
    grown[routine->block_count] = block;
    routine->blocks = grown;
    routine->block_count++;
    return true;
}

static bool
append_routine(MIRProgram *mir, MIRRoutine routine)
{
    MIRRoutine *grown;
    if (mir == NULL)
        return false;
    grown = realloc(mir->routines, (mir->routine_count + 1) * sizeof(MIRRoutine));
    if (grown == NULL)
        return false;
    grown[mir->routine_count] = routine;
    mir->routines = grown;
    mir->routine_count++;
    return true;
}

static bool
copy_indices(size_t **dst, size_t *dst_count, const size_t *src, size_t src_count)
{
    if (src_count == 0) {
        *dst = NULL;
        *dst_count = 0;
        return true;
    }
    *dst = malloc(src_count * sizeof(size_t));
    if (*dst == NULL)
        return false;
    memcpy(*dst, src, src_count * sizeof(size_t));
    *dst_count = src_count;
    return true;
}

static MIRScopeKind
mir_scope_kind_from_hir(const HIRRoutine *routine)
{
    if (routine == NULL)
        return MIR_SCOPE_FUNCTION;
    if (routine->kind == HIR_TOPLEVEL_INTENT)
        return MIR_SCOPE_INTENT;
    if (routine->is_hosted || routine->is_action_like)
        return MIR_SCOPE_METHOD;
    return MIR_SCOPE_FUNCTION;
}

static const RIRScope *
mir_find_matching_rir_scope(const RIRProgram *rir, const HIRRoutine *routine)
{
    RIRScopeKind wanted_kind;
    if (rir == NULL || routine == NULL || routine->name == NULL)
        return NULL;

    if (routine->kind == HIR_TOPLEVEL_INTENT) {
        wanted_kind = RIR_SCOPE_INTENT;
    } else if (routine->is_hosted || routine->is_action_like) {
        wanted_kind = RIR_SCOPE_METHOD;
    } else {
        wanted_kind = RIR_SCOPE_FUNCTION;
    }

    for (size_t i = 0; i < rir->scope_count; i++) {
        const RIRScope *scope = &rir->scopes[i];
        if (scope->kind == wanted_kind
            && scope->name != NULL
            && strcmp(scope->name, routine->name) == 0) {
            return scope;
        }
    }
    return NULL;
}

static bool
mir_add_phi_placeholders(MIRRoutine *routine, MIRBasicBlock *block, const HIRBasicBlock *hir_block)
{
    if (routine == NULL || block == NULL || hir_block == NULL)
        return false;

    for (size_t i = 0; i < hir_block->phi_node_count; i++) {
        MIRInstruction inst;
        memset(&inst, 0, sizeof(inst));
        inst.id = routine->instruction_count++;
        inst.kind = MIR_INST_PHI_PLACEHOLDER;
        inst.name = hir_block->phi_nodes[i].name;
        inst.arg0 = "phi";
        if (!append_instruction(block, inst))
            return false;
    }
    return true;
}

static bool
mir_add_cleanup_instruction(MIRRoutine *routine, MIRBasicBlock *block, const RIROp *op)
{
    MIRInstruction inst;
    memset(&inst, 0, sizeof(inst));
    inst.id = routine->instruction_count++;
    inst.kind = MIR_INST_CLEANUP_EDGE;
    inst.name = rir_op_kind_name(op->kind);
    inst.arg0 = op->subject;
    inst.arg1 = op->arg0;
    inst.rir_op = op;
    inst.ast = op->ast;
    routine->cleanup_instruction_count++;
    return append_instruction(block, inst);
}

static bool
mir_add_resource_instruction(MIRRoutine *routine, MIRBasicBlock *block, const RIROp *op)
{
    MIRInstruction inst;
    memset(&inst, 0, sizeof(inst));
    inst.id = routine->instruction_count++;
    inst.kind = MIR_INST_RESOURCE_OP;
    inst.name = rir_op_kind_name(op->kind);
    inst.arg0 = op->subject;
    inst.arg1 = op->arg0;
    inst.rir_op = op;
    inst.ast = op->ast;
    return append_instruction(block, inst);
}

static bool
mir_populate_instructions(MIRRoutine *routine)
{
    const RIRScope *rir_scope;
    MIRBasicBlock *entry;
    MIRBasicBlock *cleanup;

    if (routine == NULL || routine->block_count == 0)
        return true;

    rir_scope = routine->rir_scope;
    entry = &routine->blocks[routine->entry_block];
    cleanup = routine->has_cleanup_block ? &routine->blocks[routine->cleanup_block] : NULL;

    if (rir_scope == NULL)
        return true;

    for (size_t i = 0; i < rir_scope->op_count; i++) {
        const RIROp *op = &rir_scope->ops[i];
        switch (op->kind) {
            case RIR_OP_ABORT_INTENT:
            case RIR_OP_COMPENSATE_INTENT_STEP:
                if (cleanup != NULL) {
                    if (!mir_add_cleanup_instruction(routine, cleanup, op))
                        return false;
                    break;
                }
                /* fallthrough */
            default:
                if (!mir_add_resource_instruction(routine, entry, op))
                    return false;
                break;
        }
    }

    return true;
}

static bool
mir_build_blocks_from_hir(MIRRoutine *routine, const HIRRoutine *hir_routine)
{
    if (routine == NULL)
        return false;

    if (hir_routine == NULL || !hir_routine->has_cfg || hir_routine->cfg.block_count == 0) {
        MIRBasicBlock block;
        memset(&block, 0, sizeof(block));
        block.id = 0;
        block.is_entry = true;
        block.is_reachable = true;
        block.source_hir_block_id = SIZE_MAX;
        routine->entry_block = 0;
        return append_block(routine, block);
    }

    routine->entry_block = hir_routine->cfg.entry_block;
    for (size_t i = 0; i < hir_routine->cfg.block_count; i++) {
        const HIRBasicBlock *src = &hir_routine->cfg.blocks[i];
        MIRBasicBlock block;
        memset(&block, 0, sizeof(block));
        block.id = i;
        block.is_entry = (i == hir_routine->cfg.entry_block);
        block.is_reachable = src->is_reachable;
        block.source_hir_block_id = src->id;
        block.succ_true = src->succ_true;
        block.succ_false = src->succ_false;
        block.has_succ_true = src->has_succ_true;
        block.has_succ_false = src->has_succ_false;
        if (!copy_indices(&block.predecessors,
                          &block.predecessor_count,
                          src->predecessors,
                          src->predecessor_count)) {
            free(block.predecessors);
            return false;
        }
        if (!append_block(routine, block))
            return false;
    }

    for (size_t i = 0; i < hir_routine->cfg.block_count; i++) {
        if (!mir_add_phi_placeholders(routine, &routine->blocks[i], &hir_routine->cfg.blocks[i]))
            return false;
    }

    return true;
}

static bool
mir_append_cleanup_block(MIRRoutine *routine, const RIRScope *rir_scope)
{
    bool needs_cleanup = false;
    MIRBasicBlock block;

    if (routine == NULL || rir_scope == NULL)
        return true;

    for (size_t i = 0; i < rir_scope->op_count; i++) {
        if (rir_scope->ops[i].kind == RIR_OP_ABORT_INTENT
            || rir_scope->ops[i].kind == RIR_OP_COMPENSATE_INTENT_STEP) {
            needs_cleanup = true;
            break;
        }
    }

    if (!needs_cleanup)
        return true;

    memset(&block, 0, sizeof(block));
    block.id = routine->block_count;
    block.is_cleanup = true;
    block.is_reachable = true;
    block.source_hir_block_id = SIZE_MAX;
    routine->cleanup_block = block.id;
    routine->has_cleanup_block = true;
    return append_block(routine, block);
}

MIRProgram *
mir_lower(const HIRProgram *hir, const RIRProgram *rir, char **error_message)
{
    MIRProgram *mir;
    if (error_message != NULL)
        *error_message = NULL;
    if (hir == NULL) {
        if (error_message != NULL)
            *error_message = pergyra_strdup("MIR lowering requires HIR");
        return NULL;
    }

    mir = calloc(1, sizeof(MIRProgram));
    if (mir == NULL) {
        if (error_message != NULL)
            *error_message = pergyra_strdup("out of memory");
        return NULL;
    }

    for (size_t i = 0; i < hir->routine_count; i++) {
        const HIRRoutine *hir_routine = &hir->routines[i];
        MIRRoutine routine;
        memset(&routine, 0, sizeof(routine));
        routine.id = mir->routine_count;
        routine.kind = mir_scope_kind_from_hir(hir_routine);
        routine.owner_name = NULL;
        routine.name = hir_routine->name;
        routine.hir_routine = hir_routine;
        routine.rir_scope = mir_find_matching_rir_scope(rir, hir_routine);

        if (!mir_build_blocks_from_hir(&routine, hir_routine)
            || !mir_append_cleanup_block(&routine, routine.rir_scope)
            || !mir_populate_instructions(&routine)
            || !append_routine(mir, routine)) {
            if (error_message != NULL)
                *error_message = pergyra_strdup("out of memory");
            mir_destroy(mir);
            return NULL;
        }
    }

    return mir;
}

void
mir_destroy(MIRProgram *mir)
{
    if (mir == NULL)
        return;
    for (size_t i = 0; i < mir->routine_count; i++) {
        MIRRoutine *routine = &mir->routines[i];
        for (size_t j = 0; j < routine->block_count; j++) {
            free(routine->blocks[j].predecessors);
            free(routine->blocks[j].instructions);
        }
        free(routine->blocks);
    }
    free(mir->routines);
    free(mir);
}

const char *
mir_scope_kind_name(MIRScopeKind kind)
{
    switch (kind) {
        case MIR_SCOPE_FUNCTION: return "function";
        case MIR_SCOPE_METHOD: return "method";
        case MIR_SCOPE_INTENT: return "intent";
        default: return "unknown";
    }
}

const char *
mir_inst_kind_name(MIRInstKind kind)
{
    switch (kind) {
        case MIR_INST_RESOURCE_OP: return "resource-op";
        case MIR_INST_PHI_PLACEHOLDER: return "phi";
        case MIR_INST_BRANCH: return "branch";
        case MIR_INST_RETURN: return "return";
        case MIR_INST_CLEANUP_EDGE: return "cleanup";
        default: return "unknown";
    }
}

void
mir_dump(const MIRProgram *mir, FILE *out)
{
    if (out == NULL)
        out = stdout;
    if (mir == NULL) {
        fprintf(out, "MIR: (null)\n");
        return;
    }

    fprintf(out, "MIR Program\n  routines: %zu\n", mir->routine_count);
    for (size_t i = 0; i < mir->routine_count; i++) {
        const MIRRoutine *routine = &mir->routines[i];
        fprintf(out,
                "  routine[%02zu] %-8s %s blocks=%zu instructions=%zu cleanup-block=%s\n",
                i,
                mir_scope_kind_name(routine->kind),
                routine->name != NULL ? routine->name : "(anonymous)",
                routine->block_count,
                routine->instruction_count,
                routine->has_cleanup_block ? "yes" : "no");
        for (size_t j = 0; j < routine->block_count; j++) {
            const MIRBasicBlock *block = &routine->blocks[j];
            fprintf(out,
                    "    block[%02zu] reachable=%s cleanup=%s preds=%zu succT=%s succF=%s instructions=%zu\n",
                    j,
                    block->is_reachable ? "yes" : "no",
                    block->is_cleanup ? "yes" : "no",
                    block->predecessor_count,
                    block->has_succ_true ? "yes" : "no",
                    block->has_succ_false ? "yes" : "no",
                    block->instruction_count);
            for (size_t k = 0; k < block->instruction_count; k++) {
                const MIRInstruction *inst = &block->instructions[k];
                fprintf(out,
                        "      inst[%02zu] %-12s name=%s arg0=%s arg1=%s\n",
                        k,
                        mir_inst_kind_name(inst->kind),
                        inst->name != NULL ? inst->name : "-",
                        inst->arg0 != NULL ? inst->arg0 : "-",
                        inst->arg1 != NULL ? inst->arg1 : "-");
            }
        }
    }
}
