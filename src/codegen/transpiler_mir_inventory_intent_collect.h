static size_t
transpiler_collect_mir_intent_who_aliases(const MIRRoutine *routine,
                                          const char *step_name,
                                          const char ***aliases_out)
{
    const char **aliases = NULL;
    size_t count = 0;
    size_t capacity = 0;

    if (aliases_out != NULL)
        *aliases_out = NULL;
    if (routine == NULL || aliases_out == NULL)
        return 0;

    for (size_t bi = 0; bi < routine->block_count; bi++) {
        const MIRBasicBlock *block = &routine->blocks[bi];
        if (block->is_cleanup || !block->is_reachable)
            continue;
        for (size_t ii = 0; ii < block->instruction_count; ii++) {
            const MIRInstruction *inst = &block->instructions[ii];
            const char *payload = mir_instruction_intent_payload(inst);
            const char **grown;

            if (!mir_instruction_is_intent_stmt(inst, "IntentWho"))
                continue;
            if (payload == NULL)
                continue;
            if (!mir_instruction_intent_step_matches(inst, step_name))
                continue;

            if (count >= capacity) {
                size_t new_capacity = capacity == 0 ? 4 : capacity * 2;
                grown = realloc((void *)aliases, new_capacity * sizeof(const char *));
                if (grown == NULL) {
                    free((void *)aliases);
                    return 0;
                }
                aliases = grown;
                capacity = new_capacity;
            }
            aliases[count++] = payload;
        }
    }

    *aliases_out = aliases;
    return count;
}

static size_t
transpiler_collect_mir_intent_authorized_aliases(const MIRRoutine *routine,
                                                 const char *step_name,
                                                 const char ***aliases_out)
{
    const char **aliases = NULL;
    size_t count = 0;
    size_t capacity = 0;

    if (aliases_out != NULL)
        *aliases_out = NULL;
    if (routine == NULL || aliases_out == NULL)
        return 0;

    for (size_t bi = 0; bi < routine->block_count; bi++) {
        const MIRBasicBlock *block = &routine->blocks[bi];
        if (block->is_cleanup || !block->is_reachable)
            continue;
        for (size_t ii = 0; ii < block->instruction_count; ii++) {
            const MIRInstruction *inst = &block->instructions[ii];
            const char *payload = mir_instruction_intent_payload(inst);
            const char **grown;

            if (!mir_instruction_is_intent_stmt(inst, "IntentAuthorizedBy"))
                continue;
            if (payload == NULL)
                continue;
            if (!mir_instruction_intent_step_matches(inst, step_name))
                continue;

            if (count >= capacity) {
                size_t new_capacity = capacity == 0 ? 4 : capacity * 2;
                grown = realloc((void *)aliases, new_capacity * sizeof(const char *));
                if (grown == NULL) {
                    free((void *)aliases);
                    return 0;
                }
                aliases = grown;
                capacity = new_capacity;
            }
            aliases[count++] = payload;
        }
    }

    *aliases_out = aliases;
    return count;
}

static size_t
transpiler_collect_mir_intent_participants(const MIRRoutine *routine,
                                           const char ***aliases_out,
                                           const char ***types_out)
{
    const char **aliases = NULL;
    const char **types = NULL;
    size_t count = 0;
    size_t capacity = 0;

    if (aliases_out != NULL)
        *aliases_out = NULL;
    if (types_out != NULL)
        *types_out = NULL;
    if (routine == NULL || aliases_out == NULL || types_out == NULL)
        return 0;

    for (size_t bi = 0; bi < routine->block_count; bi++) {
        const MIRBasicBlock *block = &routine->blocks[bi];
        if (block->is_cleanup || !block->is_reachable)
            continue;
        for (size_t ii = 0; ii < block->instruction_count; ii++) {
            const MIRInstruction *inst = &block->instructions[ii];
            const char *payload = mir_instruction_intent_payload(inst);
            const char **grown_aliases;
            const char **grown_types;

            if (!mir_instruction_is_intent_stmt(inst, "IntentParticipant"))
                continue;
            if (payload == NULL || inst->arg1 == NULL)
                continue;

            if (count >= capacity) {
                size_t new_capacity = capacity == 0 ? 4 : capacity * 2;
                grown_aliases = malloc(new_capacity * sizeof(const char *));
                grown_types = malloc(new_capacity * sizeof(const char *));
                if (grown_aliases == NULL || grown_types == NULL) {
                    free((void *)grown_aliases);
                    free((void *)grown_types);
                    free((void *)aliases);
                    free((void *)types);
                    return 0;
                }
                if (count > 0) {
                    memcpy((void *)grown_aliases, (const void *)aliases,
                           count * sizeof(const char *));
                    memcpy((void *)grown_types, (const void *)types,
                           count * sizeof(const char *));
                }
                free((void *)aliases);
                free((void *)types);
                aliases = grown_aliases;
                types = grown_types;
                capacity = new_capacity;
            }
            aliases[count] = payload;
            types[count] = inst->arg1;
            count++;
        }
    }

    *aliases_out = aliases;
    *types_out = types;
    return count;
}

static size_t
transpiler_collect_mir_intent_dispatch_aliases(const MIRRoutine *routine,
                                               const char *step_name,
                                               const char ***aliases_out)
{
    const char **aliases = NULL;
    size_t count = 0;
    size_t capacity = 0;

    if (aliases_out != NULL)
        *aliases_out = NULL;
    if (routine == NULL || aliases_out == NULL)
        return 0;

    for (size_t bi = 0; bi < routine->block_count; bi++) {
        const MIRBasicBlock *block = &routine->blocks[bi];
        if (block->is_cleanup || !block->is_reachable)
            continue;
        for (size_t ii = 0; ii < block->instruction_count; ii++) {
            const MIRInstruction *inst = &block->instructions[ii];
            const char *payload = mir_instruction_intent_payload(inst);
            const char **grown;

            if (!mir_instruction_is_intent_stmt(inst, "IntentDispatch"))
                continue;
            if (payload == NULL)
                continue;
            if (!mir_instruction_intent_step_matches(inst, step_name))
                continue;

            if (count >= capacity) {
                size_t new_capacity = capacity == 0 ? 4 : capacity * 2;
                grown = realloc((void *)aliases, new_capacity * sizeof(const char *));
                if (grown == NULL) {
                    free((void *)aliases);
                    return 0;
                }
                aliases = grown;
                capacity = new_capacity;
            }
            aliases[count++] = payload;
        }
    }

    *aliases_out = aliases;
    return count;
}
