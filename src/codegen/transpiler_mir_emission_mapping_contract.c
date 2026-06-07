#include "transpiler_mir_emission_mapping_contract.h"

#include <stdlib.h>
#include <string.h>

#include "../compiler/mir.h"
#include "../parser/ast_api.h"
#include "transpiler_inventory_view.h"
#include "transpiler_mir_inventory_intent_collect.h"
#include "transpiler_mir_local_binding.h"
#include "transpiler_mir_pending_uses.h"
#include "transpiler_mir_resource_name_helpers.h"
#include "transpiler_mir_ssa_contract.h"
#include "transpiler_mir_ssa_entry.h"
#include "transpiler_mir_ssa_lookup.h"
#include "transpiler_mir_ssa_map.h"
#include "transpiler_mir_pin_emit.h"

/*
 * C backend MIR emission mapping precheck.
 *
 * This owner validates that every emitted MIR block has enough SSA/name
 * materialization to lower without falling back to AST traversal.
 */
static bool
transpiler_seed_aliases_from_mir_metadata(TranspilerSSANameMap *ssa_map,
                                          const char **aliases,
                                          size_t alias_count)
{
    if (ssa_map == NULL)
        return false;
    for (size_t i = 0; i < alias_count; i++) {
        const char *alias = aliases != NULL ? aliases[i] : NULL;
        if (alias == NULL)
            return false;
        if (!transpiler_ssa_name_map_set(ssa_map, alias, alias))
            return false;
    }
    return true;
}

static bool
transpiler_seed_intent_aliases_for_mapping(TranspilerSSANameMap *ssa_map,
                                           const MIRRoutine *routine,
                                           const ASTNode *intent_decl)
{
    const char **binding_kinds = NULL;
    const char **binding_aliases = NULL;
    const char **binding_types = NULL;
    size_t mir_binding_count;
    bool ok = true;

    if (ssa_map == NULL || routine == NULL || intent_decl == NULL
        || intent_decl->type != AST_INTENT_DECL) {
        return false;
    }

    mir_binding_count = transpiler_collect_mir_intent_bindings(
        routine, &binding_kinds, &binding_aliases, &binding_types);

    for (size_t i = 0; i < mir_binding_count; i++) {
        if (binding_kinds == NULL || binding_aliases == NULL
            || binding_types == NULL || binding_kinds[i] == NULL
            || binding_aliases[i] == NULL || binding_types[i] == NULL
            || (strcmp(binding_kinds[i], "participant") != 0
                && strcmp(binding_kinds[i], "value") != 0)) {
            ok = false;
            goto cleanup;
        }
    }
    ok = transpiler_seed_aliases_from_mir_metadata(
        ssa_map, binding_aliases, mir_binding_count);

cleanup:
    free((void *)binding_kinds);
    free((void *)binding_aliases);
    free((void *)binding_types);
    return ok;
}

