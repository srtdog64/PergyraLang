#!/usr/bin/env bash
# Dynamic target-index and RHS uses share one value-result Array<String> fact.
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
source "$ROOT_DIR/tests/self_hosted/parity/emitted_c_runtime_header_owner.sh"
pgy_prepend_windows_runtime_paths

LABEL="self-host-direct-mir-scalar-array-string-dynamic-indexed-assignment"
DRIVER="$(pgy_select_optional_exe_binary "${PGY_SELF_DRIVER_BIN:-$ROOT_DIR/bin/pgy-self-driver}")"
CC="${PGY_SELFHOST_CC:-gcc}"
CLANG="${PGY_SELFHOST_CLANG:-clang}"
WORK_REL=".tmp/self_hosted/direct_mir_array_string_value_result_dynamic_indexed_assignment"
WORK_DIR="$ROOT_DIR/$WORK_REL"
SOURCE_REL="tests/self_hosted/fixtures/direct_mir_array_string_value_result_dynamic_indexed_assignment.pgy"
MIR_REL="$WORK_REL/program.mir.json"
MIR="$ROOT_DIR/$MIR_REL"
MUTATIONS="$ROOT_DIR/tests/self_hosted/parity/direct_mir_scalar_array_string_dynamic_indexed_assignment_mutations.py"

fail() { echo "[$LABEL] $*" >&2; exit 1; }
pgy_require_runnable_binary_here "$LABEL" "$DRIVER" || exit 1
command -v "$CC" >/dev/null 2>&1 || fail "missing C compiler: $CC"
command -v "$CLANG" >/dev/null 2>&1 || fail "missing LLVM compiler: $CLANG"
mkdir -p "$WORK_DIR"
(cd "$ROOT_DIR" && "$DRIVER" --emit-mir-json-verified \
    "$SOURCE_REL" -o "$MIR_REL") >"$WORK_DIR/producer.out" \
    2>"$WORK_DIR/producer.err" || fail "MIR production failed"
[[ "$(sha256sum "$MIR" | cut -d' ' -f1)" == \
    "b6106a4e84f1db294ead7caadcf875a3ce216140523d2a8a2a92363843eabef0" ]] ||
    fail "producer MIR identity drifted"
[[ "$(grep -o '"arg1":"inout_param"' "$MIR" | wc -l)" -eq 2 ]] ||
    fail "producer omitted the two formal indexed assignments"
grep -Fq '"binding_kind":"formal_parameter","binding_ordinal":0' "$MIR" ||
    fail "producer omitted exact target parameter identity"
printf 'left\nmiddle\nright\n' >"$WORK_DIR/expected.run"

for backend in c llvm; do
    extension="$backend"; [[ "$backend" == llvm ]] && extension="ll"
    artifact_rel="$WORK_REL/program.$extension"; artifact="$ROOT_DIR/$artifact_rel"
    bin="$WORK_DIR/program-$backend.exe"
    (cd "$ROOT_DIR" && "$DRIVER" "--mir-json-backend=$backend" \
        "$MIR_REL" -o "$artifact_rel") >"$WORK_DIR/$backend.project.out" \
        2>"$WORK_DIR/$backend.project.err" || fail "$backend projection failed"
    [[ -s "$artifact" ]] || fail "$backend projection emitted no artifact"
    if [[ "$backend" == c ]]; then
        [[ "$(grep -Fc 'pgy_as_set(&pgy_param_0' "$artifact")" -eq 2 ]] ||
            fail "C artifact omitted the two formal dynamic writes"
        command=("$CC" -x c -std=c11 "$artifact")
        if pgy_selfhost_emitted_c_uses_runtime_headers "$artifact"; then
            command+=("-I$ROOT_DIR/src" "-I$ROOT_DIR/src/runtime" -pthread)
        fi
        command+=(-lm -o "$bin")
        "${command[@]}" >"$WORK_DIR/$backend.compile.out" 2>"$WORK_DIR/$backend.compile.err" ||
            fail "C artifact did not compile"
    else
        [[ "$(grep -Fc 'call void @pgy_as_set(ptr %pgy.param.0.local' "$artifact")" -eq 2 ]] ||
            fail "LLVM artifact omitted the two formal dynamic writes"
        runtime_obj="$WORK_DIR/runtime.o"
        "$CLANG" -DPGY_LLVM_ENABLED -I"$ROOT_DIR/src" -I"$ROOT_DIR/src/runtime" \
            -c "$ROOT_DIR/src/runtime/pgy_runtime_lib.c" -o "$runtime_obj" \
            >"$WORK_DIR/runtime.compile.out" 2>"$WORK_DIR/runtime.compile.err" ||
            fail "runtime ABI object did not compile"
        "$CLANG" -x ir "$artifact" -x none "$runtime_obj" -pthread -lm -o "$bin" \
            >"$WORK_DIR/$backend.compile.out" 2>"$WORK_DIR/$backend.compile.err" ||
            fail "LLVM artifact did not compile"
    fi
    "$bin" | tr -d '\r' >"$WORK_DIR/$backend.run"
    cmp -s "$WORK_DIR/expected.run" "$WORK_DIR/$backend.run" ||
        fail "$backend runtime output drifted"
done

for mutation in target-binding-kind target-binding-ordinal target-index-edge \
    index-use-order predecessor-use result-chain carriage abi-layout value-type \
    source-tag; do
    mutated_rel="$WORK_REL/$mutation.mir.json"
    python "$MUTATIONS" "$MIR" "$mutation" "$ROOT_DIR/$mutated_rel"
    for backend in c llvm; do
        output_rel="$WORK_REL/$mutation.$backend"; rm -f "$ROOT_DIR/$output_rel"
        if (cd "$ROOT_DIR" && "$DRIVER" "--mir-json-backend=$backend" \
                "$mutated_rel" -o "$output_rel") \
                >"$WORK_DIR/$mutation.$backend.out" 2>"$WORK_DIR/$mutation.$backend.err"; then
            fail "$backend accepted $mutation"
        fi
        [[ ! -e "$ROOT_DIR/$output_rel" ]] ||
            fail "$backend published an artifact for $mutation"
    done
done

echo "[$LABEL] dynamic Array<String> indexed assignment C/LLVM parity + negatives: PASS"
