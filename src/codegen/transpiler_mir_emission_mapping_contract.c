#include "transpiler_mir_emission_mapping_contract.h"

#include "../compiler/mir.h"
#include "../parser/ast_api.h"
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
bool
transpiler_has_mapping_for_all_emitted_blocks(const TranspilerCtx *ctx,
                                             const MIRRoutine *routine,
                                             const ASTNode *func_decl,
                                             bool require_non_cleanup,
                                             char *reason,
                                             size_t reason_cap)
{
    const char *routine_name =
        routine != NULL && routine->name != NULL ? routine->name : "<routine>";
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
                ASTNode **involves_nodes;
                size_t involve_count;
                ASTNode **values;
                size_t value_count;

                involves_nodes = ast_intent_decl_involves(func_decl,
                    &involve_count);
                for (size_t p = 0; p < involve_count; p++) {
                    ASTNode *involves = involves_nodes[p];
                    if (involves != NULL
                        && involves->type == AST_INTENT_INVOLVES
                        && ast_intent_involves_alias(involves) != NULL) {
                        transpiler_ssa_name_map_set(&ssa_map,
                            ast_intent_involves_alias(involves),
                            ast_intent_involves_alias(involves));
                    }
                }
                values = ast_intent_decl_values(func_decl, &value_count);
                for (size_t p = 0; p < value_count; p++) {
                    ASTNode *value = values[p];
                    if (value != NULL && value->type == AST_INTENT_VALUE
                        && ast_intent_value_alias(value) != NULL) {
                        transpiler_ssa_name_map_set(&ssa_map,
                            ast_intent_value_alias(value),
                            ast_intent_value_alias(value));
                    }
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
