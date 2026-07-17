#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
PROBE="${PGY_MACHINE_LAYER_MANIFEST_PROBE:-$ROOT_DIR/bin/test_machine_layer_manifest}"

if [[ ! -x "$PROBE" && -x "${PROBE}.exe" ]]; then
    PROBE="${PROBE}.exe"
fi
if [[ ! -x "$PROBE" ]]; then
    echo "[machine-layer-manifest] missing probe: $PROBE" >&2
    exit 1
fi

output="$($PROBE)"
grep -Fq -- "C/LLVM distinct projections, physical declaration provider, runtime fingerprint/mapping bind" <<<"$output" || {
    echo "[machine-layer-manifest] probe output missing distinct projection evidence" >&2
    exit 1
}

for term in \
    '"cpu-c", "c-abi-runtime-handle"' \
    '"cpu-llvm", "llvm-ssa-address-space"' \
    'pgy_machine_layer_manifest_fingerprint'; do
    if ! grep -Fq -- "$term" \
        "$ROOT_DIR/src/compiler/machine_layer_manifest.c"; then
        echo "[machine-layer-manifest] missing manifest owner term: $term" >&2
        exit 1
    fi
done
grep -Fq -- 'pgy_machine_layer_runtime_bind_export' \
    "$ROOT_DIR/src/test_machine_layer_manifest.c" || {
    echo "[machine-layer-manifest] runtime fingerprint bind probe is missing" >&2
    exit 1
}
for term in \
    'PGY_MACHINE_LAYER_PHYSICAL_MANIFEST_ID' \
    'PGY_MACHINE_LAYER_PHYSICAL_MANIFEST_PREFIX' \
    'pgy_machine_layer_physical_manifest_fingerprint' \
    'pgy_machine_layer_physical_manifest_validate' \
    'pgy_machine_layer_physical_manifest_bind' \
    'pgy_machine_layer_physical_projection_ready_for_backend' \
    'pgy_machine_layer_physical_manifest_grant' \
    'physical declaration shape disagrees with owner' \
    'device grant is not admitted'; do
    grep -Fq -- "$term" "$ROOT_DIR/src/compiler/machine_layer_manifest.c" \
        "$ROOT_DIR/src/compiler/machine_layer_manifest.h" || {
        echo "[machine-layer-manifest] missing physical declaration owner term: $term" >&2
        exit 1
    }
done
for runtime_header in \
    "$ROOT_DIR/src/runtime/pgy_runtime_machine_layer_inline.h" \
    "$ROOT_DIR/src/runtime/pgy_runtime_lib_machine_layer_exports.h"; do
    grep -Fq -- 'pgy_machine_layer_runtime_bind_mapping_export' "$runtime_header" || {
        echo "[machine-layer-manifest] runtime mapping bind seam is missing: $runtime_header" >&2
        exit 1
    }
done
for runtime_header in \
    "$ROOT_DIR/src/runtime/pgy_runtime_machine_layer_inline.h" \
    "$ROOT_DIR/src/runtime/pgy_runtime_lib_machine_layer_exports.h"; do
    grep -Fq -- 'pgy_machine_layer_runtime_provider_bind_export' "$runtime_header" || {
        echo "[machine-layer-manifest] runtime provider seam is missing: $runtime_header" >&2
        exit 1
    }
done
for term in \
    'machine_layer_physical_grant_base' \
    'machine_layer_physical_grant_size' \
    'machine_layer_physical_grant_mode'; do
    grep -Fq -- "$term" "$ROOT_DIR/src/compiler/verified_projection_plan.h" || {
        echo "[machine-layer-manifest] physical grant shape escaped verified-plan row: $term" >&2
        exit 1
    }
done

# The verified projection planner is the sole native consumer of the abstract
# manifest validation/fingerprint. Backends consume only the plan-bound row.
grep -Fq -- 'pgy_machine_layer_physical_projection_ready_for_backend' \
    "$ROOT_DIR/src/compiler/verified_projection_plan.c" || {
    echo "[machine-layer-manifest] physical projection validation escaped verified-plan owner" >&2
    exit 1
}
grep -Fq -- 'pgy_machine_layer_manifest_fingerprint' \
    "$ROOT_DIR/src/compiler/verified_projection_plan.c" || {
    echo "[machine-layer-manifest] manifest fingerprint is not bound by verified-plan owner" >&2
    exit 1
}
grep -Fq -- 'machine_layer_physical_manifest_fingerprint' \
    "$ROOT_DIR/src/compiler/verified_projection_plan.c" || {
    echo "[machine-layer-manifest] physical declaration fingerprint is not bound by verified-plan owner" >&2
    exit 1
}
grep -Fq -- 'pgy_machine_layer_manifest_validate_site' \
    "$ROOT_DIR/src/compiler/air_validate.c" || {
    echo "[machine-layer-manifest] AIR site validation bypassed manifest owner" >&2
    exit 1
}
if grep -Fq -- 'pgy_machine_layer_manifest_operation_at' \
    "$ROOT_DIR/src/compiler/air_validate.c"; then
    echo "[machine-layer-manifest] AIR duplicated manifest operation traversal" >&2
    exit 1
fi
for term in \
    'transpiler_machine_layer_projection_is_bound' \
    'machine-layer projection is not admitted'; do
    if ! grep -Fq -- "$term" \
        "$ROOT_DIR/src/codegen/transpiler_context.c" \
        "$ROOT_DIR/src/codegen/transpiler_mir_resource_op_core.c" \
        "$ROOT_DIR/src/codegen/transpiler_slot_builtin_emit.c"; then
        echo "[machine-layer-manifest] missing C plan consumer term: $term" >&2
        exit 1
    fi
