#include "mir_validation.h"

#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char *
mir_validation_strdup_fmt(const char *fmt, ...)
{
    va_list args;
    va_list copy;
    int length;
    int written;
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
    written = vsnprintf(result, (size_t)length + 1, fmt, args);
    va_end(args);
    if (written < 0 || written != length) {
        free(result);
        return NULL;
    }
    return result;
}

static bool
mir_validation_name_set_contains(const char **names, size_t count, const char *name)
{
    if (name == NULL)
        return false;
    for (size_t i = 0; i < count; i++) {
        if (names[i] != NULL && strcmp(names[i], name) == 0)
            return true;
    }
    return false;
}

typedef struct MIRValidationSeenDefinitions {
    const char **slots;
    size_t capacity;
} MIRValidationSeenDefinitions;

static size_t
mir_validation_name_hash(const char *name)
{
    size_t hash = (size_t)1469598103934665603ULL;

    if (name == NULL)
        return 0;
    while (*name != '\0') {
        hash ^= (unsigned char)*name++;
        hash *= (size_t)1099511628211ULL;
    }
    return hash;
}

static bool
mir_validation_seen_definitions_init(MIRValidationSeenDefinitions *seen,
                                     size_t name_count)
{
    size_t capacity = 16;
    size_t required;

    if (seen == NULL)
        return false;
    seen->slots = NULL;
    seen->capacity = 0;
    if (name_count > SIZE_MAX / 2)
        return false;
    required = name_count * 2;
    while (capacity < required) {
        if (capacity > SIZE_MAX / 2)
            return false;
        capacity *= 2;
    }
    seen->slots = calloc(capacity, sizeof(const char *));
    if (seen->slots == NULL)
        return false;
    seen->capacity = capacity;
    return true;
}

static bool
mir_validation_seen_definitions_contains(
    const MIRValidationSeenDefinitions *seen,
    const char *name)
{
    size_t mask;
    size_t index;

    if (seen == NULL || seen->slots == NULL || seen->capacity == 0
        || name == NULL) {
        return false;
    }
    mask = seen->capacity - 1;
    index = mir_validation_name_hash(name) & mask;
    for (size_t probes = 0; probes < seen->capacity; probes++) {
        const char *candidate = seen->slots[index];
        if (candidate == NULL)
            return false;
        if (strcmp(candidate, name) == 0)
            return true;
        index = (index + 1) & mask;
    }
    return false;
}

static bool
mir_validation_seen_definitions_insert(MIRValidationSeenDefinitions *seen,
                                       const char *name)
{
    size_t mask;
    size_t index;

    if (name == NULL)
        return true;
    if (seen == NULL || seen->slots == NULL || seen->capacity == 0)
        return false;
    mask = seen->capacity - 1;
    index = mir_validation_name_hash(name) & mask;
    for (size_t probes = 0; probes < seen->capacity; probes++) {
        const char *candidate = seen->slots[index];
        if (candidate == NULL) {
            seen->slots[index] = name;
            return true;
        }
        if (strcmp(candidate, name) == 0)
            return true;
        index = (index + 1) & mask;
    }
    return false;
}

static bool
mir_validation_block_has_value(const MIRBasicBlock *block, const char *name)
{
    if (block == NULL || name == NULL)
        return false;
    return mir_validation_name_set_contains(block->ssa_entry_values, block->ssa_entry_value_count, name)
           || mir_validation_name_set_contains(block->def_names, block->def_name_count, name)
           || mir_validation_name_set_contains(block->ssa_exit_values, block->ssa_exit_value_count, name);
}

static bool
mir_validation_block_has_predecessor(const MIRBasicBlock *block, size_t predecessor)
{
    if (block == NULL)
        return false;
    for (size_t i = 0; i < block->predecessor_count; i++) {
        if (block->predecessors[i] == predecessor)
            return true;
    }
    return false;
}

bool
mir_validate_block_liveness_sets(const MIRRoutine *routine,
                                 const MIRBasicBlock *block,
                                 size_t block_index,
                                 char **error_message)
{
    if (block == NULL)
        return false;

    for (size_t i = 0; i < block->live_out_name_count; i++) {
        const char *name = block->live_out_names[i];
        if (name == NULL)
            continue;
        if (!mir_validation_block_has_value(block, name)
            && !mir_validation_name_set_contains(block->live_in_names,
                                                 block->live_in_name_count,
                                                 name)) {
            if (error_message != NULL) {
                *error_message = mir_validation_strdup_fmt(
                    "MIR routine '%s' block[%zu] live-out '%s' is not produced by block state",
                    routine->name != NULL ? routine->name : "(anonymous)",
                    block_index,
                    name);
            }
            return false;
        }
    }

    return true;
}

