#include "mir_ssa_rename_internal.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "../common/string_compat.h"
#include "mir_base_helpers.h"

static bool
mir_append_versioned_use(MIRInstruction *inst, const char *base, size_t version)
{
    char *versioned;
    if (inst == NULL || base == NULL)
        return true;
    versioned = mir_make_versioned_name(base, version);
    if (versioned == NULL)
        return false;
    return append_owned_name(&inst->uses, &inst->use_count, &inst->use_capacity,
                             versioned);
}

static bool
mir_append_block_versioned_name(MIRBasicBlock *block,
                                bool is_entry,
                                const char *base,
                                size_t version)
{
    char *versioned;
    const char ***names;
    size_t *count;
    size_t *capacity;
    if (block == NULL || base == NULL)
        return true;
    versioned = mir_make_versioned_name(base, version);
    if (versioned == NULL)
        return false;
    names = is_entry ? &block->ssa_entry_values : &block->ssa_exit_values;
    count = is_entry ? &block->ssa_entry_value_count : &block->ssa_exit_value_count;
    capacity = is_entry ? &block->ssa_entry_value_capacity : &block->ssa_exit_value_capacity;
    return append_owned_name(names, count, capacity, versioned);
}

static char *
mir_parse_versioned_name_owned(const char *versioned, size_t *version_out)
{
    const char *dot;
    size_t len;

    if (versioned == NULL || version_out == NULL)
        return NULL;
    dot = strrchr(versioned, '.');
    if (dot == NULL)
        return NULL;
    len = (size_t)(dot - versioned);
    *version_out = (size_t)strtoull(dot + 1, NULL, 10);
    return pergyra_strndup(versioned, len);
}

static ASTNode *
mir_def_instruction_source_expr(const MIRInstruction *inst)
{
    if (inst == NULL || inst->kind != MIR_INST_DEF)
        return NULL;
    return inst->expr0;
}

static bool
mir_record_instruction_expr_uses(MIRRoutine *routine,
                                 MIRInstruction *inst,
                                 const char **ssa_names,
                                 size_t ssa_name_count,
                                 const size_t *current_versions)
{
    const char **raw_uses = NULL;
    size_t raw_use_count = 0;
    size_t raw_use_capacity = 0;
    ASTNode *expr = inst->expr0 != NULL ? inst->expr0 : inst->expr1;

    if (expr == NULL)
        expr = mir_instruction_source_payload(inst);
    if (expr != NULL
        && !mir_collect_expr_identifier_uses(expr, &raw_uses,
            &raw_use_count, &raw_use_capacity)) {
        free((void *)raw_uses);
        return false;
    }

    if (raw_use_count == 0
        && (inst->kind == MIR_INST_RESOURCE_OP
            || inst->kind == MIR_INST_CLEANUP_EDGE)) {
        const char *candidates[2] = {inst->arg0, inst->arg1};
        for (size_t j = 0; j < 2; j++) {
            int idx = mir_find_ssa_name_index(ssa_names, ssa_name_count,
                                              candidates[j]);
            if (idx >= 0) {
                if (!mir_append_versioned_use(inst, candidates[j],
                        current_versions[idx])) {
                    free((void *)raw_uses);
                    return false;
                }
                routine->use_edge_count++;
            }
        }
    } else {
        for (size_t j = 0; j < raw_use_count; j++) {
            int idx = mir_find_ssa_name_index(ssa_names, ssa_name_count,
                                              raw_uses[j]);
            if (idx >= 0) {
                if (!mir_append_versioned_use(inst, raw_uses[j],
                        current_versions[idx])) {
                    free((void *)raw_uses);
                    return false;
                }
                routine->use_edge_count++;
            }
        }
    }

    free((void *)raw_uses);
    return true;
}

static bool
mir_update_current_version_from_result(const char **ssa_names,
                                       size_t ssa_name_count,
                                       size_t *current_versions,
                                       const char *result_name)
{
    size_t version = 0;
    int idx;
    char *base;

    if (result_name == NULL)
        return true;
    base = mir_parse_versioned_name_owned(result_name, &version);
    if (base == NULL)
        return true;
    idx = mir_find_ssa_name_index(ssa_names, ssa_name_count, base);
    if (idx >= 0)
        current_versions[idx] = version;
    free(base);
    return true;
}

