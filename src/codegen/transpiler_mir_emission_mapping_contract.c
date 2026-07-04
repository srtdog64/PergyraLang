#include "transpiler_mir_emission_mapping_contract.h"

#include <stdlib.h>
#include <string.h>

#include "../compiler/mir.h"
#include "../parser/ast_api.h"
#include "transpiler_inventory_view.h"
#include "transpiler_mir_inventory_intent_collect.h"
#include "transpiler_mir_local_binding.h"
#include "transpiler_mir_pending_uses.h"
#include "codegen_mir_resource_name_helpers.h"
#include "transpiler_mir_ssa_contract.h"
#include "transpiler_mir_ssa_entry.h"
#include "transpiler_mir_ssa_lookup.h"
#include "transpiler_mir_ssa_map.h"
#include "transpiler_mir_pin_emit.h"
#include "transpiler_mir_stmt_emit.h"

/*
 * C backend MIR emission mapping precheck.
 *
 * This owner validates that every emitted MIR block has enough SSA/name
 * materialization to lower without falling back to AST traversal.
 */
static bool
transpiler_seed_aliases_from_mir_metadata(
    TranspilerSSANameMap *ssa_map,
    const IntentBindingMetadataView *bindings)
{
    if (ssa_map == NULL)
        return false;
    if (bindings == NULL)
        return false;
    for (size_t i = 0; i < bindings->count; i++) {
        const char *alias =
            intent_binding_metadata_view_alias_at(bindings, i);
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
    IntentBindingMetadataView binding_metadata = {0};
    size_t mir_binding_count;
    bool ok = true;

    if (ssa_map == NULL || routine == NULL || intent_decl == NULL
        || intent_decl->type != AST_INTENT_DECL) {
        return false;
    }

    mir_binding_count = transpiler_collect_mir_intent_bindings(
        routine, &binding_metadata);

    for (size_t i = 0; i < mir_binding_count; i++) {
        if (!intent_binding_metadata_view_has_supported_row(
                &binding_metadata, i)) {
            ok = false;
            goto cleanup;
        }
    }
    ok = transpiler_seed_aliases_from_mir_metadata(
        ssa_map, &binding_metadata);

cleanup:
    intent_binding_metadata_view_dispose(&binding_metadata);
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
                size_t param_count = mir_routine_param_count(routine);
                for (size_t p = 0; p < param_count; p++) {
                    FuncParam *param = mir_routine_param(routine, p);
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
                                                          routine,
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
                /* Resource ops attributed to the SSA def-block by upstream MIR
                 * lowering may carry an expr0 whose identifiers are only mapped
                 * in their owning use-block. Skip the mapping contract for a
                 * Write resource op whose paired MIR_INST_STMT lives elsewhere
                 * -- the C-side emit policy (transpiler_emit_mir_resource_hook)
                 * already routes that op to export-only, so the def-block does
                 * not need to resolve identifiers that get their SSA version
                 * inside the use-block stmt. */
                bool skip_resource_mapping =
                    inst->kind == MIR_INST_RESOURCE_OP
                    && !transpiler_mir_resource_has_mirroring_stmt_in_block(
                            block, inst);
                if (payload_expr != NULL
                    && !skip_resource_mapping
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
