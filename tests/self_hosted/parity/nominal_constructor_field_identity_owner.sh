#!/usr/bin/env bash
# Native-built current Pergyra admission owners, not an installed replacement.
set -euo pipefail
ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
pgy_prepend_windows_runtime_paths
PGY="$(pgy_select_optional_exe_binary "${PGY_BIN:-$ROOT_DIR/bin/pgy}")"
pgy_require_runnable_binary_here nominal-constructor-field-identity "$PGY"
mkdir -p "$ROOT_DIR/.tmp/self_hosted"
WORK="$(mktemp -d "$ROOT_DIR/.tmp/self_hosted/nominal-constructor-field-identity.XXXXXX")"
REL="${WORK#"$ROOT_DIR/"}"
SOURCE=tests/self_hosted/fixtures/nominal_constructor_field_identity.pgy
SOURCE_PROBE=tests/self_hosted/fixtures/nominal_constructor_source_arity_probe.pgy
cd "$ROOT_DIR"
sha256sum "$SOURCE" "$SOURCE_PROBE" \
    src/self_hosted/semantic/nominal_constructor_argument_policy_owner.pgy \
    src/self_hosted/semantic/ast_expression_graph_nominal_constructor_argument_owner.pgy \
    src/self_hosted/semantic/ast_expression_graph_nominal_constructor_call_owner.pgy \
    src/self_hosted/semantic/ast_expression_verdict_owner.pgy >"$WORK/source.before.sha256"
sha256sum "$PGY" >"$WORK/native.before.sha256"
for ((i=0; i<31; i++)); do printf 'true\n'; done >"$WORK/expected"
for backend in c llvm; do
    timeout 120 "$PGY" --native-pipeline "$SOURCE" "--backend=$backend" \
        --opt=dev -o "$REL/$backend.exe" >"$WORK/$backend.compile" 2>&1
    timeout 30 "$WORK/$backend.exe" >"$WORK/$backend.raw" 2>"$WORK/$backend.err"
    tr -d '\r' <"$WORK/$backend.raw" >"$WORK/$backend.run"
    [[ ! -s "$WORK/$backend.err" ]]
    cmp "$WORK/expected" "$WORK/$backend.run"
    echo "[nominal-constructor-field-identity] native-$backend: 31 identity/type/shadow checks PASS"
done
timeout 120 "$PGY" --native-pipeline "$SOURCE_PROBE" --backend=c --opt=dev \
    -o "$REL/source-probe.exe" >"$WORK/source-probe.compile" 2>&1
for name in constructor_nominal_prefix_positive \
        constructor_declared_callable_exact_positive \
        constructor_declared_callable_prefix_negative \
        constructor_nominal_missing_call_negative \
        constructor_nominal_heterogeneous_negative \
        constructor_nested_nominal_heterogeneous_negative \
        constructor_nested_nominal_missing_call_negative \
        constructor_nested_nominal_prefix_positive; do
    input="tests/self_hosted/parity/fixture/$name.pgy"
    sha256sum "$input" >"$WORK/$name.sha256"
    timeout 30 "$WORK/source-probe.exe" "$input" >"$WORK/$name.raw" 2>"$WORK/$name.err"
    tr -d '\r' <"$WORK/$name.raw" >"$WORK/$name.run"
    [[ ! -s "$WORK/$name.err" ]]
    if [[ "$name" == constructor_declared_callable_prefix_negative ]]; then
        printf 'body_ok=false\nbody_diagnostic=call_arity_mismatch\n' >"$WORK/$name.expected"
    elif [[ "$name" == constructor_nominal_missing_call_negative ||
            "$name" == constructor_nested_nominal_missing_call_negative ]]; then
        printf 'body_ok=false\nbody_diagnostic=undefined_function\n' >"$WORK/$name.expected"
    elif [[ "$name" == constructor_nominal_heterogeneous_negative ||
            "$name" == constructor_nested_nominal_heterogeneous_negative ]]; then
        printf 'body_ok=false\nbody_diagnostic=ast_artifact_invalid\n' >"$WORK/$name.expected"
    else
        printf 'body_ok=true\nbody_diagnostic=\n' >"$WORK/$name.expected"
    fi
    cmp "$WORK/$name.expected" "$WORK/$name.run"
    sha256sum -c "$WORK/$name.sha256"
done
echo '[nominal-constructor-field-identity] actual source initializer: 8 admission/diagnostic checks PASS (inputs not emitted/executed)'
sha256sum -c "$WORK/source.before.sha256"
sha256sum -c "$WORK/native.before.sha256"
echo "[nominal-constructor-field-identity] evidence: $REL"
