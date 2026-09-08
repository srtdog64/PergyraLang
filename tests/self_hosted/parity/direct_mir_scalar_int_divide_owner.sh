#!/usr/bin/env bash
# Int division uses the canonical checked i32 boundary on both target backends.
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
source "$ROOT_DIR/tests/self_hosted/parity/emitted_c_runtime_header_owner.sh"
pgy_prepend_windows_runtime_paths

LABEL="self-host-direct-mir-scalar-int-divide"
PGY="$(pgy_select_optional_exe_binary "${PGY_BIN:-$ROOT_DIR/bin/pgy}")"
DRIVER="$(pgy_select_optional_exe_binary "${PGY_SELF_DRIVER_BIN:-$ROOT_DIR/bin/pgy-self-driver}")"
CC="${PGY_SELFHOST_CC:-gcc}"
CLANG="${PGY_SELFHOST_CLANG:-clang}"
WORK_BASE="$ROOT_DIR/.tmp/self_hosted/direct_mir_scalar_int_divide"
mkdir -p "$WORK_BASE"
WORK_DIR="$(mktemp -d "$WORK_BASE/run.XXXXXX")"
WORK_REL="${WORK_DIR#"$ROOT_DIR"/}"
SOURCE_REL="tests/self_hosted/fixtures/direct_mir_scalar_int_divide.pgy"
MIR_REL="$WORK_REL/program.mir.json"
MIR="$ROOT_DIR/$MIR_REL"
MUTATIONS="$ROOT_DIR/tests/self_hosted/parity/direct_mir_scalar_int_divide_mutations.py"
KIND_ID="$ROOT_DIR/src/self_hosted/compiler/direct_mir_scalar_program_case_math_expression_kind_owner.pgy"
KIND_OWNER="$ROOT_DIR/src/self_hosted/compiler/direct_mir_scalar_program_expression_kind_owner.pgy"
READY_OWNER="$ROOT_DIR/src/self_hosted/compiler/direct_mir_scalar_program_checked_int_owner.pgy"
C_OWNER="$ROOT_DIR/src/self_hosted/compiler/direct_mir_scalar_program_c_case_math_expression_owner.pgy"
LLVM_OWNER="$ROOT_DIR/src/self_hosted/compiler/direct_mir_scalar_program_llvm_case_math_expression_owner.pgy"

fail() { echo "[$LABEL] $*" >&2; exit 1; }
pgy_require_runnable_binary_here "$LABEL" "$DRIVER" || exit 1
pgy_require_runnable_binary_here "$LABEL" "$PGY" || exit 1
command -v "$CC" >/dev/null 2>&1 || fail "missing C compiler: $CC"
command -v "$CLANG" >/dev/null 2>&1 || fail "missing LLVM compiler: $CLANG"
grep -Fq 'DirectMirScalarProgramExprDivideInt() -> Int { return 74; }' "$KIND_ID" ||
    fail "divide expression identity drifted"
grep -Fq 'source_kind == AstExpressionNodeDivide()' "$KIND_OWNER" ||
    fail "divide source/type join is missing"
grep -Fq 'kind == DirectMirScalarProgramExprDivideInt()' "$READY_OWNER" ||
    fail "divide readiness is missing"
grep -Fq 'let callee: String = abi.symbol;' "$C_OWNER" || fail "C checked runtime call is missing"
grep -Fq '".lhs = trunc i64 "' "$LLVM_OWNER" || fail "LLVM lost the i32 operand boundary"
if grep -Fq '" = sdiv i64 "' "$ROOT_DIR/src/self_hosted/compiler/direct_mir_scalar_program_llvm_expression_owner.pgy"; then
    fail "raw Int sdiv returned"
fi

(cd "$ROOT_DIR" && "$DRIVER" --emit-mir-json-verified \
    "$SOURCE_REL" -o "$MIR_REL") >"$WORK_DIR/producer.out" \
    2>"$WORK_DIR/producer.err" || fail "MIR production failed"
grep -Fq '"kind":"divide","text":"value / 4"' "$MIR" ||
    fail "producer omitted the divide identity"
printf '3\n' >"$WORK_DIR/expected.run"

