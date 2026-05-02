#ifndef PERGYRA_MIR_STMT_POPULATION_H
#define PERGYRA_MIR_STMT_POPULATION_H

#include "mir_cfg_contract_control.h"

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
mir_stmt_is_def_source(const ASTNode *stmt)
{
    if (stmt == NULL)
        return false;
    if (stmt->type == AST_LET_DECL)
        return true;
    if (stmt->type == AST_ASSIGNMENT
        && stmt->data.assignment.target != NULL
        && stmt->data.assignment.target->type == AST_IDENTIFIER)
        return true;
    return false;
}

static const char *
mir_stmt_def_name(const ASTNode *stmt)
{
    if (stmt == NULL)
        return NULL;
    if (stmt->type == AST_LET_DECL)
        return stmt->data.let_decl.name;
    if (stmt->type == AST_ASSIGNMENT
        && stmt->data.assignment.target != NULL
        && stmt->data.assignment.target->type == AST_IDENTIFIER) {
        return stmt->data.assignment.target->data.identifier.name;
    }
    return NULL;
}

static bool
mir_let_decl_requires_stmt_preservation(const ASTNode *stmt)
{
    ASTNode *init;
    ASTNode *callee;
    const char *name;

    if (stmt == NULL || stmt->type != AST_LET_DECL)
        return false;

    init = stmt->data.let_decl.initializer;
    if (init == NULL || init->type != AST_CALL)
        return false;

    callee = init->data.call.callee;
    if (callee == NULL
        || callee->type != AST_IDENTIFIER
        || callee->data.identifier.name == NULL) {
        return false;
    }

    name = callee->data.identifier.name;
    return strcmp(name, "Read") == 0
        || strcmp(name, "ViewRead") == 0
        || strcmp(name, "ViewWrite") == 0
        || strcmp(name, "Move") == 0;
}

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
static bool
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
mir_append_non_cfg_body_statements(MIRRoutine *routine, MIRBasicBlock *entry);

static void
mir_set_inst_source_statement_index(MIRInstruction *inst, size_t index)
{
    if (inst == NULL)
        return;
    inst->source_statement_index = index;
    inst->has_source_statement_index = true;
}

