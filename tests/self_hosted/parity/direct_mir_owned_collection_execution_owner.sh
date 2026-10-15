#!/usr/bin/env bash
# Native stage-0 typed MIR -> Pergyra admission only. Never emit/run malformed MIR.
set -Eeuo pipefail
if [[ -n "${SOURCE_PROBE_DIR:-}" || -n "${PGY_COLLECTION_EFFECT_PROBE_DIR:-}" ]]; then
    echo '[direct-owned-collection] source analyzer reuse does not identify this distinct MIR admission target' >&2
    exit 2
fi
ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd -P)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
pgy_prepend_windows_runtime_paths
PGY="$(pgy_select_optional_exe_binary "${PGY_BIN:-$ROOT_DIR/bin/pgy}")"
pgy_require_runnable_binary_here direct-owned-collection "$PGY"
cd "$ROOT_DIR"
mkdir -p .tmp/self_hosted
WORK="$(mktemp -d .tmp/self_hosted/direct-owned-collection.XXXXXX)"
trap 'status=$?; echo "[direct-owned-collection] failed line=$LINENO status=$status receipt=$WORK" >&2' ERR
PROBE=tests/self_hosted/fixtures/direct_mir_owned_collection_execution_probe.pgy
MUTATOR=tests/self_hosted/parity/direct_mir_owned_collection_execution_mutations.py
REFUSAL_PROTOCOL=tests/self_hosted/parity/direct_mir_owned_collection_refusal_protocol.sh
source "$REFUSAL_PROTOCOL"
owned_collection_refusal_selftest "$WORK"
TYPE_PROBE=tests/self_hosted/fixtures/direct_mir_semantic_type_projection_probe.pgy
TYPE_MUTATOR=tests/self_hosted/parity/direct_mir_semantic_type_projection_mutations.py
FIXTURES=tests/self_hosted/parity/fixture/collection_field_lifetime
cases=(live retired clone balanced_loop fresh_loop_live branch_unborn)
modes=(flip-final delete-drop duplicate-drop missing-receipt wrong-receipt-op wrong-receipt-root wrong-receipt-source missing-declaration wrong-declaration wrong-declaration-role duplicate-definition wrong-producer wrong-operand-type prior-live-final-drop)
type_positive_modes=(compact-semantic-types spaced-semantic-types)
modes+=(malformed-type-angle malformed-type-comma unknown-source-type unknown-map-value-type coherent-wrong-key-type coherent-wrong-value-type wrong-key-argument wrong-value-argument wrong-call-arity wrong-call-identity)
sha256sum "$PGY" >"$WORK/native.sha256"
find src/self_hosted -name '*.pgy' -type f -print | LC_ALL=C sort | xargs sha256sum >"$WORK/source.sha256"
sha256sum "$PROBE" "$MUTATOR" "$REFUSAL_PROTOCOL" "$TYPE_PROBE" "$TYPE_MUTATOR" "${BASH_SOURCE[0]}" >"$WORK/inputs.sha256"
# The seventh input owns a projection-only target. Its unsupported HashMap
# routine route is not presented as a passing whole-program GraphPlan.
for case_name in "${cases[@]}" parameter_return; do
    fixture="$FIXTURES/direct_owned_collection_${case_name}_positive.pgy"
    sha256sum "$fixture" >>"$WORK/inputs.sha256"
    timeout 120 "$PGY" --native-pipeline --mir-json "$fixture" >"$WORK/$case_name.json" 2>"$WORK/$case_name.native.err"
    sha256sum "$WORK/$case_name.json" >>"$WORK/typed-inputs.sha256"
