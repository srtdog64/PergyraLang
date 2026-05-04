#include "mir_stmt_population.h"

#include <stdlib.h>
#include <string.h>

#include "mir_call_fact.h"
#include "mir_cfg_contract_control.h"
#include "mir_type_helpers.h"

/* ---------------------------------------------------------------------------
 * mir_populate_stmt_instructions
 *
 * After SSA rename has placed DEF instructions, this pass walks each HIR
 * block's statement list and rebuilds the MIR block instruction array so that
 * general statements (function calls, expression statements, assignments to
 * non-identifier targets, etc.) are represented as MIR_INST_STMT instructions
 * interleaved with the existing DEF/PHI/BRANCH/RETURN instructions in the
 * correct source order.
 * -------------------------------------------------------------------------*/
static bool
mir_routine_has_def_for_name(const MIRRoutine *routine, const char *base_name)
{
    if (routine == NULL || base_name == NULL)
        return false;

    for (size_t block_id = 0; block_id < routine->block_count; block_id++) {
        const MIRBasicBlock *block = &routine->blocks[block_id];
        for (size_t inst_id = 0; inst_id < block->instruction_count; inst_id++) {
            const MIRInstruction *inst = &block->instructions[inst_id];
            const char *def_name = NULL;

            if (inst->kind != MIR_INST_DEF)
                continue;

            def_name = inst->arg0 != NULL ? inst->arg0 : inst->slot_anchor;
            if (def_name != NULL && strcmp(def_name, base_name) == 0)
                return true;
        }
    }

    return false;
}

static void
mir_consume_matching_def_instruction(MIRInstruction *old_insts,
                                     size_t old_count,
                                     size_t *def_cursor,
                                     bool *copied_flags,
                                     const char *base_name)
{
    if (old_insts == NULL || def_cursor == NULL || copied_flags == NULL
        || base_name == NULL) {
        return;
    }

    while (*def_cursor < old_count) {
        MIRInstruction *inst = &old_insts[*def_cursor];
        const char *def_name;

        if (inst->kind != MIR_INST_DEF) {
            (*def_cursor)++;
            continue;
        }

        def_name = inst->arg0 != NULL ? inst->arg0 : inst->slot_anchor;
        if (def_name != NULL && strcmp(def_name, base_name) == 0) {
            copied_flags[*def_cursor] = true;
            (*def_cursor)++;
        }
        return;
    }
}

/* Check if a statement is control flow that the HIR has already lowered into
 * separate CFG blocks (and therefore should NOT be emitted as a STMT
 * instruction, because the CFG blocks handle it).
 *
 * Only skip statements whose control flow was actually expanded by the HIR
 * builder. The HIR builder expands CFG-owned control containers into separate
 * CFG blocks when the target MIR block has successor edges. `for` preheader
 * initialization is represented as MIR_INST_LOOP_INIT rather than fallback
 * STMT; its condition and backedge are CFG-owned.
 *
 * AST_RETURN is always skipped because MIR already has MIR_INST_RETURN. */
bool
mir_stmt_is_control_flow(const ASTNode *stmt, const MIRBasicBlock *mir_block)
{
    if (stmt == NULL)
        return true;
    if (stmt->type == AST_RETURN)
        return true;
    if (mir_block->has_succ_true || mir_block->has_succ_false) {
        if (mir_stmt_ast_is_cfg_owned_control(stmt))
            return true;
    }
    return false;
}

static bool
mir_stmt_is_for_loop_init_payload(const ASTNode *stmt, const MIRBasicBlock *mir_block)
{
    return stmt != NULL
        && stmt->type == AST_FOR_LOOP
        && mir_block != NULL
        && (mir_block->has_succ_true || mir_block->has_succ_false);
}

static bool
mir_stmt_is_inline_cfg_wrapper(const ASTNode *stmt)
{
    return stmt != NULL
        && (stmt->type == AST_WITH_STMT
            || stmt->type == AST_UNSAFE_BLOCK);
}

static bool
mir_stmt_population_is_semantic_carrier(const MIRInstruction *inst)
{
    if (inst == NULL || inst->kind != MIR_INST_STMT || inst->name == NULL)
        return false;
    return strncmp(inst->name, "Intent", 6) == 0;
}

void
mir_set_inst_source_statement_index(MIRInstruction *inst, size_t index)
{
    if (inst == NULL)
        return;
    inst->source_statement_index = index;
    inst->has_source_statement_index = true;
}

size_t
mir_block_source_inventory_count(const MIRBasicBlock *block)
{
    return block != NULL ? block->source_statement_inventory.count : 0;
}

ASTNode **
mir_block_source_inventory_items(const MIRBasicBlock *block)
{
    return block != NULL ? block->source_statement_inventory.items : NULL;
}