bool
transpiler_has_mapping_for_all_emitted_blocks(const TranspilerCtx *ctx,
                                             const MIRRoutine *routine,
                                             const ASTNode *func_decl,
                                             bool require_non_cleanup,
                                             char *reason,
                                             size_t reason_cap)
{
    const char *routine_name = transpiler_mir_routine_name(routine);
    if (routine_name == NULL)
        routine_name = "<routine>";
    if (routine == NULL || routine->blocks == NULL)
        return false;
    for (size_t i = 0; i < routine->block_count; i++) {
        const MIRBasicBlock *block = &routine->blocks[i];
        TranspilerSSANameMap ssa_map = {0};

        if (block == NULL || (require_non_cleanup && block->is_cleanup)
            || !block->is_reachable)
            continue;
        if (!transpiler_emit_mir_block_with_ssa_map(&ssa_map, block))
            return false;
        if (block->is_pin_region
            && block->pin_view_name != NULL
            && block->pin_source_name != NULL) {
            if (!transpiler_ssa_name_map_set(&ssa_map,
                                             block->pin_view_name,
                                             block->pin_source_name)) {
                return false;
            }
        }
        if (func_decl != NULL) {
            if (func_decl->type == AST_FUNC_DECL) {
                bool routine_has_signature =
                    mir_routine_has_signature(routine);
                size_t param_count = routine_has_signature
                    ? mir_routine_param_count(routine)
                    : ast_func_param_count(func_decl);
                for (size_t p = 0; p < param_count; p++) {
                    FuncParam *param = routine_has_signature
                        ? mir_routine_param(routine, p)
                        : ast_func_param(func_decl, p);
                    if (param != NULL && param->name != NULL)
                        transpiler_ssa_name_map_set(&ssa_map,
                            param->name, param->name);
                }
                transpiler_register_with_alias_bindings_in_block(&ssa_map,
                    ast_func_body(func_decl));
            } else if (func_decl->type == AST_INTENT_DECL) {
                if (!transpiler_seed_intent_aliases_for_mapping(
                        &ssa_map,
                        routine,
                        func_decl)) {
                    return false;
                }
            }
        }
        for (size_t j = 0; j < block->instruction_count; j++) {
            const MIRInstruction *inst = &block->instructions[j];
            if (!transpiler_mir_seed_resource_alias_local(&ssa_map, inst))
                return false;
            if (inst->kind == MIR_INST_STMT) {
                if (mir_instruction_source_is_local_decl(inst)
                    && inst->arg0 != NULL) {
                    const char *versioned_name =
                        transpiler_find_block_exit_ssa_name(block, inst->arg0);
                    if (versioned_name != NULL
                        && !transpiler_ssa_name_map_set(&ssa_map,
                                                        inst->arg0,
                                                        versioned_name)) {
                        return false;
                    }
                } else if (mir_instruction_source_is_local_destructure(inst)
                           && block->source_local_defs != NULL) {
                    for (size_t dn = 0;
                         dn < block->source_local_def_count;
                         dn++) {
                        const char *binding =
                            block->source_local_defs[dn];
                        const char *versioned_name;
                        if (binding == NULL)
                            continue;
                        versioned_name =
                            transpiler_find_block_exit_ssa_name(block,
                                binding);
                        if (versioned_name != NULL
                            && !transpiler_ssa_name_map_set(&ssa_map,
                                                            binding,
                                                            versioned_name)) {
                            return false;
                        }
                    }
                } else if (mir_instruction_source_is_assignment(inst)
                           && inst->arg0 != NULL) {
                    const char *target_name = inst->arg0;
                    const char *versioned_name =
                        transpiler_find_block_exit_ssa_name(block, target_name);
                    if (versioned_name != NULL
                        && !transpiler_ssa_name_map_set(&ssa_map,
                                                        target_name,
                                                        versioned_name)) {
                        return false;
                    }
                }
            }
            if (!transpiler_materialize_pending_inst_uses(NULL,
                                                          (TranspilerCtx *)ctx,
                                                          func_decl,
                                                          block,
                                                          inst,
                                                          &ssa_map,
                                                          0,
                                                          false,
                                                          reason,
                                                          reason_cap)) {
                return false;
            }
            if (inst->kind == MIR_INST_BRANCH || inst->kind == MIR_INST_RETURN
                || (inst->kind == MIR_INST_RESOURCE_OP
                    && transpiler_mir_resource_op_lookup(inst->name)
                        == TRANS_MIR_RESOURCE_OP_WRITE)) {
                ASTNode *payload_expr = inst->expr0;
                if (payload_expr != NULL
                    && !transpiler_expr_identifiers_mapped(ctx, payload_expr,
                                                          &ssa_map,
                                                          routine_name,
                                                          reason,
                                                          reason_cap)) {
                    return false;
                }
            }
            if (inst->kind == MIR_INST_DEF || inst->kind == MIR_INST_PHI) {
                char base[128];
                size_t version = 0;
                const char *base_name = inst->kind == MIR_INST_PHI
                    ? inst->name
                    : inst->arg0;
                if (base_name != NULL && base_name[0] != '\0'
                    && inst->result_name != NULL
                    && transpiler_parse_versioned_name(inst->result_name,
                        base, sizeof(base), &version)
                    && !transpiler_ssa_name_map_set(&ssa_map, base_name,
                        inst->result_name)) {
                    return false;
                }
            }
        }
    }
    return true;
}