done
for term in \
    'llvm_machine_layer_projection_is_bound' \
    'machine-layer projection is not admitted'; do
    if ! grep -Fq -- "$term" \
        "$ROOT_DIR/src/codegen/llvm_api.c" \
        "$ROOT_DIR/src/codegen/llvm_expr_slot_device_calls.c" \
        "$ROOT_DIR/src/codegen/llvm_stmt_let_slots.c"; then
        echo "[machine-layer-manifest] missing LLVM plan consumer term: $term" >&2
        exit 1
    fi
done

SELFHOST_OWNER="$ROOT_DIR/src/self_hosted/compiler/machine_layer_runtime_projection_owner.pgy"
for term in \
    'CompilerRuntimeCallAbiMachineLayerManifestId' \
    'CompilerRuntimeCallAbiMachineLayerContactCount' \
    'CompilerRuntimeCallAbiMachineLayerRowAt' \
    'CompilerRuntimeCallAbiMachineLayerProjectionReady' \
    'CompilerRuntimeCallAbiMachineLayerDeviceCValueType' \
    'CompilerRuntimeCallAbiMachineLayerDeviceRuntimeCName' \
    'CompilerRuntimeCallAbiMachineLayerRemoteFutureCValueType' \
    'CompilerRuntimeCallAbiMachineLayerRemoteFutureAwaitCName' \
    'CompilerRuntimeCallAbiMachineLayerRemoteFutureAwaitHelperBlock' \
    'CompilerRuntimeCallAbiMachineLayerRuntimeCPreamble' \
    'pergyra.abstract-device-slot.v1' \
    'submit-read'; do
    grep -Fq -- "$term" "$SELFHOST_OWNER" || {
        echo "[machine-layer-manifest] self-hosted projection missing: $term" >&2
        exit 1
    }
done

for term in \
    'CompilerRuntimeCallAbiMachineLayerDeviceCValueType' \
    'RewriteSemanticMachineCall' \
    'usage.uses_machine_layer' \
    'pgy_runtime.h'; do
    grep -Fq -- "$term" \
        "$SELFHOST_OWNER" \
        "$ROOT_DIR/src/self_hosted/codegen/abi_layout/abi_layout_owner.pgy" \
        "$ROOT_DIR/src/self_hosted/codegen/emission/expr_semantic_call_emit_owner.pgy" \
        "$ROOT_DIR/src/self_hosted/codegen/emission/program_emit.pgy" || {
        echo "[machine-layer-manifest] self-hosted C machine consumer missing: $term" >&2
        exit 1
    }
done

SELFHOST_DECLARATION_CONSUMER="$ROOT_DIR/src/self_hosted/compiler/machine_layer_declaration_consumer.pgy"
SELFHOST_BINDING_OWNER="$ROOT_DIR/src/self_hosted/compiler/machine_layer_runtime_binding_owner.pgy"
for term in \
    'SelfHostMachineLayerDeclarationFromPath' \
    'SelfHostMachineLayerDeclarationReady' \
    'pgy.machine-layer.declaration.v1' \
    'physical_manifest_id' \
    'physical_board_id' \
    'physical_boot_contract' \
    'physical_linker_contract' \
    'MachineDeclarationPhysicalManifestIdReady' \
    'grant_base' \
    'grant_size' \
    'grant_mode'; do
    grep -Fq -- "$term" "$SELFHOST_DECLARATION_CONSUMER" || {
        echo "[machine-layer-manifest] self-hosted declaration consumer missing: $term" >&2
        exit 1
    }
done
for term in \
    'CompilerMachineLayerRuntimeBindingBlock' \
    'CompilerMachineLayerRuntimeBindingStatement' \
    'pgy_machine_layer_runtime_bind_mapping_export'; do
    grep -Fq -- "$term" "$SELFHOST_BINDING_OWNER" || {
        echo "[machine-layer-manifest] self-hosted startup binding owner missing: $term" >&2
        exit 1
    }
done
for term in '2147483647' '268435456' '4096' 'device-slot0' 'host-sim-device'; do
    if grep -Fq -- "$term" "$SELFHOST_OWNER"; then
        echo "[machine-layer-manifest] self-hosted abstract owner repeated physical literal: $term" >&2
        exit 1
    fi
done

SELFHOST_MIR_OWNER="$ROOT_DIR/src/self_hosted/mir_lower/machine_layer_fact_owner.pgy"
for term in \
    'MirMachineLayerFactsReady' \
    'MirMachineLayerObjectReady' \
    'machine_contact_kind' \
    'physical_grant' \
    'physical_base' \
    'physical_size' \
    'physical_mode'; do
    grep -Fq -- "$term" "$SELFHOST_MIR_OWNER" || {
        echo "[machine-layer-manifest] self-hosted MIR machine consumer missing: $term" >&2
        exit 1
    }
done
for term in \
    'CompilerRuntimeCallAbiMachineLayerManifestId' \
    'machine_layer_sites' \
    'MACHINE-AIR ERROR'; do
    grep -Fq -- "$term" \
        "$ROOT_DIR/src/self_hosted/tools/machine_layer_air_validator/main.pgy" || {
        echo "[machine-layer-manifest] self-hosted AIR machine consumer missing: $term" >&2
        exit 1
    }
done
for term in \
    'machine_contact' \
    'CompilerRuntimeCallAbiMachineLayerContactNameAt' \
    'MACHINE-RIR ERROR'; do
    grep -Fq -- "$term" \
        "$ROOT_DIR/src/self_hosted/tools/machine_layer_rir_validator/main.pgy" || {
        echo "[machine-layer-manifest] self-hosted RIR machine consumer missing: $term" >&2
        exit 1
    }
done

echo "[machine-layer-manifest] C and LLVM consume distinct physical projections with checked physical declaration and fail-closed owner validation"