for backend in c llvm; do
    extension="$backend"
    [[ "$backend" == llvm ]] && extension="ll"
    artifact_rel="$WORK_REL/program.$extension"
    artifact="$ROOT_DIR/$artifact_rel"
    bin="$WORK_DIR/program-$backend.exe"
    (cd "$ROOT_DIR" && "$DRIVER" "--mir-json-backend=$backend" \
        "$MIR_REL" -o "$artifact_rel") >"$WORK_DIR/$backend.project.out" \
        2>"$WORK_DIR/$backend.project.err" || {
            cat "$WORK_DIR/$backend.project.out" \
                "$WORK_DIR/$backend.project.err" >&2
            fail "$backend projection failed"
        }
    [[ -s "$artifact" ]] || fail "$backend projection emitted no artifact"
    if [[ "$backend" == c ]]; then
        grep -Fq 'pgy_checked_div_i32_export(' "$artifact" || fail "C omitted checked Int division"
        command=("$CC" -x c -std=c11 "$artifact")
        if pgy_selfhost_emitted_c_uses_runtime_headers "$artifact"; then
            command+=("-I$ROOT_DIR/src" "-I$ROOT_DIR/src/runtime" -pthread)
        fi
        command+=(-lm -o "$bin")
        "${command[@]}" >"$WORK_DIR/c.compile.out" \
            2>"$WORK_DIR/c.compile.err" || fail "C artifact did not compile"
    else
        grep -Fq 'call i32 @pgy_checked_div_i32_export(' "$artifact" || fail "LLVM omitted checked Int division"
        "$CC" -std=c11 -O0 -DPGY_LLVM_ENABLED -I"$ROOT_DIR/src" -I"$ROOT_DIR/src/runtime" -pthread \
            -c "$ROOT_DIR/src/runtime/pgy_runtime_lib.c" -o "$WORK_DIR/runtime.o" \
            >"$WORK_DIR/runtime.compile.out" 2>"$WORK_DIR/runtime.compile.err" || fail "runtime compilation failed"
        "$CLANG" -x ir "$artifact" -x none "$WORK_DIR/runtime.o" -pthread -lm -o "$bin" >"$WORK_DIR/llvm.compile.out" \
            2>"$WORK_DIR/llvm.compile.err" || fail "LLVM artifact did not compile"
    fi
    "$bin" | tr -d '\r' >"$WORK_DIR/$backend.run"
    cmp -s "$WORK_DIR/expected.run" "$WORK_DIR/$backend.run" ||
        fail "$backend runtime output drifted"
done

for mutation in missing-right-edge wrong-right-type; do
    mutated_rel="$WORK_REL/$mutation.mir.json"
    python "$MUTATIONS" "$MIR" "$mutation" "$ROOT_DIR/$mutated_rel"
    for backend in c llvm; do
        output_rel="$WORK_REL/$mutation.$backend"
        if (cd "$ROOT_DIR" && "$DRIVER" "--mir-json-backend=$backend" \
            "$mutated_rel" -o "$output_rel") >"$WORK_DIR/$mutation.$backend.out" \
            2>"$WORK_DIR/$mutation.$backend.err"; then
            fail "$backend accepted $mutation"
        fi
        [[ ! -e "$ROOT_DIR/$output_rel" ]] ||
            fail "$backend published an artifact for $mutation"
    done
done

(cd "$ROOT_DIR" && "$PGY" --native-pipeline --backend=c --opt=dev \
    tests/self_hosted/parity/fixture/checked_int_division_contract.pgy -o "$WORK_REL/contract.exe") \
    >"$WORK_DIR/contract.compile.out" 2>"$WORK_DIR/contract.compile.err" || fail "normalized contract did not compile"
"$WORK_DIR/contract.exe" >"$WORK_DIR/contract.out" 2>"$WORK_DIR/contract.err" || fail "normalized contract failed"
grep -Fq 'checked division ABI and five crossed-fact refusals: PASS' "$WORK_DIR/contract.out" || fail "normalized contract omitted its result"
echo "[$LABEL] checked Int divide C/LLVM parity and negatives: PASS ($WORK_DIR)"