static ASTNode *
mir_block_source_inventory_at(const MIRBasicBlock *block, size_t index)
{
    if (block == NULL
        || block->source_statement_inventory.items == NULL
        || index >= block->source_statement_inventory.count) {
        return NULL;
    }
    return block->source_statement_inventory.items[index];
}

static bool
mir_resource_op_matches_source_stmt(const MIRInstruction *inst,
                                    const ASTNode *stmt)
{
    const char *anchor;

    if (inst == NULL || stmt == NULL || inst->kind != MIR_INST_RESOURCE_OP)
        return false;
    if (inst->ast == stmt)
        return true;
    if (inst->has_source_location
        && stmt->line != 0
        && inst->source_line == stmt->line
        && inst->source_column == stmt->column) {
        return true;
    }
    if (stmt->type != AST_WITH_STMT
        || inst->name == NULL
        || strcmp(inst->name, "Claim") != 0) {
        return false;
    }
    anchor = inst->slot_anchor != NULL ? inst->slot_anchor : inst->arg0;
    return anchor != NULL
        && stmt->data.with_stmt.alias != NULL
        && strcmp(anchor, stmt->data.with_stmt.alias) == 0;
}

static void
mir_copy_resource_ops_for_stmt(MIRInstruction *new_insts,
                               size_t *new_count,
                               MIRInstruction *old_insts,
                               size_t old_count,
                               bool *copied_flags,
                               const ASTNode *stmt,
                               size_t source_statement_index)
{
    if (new_insts == NULL || new_count == NULL || old_insts == NULL
        || copied_flags == NULL || stmt == NULL) {
        return;
    }

    for (size_t r = 0; r < old_count; r++) {
        if (copied_flags[r])
            continue;
        if (!mir_resource_op_matches_source_stmt(&old_insts[r], stmt))
            continue;
        new_insts[(*new_count)++] = old_insts[r];
        mir_set_inst_source_statement_index(&new_insts[*new_count - 1],
                                            source_statement_index);
        copied_flags[r] = true;
    }
}

static void
mir_assign_resource_op_source_statement_indices(MIRInstruction *insts,
                                                size_t inst_count,
                                                ASTNode **source_items,
                                                size_t source_count)
{
    if (insts == NULL || source_items == NULL || source_count == 0)
        return;

    for (size_t i = 0; i < inst_count; i++) {
        if (insts[i].kind != MIR_INST_RESOURCE_OP
            || insts[i].has_source_statement_index) {
            continue;
        }
        for (size_t s = 0; s < source_count; s++) {
            if (mir_resource_op_matches_source_stmt(&insts[i], source_items[s])) {
                mir_set_inst_source_statement_index(&insts[i], s);
                break;
            }
        }
    }
}

