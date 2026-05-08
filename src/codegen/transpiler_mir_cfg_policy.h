/*
 * Copyright (c) 2026 Pergyra Language Project
 * C backend MIR CFG control policy helpers.
 */

#ifndef PERGYRA_TRANSPILER_MIR_CFG_POLICY_H
#define PERGYRA_TRANSPILER_MIR_CFG_POLICY_H

#include "transpiler.h"

bool transpiler_mir_stmt_is_cfg_container(const ASTNode *node);
const char *transpiler_mir_for_in_length_field(const char *collection_type);
const MIRInstruction *transpiler_mir_find_incoming_for_in_branch(
    const MIRRoutine *routine,
    const MIRBasicBlock *block);
const MIRInstruction *transpiler_mir_find_loop_branch_inst(
    const MIRBasicBlock *block);

#endif /* PERGYRA_TRANSPILER_MIR_CFG_POLICY_H */
