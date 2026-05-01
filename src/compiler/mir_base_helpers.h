static bool
append_instruction(MIRBasicBlock *block, MIRInstruction inst)
{
    if (block == NULL)
        return false;
    if (block->instruction_count == block->instruction_capacity) {
        size_t next_capacity = block->instruction_capacity == 0 ? 8 : block->instruction_capacity * 2;
        MIRInstruction *grown =
            realloc(block->instructions, next_capacity * sizeof(MIRInstruction));
        if (grown == NULL)
            return false;
        block->instructions = grown;
        block->instruction_capacity = next_capacity;
    }
    block->instructions[block->instruction_count] = inst;
    block->instruction_count++;
    return true;
}

static char *
mir_strdup_fmt(const char *fmt, ...)
{
    va_list args;
    va_list copy;
    int length;
    char *result;

    va_start(args, fmt);
    va_copy(copy, args);
    length = vsnprintf(NULL, 0, fmt, copy);
    va_end(copy);
    if (length < 0) {
        va_end(args);
        return NULL;
    }

    result = malloc((size_t)length + 1);
    if (result == NULL) {
        va_end(args);
        return NULL;
    }
    vsnprintf(result, (size_t)length + 1, fmt, args);
    va_end(args);
    return result;
}

static bool
insert_instruction(MIRBasicBlock *block, size_t index, MIRInstruction inst)
{
    if (block == NULL)
        return false;
    if (index > block->instruction_count)
        index = block->instruction_count;
    if (block->instruction_count == block->instruction_capacity) {
        size_t next_capacity = block->instruction_capacity == 0 ? 8 : block->instruction_capacity * 2;
        MIRInstruction *grown =
            realloc(block->instructions, next_capacity * sizeof(MIRInstruction));
        if (grown == NULL)
            return false;
        block->instructions = grown;
        block->instruction_capacity = next_capacity;
    }
    memmove(&block->instructions[index + 1],
            &block->instructions[index],
            (block->instruction_count - index) * sizeof(MIRInstruction));
    block->instructions[index] = inst;
    block->instruction_count++;
    return true;
}

static bool
append_name(const char ***names, size_t *count, size_t *capacity, const char *name)
{
    const char **grown;
    if (names == NULL || count == NULL || capacity == NULL || name == NULL)
        return false;
    if (*count == *capacity) {
        size_t next_capacity = *capacity == 0 ? 8 : *capacity * 2;
        grown = realloc((void *)*names, next_capacity * sizeof(const char *));
        if (grown == NULL)
            return false;
        *names = grown;
        *capacity = next_capacity;
    }
    (*names)[*count] = name;
    (*count)++;
    return true;
}

static bool
append_owned_name(const char ***names, size_t *count, size_t *capacity, char *name)
{
    const char **grown;
    if (names == NULL || count == NULL || capacity == NULL || name == NULL)
        return false;
    if (*count == *capacity) {
        size_t next_capacity = *capacity == 0 ? 8 : *capacity * 2;
        grown = realloc((void *)*names, next_capacity * sizeof(const char *));
        if (grown == NULL) {
            free(name);
            return false;
        }
        *names = grown;
        *capacity = next_capacity;
    }
    (*names)[*count] = name;
    (*count)++;
    return true;
}

static bool
append_name_unique(const char ***names, size_t *count, size_t *capacity, const char *name)
{
    if (names == NULL || count == NULL || capacity == NULL || name == NULL)
        return false;
    for (size_t i = 0; i < *count; i++) {
        if ((*names)[i] != NULL && strcmp((*names)[i], name) == 0)
            return true;
    }
    return append_name(names, count, capacity, name);
}

static bool
mir_name_set_contains(const char **names, size_t count, const char *name)
{
    if (names == NULL || name == NULL)
        return false;
    for (size_t i = 0; i < count; i++) {
        if (names[i] != NULL && strcmp(names[i], name) == 0)
            return true;
    }
    return false;
}