static bool
mir_populate_stmt_instructions(MIRRoutine *routine)
{
    bool has_stmt_inst = false;
    if (routine == NULL)
        return true;
    if (routine->hir_routine == NULL || !routine->hir_routine->has_cfg)
        return true;

    for (size_t block_id = 0; block_id < routine->block_count; block_id++) {
        MIRBasicBlock *block = &routine->blocks[block_id];
        if (block->is_cleanup)
            continue;
        if (block->source_statement_count == 0)
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
        for (size_t s = 0; s < block->source_statement_count; s++) {
            ASTNode *stmt = block->source_statements[s];
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

        for (size_t s = 0; s < block->source_statement_count; s++) {
            ASTNode *stmt = block->source_statements[s];
            if (mir_stmt_is_for_loop_init_payload(stmt, block)) {
                MIRInstruction inst;
                memset(&inst, 0, sizeof(inst));
                inst.id = routine->instruction_count++;
                inst.kind = MIR_INST_LOOP_INIT;
                inst.name = "loop-init";
                inst.ast = stmt;
                inst.arg0 = stmt->data.for_loop.variable;
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
                if (mir_stmt_is_inline_cfg_wrapper(stmt))
                    continue;
                if (block->has_succ_true || block->has_succ_false)
                    break;
                continue;
            }
            if (mir_assignment_requires_stmt_preservation(routine->ast,
                                                          block->source_statements,
                                                          block->source_statement_count,
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
                        mir_set_inst_source_statement_index(&def_inst, s);
                        new_insts[new_count++] = def_inst;
                        continue;
                    }
                    /* Attach the full statement AST so LLVM emitter can
                     * extract both the type annotation and the initializer. */
                    if (def_inst.ast == NULL)
                        def_inst.ast = stmt;
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
                mir_set_inst_source_statement_index(&inst, s);
                new_insts[new_count++] = inst;
            }
        }

        /* Copy remaining semantic-carrier / RESOURCE_OP / CLEANUP_EDGE after DEFs
         * (preserve order). Intent metadata is MIR semantic inventory, not AST
         * fallback body emission, so CFG-backed routines must retain it. */
        for (size_t r = 0; r < old_count; r++) {
            if ((old_insts[r].kind == MIR_INST_STMT
                    && mir_stmt_is_semantic_carrier(&old_insts[r]))
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
        free(copied_flags);
        free(old_insts);
        block->instructions = new_insts;
        block->instruction_count = new_count;
        block->instruction_capacity = new_cap;
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

static bool
mir_append_non_cfg_body_statements(MIRRoutine *routine, MIRBasicBlock *entry)
{
    ASTNode *func_decl;
    ASTNode *body;
    ASTNode **statements = NULL;
    size_t statement_count = 0;

    if (routine == NULL || entry == NULL || routine->ast == NULL)
        return true;

    func_decl = routine->ast;
    if (func_decl->type != AST_FUNC_DECL
        || func_decl->data.func_decl.body == NULL) {
        return true;
    }

    body = func_decl->data.func_decl.body;
    if (body->type != AST_BLOCK)
        return append_instruction(entry, (MIRInstruction){
            .id = routine->instruction_count++,
            .kind = MIR_INST_STMT,
            .name = "stmt",
            .ast = body,
            .source_statement_index = 0,
            .has_source_statement_index = true,
        });

    if (entry->source_statements != NULL && entry->source_statement_count > 0) {
        statements = entry->source_statements;
        statement_count = entry->source_statement_count;
    } else {
        statements = body->data.block.statements;
        statement_count = body->data.block.count;
    }

    for (size_t i = 0; i < statement_count; i++) {
        ASTNode *stmt = statements[i];
        bool matched_def = false;
        if (stmt == NULL)
            continue;
        if (mir_stmt_is_control_flow(stmt, entry))
            continue;
        if (mir_assignment_requires_stmt_preservation(func_decl,
                                                      statements,
                                                      statement_count,
                                                      i,
                                                      stmt)) {
            if (!append_instruction(entry, (MIRInstruction){
                    .id = routine->instruction_count++,
                    .kind = MIR_INST_STMT,
                    .name = "stmt",
                    .ast = stmt,
                    .source_statement_index = i,
                    .has_source_statement_index = true,
                })) {
                return false;
            }
            continue;
        }
        if (stmt->type == AST_LET_DECL
            && mir_let_decl_requires_stmt_preservation(stmt)) {
            if (!append_instruction(entry, (MIRInstruction){
                    .id = routine->instruction_count++,
                    .kind = MIR_INST_STMT,
                    .name = "stmt",
                    .ast = stmt,
                    .source_statement_index = i,
                    .has_source_statement_index = true,
                })) {
                return false;
            }
            continue;
        }
        if (mir_stmt_is_def_source(stmt)) {
            const char *stmt_name = mir_stmt_def_name(stmt);
            for (size_t j = 0; j < entry->instruction_count; j++) {
                MIRInstruction *inst = &entry->instructions[j];
                const char *def_name;
                if (inst->kind != MIR_INST_DEF)
                    continue;
                def_name = inst->arg0 != NULL ? inst->arg0 : inst->slot_anchor;
                if (stmt_name == NULL || def_name == NULL
                    || strcmp(stmt_name, def_name) != 0) {
                    continue;
                }
                if (inst->ast == NULL)
                    inst->ast = stmt;
                mir_set_inst_source_statement_index(inst, i);
                matched_def = true;
                break;
            }
            if (matched_def)
                continue;
        }
        if (!append_instruction(entry, (MIRInstruction){
                .id = routine->instruction_count++,
                .kind = MIR_INST_STMT,
                .name = "stmt",
                .ast = stmt,
                .source_statement_index = i,
                .has_source_statement_index = true,
            })) {
            return false;
        }
    }

    return true;
}

#endif
