#include "mir_stmt_population.h"

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
