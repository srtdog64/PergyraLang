/*
 * Copyright (c) 2026 Pergyra Language Project
 * MIR intent statement query helpers for C backend emission.
 */

#include "transpiler_mir_intent_query.h"

#include <string.h>

bool
transpiler_mir_intent_has_stmt(const MIRRoutine *routine,
                               const char *step_name,
                               const char *inst_name,
                               const char *arg0)
{
    if (routine == NULL || inst_name == NULL)
        return false;

    for (size_t bi = 0; bi < routine->block_count; bi++) {
        const MIRBasicBlock *block = &routine->blocks[bi];
        if (block->is_cleanup || !block->is_reachable)
            continue;
        for (size_t ii = 0; ii < block->instruction_count; ii++) {
            const MIRInstruction *inst = &block->instructions[ii];
            const char *payload = mir_instruction_intent_payload(inst);
            if (!mir_instruction_is_intent_stmt(inst, inst_name))
                continue;
            if (!mir_instruction_intent_step_matches(inst, step_name))
                continue;
            if (arg0 != NULL) {
                if (payload == NULL || strcmp(payload, arg0) != 0)
                    continue;
            }
            return true;
        }
    }

    return false;
}
