#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
pgy_prepend_windows_runtime_paths

require_text() {
    local file="$1"
    local text="$2"
    grep -Fq -- "$text" "$ROOT_DIR/$file" || {
        echo "[target-capability] missing $file contract: $text" >&2
        exit 1
    }
}

reject_text() {
    local file="$1"
    local text="$2"
    if grep -Fq -- "$text" "$ROOT_DIR/$file"; then
        echo "[target-capability] forbidden local fallback remains in $file: $text" >&2
        exit 1
    fi
}

require_text "src/compiler/target_capability_contract.c" \
    "pgy.selfhost.target-capability-envelope.v1"
require_text "src/compiler/target_capability_contract.c" \
    "pgy_target_capability_fingerprint"
require_text "src/compiler/target_capability_contract.c" \
    "target capability: required fact is missing"
for fact in intent_graph effect_set authority_evidence coordination \
    slot_ownership layout_shape loss_budget materialization_reason; do
    require_text "src/compiler/target_capability_contract.c" "\"$fact\""
    require_text "src/self_hosted/compiler/target_capability_owner.pgy" "$fact"
done
for fallback in unsupported_shape forbidden_loss_budget retained_effect \
    missing_authority_evidence host_only_slot_boundary; do
    require_text "src/compiler/target_capability_contract.c" "\"$fallback\""
    require_text "src/self_hosted/compiler/target_capability_owner.pgy" "$fallback"
done
require_text "src/compiler/target_capability_contract.c" '"cpu-c", "cpu-llvm", "self-hosted"'
require_text "src/self_hosted/compiler/target_capability_owner.pgy" \
    "CompilerTargetProjectionCount"
require_text "src/codegen/transpiler_entry.c" \
    "transpiler_set_mir_inventory_missing"
require_text "src/codegen/llvm_api.c" \
    "llvm_set_mir_inventory_missing"
require_text "src/compiler/verified_projection_plan.c" \
    "pgy_target_capability_ready_for_projection"
require_text "src/compiler/verified_projection_plan.c" \
    "row_out->target_capability_fingerprint = target_capability_fingerprint"
reject_text "src/codegen/transpiler_entry.c" \
    "pgy_target_capability_ready_for_projection"
reject_text "src/codegen/llvm_api.c" \
    "pgy_target_capability_ready_for_projection"
reject_text "src/codegen/transpiler_entry.c" \
    "pgy_target_capability_fingerprint"
reject_text "src/codegen/llvm_api.c" \
    "pgy_target_capability_fingerprint"
reject_text "src/codegen/transpiler_entry.c" \
    "pgy_target_capability_envelope"
reject_text "src/codegen/llvm_api.c" \
    "pgy_target_capability_envelope"
require_text "src/self_hosted/compiler/target_capability_owner.pgy" \
    "CompilerTargetCpuCProjection"
require_text "src/self_hosted/compiler/target_capability_owner.pgy" \
    "CompilerTargetCpuLlvmProjection"
require_text "src/test_target_capability_contract.c" \
    "missing target fact did not fail closed"
reject_text "src/codegen/transpiler_entry.c" \
    'target == PGY_PROJECTION_TARGET_C ? "cpu-c"'
reject_text "src/codegen/llvm_api.c" \
    'target == PGY_PROJECTION_TARGET_LLVM ? "cpu-llvm"'

PROBE="${PGY_TARGET_CAPABILITY_PROBE:-$ROOT_DIR/bin/test_target_capability_contract}"
if [[ "$PROBE" != *.exe && -x "${PROBE}.exe" ]]; then
    PROBE="${PROBE}.exe"
fi
if [[ ! -x "$PROBE" ]]; then
    echo "[target-capability] missing probe: $PROBE" >&2
    exit 1
fi
pgy_require_runnable_binary_here "target-capability" "$PROBE"
"$PROBE"

echo "[target-capability] planner is the sole native target-envelope consumer; backends receive only the derived plan row"
