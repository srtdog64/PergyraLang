#include "mir.h"

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
