#ifndef PERGYRA_MIR_STMT_POPULATION_INTERNAL_H
#define PERGYRA_MIR_STMT_POPULATION_INTERNAL_H

#include "mir_stmt_population.h"

bool mir_stmt_population_append(MIRInstruction *new_insts,
                                size_t new_cap,
                                size_t *new_count,
                                MIRInstruction inst);
bool mir_routine_has_def_for_name(const MIRRoutine *routine,
                                  const char *base_name);
void mir_consume_matching_def_instruction(MIRInstruction *old_insts,
                                          size_t old_count,
                                          size_t *def_cursor,
                                          bool *copied_flags,
                                          const char *base_name);
bool mir_stmt_is_for_loop_init_payload(const ASTNode *stmt,
                                       const MIRBasicBlock *mir_block);
bool mir_stmt_is_inline_cfg_wrapper(const ASTNode *stmt);
bool mir_stmt_population_is_semantic_carrier(const MIRInstruction *inst);
MIRInstruction mir_make_source_stmt_instruction(MIRRoutine *routine,
                                                ASTNode *stmt,
                                                size_t source_statement_index);
MIRInstruction mir_make_destructure_instruction(MIRRoutine *routine,
                                                ASTNode *stmt,
                                                size_t source_statement_index);
MIRInstruction mir_make_assignment_instruction(MIRRoutine *routine,
                                               ASTNode *stmt,
                                               size_t source_statement_index);
MIRInstruction mir_make_loop_init_instruction(MIRRoutine *routine,
                                              ASTNode *stmt,
                                              size_t source_statement_index);
ASTNode *mir_block_source_inventory_at(const MIRBasicBlock *block,
                                       size_t index);
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
