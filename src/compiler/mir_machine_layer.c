#include "mir_machine_layer.h"

#include "machine_layer_manifest.h"

#include <string.h>

bool
mir_attach_machine_layer_fact(MIRInstruction *inst, const RIROp *op)
{
    const PgyMachineLayerTargetManifest *manifest;
    const PgyMachineLayerPhysicalManifest *physical_manifest;
    const PgyMachineLayerPhysicalGrant *physical_grant;
    const PgyMachineLayerOperationManifest *operation;

    if (inst == NULL || op == NULL)
        return false;
    if (!rir_machine_contact_kind_is_present(op->machine_contact_kind))
        return true;
    inst->machine_layer_fact_required = true;
    manifest = pgy_machine_layer_target_manifest();
    physical_manifest = pgy_machine_layer_physical_manifest();
    operation = pgy_machine_layer_manifest_operation(
        manifest, op->machine_contact_kind);
    if (operation == NULL) {
        return false;
    }
    if (!pgy_machine_layer_physical_manifest_validate(physical_manifest, NULL)
        || physical_manifest->device_grant_id == NULL) {
        return false;
    }
    physical_grant = pgy_machine_layer_physical_manifest_grant(
        physical_manifest, physical_manifest->device_grant_id);
    if (physical_grant == NULL)
        return false;
    inst->machine_contact_kind = op->machine_contact_kind;
    inst->machine_layer_fact_present = true;
    inst->machine_layer_manifest_id = manifest->manifest_id;
    inst->machine_layer_physical_grant_id = physical_manifest->device_grant_id;
    inst->machine_layer_physical_base = physical_grant->base;
    inst->machine_layer_physical_size = physical_grant->size;
    inst->machine_layer_physical_mode =
        pgy_machine_layer_physical_access_mode_name(physical_grant->mode);
    inst->machine_layer_runtime_operation = operation->runtime_operation;
    inst->machine_layer_hardware_adequate = manifest->hardware_adequate;
    /* The manifest operation row owns these proof requirements. */
    inst->machine_layer_authority_required = operation->requires_authority;
    inst->machine_layer_live_lease_required = operation->requires_live_lease;
    return true;
}

bool
mir_attach_machine_layer_fact_for_ast(MIRRoutine *routine,
                                      MIRInstruction *inst,
                                      const ASTNode *ast)
{
    const RIROp *op = NULL;
    const ASTNode *lookup = ast;

    if (routine == NULL || inst == NULL || routine->rir_scope == NULL
        || ast == NULL)
        return true;
    op = rir_scope_find_op_by_ast(routine->rir_scope, lookup);
    if (op == NULL && ast->type == AST_LET_DECL)
        lookup = ast_let_initializer((ASTNode *)ast);
    if (op == NULL && lookup != NULL)
        op = rir_scope_find_op_by_ast(routine->rir_scope, lookup);
    if (op == NULL)
        return true;
    return mir_attach_machine_layer_fact(inst, op);
}

bool
mir_enrich_machine_layer_facts(MIRRoutine *routine)
{
    if (routine == NULL)
        return false;
    for (size_t b = 0; b < routine->block_count; b++) {
        MIRBasicBlock *block = &routine->blocks[b];
        for (size_t i = 0; i < block->instruction_count; i++) {
            MIRInstruction *inst = &block->instructions[i];
            if (inst->machine_layer_fact_present)
                continue;
            if (!mir_attach_machine_layer_fact_for_ast(routine, inst,
                                                       inst->ast))
                return false;
        }
    }
    return true;
}

bool
mir_machine_layer_fact_is_valid(const MIRInstruction *inst)
{
    const PgyMachineLayerTargetManifest *manifest;
    const PgyMachineLayerPhysicalManifest *physical_manifest;
    const PgyMachineLayerPhysicalGrant *physical_grant;

    if (inst == NULL)
        return false;
    if (!rir_machine_contact_kind_is_present(inst->machine_contact_kind))
        return !inst->machine_layer_fact_present
            && !inst->machine_layer_fact_required;
    manifest = pgy_machine_layer_target_manifest();
    physical_manifest = pgy_machine_layer_physical_manifest();
    physical_grant = inst->machine_layer_physical_grant_id != NULL
        ? pgy_machine_layer_physical_manifest_grant(
            physical_manifest, inst->machine_layer_physical_grant_id)
        : NULL;
    const PgyMachineLayerOperationManifest *operation =
        pgy_machine_layer_manifest_operation(manifest,
                                             inst->machine_contact_kind);
    return inst->machine_layer_fact_present
        && inst->machine_layer_manifest_id != NULL
        && strcmp(inst->machine_layer_manifest_id, manifest->manifest_id) == 0
        && pgy_machine_layer_physical_manifest_validate(
            physical_manifest, NULL)
        && inst->machine_layer_physical_grant_id != NULL
        && physical_manifest->device_grant_id != NULL
        && strcmp(inst->machine_layer_physical_grant_id,
                  physical_manifest->device_grant_id) == 0
        && physical_grant != NULL
        && inst->machine_layer_physical_base == physical_grant->base
        && inst->machine_layer_physical_size == physical_grant->size
        && inst->machine_layer_physical_mode != NULL
        && strcmp(inst->machine_layer_physical_mode,
                  pgy_machine_layer_physical_access_mode_name(
                      physical_grant->mode)) == 0
        && operation != NULL
        && inst->machine_layer_runtime_operation != NULL
        && strcmp(inst->machine_layer_runtime_operation,
                  operation->runtime_operation) == 0
        && inst->machine_layer_hardware_adequate
        && inst->machine_layer_authority_required == operation->requires_authority
        && inst->machine_layer_live_lease_required == operation->requires_live_lease
        && pgy_machine_layer_manifest_supports(manifest,
                                               inst->machine_contact_kind);
}

bool
mir_machine_layer_fact_matches_runtime_operation(
    const MIRInstruction *inst,
    const char *runtime_operation)
{
    return inst != NULL
        && runtime_operation != NULL
        && mir_machine_layer_fact_is_valid(inst)
        && inst->machine_layer_runtime_operation != NULL
        && strcmp(inst->machine_layer_runtime_operation,
                  runtime_operation) == 0;
}

const char *
mir_machine_layer_runtime_operation(const MIRInstruction *inst)
{
    return mir_machine_layer_fact_is_valid(inst)
        ? inst->machine_layer_runtime_operation
        : NULL;
}
