#ifndef PERGYRA_MIR_FACT_VALIDATE_H
#define PERGYRA_MIR_FACT_VALIDATE_H

static bool
mir_validate_statement_inventory(const MIRRoutine *routine,
                                 const MIRBasicBlock *block,
                                 size_t block_index,
                                 char **error_message)
{
    if (routine == NULL || block == NULL)
        return false;

    if (block->source_statement_inventory.count > 0
        && block->source_statement_inventory.items == NULL) {
        if (error_message != NULL) {
            *error_message = mir_strdup_fmt(
                "MIR routine '%s' block[%zu] statement inventory has %zu item(s) but no storage",
                routine->name != NULL ? routine->name : "(anonymous)",
                block_index,
                block->source_statement_inventory.count);
        }
        return false;
    }

    for (size_t i = 0; i < block->instruction_count; i++) {
        const MIRInstruction *inst = &block->instructions[i];
        if (!inst->has_source_statement_index)
            continue;
        if (inst->source_statement_index >= block->source_statement_inventory.count) {
            if (error_message != NULL) {
                *error_message = mir_strdup_fmt(
                    "MIR routine '%s' block[%zu] instruction[%zu] source statement index %zu exceeds inventory count %zu",
                    routine->name != NULL ? routine->name : "(anonymous)",
                    block_index,
                    i,
                    inst->source_statement_index,
                    block->source_statement_inventory.count);
            }
            return false;
        }
    }

    return true;
}

static bool
mir_validate_instruction_surface_usage(const MIRRoutine *routine,
                                       const MIRBasicBlock *block,
                                       size_t block_index,
                                       char **error_message)
{
    if (routine == NULL || block == NULL)
        return false;

    for (size_t i = 0; i < block->instruction_count; i++) {
        const MIRInstruction *inst = &block->instructions[i];
        bool has_surface_payload = inst->ast != NULL
                                   || inst->expr0 != NULL
                                   || inst->expr1 != NULL
                                   || inst->has_source_location;
        if (!has_surface_payload)
            continue;
        if (!inst->has_surface_usage_facts) {
            if (error_message != NULL) {
                *error_message = mir_strdup_fmt(
                    "MIR routine '%s' block[%zu] instruction[%zu] has source payload without surface usage facts",
                    routine->name != NULL ? routine->name : "(anonymous)",
                    block_index,
                    i);
            }
            return false;
        }
    }

    return true;
}

#endif /* PERGYRA_MIR_FACT_VALIDATE_H */
