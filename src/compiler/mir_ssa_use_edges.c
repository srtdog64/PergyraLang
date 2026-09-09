#include "mir_ssa_rename_internal.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "../common/numeric_parse.h"
#include "../common/string_compat.h"
#include "../parser/ast_api.h"
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
    if (!pgy_parse_size_strict_allow_zero(dot + 1, version_out))
        return NULL;
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
mir_use_binding_index(const MIRLocalBinding *names, size_t count,
                      MIRLocalBinding use, int *index)
{
    *index = mir_find_ssa_binding_index(names, count, use.binding_syntax_id);
    if (*index >= 0)
        return strcmp(names[*index].name, use.name) == 0;
    /* Non-SSA parameters, callables and fields are legitimate. A local read
     * without its semantic identity is not repaired from the display name. */
    if (use.binding_syntax_id == 0) {
        for (size_t i = 0; i < count; i++) {
            if (strcmp(names[i].name, use.name) == 0)
                return false;
        }
    }
    return true;
}

/* Legacy RIR operation arguments own only a spelling here. Keep that separate
 * from expression binding resolution, and refuse ambiguity instead of choosing
 * the first of two lexical declarations. */
static int
mir_resource_use_index(const MIRLocalBinding *names, size_t count,
                       const char *name)
{
    int index = -1;
    if (name == NULL)
        return index;
    for (size_t i = 0; i < count; i++) {
        if (strcmp(names[i].name, name) == 0) {
            if (index >= 0)
                return -2;
            index = (int)i;
        }
    }
    return index;
}

static bool
mir_record_instruction_expr_uses(MIRRoutine *routine,
                                 MIRInstruction *inst,
                                 const MIRLocalBinding *ssa_names,
                                 size_t ssa_name_count,
                                 const size_t *current_versions,
                                 char **error_message)
{
    MIRLocalBinding *raw_uses = NULL;
    size_t raw_use_count = 0;
    size_t raw_use_capacity = 0;
    ASTNode *exprs[2] = {inst->kind == MIR_INST_BRANCH
        && inst->branch_shape == MIR_BRANCH_FOR_RANGE
        ? inst->expr1
        : (inst->expr0 != NULL ? inst->expr0 : inst->expr1), NULL};

    /* RIR resource rows carry effect/ABI evidence, not lexical value reads.
     * They may be summarized in the entry block before the defining scope.
     * DEF/STMT expressions own their executable operands; with-scope claim
     * and release are the two resource rows materialized directly instead. */
    if (inst->kind == MIR_INST_RESOURCE_OP
        && !mir_instruction_source_is_with_slot_claim(inst)
        && !mir_instruction_source_is_with_slot_release(inst))
        return true;

    if (inst->kind == MIR_INST_ASSIGN || inst->kind == MIR_INST_LOOP_INIT) {
        if (inst->expr0 == NULL || inst->expr1 == NULL) {
            if (error_message != NULL && *error_message == NULL) {
                *error_message = mir_strdup_fmt(
                    "MIR routine '%s' instruction[%zu] %s is missing MIR expression facts for SSA use edges",
                    routine->name != NULL ? routine->name : "(anonymous)",
                    inst->id,
                    inst->kind == MIR_INST_ASSIGN ? "ASSIGN" : "LOOP_INIT");
            }
            return false;
        }
        /* Typed statements own both RHS/bounds and target-address reads.
         * Omitting either side lets DCE erase a still-consumed join value. */
        exprs[0] = inst->expr0;
        exprs[1] = inst->expr1;
    }
    for (size_t e = 0; e < 2; e++) {
        if (exprs[e] == NULL || (e == 1 && exprs[e] == exprs[0]))
            continue;
        if (!mir_collect_expr_identifier_uses(exprs[e], &raw_uses,
                &raw_use_count, &raw_use_capacity)) {
            free((void *)raw_uses);
            return false;
        }
    }
    size_t expression_use_count = raw_use_count;
    inst->return_expression_use_count = 0;
    /* Inout copy-out is an observable return consumer even when the source
     * return expression does not mention the parameter. Its exact declaration
     * and current version must stay live through the final join. */
    if (inst->kind == MIR_INST_RETURN) {
        for (size_t p = 0; p < mir_routine_param_count(routine); p++) {
            FuncParam *param = mir_routine_param(routine, p);
            if (param == NULL || mir_routine_param_carriage(routine, p)
                    != MIR_PARAM_CARRIAGE_VALUE_RESULT)
                continue;
            if (!mir_append_ssa_binding(&raw_uses, &raw_use_count,
                    &raw_use_capacity, (MIRLocalBinding){param->name,
                        ast_func_param_stable_id(param)})) {
                free(raw_uses);
                return false;
            }
        }
    }
    if (raw_use_count == 0
        && (inst->kind == MIR_INST_RESOURCE_OP
            || inst->kind == MIR_INST_CLEANUP_EDGE)) {
        /* Claim's subject is its output resource, not a read of a prior SSA
         * version. Any initializer operand reads were collected above. */
        const char *candidates[2] = {
            mir_instruction_resource_op_is_claim(inst) ? NULL : inst->arg0,
            inst->arg1
        };
        for (size_t j = 0; j < 2; j++) {
            int idx = mir_resource_use_index(ssa_names, ssa_name_count,
                                              candidates[j]);
            if (idx == -2) {
                free(raw_uses);
                return false;
            }
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
            int idx;
            if (!mir_use_binding_index(ssa_names, ssa_name_count, raw_uses[j], &idx)) {
                if (error_message != NULL && *error_message == NULL)
                    *error_message = mir_strdup_fmt(
                        "MIR routine '%s' instruction[%zu] line %u: SSA use '%s' is missing or contradicts its semantic binding identity",
                        routine->name, inst->id, mir_instruction_source_line(inst), raw_uses[j].name);
                free(raw_uses);
                return false;
            }
            if (idx >= 0) {
                if (!mir_append_versioned_use(inst, raw_uses[j].name,
                        current_versions[idx])) {
                    free((void *)raw_uses);
                    return false;
                }
                routine->use_edge_count++;
            }
            if (inst->kind == MIR_INST_RETURN && j < expression_use_count)
                inst->return_expression_use_count = inst->use_count;
        }
    }

    free((void *)raw_uses);
    return true;
}

