#include "mir_stmt_population.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "mir_call_fact.h"
#include "mir_cfg_contract_control.h"
#include "mir_stmt_population_internal.h"
#include "mir_type_helpers.h"
#include "../parser/ast_api.h"

/* ---------------------------------------------------------------------------
 * mir_populate_stmt_instructions
 *
 * After SSA rename has placed DEF instructions, this pass walks each HIR
 * block's statement list and rebuilds the MIR block instruction array so local
 * dataflow statements stay on typed MIR instructions. Local definitions remain
 * MIR_INST_DEF when SSA produced a matching definition; assignment and
 * destructuring residuals use MIR_INST_ASSIGN / MIR_INST_DESTRUCTURE; only the
 * remaining side-effect statement surface is allowed to stay MIR_INST_STMT.
 * These instructions are interleaved with the existing PHI/BRANCH/RETURN facts
 * in source order.
 * -------------------------------------------------------------------------*/
bool
mir_stmt_population_append(MIRInstruction *new_insts,
                           size_t new_cap,
                           size_t *new_count,
                           MIRInstruction inst)
{
    if (new_insts == NULL || new_count == NULL || *new_count >= new_cap)
        return false;
    new_insts[(*new_count)++] = inst;
    return true;
}

static bool
mir_append_matching_def_for_stmt(MIRInstruction *new_insts,
                                 size_t new_cap,
                                 size_t *new_count,
                                 MIRInstruction *old_insts,
                                 size_t old_count,
                                 bool *copied_flags,
                                 const MIRBasicBlock *block,
                                 ASTNode *stmt,
                                 size_t source_statement_index,
                                 bool *handled_out)
{
    const char *stmt_name = mir_stmt_def_name(stmt);

    if (handled_out != NULL)
        *handled_out = false;
    if (new_insts == NULL || new_count == NULL || old_insts == NULL
        || copied_flags == NULL || stmt_name == NULL) {
        return true;
    }

    for (size_t i = 0; i < old_count; i++) {
        MIRInstruction def_inst;
        const char *def_name;

        if (copied_flags[i] || old_insts[i].kind != MIR_INST_DEF)
            continue;
        def_name = old_insts[i].arg0 != NULL
            ? old_insts[i].arg0
            : old_insts[i].slot_anchor;
        if (def_name == NULL || strcmp(stmt_name, def_name) != 0)
            continue;

        def_inst = old_insts[i];
        if (def_inst.ast == NULL)
            def_inst.ast = stmt;
        mir_attach_def_initializer_call_fact(&def_inst, stmt);
        mir_set_inst_source_statement_index(&def_inst,
                                            source_statement_index);
        mir_mark_select_receive_statement_emit(block, &def_inst);
        if (!mir_stmt_population_append(new_insts, new_cap, new_count,
                                        def_inst)) {
            return false;
        }
        copied_flags[i] = true;
        if (handled_out != NULL)
            *handled_out = true;
        return true;
    }

    return true;
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
    if (mir_block == NULL)
        return mir_stmt_ast_is_cfg_owned_control(stmt);
    if (stmt->type == AST_RETURN)
        return true;
    if (stmt->type == AST_WITH_STMT || stmt->type == AST_UNSAFE_BLOCK)
        return true;
    if (mir_block->has_succ_true || mir_block->has_succ_false) {
        if (mir_stmt_ast_is_cfg_owned_control(stmt))
            return true;
    }
    return false;
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
        bool *copied_flags = old_count > 0
            ? calloc(old_count, sizeof(bool))
            : NULL;
        if (old_count > 0 && copied_flags == NULL)
            return false;
        mir_assign_resource_op_source_statement_indices(
            old_insts,
            old_count,
            mir_block_source_inventory_items(block),
            inventory_count);

        /* Count max possible inserted instructions (worst case: all
         * non-control-flow statements, including let/assignment records that
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
        if (old_count > SIZE_MAX - stmt_count) {
            free(copied_flags);
            return false;
        }
        size_t new_cap = old_count + stmt_count;
        if (new_cap > SIZE_MAX / sizeof(MIRInstruction)) {
            free(copied_flags);
            return false;
        }
        MIRInstruction *new_insts = calloc(new_cap, sizeof(MIRInstruction));
        if (new_insts == NULL) {
            free(copied_flags);
            return false;
        }
        size_t new_count = 0;

        /* Phase 1: copy PHI instructions */
        size_t old_cursor = 0;
        while (old_cursor < old_count && old_insts[old_cursor].kind == MIR_INST_PHI) {
            if (!mir_stmt_population_append(new_insts,
                                            new_cap,
                                            &new_count,
                                            old_insts[old_cursor])) {
                free(copied_flags);
                free(new_insts);
                return false;
            }
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
                if (old_insts[r].kind == MIR_INST_RESOURCE_OP
                    && mir_instruction_has_source_statement_order(&old_insts[r])) {
                    continue;
                }
                if (!mir_stmt_population_append(new_insts,
                                                new_cap,
                                                &new_count,
                                                old_insts[r])) {
                    free(copied_flags);
                    free(new_insts);
                    return false;
                }
                copied_flags[r] = true;
            }
        }

        for (size_t s = 0; s < inventory_count; s++) {
            ASTNode *stmt = mir_block_source_inventory_at(block, s);
            if (mir_stmt_is_for_loop_init_payload(stmt, block)) {
                MIRInstruction inst =
                    mir_make_loop_init_instruction(routine, stmt, s);
                if (!mir_stmt_population_append(new_insts,
                                                new_cap,
                                                &new_count,
                                                inst)) {
                    free(copied_flags);
                    free(new_insts);
                    return false;
                }
                continue;
            }
            if (mir_stmt_is_control_flow(stmt, block)) {
                if (mir_stmt_is_inline_cfg_wrapper(stmt)) {
                    if (!mir_copy_resource_ops_for_stmt(new_insts, new_cap,
                                                        &new_count,
                                                        old_insts, old_count,
                                                        copied_flags, stmt, s)) {
                        free(copied_flags);
                        free(new_insts);
                        return false;
                    }
                    continue;
                }
                if (block->has_succ_true || block->has_succ_false)
                    break;
                continue;
            }
            if (!mir_copy_resource_ops_for_stmt(new_insts, new_cap,
                                                &new_count,
                                                old_insts, old_count,
                                                copied_flags, stmt, s)) {
                free(copied_flags);
                free(new_insts);
                return false;
            }
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
                MIRInstruction inst =
                    mir_make_assignment_instruction(routine, stmt, s);
                if (!mir_stmt_population_append(new_insts,
                                                new_cap,
                                                &new_count,
                                                inst)) {
                    free(copied_flags);
                    free(new_insts);
                    return false;
                }
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
                        if (mir_stmt_requires_source_local_preservation(stmt)) {
                            bool handled = false;
                            if (!mir_append_matching_def_for_stmt(
                                    new_insts, new_cap, &new_count,
                                    old_insts, old_count, copied_flags,
                                    block, stmt, s, &handled)) {
                                free(copied_flags);
                                free(new_insts);
                                return false;
                            }
                            if (!handled) {
                                MIRInstruction inst =
                                    mir_make_source_stmt_instruction(routine, stmt, s);
                                if (!mir_stmt_population_append(new_insts,
                                                                new_cap,
                                                                &new_count,
                                                                inst)) {
                                    free(copied_flags);
                                    free(new_insts);
                                    return false;
                                }
                            }
                            continue;
                        }
                        if (owned_by_later_def) {
                            /* The CFG/SSA path already materializes this
                             * binding in another block. Do not resurrect the
                             * original source let/assignment as a fallback
                             * STMT here, or C/LLVM backends will emit the
                             * declaration twice (plain AST stmt + SSA DEF). */
                            continue;
                        }
                        memset(&def_inst, 0, sizeof(def_inst));
                        def_inst =
                            stmt != NULL && stmt->type == AST_ASSIGNMENT
                                ? mir_make_assignment_instruction(routine, stmt, s)
                                : (stmt != NULL && stmt->type == AST_LET_DESTRUCTURE
                                    ? mir_make_destructure_instruction(routine, stmt, s)
                                    : mir_make_source_stmt_instruction(routine, stmt, s));
                        if (!mir_stmt_population_append(new_insts,
                                                        new_cap,
                                                        &new_count,
                                                        def_inst)) {
                            free(copied_flags);
                            free(new_insts);
                            return false;
                        }
                        continue;
                    }
                    /* Attach the full statement AST so LLVM emitter can
                     * extract both the type annotation and the initializer. */
                    if (def_inst.ast == NULL)
                        def_inst.ast = stmt;
                    mir_attach_def_initializer_call_fact(&def_inst, stmt);
                    mir_set_inst_source_statement_index(&def_inst, s);
                    mir_mark_select_receive_statement_emit(block, &def_inst);
                    if (!mir_stmt_population_append(new_insts,
                                                    new_cap,
                                                    &new_count,
                                                    def_inst)) {
                        free(copied_flags);
                        free(new_insts);
                        return false;
                    }
                    copied_flags[def_cursor] = true;
                    def_cursor++;
                } else {
                    /* No matching DEF (SSA had no local_defs for this var).
                     * Emit the let/assignment as a regular STMT so it still
                     * generates code. */
                    if (mir_stmt_requires_source_local_preservation(stmt)) {
                        bool handled = false;
                        if (!mir_append_matching_def_for_stmt(
                                new_insts, new_cap, &new_count,
                                old_insts, old_count, copied_flags,
                                block, stmt, s, &handled)) {
                            free(copied_flags);
                            free(new_insts);
                            return false;
                        }
                        if (!handled) {
                            MIRInstruction inst =
                                mir_make_source_stmt_instruction(routine, stmt, s);
                            if (!mir_stmt_population_append(new_insts,
                                                            new_cap,
                                                            &new_count,
                                                            inst)) {
                                free(copied_flags);
                                free(new_insts);
                                return false;
                            }
                        }
                        continue;
                    }
                    if (stmt_name != NULL
                        && stmt != NULL
                        && stmt->type == AST_LET_DECL
                        && mir_routine_has_def_for_name(routine, stmt_name)) {
                        def_cursor = saved_cursor;
                        continue;
                    }
                    def_cursor = saved_cursor;
                    MIRInstruction inst =
                        stmt != NULL && stmt->type == AST_ASSIGNMENT
                            ? mir_make_assignment_instruction(routine, stmt, s)
                            : (stmt != NULL && stmt->type == AST_LET_DESTRUCTURE
                                ? mir_make_destructure_instruction(routine, stmt, s)
                                : mir_make_source_stmt_instruction(routine, stmt, s));
                    if (!mir_stmt_population_append(new_insts,
                                                    new_cap,
                                                    &new_count,
                                                    inst)) {
                        free(copied_flags);
                        free(new_insts);
                        return false;
                    }
                }
            } else {
                /* Create the residual instruction owned by this source shape. */
                MIRInstruction inst =
                    stmt != NULL && stmt->type == AST_ASSIGNMENT
                        ? mir_make_assignment_instruction(routine, stmt, s)
                        : (stmt != NULL && stmt->type == AST_LET_DESTRUCTURE
                            ? mir_make_destructure_instruction(routine, stmt, s)
                            : mir_make_source_stmt_instruction(routine, stmt, s));
                if (!mir_stmt_population_append(new_insts,
                                                new_cap,
                                                &new_count,
                                                inst)) {
                    free(copied_flags);
                    free(new_insts);
                    return false;
                }
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
                if (!mir_stmt_population_append(new_insts,
                                                new_cap,
                                                &new_count,
                                                old_insts[r])) {
                    free(copied_flags);
                    free(new_insts);
                    return false;
                }
                copied_flags[r] = true;
            }
        }

        /* Phase 3: copy terminators */
        for (size_t t = 0; t < old_count; t++) {
            if (old_insts[t].kind == MIR_INST_BRANCH
                || old_insts[t].kind == MIR_INST_RETURN) {
                if (!mir_stmt_population_append(new_insts,
                                                new_cap,
                                                &new_count,
                                                old_insts[t])) {
                    free(copied_flags);
                    free(new_insts);
                    return false;
                }
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
