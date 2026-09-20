#ifndef PERGYRA_MIR_BRANCH_SOURCE_FACTS_H
#define PERGYRA_MIR_BRANCH_SOURCE_FACTS_H

#include "mir.h"

MIRBranchShape mir_branch_shape_from_ast(const ASTNode *node);
ASTNode *mir_select_case_channel(ASTNode *node);
bool mir_copy_match_binding_type_facts(MIRRoutine *routine,
                                       const HIRRoutine *hir_routine,
                                       char **error_message);
void mir_free_match_binding_type_facts(MIRRoutine *routine);
const MIRMatchBindingTypeFact *mir_routine_match_binding_type_fact(
    const MIRRoutine *routine,
    uint32_t match_case_syntax_id,
    size_t binding_index);
const MIRMatchBindingTypeFact *
mir_routine_match_binding_type_fact_by_binding_syntax_id(
    const MIRRoutine *routine,
    uint32_t binding_syntax_id);
bool mir_copy_collection_ownership_facts(MIRRoutine *routine,
                                         const HIRRoutine *hir_routine,
                                         char **error_message);
void mir_free_collection_ownership_facts(MIRRoutine *routine);
const MIRCollectionOwnershipFact *mir_routine_collection_ownership_fact(
    const MIRRoutine *routine,
    uint32_t binding_syntax_id);
bool mir_validate_collection_ownership_facts(
    const MIRRoutine *routine,
    char **error_message);
bool mir_capture_match_case_facts(MIRRoutine *routine,
                                  MIRInstruction *inst,
                                  ASTNode *case_node,
                                  ASTNode *subject_node);

#endif /* PERGYRA_MIR_BRANCH_SOURCE_FACTS_H */
