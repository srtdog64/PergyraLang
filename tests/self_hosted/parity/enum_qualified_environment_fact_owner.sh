#!/usr/bin/env bash
# Execute the qualified enum-row owner on C and LLVM without an installed driver.
set -euo pipefail
ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd -P)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
source "$ROOT_DIR/tests/portable_process_helpers.sh"
pgy_prepend_windows_runtime_paths
LABEL=enum-qualified-environment
PGY="$(pgy_select_optional_exe_binary "${PGY_BIN:-$ROOT_DIR/bin/pgy}")"
pgy_require_runnable_binary_here "$LABEL" "$PGY"
mkdir -p "$ROOT_DIR/.tmp/self_hosted"
WORK="$(mktemp -d "$ROOT_DIR/.tmp/self_hosted/enum-qualified-environment.XXXXXX")"
REL="${WORK#"$ROOT_DIR/"}"
cd "$ROOT_DIR"
PROBE=tests/self_hosted/fixtures/enum_qualified_environment_fact_probe.pgy
sha256sum "$PGY" "$PROBE" \
    src/self_hosted/semantic/ast_enum_fact_owner.pgy \
    src/self_hosted/semantic/ast_expression_environment_owner.pgy \
    src/self_hosted/semantic/enum_callable_signature_owner.pgy >"$WORK/inputs.sha256"
for backend in c llvm; do
    pgy_run_with_timeout 240 "$WORK/$backend.compile" "$WORK/$backend.compile.err" \
        "$PGY" --native-pipeline "$PROBE" \
        "--backend=$backend" --opt=dev -o "$REL/$backend.exe"
    pgy_run_with_timeout 30 "$WORK/$backend.out" "$WORK/$backend.err" \
        "$WORK/$backend.exe"
    [[ ! -s "$WORK/$backend.err" ]]
    tr -d '\r' <"$WORK/$backend.out" >"$WORK/$backend.normalized"
    [[ "$(<"$WORK/$backend.normalized")" == 'enum-qualified-environment: PASS' ]]
done
cmp "$WORK/c.normalized" "$WORK/llvm.normalized"
sha256sum -c "$WORK/inputs.sha256" >"$WORK/bindings.check"
echo "[$LABEL] projection, eight refusals, shadowing and retained-fact lifetime C/LLVM PASS; evidence=$REL"