static bool
append_block(MIRRoutine *routine, MIRBasicBlock block)
{
    if (routine == NULL)
        return false;
    if (routine->block_count == routine->block_capacity) {
        size_t next_capacity = routine->block_capacity == 0 ? 8 : routine->block_capacity * 2;
        MIRBasicBlock *grown = realloc(routine->blocks, next_capacity * sizeof(MIRBasicBlock));
        if (grown == NULL)
            return false;
        routine->blocks = grown;
        routine->block_capacity = next_capacity;
    }
    routine->blocks[routine->block_count] = block;
    routine->block_count++;
    return true;
}

static bool
append_routine(MIRProgram *mir, MIRRoutine routine)
{
    if (mir == NULL)
        return false;
    if (mir->routine_count == mir->routine_capacity) {
        size_t next_capacity = mir->routine_capacity == 0 ? 8 : mir->routine_capacity * 2;
        MIRRoutine *grown = realloc(mir->routines, next_capacity * sizeof(MIRRoutine));
        if (grown == NULL)
            return false;
        mir->routines = grown;
        mir->routine_capacity = next_capacity;
    }
    mir->routines[mir->routine_count] = routine;
    mir->routine_count++;
    return true;
}

static char *
mir_make_versioned_name(const char *base, size_t version)
{
    char buffer[128];
    size_t length;
    char *result;
    if (base == NULL)
        base = "tmp";
    snprintf(buffer, sizeof(buffer), "%s.%zu", base, version);
    length = strlen(buffer);
    result = malloc(length + 1);
    if (result == NULL)
        return NULL;
    memcpy(result, buffer, length + 1);
    return result;
}

static bool
copy_indices(size_t **dst, size_t *dst_count, const size_t *src, size_t src_count)
{
    if (src_count == 0) {
        *dst = NULL;
        *dst_count = 0;
        return true;
    }
    *dst = malloc(src_count * sizeof(size_t));
    if (*dst == NULL)
        return false;
    memcpy(*dst, src, src_count * sizeof(size_t));
    *dst_count = src_count;
    return true;
}

static bool
copy_versions(size_t **dst, const size_t *src, size_t count)
{
    if (count == 0) {
        *dst = NULL;
        return true;
    }
    *dst = malloc(count * sizeof(size_t));
    if (*dst == NULL)
        return false;
    memcpy(*dst, src, count * sizeof(size_t));
    return true;
}

static bool
mir_store_block_versions(MIRBasicBlock *block, bool is_entry, const size_t *versions, size_t count)
{
    size_t **target;
    if (block == NULL)
        return false;
    target = is_entry ? &block->ssa_entry_versions : &block->ssa_exit_versions;
    free(*target);
    *target = NULL;
    if (!copy_versions(target, versions, count))
        return false;
    block->ssa_version_count = count;
    return true;
}

static MIRScopeKind
mir_scope_kind_from_hir(const HIRRoutine *routine)
{
    if (routine == NULL)
        return MIR_SCOPE_FUNCTION;
    if (routine->kind == HIR_TOPLEVEL_INTENT)
        return MIR_SCOPE_INTENT;
    if (routine->is_hosted || routine->is_action_like)
        return MIR_SCOPE_METHOD;
    return MIR_SCOPE_FUNCTION;
}

static const RIRScope *
mir_find_matching_rir_scope(const RIRProgram *rir, const HIRRoutine *routine)
{
    RIRScopeKind wanted_kind;
    if (rir == NULL || routine == NULL || routine->name == NULL)
        return NULL;

    if (routine->kind == HIR_TOPLEVEL_INTENT) {
        wanted_kind = RIR_SCOPE_INTENT;
    } else if (routine->is_hosted || routine->is_action_like) {
        wanted_kind = RIR_SCOPE_METHOD;
    } else {
        wanted_kind = RIR_SCOPE_FUNCTION;
    }

    for (size_t i = 0; i < rir->scope_count; i++) {
        const RIRScope *scope = &rir->scopes[i];
        if (scope->kind == wanted_kind
            && scope->name != NULL
            && strcmp(scope->name, routine->name) == 0) {
            if (routine->owner_name != NULL) {
                if (scope->owner_name == NULL
                    || strcmp(scope->owner_name, routine->owner_name) != 0) {
                    continue;
                }
            }
            return scope;
        }
    }
    return NULL;
}