static bool
mir_update_current_version_from_result(const MIRLocalBinding *ssa_names,
                                       size_t ssa_name_count,
                                       size_t *current_versions,
                                       const MIRInstruction *inst)
{
    size_t version = 0;
    int idx;
    char *base;

    if (inst->result_name == NULL)
        return true;
    base = mir_parse_versioned_name_owned(inst->result_name, &version);
    if (base == NULL)
        return false;
    idx = mir_find_ssa_binding_index(ssa_names, ssa_name_count, inst->binding_syntax_id);
    bool ok = idx >= 0 && strcmp(ssa_names[idx].name, base) == 0;
    if (ok)
        current_versions[idx] = version;
    free(base);
    return ok;
}

static bool
mir_record_def_uses(MIRRoutine *routine,
                    MIRInstruction *inst,
                    const MIRLocalBinding *ssa_names,
                    size_t ssa_name_count,
                    size_t *current_versions,
                    char **error_message)
{
    ASTNode *expr = mir_def_instruction_source_expr(inst);
    if (expr != NULL) {
        MIRLocalBinding *raw_uses = NULL;
        size_t raw_use_count = 0;
        size_t raw_use_capacity = 0;
        if (!mir_collect_expr_identifier_uses(expr, &raw_uses,
                &raw_use_count, &raw_use_capacity)) {
            free((void *)raw_uses);
            return false;
        }
        for (size_t j = 0; j < raw_use_count; j++) {
            int idx;
            if (!mir_use_binding_index(ssa_names, ssa_name_count, raw_uses[j], &idx)) {
                if (error_message != NULL && *error_message == NULL)
                    *error_message = mir_strdup_fmt(
                        "MIR routine '%s' instruction[%zu] line %u: SSA initializer use '%s' is missing or contradicts its semantic binding identity",
                        routine->name, inst->id, mir_instruction_source_line(inst), raw_uses[j].name);
                free(raw_uses);
                return false;
            }
            if (idx >= 0) {
                if (!mir_append_versioned_use(inst, raw_uses[j].name,
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
                                                  inst);
}

static bool
mir_record_phi_uses(MIRRoutine *routine,
                    MIRInstruction *inst,
                    const MIRLocalBinding *ssa_names,
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
                                                  inst);
}

static bool
mir_populate_block_use_edges(MIRRoutine *routine,
                             MIRBasicBlock *block,
                             const MIRLocalBinding *ssa_names,
                             size_t ssa_name_count,
                             char **error_message)
{
    size_t *current_versions;

    /* Semantic flow intentionally does not type a proven unreachable tail.
     * Such blocks never receive executable SSA read facts. */
    if (!block->is_reachable || block->ssa_entry_versions == NULL
        || block->ssa_version_count != ssa_name_count)
        return true;
    for (size_t n = 0; n < ssa_name_count; n++) {
        if (block->ssa_entry_versions[n] == 0)
            continue;
        if (!mir_append_block_versioned_name(block, true, ssa_names[n].name,
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
                    ssa_name_count, current_versions, error_message))
                goto fail;
            continue;
        }
        if (inst->kind == MIR_INST_BRANCH || inst->kind == MIR_INST_RETURN
            || inst->kind == MIR_INST_STMT
            || inst->kind == MIR_INST_ASSIGN
            || inst->kind == MIR_INST_LOOP_INIT
            || inst->kind == MIR_INST_DESTRUCTURE
            || inst->kind == MIR_INST_RESOURCE_OP
            || inst->kind == MIR_INST_CLEANUP_EDGE) {
            if (!mir_record_instruction_expr_uses(routine, inst, ssa_names,
                    ssa_name_count, current_versions, error_message))
                goto fail;
        }
        if (inst->kind == MIR_INST_DESTRUCTURE) {
            if (inst->destructure_binding_ids == NULL || inst->destructure_binding_names == NULL
                || inst->destructure_binding_count == 0
                || inst->destructure_binding_count > SIZE_MAX / sizeof(const char *)
                || block->renamed_local_count != block->source_local_def_count)
                goto destructure_fail;
            inst->destructure_result_names = pgy_arena_calloc(&routine->scratch,
                inst->destructure_binding_count * sizeof(const char *));
            if (inst->destructure_result_names == NULL)
                goto destructure_fail;
            for (size_t d = 0; d < inst->destructure_binding_count; d++) {
                uint32_t identity = inst->destructure_binding_ids[d];
                int index = mir_find_ssa_binding_index(ssa_names, ssa_name_count, identity);
                size_t local = 0;
                for (; local < block->source_local_def_count; local++)
                    if (block->source_local_defs[local].binding_syntax_id == identity)
                        break;
                if (identity == 0 || index < 0 || local == block->source_local_def_count
                    || inst->destructure_binding_names[d] == NULL
                    || strcmp(ssa_names[index].name, inst->destructure_binding_names[d]) != 0)
                    goto destructure_fail;
                MIRInstruction output = {0};
                output.binding_syntax_id = identity;
                output.result_name = block->renamed_locals[local];
                if (!mir_update_current_version_from_result(ssa_names,
                        ssa_name_count, current_versions, &output))
                    goto destructure_fail;
                inst->destructure_result_names[d] = output.result_name;
            }
        }
        continue;
destructure_fail:
        if (error_message != NULL && *error_message == NULL)
            *error_message = mir_strdup_fmt(
                "MIR routine '%s' destructure instruction[%zu] is missing or contradicts its positional SSA binding identity",
                routine->name != NULL ? routine->name : "(anonymous)", inst->id);
        goto fail;
    }

    for (size_t n = 0; n < ssa_name_count; n++) {
        if (current_versions[n] == 0)
            continue;
        if (!mir_append_block_versioned_name(block, false, ssa_names[n].name,
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
mir_populate_use_edges(MIRRoutine *routine, char **error_message)
{
    if (routine == NULL || routine->hir_routine == NULL)
        return false;
    if (!routine->hir_routine->has_cfg)
        return true;
    const MIRLocalBinding *ssa_names = routine->ssa_bindings;
    size_t ssa_name_count = routine->ssa_binding_count;
    if (ssa_name_count == 0)
        return true;
    if (ssa_names == NULL || ssa_name_count > SIZE_MAX / sizeof(size_t))
        return false;

    for (size_t block_id = 0; block_id < routine->block_count; block_id++) {
        if (!mir_populate_block_use_edges(routine,
                                          &routine->blocks[block_id],
                                          ssa_names,
                                          ssa_name_count, error_message)) {
            return false;
        }
    }
    return true;
}
