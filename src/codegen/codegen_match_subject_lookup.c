#include "codegen_match_subject_lookup.h"

ASTNode *
pgy_codegen_match_subject_for_branch(const MIRInstruction *inst)
{
    if (inst == NULL || inst->kind != MIR_INST_BRANCH
        || inst->branch_shape != MIR_BRANCH_MATCH_CASE)
        return NULL;
    return inst->expr0;
}
