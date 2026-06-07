#include "codegen_match_subject_lookup.h"

#include "../parser/ast.h"

ASTNode *
pgy_codegen_match_subject_for_branch(const MIRInstruction *inst)
{
    ASTNode *case_node;

    if (inst == NULL || inst->kind != MIR_INST_BRANCH
        || inst->branch_shape != MIR_BRANCH_MATCH_CASE)
        return NULL;
    case_node = mir_instruction_source_payload(inst);
    if (case_node == NULL || case_node->type != AST_MATCH_CASE)
        return NULL;
    return inst->expr0;
}