done
for backend in c llvm; do
    timeout 120 "$PGY" --native-pipeline "$TYPE_PROBE" "--backend=$backend" --opt=dev -o "$WORK/$backend-type-probe.exe" >"$WORK/$backend-type.compile" 2>&1
    sha256sum "$WORK/$backend-type-probe.exe" >>"$WORK/probes.sha256"
    timeout 30 "$WORK/$backend-type-probe.exe" "$WORK/parameter_return.json" >"$WORK/$backend-type.baseline" 2>"$WORK/$backend-type.baseline.err"
    [[ "$(tr -d '\r\n' <"$WORK/$backend-type.baseline")" == true && ! -s "$WORK/$backend-type.baseline.err" ]]
    for type_mode in unknown-return parameter-abi-mismatch signature-mismatch; do
        timeout 30 "$WORK/$backend-type-probe.exe" "$WORK/parameter_return.json" >"$WORK/$backend-$type_mode.baseline" 2>"$WORK/$backend-$type_mode.baseline.err"
        [[ "$(tr -d '\r\n' <"$WORK/$backend-$type_mode.baseline")" == true && ! -s "$WORK/$backend-$type_mode.baseline.err" ]]
        python "$TYPE_MUTATOR" "$WORK/parameter_return.json" "$type_mode" "$WORK/$type_mode.json"
        timeout 30 "$WORK/$backend-type-probe.exe" "$WORK/$type_mode.json" >"$WORK/$backend-$type_mode.run" 2>"$WORK/$backend-$type_mode.err"
        [[ "$(tr -d '\r\n' <"$WORK/$backend-$type_mode.run")" == false && ! -s "$WORK/$backend-$type_mode.err" ]]
    done
    timeout 120 "$PGY" --native-pipeline "$PROBE" "--backend=$backend" --opt=dev -o "$WORK/$backend-probe.exe" >"$WORK/$backend.compile" 2>&1
    sha256sum "$WORK/$backend-probe.exe" >>"$WORK/probes.sha256"
    for case_name in "${cases[@]}"; do
        timeout 30 "$WORK/$backend-probe.exe" "$WORK/$case_name.json" >"$WORK/$backend-$case_name.run" 2>"$WORK/$backend-$case_name.err"
        [[ "$(tr -d '\r\n' <"$WORK/$backend-$case_name.run")" == true && ! -s "$WORK/$backend-$case_name.err" ]]
    done
    for mode in "${type_positive_modes[@]}"; do
        timeout 30 "$WORK/$backend-probe.exe" "$WORK/retired.json" >"$WORK/$backend-$mode.baseline" 2>"$WORK/$backend-$mode.baseline.err"
        [[ "$(tr -d '\r\n' <"$WORK/$backend-$mode.baseline")" == true && ! -s "$WORK/$backend-$mode.baseline.err" ]]
        python "$MUTATOR" "$WORK/retired.json" "$mode" "$WORK/$mode.json"
        timeout 30 "$WORK/$backend-probe.exe" "$WORK/$mode.json" >"$WORK/$backend-$mode.run" 2>"$WORK/$backend-$mode.err"
        [[ "$(tr -d '\r\n' <"$WORK/$backend-$mode.run")" == true && ! -s "$WORK/$backend-$mode.err" ]]
    done
    for mode in "${modes[@]}"; do
        control=retired
        [[ "$mode" != prior-live-final-drop ]] || control=balanced_loop
        # A real baseline must pass immediately before each malformed check.
        timeout 30 "$WORK/$backend-probe.exe" "$WORK/$control.json" >"$WORK/$backend-$mode.baseline" 2>"$WORK/$backend-$mode.baseline.err"
        [[ "$(tr -d '\r\n' <"$WORK/$backend-$mode.baseline")" == true && ! -s "$WORK/$backend-$mode.baseline.err" ]]
        python "$MUTATOR" "$WORK/$control.json" "$mode" "$WORK/$mode.json"
        status=0
        timeout 30 "$WORK/$backend-probe.exe" "$WORK/$mode.json" >"$WORK/$backend-$mode.run" 2>"$WORK/$backend-$mode.err" || status=$?
        owned_collection_refusal_ready "$mode" "$status" "$WORK/$backend-$mode.run" "$WORK/$backend-$mode.err"
        if [[ "$mode" == flip-final ]]; then
            timeout 30 "$WORK/$backend-probe.exe" "$WORK/live.json" >"$WORK/$backend-$mode-live.baseline" 2>"$WORK/$backend-$mode-live.baseline.err"
            [[ "$(tr -d '\r\n' <"$WORK/$backend-$mode-live.baseline")" == true && ! -s "$WORK/$backend-$mode-live.baseline.err" ]]
            python "$MUTATOR" "$WORK/live.json" "$mode" "$WORK/$mode-live.json"
            status=0
            timeout 30 "$WORK/$backend-probe.exe" "$WORK/$mode-live.json" >"$WORK/$backend-$mode-live.run" 2>"$WORK/$backend-$mode-live.err" || status=$?
            owned_collection_refusal_ready flip-final-live "$status" "$WORK/$backend-$mode-live.run" "$WORK/$backend-$mode-live.err"
        fi
    done
done
for manifest in native source inputs probes typed-inputs; do sha256sum --quiet -c "$WORK/$manifest.sha256"; done
echo "[direct-owned-collection] stage-0 validation-only 6 fullgraph controls + 2 type presentations + 24 faults; separate signature/parameter projection 1 control + 3 faults per backend PASS; $WORK"
