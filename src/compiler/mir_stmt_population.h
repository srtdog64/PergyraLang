#ifndef PERGYRA_MIR_STMT_POPULATION_H
#define PERGYRA_MIR_STMT_POPULATION_H

#include <stdbool.h>

#include "mir.h"

bool mir_stmt_is_def_source(const ASTNode *stmt);
const char *mir_stmt_def_name(const ASTNode *stmt);
bool mir_let_decl_requires_stmt_preservation(const ASTNode *stmt);
bool mir_stmt_is_control_flow(const ASTNode *stmt,
                              const MIRBasicBlock *mir_block);
void mir_set_inst_source_statement_index(MIRInstruction *inst, size_t index);
size_t mir_block_source_inventory_count(const MIRBasicBlock *block);
ASTNode **mir_block_source_inventory_items(const MIRBasicBlock *block);
bool mir_populate_stmt_instructions(MIRRoutine *routine);

#endif /* PERGYRA_MIR_STMT_POPULATION_H */
