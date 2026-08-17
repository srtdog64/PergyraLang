#!/usr/bin/env bash
# Collection SSA joins reuse the target-neutral PhiValue memory carrier.
set -euo pipefail
ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
source "$ROOT_DIR/tests/self_hosted/parity/emitted_c_runtime_header_owner.sh"
pgy_prepend_windows_runtime_paths
LABEL="self-host-direct-mir-scalar-collection-phi-value"
DRIVER="$(pgy_select_optional_exe_binary "${PGY_SELF_DRIVER_BIN:-$ROOT_DIR/bin/pgy-self-driver}")"
CC="${PGY_SELFHOST_CC:-gcc}"
CLANG="${PGY_SELFHOST_CLANG:-clang}"
WORK_REL=".tmp/self_hosted/direct_mir_collection_phi_value"
WORK_DIR="$ROOT_DIR/$WORK_REL"
SOURCE_REL="tests/self_hosted/fixtures/direct_mir_collection_phi_value.pgy"
MIR_REL="$WORK_REL/program.mir.json"
MIR="$ROOT_DIR/$MIR_REL"
OWNER="$ROOT_DIR/src/self_hosted/compiler/direct_mir_scalar_cfg_string_expression_owner.pgy"
TYPED="$ROOT_DIR/src/self_hosted/compiler/direct_mir_scalar_cfg_typed_readiness_owner.pgy"
MUTATIONS="$ROOT_DIR/tests/self_hosted/parity/direct_mir_scalar_collection_phi_value_mutations.py"
fail() { echo "[$LABEL] $*" >&2; exit 1; }
pgy_require_runnable_binary_here "$LABEL" "$DRIVER" || exit 1
for command in "$CC" "$CLANG" python; do command -v "$command" >/dev/null ||
    fail "missing command: $command"; done
grep -Fq 'DirectMirScalarCfgPhiValueTypeReady(' "$OWNER" ||
    fail "phi owner has no value-carrier classification"
for type_name in ArrayInt ArrayBool ArrayString; do
    grep -Fq "CompilerAbiLayout${type_name}TypeName()" "$OWNER" ||
        fail "phi owner omits $type_name"
done
grep -Fq 'DirectMirScalarCfgPhiValueTypeReady(' "$TYPED" ||
    fail "typed readiness reclassifies PhiValue"
mkdir -p "$WORK_DIR"
rm -f "$WORK_DIR"/*
(cd "$ROOT_DIR" && "$DRIVER" --emit-mir-json-verified "$SOURCE_REL" \
    -o "$MIR_REL") >"$WORK_DIR/producer.out" 2>"$WORK_DIR/producer.err" ||
    fail "MIR production failed"
[[ "$(grep -o '"kind":"phi"' "$MIR" | wc -l)" -eq 4 ]] ||
    fail "producer omitted the local/value-result collection phis"
[[ "$(grep -o '"kind":"phi","name":"names"' "$MIR" | wc -l)" -eq 2 ]] ||
    fail "producer omitted the Array<String> phi pair"
grep -Fq '"arg1":"inout_param"' "$MIR" ||
    fail "producer omitted the value-result indexed assignment"
printf 'b\nb\n2\n' >"$WORK_DIR/expected.run"
for backend in c llvm; do
    artifact="$WORK_DIR/program.$backend"
    bin="$WORK_DIR/program-$backend.exe"
    (cd "$ROOT_DIR" && "$DRIVER" "--mir-json-backend=$backend" "$MIR_REL" \
        -o "$WORK_REL/program.$backend") >"$WORK_DIR/$backend.out" \
        2>"$WORK_DIR/$backend.err" || fail "$backend projection failed"
    if [[ "$backend" == c ]]; then
        command=("$CC" -x c -std=c11 "$artifact")
        if pgy_selfhost_emitted_c_uses_runtime_headers "$artifact"; then
            command+=("-I$ROOT_DIR/src" "-I$ROOT_DIR/src/runtime" -pthread)
        fi
        command+=(-lm -o "$bin")
        "${command[@]}" || fail "C compile failed"
    else
        runtime_obj="$WORK_DIR/runtime.o"
        "$CLANG" -DPGY_LLVM_ENABLED -I"$ROOT_DIR/src" -I"$ROOT_DIR/src/runtime" \
            -c "$ROOT_DIR/src/runtime/pgy_runtime_lib.c" -o "$runtime_obj" ||
            fail "runtime ABI compile failed"
        "$CLANG" -x ir "$artifact" -x none "$runtime_obj" -pthread -lm \
            -o "$bin" || fail "LLVM compile failed"
    fi
    "$bin" | tr -d '\r' >"$WORK_DIR/$backend.run"
    cmp -s "$WORK_DIR/expected.run" "$WORK_DIR/$backend.run" ||
        fail "$backend runtime output drifted"
done
for mutation in incoming-local result-type missing-incoming value-result-incoming; do
    mutated="$WORK_DIR/$mutation.mir.json"
    python "$MUTATIONS" "$MIR" "$mutation" "$mutated"
    for backend in c llvm; do
        output="$WORK_DIR/$mutation.$backend"
        rm -f "$output"
        if (cd "$ROOT_DIR" && "$DRIVER" "--mir-json-backend=$backend" \
            "$WORK_REL/$mutation.mir.json" -o "$WORK_REL/$mutation.$backend") \
            >"$WORK_DIR/$mutation.$backend.out" \
            2>"$WORK_DIR/$mutation.$backend.err"; then
            fail "$backend accepted $mutation"
        fi
        [[ ! -e "$output" ]] || fail "$backend published $mutation"
    done
done
echo "[$LABEL] local/value-result ArrayInt/ArrayString PhiValue C/LLVM parity and negatives: PASS"
