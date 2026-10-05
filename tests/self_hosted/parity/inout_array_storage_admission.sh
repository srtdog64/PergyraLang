#!/usr/bin/env bash
# Source admission only. Supplied programs are never emitted or executed.
set -euo pipefail
ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd -P)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
pgy_prepend_windows_runtime_paths
PGY="$(pgy_select_optional_exe_binary "${PGY_BIN:-$ROOT_DIR/bin/pgy}")"
pgy_require_runnable_binary_here inout-array-storage "$PGY"
mkdir -p "$ROOT_DIR/.tmp/self_hosted"
WORK="$(mktemp -d "$ROOT_DIR/.tmp/self_hosted/inout-array-storage.XXXXXX")"
REL="${WORK#"$ROOT_DIR/"}"
cd "$ROOT_DIR"
FIXTURES=tests/concept_semantics/hashmap
PROBE=tests/self_hosted/fixtures/array_storage_call_preservation_probe.pgy
fail() { echo "[inout-array-storage] $*; evidence=$REL" >&2; exit 1; }
observe_source() {
    "$observer" "$1" >"$2.raw" 2>&1 || fail "$backend observer failed: $1"
    tr -d '\r' <"$2.raw" >"$2"
}
for backend in c llvm; do
    observer="$WORK/observer-$backend.exe"
    "$PGY" --native-pipeline --opt=dev "--backend=$backend" "$PROBE" -o "$observer" \
        >"$WORK/$backend.compile.log" 2>&1 || fail "$backend observer compilation failed"
    for name in inout_write inout_mutation inout_nested inout_record ref_result_plain; do
        observe_source "$FIXTURES/public_array_drop_$name.pgy" "$WORK/$backend-$name.log"
        grep -Fxq 'body_ok=true' "$WORK/$backend-$name.log" || fail "$backend refused: $name"
        grep -Fxq 'coverage_ok=true' "$WORK/$backend-$name.log" || fail "$backend lacks coverage: $name"
    done
    for name in inout_alias_negative inout_nested_rebind_negative inout_rebind_negative inout_negative ref_result_resource_negative; do
        observe_source "$FIXTURES/public_array_drop_$name.pgy" "$WORK/$backend-$name.log"
        grep -Fxq 'body_ok=false' "$WORK/$backend-$name.log" || fail "$backend accepted: $name"
        grep -Eq '^body_diagnostic=(borrow_boundary_escape|value_param_collection_mutation)$' "$WORK/$backend-$name.log" || fail "$backend lost ownership diagnostic: $name"
    done
    observe_source tests/self_hosted/fixtures/array_storage_match_string_read_scope.pgy "$WORK/$backend-match-string.log"
    grep -Fxq 'body_ok=true' "$WORK/$backend-match-string.log" || fail "$backend refused String scope control"
    for name in inout_nested match-string; do
        grep -Fxq 'missing_tail_refused=true' "$WORK/$backend-$name.log" || fail "$backend admitted missing synthetic identity"
        grep -Fxq 'duplicate_tail_refused=true' "$WORK/$backend-$name.log" || fail "$backend admitted duplicate synthetic identity"
    done
done
echo "[inout-array-storage] PASS (C/LLVM admission observers: 6 positives, 5 ownership refusals, and 4 missing/duplicate-identity refusals each; not installed-driver proof); evidence=$REL"
