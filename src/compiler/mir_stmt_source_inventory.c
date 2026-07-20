#include "mir_stmt_population.h"
#include "../parser/ast_api.h"

void
mir_set_inst_source_statement_fact(MIRInstruction *inst,
                                   const ASTNode *stmt,
                                   size_t index)
{
    if (inst == NULL)
        return;
    inst->source_statement_index = index;
    inst->has_source_statement_index = true;
    inst->source_statement_stable_id = ast_node_stable_id(stmt);
    inst->has_source_statement_stable_id =
        inst->source_statement_stable_id != 0;
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
