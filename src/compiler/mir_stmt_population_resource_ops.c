#include "mir_stmt_population_internal.h"

#include <string.h>

static bool
mir_resource_op_matches_source_stmt(const MIRInstruction *inst,
                                    const ASTNode *stmt)
{
    const char *anchor;

    if (inst == NULL || stmt == NULL || inst->kind != MIR_INST_RESOURCE_OP)
        return false;
    if (mir_instruction_source_location_matches_node(inst, stmt))
        return true;
    if (stmt->type == AST_CALL
        && mir_instruction_resource_op_is_read(inst)
        && mir_instruction_source_line_matches_node(inst, stmt)) {
        return true;
    }
    if (stmt->type != AST_WITH_STMT
        || !mir_instruction_resource_op_is_claim(inst)) {
        return false;
    }
    anchor = inst->slot_anchor != NULL ? inst->slot_anchor : inst->arg0;
    return anchor != NULL
        && ast_with_alias(stmt) != NULL
        && strcmp(anchor, ast_with_alias(stmt)) == 0;
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
        if (!mir_resource_op_matches_source_stmt(&old_insts[r], stmt))
            continue;
        if (!mir_stmt_population_append(new_insts,
                                        new_cap,
                                        new_count,
                                        old_insts[r]))
            return false;
        mir_set_inst_source_statement_index(&new_insts[*new_count - 1],
                                            source_statement_index);
        copied_flags[r] = true;
    }
    return true;
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
                mir_set_inst_source_statement_index(&insts[i], s);
                break;
            }
        }
    }
}
