#!/usr/bin/env bash
# Native C/LLVM execution of the identity checks, then the installed self-host
# driver's source-to-MIR admission verdict for each constructor fixture.
set -euo pipefail
ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../../.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
pgy_prepend_windows_runtime_paths
PGY="$(pgy_select_optional_exe_binary "${PGY_BIN:-$ROOT_DIR/bin/pgy}")"
DRIVER="$(pgy_select_optional_exe_binary "${PGY_SELF_DRIVER_BIN:-$ROOT_DIR/bin/pgy-self-driver}")"
pgy_require_runnable_binary_here nominal-constructor-field-identity "$PGY"
pgy_require_runnable_binary_here nominal-constructor-field-identity "$DRIVER"
mkdir -p "$ROOT_DIR/.tmp/self_hosted"
WORK="$(mktemp -d "$ROOT_DIR/.tmp/self_hosted/nominal-constructor-field-identity.XXXXXX")"
REL="${WORK#"$ROOT_DIR/"}"
SOURCE=tests/self_hosted/fixtures/nominal_constructor_field_identity.pgy
cd "$ROOT_DIR"
sha256sum "$SOURCE" \
    src/self_hosted/semantic/nominal_constructor_argument_policy_owner.pgy \
    src/self_hosted/semantic/ast_expression_graph_nominal_constructor_argument_owner.pgy \
    src/self_hosted/semantic/ast_expression_graph_nominal_constructor_call_owner.pgy \
    src/self_hosted/semantic/ast_expression_verdict_owner.pgy >"$WORK/source.before.sha256"
sha256sum "$PGY" "$DRIVER" >"$WORK/native.before.sha256"
for ((i=0; i<32; i++)); do printf 'true\n'; done >"$WORK/expected"
for backend in c llvm; do
    timeout 120 "$PGY" --native-pipeline "$SOURCE" "--backend=$backend" \
        --opt=dev -o "$REL/$backend.exe" >"$WORK/$backend.compile" 2>&1
    timeout 30 "$WORK/$backend.exe" >"$WORK/$backend.raw" 2>"$WORK/$backend.err"
    tr -d '\r' <"$WORK/$backend.raw" >"$WORK/$backend.run"
    [[ ! -s "$WORK/$backend.err" ]]
    cmp "$WORK/expected" "$WORK/$backend.run"
    echo "[nominal-constructor-field-identity] native-$backend: 32 identity/type/shadow checks PASS"
done
for name in constructor_struct_prefix_negative \
        constructor_declared_callable_exact_positive \
        constructor_declared_callable_prefix_negative \
        constructor_nominal_missing_call_negative \
        constructor_nominal_heterogeneous_negative \
        constructor_nested_nominal_heterogeneous_negative \
        constructor_nested_nominal_missing_call_negative \
        constructor_nested_struct_prefix_negative; do
    input="tests/self_hosted/parity/fixture/$name.pgy"
    sha256sum "$input" >"$WORK/$name.sha256"
    artifact="$REL/$name.mir.json"
    set +e
    timeout 60 "$DRIVER" --emit-mir-json-verified "$input" -o "$artifact" \
        >"$WORK/$name.raw" 2>&1
    status=$?
    set -e
    code="$(tr -d '\r' <"$WORK/$name.raw" | sed -n 's/^Code: //p' | head -n 1)"
    # A verdict is an exit status, an artifact and a Code line that agree.
    if [[ "$status" == 0 && -s "$artifact" && -z "$code" ]]; then
        printf 'body_ok=true\nbody_diagnostic=\n' >"$WORK/$name.run"
    elif [[ "$status" == 1 && ! -e "$artifact" && -n "$code" ]]; then
        printf 'body_ok=false\nbody_diagnostic=%s\n' "$code" >"$WORK/$name.run"
    else
        echo "[nominal-constructor-field-identity] $name: exit $status disagrees with its artifact or Code line" >&2
        cat "$WORK/$name.raw" >&2
        exit 1
    fi
    if [[ "$name" == constructor_declared_callable_prefix_negative ||
            "$name" == constructor_struct_prefix_negative ||
            "$name" == constructor_nested_struct_prefix_negative ]]; then
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
echo '[nominal-constructor-field-identity] installed driver source admission: 8 verdict/diagnostic checks PASS (inputs not executed)'
sha256sum -c "$WORK/source.before.sha256"
sha256sum -c "$WORK/native.before.sha256"
echo "[nominal-constructor-field-identity] evidence: $REL"