bool
mir_populate_stmt_instructions(MIRRoutine *routine)
{
    bool has_stmt_inst = false;
    if (routine == NULL)
        return true;
    if (routine->hir_routine == NULL || !routine->hir_routine->has_cfg)
        return true;

    for (size_t block_id = 0; block_id < routine->block_count; block_id++) {
        MIRBasicBlock *block = &routine->blocks[block_id];
        size_t inventory_count = mir_block_source_inventory_count(block);
        if (block->is_cleanup)
            continue;
        if (inventory_count == 0)
            continue;

        /* Separate existing instructions into categories */
        MIRInstruction *old_insts = block->instructions;
        size_t old_count = block->instruction_count;
        bool *copied_flags = calloc(old_count, sizeof(bool));
        if (copied_flags == NULL)
            return false;

        /* Count max possible new STMT instructions (worst case: all
         * non-control-flow statements including let/assignment that
         * might not have matching DEFs). */
        size_t stmt_count = 0;
        for (size_t s = 0; s < inventory_count; s++) {
            ASTNode *stmt = mir_block_source_inventory_at(block, s);
            if (mir_stmt_is_for_loop_init_payload(stmt, block)) {
                stmt_count++;
                continue;
            }
            if (mir_stmt_is_control_flow(stmt, block)) {
                if (mir_stmt_is_inline_cfg_wrapper(stmt))
                    continue;
                if (block->has_succ_true || block->has_succ_false)
                    break;
                continue;
            }
            stmt_count++;
        }
        if (stmt_count == 0) {
            free(copied_flags);
            continue;
        }

        /* Allocate new instruction array */
        size_t new_cap = old_count + stmt_count;
        MIRInstruction *new_insts = calloc(new_cap, sizeof(MIRInstruction));
        if (new_insts == NULL) {
            free(copied_flags);
            return false;
        }
        size_t new_count = 0;

        /* Phase 1: copy PHI instructions */
        size_t old_cursor = 0;
        while (old_cursor < old_count && old_insts[old_cursor].kind == MIR_INST_PHI) {
            new_insts[new_count++] = old_insts[old_cursor];
            copied_flags[old_cursor++] = true;
        }

        /* Phase 2: interleave DEFs and STMTs based on HIR statement order */
        size_t def_cursor = old_cursor;
        /* Find the first DEF in remaining old instructions */
        while (def_cursor < old_count
               && old_insts[def_cursor].kind != MIR_INST_DEF)
            def_cursor++;

        /* Copy any RESOURCE_OP between PHIs and DEFs */
        for (size_t r = old_cursor; r < def_cursor; r++) {
            if (old_insts[r].kind == MIR_INST_RESOURCE_OP
                || old_insts[r].kind == MIR_INST_CLEANUP_EDGE) {
                new_insts[new_count++] = old_insts[r];
                copied_flags[r] = true;
            }
        }

        for (size_t s = 0; s < inventory_count; s++) {
            ASTNode *stmt = mir_block_source_inventory_at(block, s);
            if (mir_stmt_is_for_loop_init_payload(stmt, block)) {
                MIRInstruction inst;
                memset(&inst, 0, sizeof(inst));
                inst.id = routine->instruction_count++;
                inst.kind = MIR_INST_LOOP_INIT;
                inst.name = "loop-init";
                inst.ast = stmt;
                inst.arg0 = stmt->data.for_loop.variable;
                inst.branch_shape = stmt->data.for_loop.iterable != NULL
                    ? MIR_BRANCH_FOR_IN
                    : MIR_BRANCH_FOR_RANGE;
                mir_set_inst_source_statement_index(&inst, s);
                if (stmt->data.for_loop.iterable != NULL) {
                    inst.expr0 = stmt->data.for_loop.iterable;
                    inst.expr1 = stmt->data.for_loop.iterable;
                } else {
                    inst.expr0 = stmt->data.for_loop.range_start;
                    inst.expr1 = stmt->data.for_loop.range_end;
                }
                new_insts[new_count++] = inst;
                continue;
            }
            if (mir_stmt_is_control_flow(stmt, block)) {
                if (mir_stmt_is_inline_cfg_wrapper(stmt)) {
                    mir_copy_resource_ops_for_stmt(new_insts, &new_count,
                                                   old_insts, old_count,
                                                   copied_flags, stmt, s);
                    continue;
                }
                if (block->has_succ_true || block->has_succ_false)
                    break;
                continue;
            }
            mir_copy_resource_ops_for_stmt(new_insts, &new_count,
                                           old_insts, old_count,
                                           copied_flags, stmt, s);
            if (mir_assignment_requires_stmt_preservation(routine->ast,
                                                          mir_block_source_inventory_items(block),
                                                          inventory_count,
                                                          s,
                                                          stmt)) {
                mir_consume_matching_def_instruction(old_insts,
                                                     old_count,
                                                     &def_cursor,
                                                     copied_flags,
                                                     mir_stmt_def_name(stmt));
                MIRInstruction inst;
                memset(&inst, 0, sizeof(inst));
                inst.id = routine->instruction_count++;
                inst.kind = MIR_INST_STMT;
                inst.name = "stmt";
                inst.ast = stmt;
                mir_attach_statement_call_fact(&inst, stmt);
                mir_set_inst_source_statement_index(&inst, s);
                new_insts[new_count++] = inst;
                continue;
            }
            if (stmt != NULL
                && stmt->type == AST_LET_DECL
                && mir_let_decl_requires_stmt_preservation(stmt)) {
                mir_consume_matching_def_instruction(old_insts,
                                                     old_count,
                                                     &def_cursor,
                                                     copied_flags,
                                                     mir_stmt_def_name(stmt));
                MIRInstruction inst;
                memset(&inst, 0, sizeof(inst));
                inst.id = routine->instruction_count++;
                inst.kind = MIR_INST_STMT;
                inst.name = "stmt";
                inst.ast = stmt;
                mir_attach_statement_call_fact(&inst, stmt);
                mir_set_inst_source_statement_index(&inst, s);
                new_insts[new_count++] = inst;
                continue;
            }
            if (mir_stmt_is_def_source(stmt)) {
                const char *stmt_name = mir_stmt_def_name(stmt);
                /* Find the next DEF from old instructions */
                size_t saved_cursor = def_cursor;
                while (def_cursor < old_count
                       && old_insts[def_cursor].kind != MIR_INST_DEF)
                    def_cursor++;
                if (def_cursor < old_count) {
                    MIRInstruction def_inst = old_insts[def_cursor];
                    const char *def_name = def_inst.arg0 != NULL
                        ? def_inst.arg0
                        : def_inst.slot_anchor;
                    if (stmt_name == NULL || def_name == NULL
                        || strcmp(stmt_name, def_name) != 0) {
                        bool owned_by_later_def =
                            stmt_name != NULL
                            && stmt != NULL
                            && stmt->type == AST_LET_DECL
                            && mir_routine_has_def_for_name(routine, stmt_name);

                        def_cursor = saved_cursor;
                        if (owned_by_later_def) {
                            /* The CFG/SSA path already materializes this
                             * binding in another block. Do not resurrect the
                             * original source let/assignment as a fallback
                             * STMT here, or C/LLVM backends will emit the
                             * declaration twice (plain AST stmt + SSA DEF). */
                            continue;
                        }
                        memset(&def_inst, 0, sizeof(def_inst));
                        def_inst.id = routine->instruction_count++;
                        def_inst.kind = MIR_INST_STMT;
                        def_inst.name = "stmt";
                        def_inst.ast = stmt;
                        mir_attach_statement_call_fact(&def_inst, stmt);
                        mir_set_inst_source_statement_index(&def_inst, s);
                        new_insts[new_count++] = def_inst;
                        continue;
                    }
                    /* Attach the full statement AST so LLVM emitter can
                     * extract both the type annotation and the initializer. */
                    if (def_inst.ast == NULL)
                        def_inst.ast = stmt;
                    mir_attach_def_initializer_call_fact(&def_inst, stmt);
                    mir_set_inst_source_statement_index(&def_inst, s);
                    new_insts[new_count++] = def_inst;
                    copied_flags[def_cursor] = true;
                    def_cursor++;
                } else {
                    /* No matching DEF (SSA had no local_defs for this var).
                     * Emit the let/assignment as a regular STMT so it still
                     * generates code. */
                    if (stmt_name != NULL
                        && stmt != NULL
                        && stmt->type == AST_LET_DECL
                        && mir_routine_has_def_for_name(routine, stmt_name)) {
                        def_cursor = saved_cursor;
                        continue;
                    }
                    def_cursor = saved_cursor;
                    MIRInstruction inst;
                    memset(&inst, 0, sizeof(inst));
                    inst.id = routine->instruction_count++;
                    inst.kind = MIR_INST_STMT;
                    inst.name = "stmt";
                    inst.ast = stmt;
                    mir_attach_statement_call_fact(&inst, stmt);
                    mir_set_inst_source_statement_index(&inst, s);
                    new_insts[new_count++] = inst;
                }
            } else {
                /* Create new STMT instruction */
                MIRInstruction inst;
                memset(&inst, 0, sizeof(inst));
                inst.id = routine->instruction_count++;
                inst.kind = MIR_INST_STMT;
                inst.name = "stmt";
                inst.ast = stmt;
                mir_attach_statement_call_fact(&inst, stmt);
                mir_set_inst_source_statement_index(&inst, s);
                new_insts[new_count++] = inst;
            }
        }

        /* Copy remaining semantic-carrier / RESOURCE_OP / CLEANUP_EDGE after DEFs
         * (preserve order). Intent metadata is MIR semantic inventory, not AST
         * fallback body emission, so CFG-backed routines must retain it. */
        for (size_t r = 0; r < old_count; r++) {
            if ((old_insts[r].kind == MIR_INST_STMT
                    && mir_stmt_population_is_semantic_carrier(&old_insts[r]))
                || old_insts[r].kind == MIR_INST_RESOURCE_OP
                || old_insts[r].kind == MIR_INST_CLEANUP_EDGE) {
                if (copied_flags[r])
                    continue;
                new_insts[new_count++] = old_insts[r];
                copied_flags[r] = true;
            }
        }

        /* Phase 3: copy terminators */
        for (size_t t = 0; t < old_count; t++) {
            if (old_insts[t].kind == MIR_INST_BRANCH
                || old_insts[t].kind == MIR_INST_RETURN) {
                new_insts[new_count++] = old_insts[t];
            }
        }

        /* Replace block's instruction array */
        mir_assign_resource_op_source_statement_indices(
            new_insts,
            new_count,
            mir_block_source_inventory_items(block),
            inventory_count);
        free(copied_flags);
        free(old_insts);
        block->instructions = new_insts;
        block->instruction_count = new_count;
        block->instruction_capacity = new_cap;
        for (size_t fact_i = 0; fact_i < block->instruction_count; fact_i++)
            mir_instruction_record_surface_usage(&block->instructions[fact_i]);
    }

    for (size_t block_id = 0; block_id < routine->block_count && !has_stmt_inst; block_id++) {
        MIRBasicBlock *block = &routine->blocks[block_id];
        for (size_t i = 0; i < block->instruction_count; i++) {
            if (block->instructions[i].kind == MIR_INST_STMT) {
                has_stmt_inst = true;
                break;
            }
        }
    }

    return true;
}