bool
mir_validate_instruction_uses(const MIRRoutine *routine,
                              const MIRBasicBlock *block,
                              size_t block_index,
                              char **error_message)
{
    MIRValidationSeenDefinitions seen;
    size_t seen_name_count;

    if (routine == NULL || block == NULL)
        return false;

    if (block->instruction_count > 0 && block->instructions == NULL) {
        if (error_message != NULL) {
            *error_message = mir_validation_strdup_fmt(
                "MIR routine '%s' block[%zu] has instruction count without instruction inventory during use validation",
                routine->name != NULL ? routine->name : "(anonymous)",
                block_index);
        }
        return false;
    }

    if (block->instruction_count > SIZE_MAX - block->live_in_name_count
        || block->instruction_count + block->live_in_name_count
               > SIZE_MAX - block->ssa_entry_value_count) {
        if (error_message != NULL) {
            *error_message = mir_validation_strdup_fmt(
                "MIR routine '%s' block[%zu] use-validation inventory is too large",
                routine->name != NULL ? routine->name : "(anonymous)",
                block_index);
        }
        return false;
    }
    seen_name_count = block->instruction_count + block->live_in_name_count
        + block->ssa_entry_value_count;
    if (!mir_validation_seen_definitions_init(&seen, seen_name_count)) {
        if (error_message != NULL) {
            *error_message = mir_validation_strdup_fmt(
                "MIR routine '%s' block[%zu] could not allocate use-validation state",
                routine->name != NULL ? routine->name : "(anonymous)",
                block_index);
        }
        return false;
    }
    for (size_t i = 0; i < block->live_in_name_count; i++) {
        if (!mir_validation_seen_definitions_insert(&seen,
                                                    block->live_in_names[i])) {
            free(seen.slots);
            return false;
        }
    }
    for (size_t i = 0; i < block->ssa_entry_value_count; i++) {
        if (!mir_validation_seen_definitions_insert(
                &seen, block->ssa_entry_values[i])) {
            free(seen.slots);
            return false;
        }
    }

    for (size_t i = 0; i < block->instruction_count; i++) {
        const MIRInstruction *inst = &block->instructions[i];
        if (inst->kind == MIR_INST_PHI) {
            for (size_t j = 0; j < inst->phi_incoming_count; j++) {
                size_t pred = inst->phi_incomings[j].predecessor_block;
                const char *value = inst->phi_incomings[j].value_name;
                if (pred >= routine->block_count) {
                    free(seen.slots);
                    if (error_message != NULL) {
                        *error_message = mir_validation_strdup_fmt(
                            "MIR routine '%s' block[%zu] phi references invalid predecessor %zu",
                            routine->name != NULL ? routine->name : "(anonymous)",
                            block_index,
                            pred);
                    }
                    return false;
                }
                if (!mir_validation_block_has_predecessor(block, pred)) {
                    free(seen.slots);
                    if (error_message != NULL) {
                        *error_message = mir_validation_strdup_fmt(
                            "MIR routine '%s' block[%zu] phi predecessor %zu is not in predecessor list",
                            routine->name != NULL ? routine->name : "(anonymous)",
                            block_index,
                            pred);
                    }
                    return false;
                }
                if (!mir_validation_name_set_contains(routine->blocks[pred].ssa_exit_values,
                                                      routine->blocks[pred].ssa_exit_value_count,
                                                      value)
                    && !mir_validation_name_set_contains(routine->blocks[pred].live_out_names,
                                                         routine->blocks[pred].live_out_name_count,
                                                         value)) {
                    free(seen.slots);
                    if (error_message != NULL) {
                        *error_message = mir_validation_strdup_fmt(
                            "MIR routine '%s' block[%zu] phi incoming '%s' is not available from predecessor %zu",
                            routine->name != NULL ? routine->name : "(anonymous)",
                            block_index,
                            value != NULL ? value : "(null)",
                            pred);
                    }
                    return false;
                }
            }
            if (!mir_validation_seen_definitions_insert(&seen,
                                                        inst->result_name)) {
                free(seen.slots);
                return false;
            }
            continue;
        }

        for (size_t j = 0; j < inst->use_count; j++) {
            const char *use = inst->uses[j];
            if (!mir_validation_seen_definitions_contains(&seen, use)) {
                free(seen.slots);
                if (error_message != NULL) {
                    *error_message = mir_validation_strdup_fmt(
                        "MIR routine '%s' block[%zu] instruction[%zu] uses '%s' before definition",
                        routine->name != NULL ? routine->name : "(anonymous)",
                        block_index,
                        i,
                        use != NULL ? use : "(null)");
                }
                return false;
            }
        }
        if (!mir_validation_seen_definitions_insert(&seen,
                                                    inst->result_name)) {
            free(seen.slots);
            return false;
        }
    }

    free(seen.slots);
    return true;
}
