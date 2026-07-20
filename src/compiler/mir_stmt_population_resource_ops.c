#include "mir_stmt_population_internal.h"

#include <string.h>

static bool
mir_resource_op_matches_source_stmt(const MIRInstruction *inst,
                                    const ASTNode *stmt)
{
    if (inst == NULL || stmt == NULL || inst->kind != MIR_INST_RESOURCE_OP)
        return false;
    return inst->has_source_statement_stable_id
        && inst->source_statement_stable_id != 0
        && inst->source_statement_stable_id == ast_node_stable_id(stmt);
}

static bool
mir_with_release_matches_body_tail(const MIRInstruction *inst,
                                    const ASTNode *stmt)
{
    ASTNode *body;
    ASTNode *tail;

    if (inst == NULL || stmt == NULL || inst->kind != MIR_INST_RESOURCE_OP
        || inst->name == NULL || strcmp(inst->name, "Release") != 0
        || inst->ast == NULL || inst->ast->type != AST_WITH_STMT)
        return false;
    body = ast_with_body(inst->ast);
    if (body == NULL || body->type != AST_BLOCK)
        return false;
    if (ast_block_statement_count(body) == 0)
        return false;
    tail = ast_block_statement(body,
        ast_block_statement_count(body) - 1);
    return tail != NULL
        && ast_node_stable_id(tail) != 0
        && ast_node_stable_id(tail) == ast_node_stable_id(stmt);
}

bool
mir_copy_resource_ops_for_stmt(MIRInstruction *new_insts,
                               size_t new_cap,
                               size_t *new_count,
                               MIRInstruction *old_insts,
                               size_t old_count,
                               bool *copied_flags,
                               const ASTNode *stmt,
                               size_t source_statement_index)
{
    if (new_insts == NULL || new_count == NULL || old_insts == NULL
        || copied_flags == NULL || stmt == NULL) {
        return true;
    }

    for (size_t r = 0; r < old_count; r++) {
        if (copied_flags[r])
            continue;
        if (old_insts[r].name != NULL
            && strcmp(old_insts[r].name, "Release") == 0
            && old_insts[r].ast != NULL
            && old_insts[r].ast->type == AST_WITH_STMT) {
            if (!mir_with_release_matches_body_tail(&old_insts[r], stmt))
                continue;
            if (!mir_stmt_population_append(new_insts,
                                            new_cap,
                                            new_count,
                                            old_insts[r]))
                return false;
            mir_set_inst_source_statement_fact(&new_insts[*new_count - 1],
                                               stmt, source_statement_index);
            copied_flags[r] = true;
            continue;
        }
        if (!mir_resource_op_matches_source_stmt(&old_insts[r], stmt))
            continue;
        if (!mir_stmt_population_append(new_insts,
                                        new_cap,
                                        new_count,
                                        old_insts[r]))
            return false;
        mir_set_inst_source_statement_fact(&new_insts[*new_count - 1], stmt,
                                           source_statement_index);
        copied_flags[r] = true;
    }
    return true;
}

void
mir_reorder_with_release_after_body_tail(MIRInstruction *insts,
                                          size_t inst_count)
{
    if (insts == NULL || inst_count < 2)
        return;

    for (size_t i = 0; i < inst_count; i++) {
        MIRInstruction release;
        size_t tail = i;

        if (!mir_instruction_source_is_with_slot_release(&insts[i])
            || !mir_instruction_has_source_statement_order(&insts[i])) {
            continue;
        }
        while (tail + 1 < inst_count
               && mir_instruction_has_source_statement_order(&insts[tail + 1])
               && insts[tail + 1].source_statement_index
                   == insts[i].source_statement_index) {
            tail++;
        }
        if (tail == i)
            continue;

        release = insts[i];
        memmove(&insts[i], &insts[i + 1],
                (tail - i) * sizeof(MIRInstruction));
        insts[tail] = release;
    }
}

void
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
                mir_set_inst_source_statement_fact(
                    &insts[i], source_items[s], s);
                break;
            }
        }
    }
}