static bool
mir_record_def_uses(MIRRoutine *routine,
                    MIRInstruction *inst,
                    const char **ssa_names,
                    size_t ssa_name_count,
                    size_t *current_versions)
{
    ASTNode *expr = mir_def_instruction_source_expr(inst);
    if (expr != NULL) {
        const char **raw_uses = NULL;
        size_t raw_use_count = 0;
        size_t raw_use_capacity = 0;
        if (!mir_collect_expr_identifier_uses(expr, &raw_uses,
                &raw_use_count, &raw_use_capacity)) {
            free((void *)raw_uses);
            return false;
        }
        for (size_t j = 0; j < raw_use_count; j++) {
            int idx = mir_find_ssa_name_index(ssa_names, ssa_name_count,
                                              raw_uses[j]);
            if (idx >= 0) {
                if (!mir_append_versioned_use(inst, raw_uses[j],
                        current_versions[idx])) {
                    free((void *)raw_uses);
                    return false;
                }
                routine->use_edge_count++;
            }
        }
        free((void *)raw_uses);
    }
    return mir_update_current_version_from_result(ssa_names,
                                                  ssa_name_count,
                                                  current_versions,
                                                  inst->result_name);
}

static bool
mir_record_phi_uses(MIRRoutine *routine,
                    MIRInstruction *inst,
                    const char **ssa_names,
                    size_t ssa_name_count,
                    size_t *current_versions)
{
    for (size_t j = 0; j < inst->phi_incoming_count; j++) {
        if (!append_owned_name(&inst->uses,
                               &inst->use_count,
                               &inst->use_capacity,
                               pergyra_strdup(inst->phi_incomings[j].value_name))) {
            return false;
        }
        routine->use_edge_count++;
    }
    return mir_update_current_version_from_result(ssa_names,
                                                  ssa_name_count,
                                                  current_versions,
                                                  inst->result_name);
}

static bool
mir_populate_block_use_edges(MIRRoutine *routine,
                             MIRBasicBlock *block,
                             const char **ssa_names,
                             size_t ssa_name_count)
{
    size_t *current_versions;

    if (block->ssa_entry_versions == NULL
        || block->ssa_version_count != ssa_name_count)
        return true;
    for (size_t n = 0; n < ssa_name_count; n++) {
        if (block->ssa_entry_versions[n] == 0)
            continue;
        if (!mir_append_block_versioned_name(block, true, ssa_names[n],
                block->ssa_entry_versions[n]))
            return false;
    }
    current_versions = calloc(ssa_name_count, sizeof(size_t));
    if (current_versions == NULL)
        return false;
    memcpy(current_versions, block->ssa_entry_versions,
           ssa_name_count * sizeof(size_t));

    for (size_t i = 0; i < block->instruction_count; i++) {
        MIRInstruction *inst = &block->instructions[i];
        if (inst->kind == MIR_INST_PHI) {
            if (!mir_record_phi_uses(routine, inst, ssa_names,
                    ssa_name_count, current_versions))
                goto fail;
            continue;
        }
        if (inst->kind == MIR_INST_DEF) {
            if (!mir_record_def_uses(routine, inst, ssa_names,
                    ssa_name_count, current_versions))
                goto fail;
            continue;
        }
        if (inst->kind == MIR_INST_BRANCH || inst->kind == MIR_INST_RETURN
            || inst->kind == MIR_INST_STMT
            || inst->kind == MIR_INST_RESOURCE_OP
            || inst->kind == MIR_INST_CLEANUP_EDGE) {
            if (!mir_record_instruction_expr_uses(routine, inst, ssa_names,
                    ssa_name_count, current_versions))
                goto fail;
        }
    }

    for (size_t n = 0; n < ssa_name_count; n++) {
        if (current_versions[n] == 0)
            continue;
        if (!mir_append_block_versioned_name(block, false, ssa_names[n],
                current_versions[n]))
            goto fail;
    }
    free(current_versions);
    return true;

fail:
    free(current_versions);
    return false;
}

bool
mir_populate_use_edges(MIRRoutine *routine)
{
    const char **ssa_names = NULL;
    size_t ssa_name_count = 0;

    if (routine == NULL || routine->hir_routine == NULL)
        return false;
    if (!routine->hir_routine->has_cfg)
        return true;
    if (!mir_collect_ssa_names(routine, &ssa_names, &ssa_name_count))
        return false;
    if (ssa_name_count == 0) {
        free((void *)ssa_names);
        return true;
    }
    if (ssa_name_count > SIZE_MAX / sizeof(size_t)) {
        free((void *)ssa_names);
        return false;
    }

    for (size_t block_id = 0; block_id < routine->block_count; block_id++) {
        if (!mir_populate_block_use_edges(routine,
                                          &routine->blocks[block_id],
                                          ssa_names,
                                          ssa_name_count)) {
            free((void *)ssa_names);
            return false;
        }
    }
    free((void *)ssa_names);
    return true;
}
