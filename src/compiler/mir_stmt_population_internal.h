#ifndef PERGYRA_MIR_STMT_POPULATION_INTERNAL_H
#define PERGYRA_MIR_STMT_POPULATION_INTERNAL_H

#include "mir_stmt_population.h"

bool mir_stmt_population_append(MIRInstruction *new_insts,
                                size_t new_cap,
                                size_t *new_count,
                                MIRInstruction inst);
bool mir_copy_resource_ops_for_stmt(MIRInstruction *new_insts,
                                    size_t new_cap,
                                    size_t *new_count,
                                    MIRInstruction *old_insts,
                                    size_t old_count,
                                    bool *copied_flags,
                                    const ASTNode *stmt,
                                    size_t source_statement_index);
void mir_assign_resource_op_source_statement_indices(MIRInstruction *insts,
                                                     size_t inst_count,
                                                     ASTNode **source_items,
                                                     size_t source_count);

#endif /* PERGYRA_MIR_STMT_POPULATION_INTERNAL_H */
