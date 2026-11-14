#include "../mir.h"
#include <string.h>

/* Assignment operation is the instruction opcode. Its operands remain the
 * ordinary checked value/place facts, including their SSA and ABI identity. */
const char *
mir_compound_assignment_opcode(AstAssignmentOperation operation)
{
    switch (operation) {
    case AST_ASSIGN_ADD: return "assign-add";
    case AST_ASSIGN_SUBTRACT: return "assign-subtract";
    case AST_ASSIGN_MULTIPLY: return "assign-multiply";
    case AST_ASSIGN_DIVIDE: return "assign-divide";
    case AST_ASSIGN_REMAINDER: return "assign-remainder";
    case AST_ASSIGN_SET: return NULL;
    }
    return NULL;
}

bool
mir_assignment_operation(const MIRInstruction *inst, AstAssignmentOperation *out)
{
    if (inst == NULL || out == NULL || inst->name == NULL)
        return false;
    if (strcmp(inst->name, "assign") == 0 || strcmp(inst->name, "ssa-def") == 0) {
        *out = AST_ASSIGN_SET;
        return true;
    }
    for (AstAssignmentOperation op = AST_ASSIGN_ADD; op <= AST_ASSIGN_REMAINDER; ++op) {
        if (strcmp(inst->name, mir_compound_assignment_opcode(op)) == 0) {
            *out = op;
            return true;
        }
    }
    return false;
}

const char *
mir_scope_kind_name(MIRScopeKind kind)
{
    switch (kind) {
        case MIR_SCOPE_FUNCTION: return "function";
        case MIR_SCOPE_METHOD: return "method";
        case MIR_SCOPE_INTENT: return "intent";
        default: return "unknown";
    }
}

const char *
mir_inst_kind_name(MIRInstKind kind)
{
    switch (kind) {
        case MIR_INST_DEF: return "def";
        case MIR_INST_RESOURCE_OP: return "resource-op";
        case MIR_INST_PHI: return "phi";
        case MIR_INST_BRANCH: return "branch";
        case MIR_INST_RETURN: return "return";
        case MIR_INST_CLEANUP_EDGE: return "cleanup";
        case MIR_INST_LOOP_INIT: return "loop-init";
        case MIR_INST_DESTRUCTURE: return "destructure";
        case MIR_INST_ASSIGN: return "assign";
        case MIR_INST_STMT: return "stmt";
        default: return "unknown";
    }
}

const char *
mir_branch_shape_name(MIRBranchShape shape)
{
    switch (shape) {
        case MIR_BRANCH_EXPR: return "expr";
        case MIR_BRANCH_FOR_RANGE: return "for-range";
        case MIR_BRANCH_FOR_IN: return "for-in";
        case MIR_BRANCH_MATCH_CASE: return "match-case";
        case MIR_BRANCH_SELECT_DISPATCH: return "select-dispatch";
        default: return "unknown";
    }
}
