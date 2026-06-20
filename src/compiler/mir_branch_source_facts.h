#ifndef PERGYRA_MIR_BRANCH_SOURCE_FACTS_H
#define PERGYRA_MIR_BRANCH_SOURCE_FACTS_H

#include "mir.h"

MIRBranchShape mir_branch_shape_from_ast(const ASTNode *node);
ASTNode *mir_select_case_channel(ASTNode *node);
void mir_capture_match_case_facts(MIRInstruction *inst, ASTNode *case_node,
                                  ASTNode *subject_node);

#endif /* PERGYRA_MIR_BRANCH_SOURCE_FACTS_H */
