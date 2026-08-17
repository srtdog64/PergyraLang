#!/usr/bin/env bash
# Option<Bool> and persisted None leaves share one admitted GraphPlan ABI receipt.
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
source "$ROOT_DIR/tests/self_hosted/parity/emitted_c_runtime_header_owner.sh"
pgy_prepend_windows_runtime_paths

LABEL="self-host-direct-mir-scalar-option-bool"
DRIVER="$(pgy_select_optional_exe_binary "${PGY_SELF_DRIVER_BIN:-$ROOT_DIR/bin/pgy-self-driver}")"
CC="${PGY_SELFHOST_CC:-gcc}"
CLANG="${PGY_SELFHOST_CLANG:-clang}"
WORK_REL=".tmp/self_hosted/direct_mir_scalar_option_bool"
WORK_DIR="$ROOT_DIR/$WORK_REL"
SOURCE_REL="tests/self_hosted/fixtures/direct_mir_four_routine_option_bool.pgy"
MIR_REL="$WORK_REL/program.mir.json"
MIR="$ROOT_DIR/$MIR_REL"
MUTATIONS="$ROOT_DIR/tests/self_hosted/parity/direct_mir_multi_routine_mutations.py"

ABI_OWNER="$ROOT_DIR/src/self_hosted/compiler/direct_mir_scalar_program_option_bool_abi_owner.pgy"
C_OWNER="$ROOT_DIR/src/self_hosted/compiler/direct_mir_scalar_program_c_option_bool_owner.pgy"
LLVM_OWNER="$ROOT_DIR/src/self_hosted/compiler/direct_mir_scalar_program_llvm_option_bool_owner.pgy"
BUILTIN_OWNER="$ROOT_DIR/src/self_hosted/compiler/direct_mir_scalar_program_option_bool_builtin_signature_owner.pgy"
ABSENCE_OWNER="$ROOT_DIR/src/self_hosted/compiler/direct_mir_scalar_program_option_absence_expression_owner.pgy"

fail() { echo "[$LABEL] $*" >&2; exit 1; }
for owner in "$ABI_OWNER" "$C_OWNER" "$LLVM_OWNER" "$BUILTIN_OWNER" \
        "$ABSENCE_OWNER" "$MUTATIONS"; do
    [[ -f "$owner" ]] || fail "missing owner: ${owner#"$ROOT_DIR/"}"
done
pgy_require_runnable_binary_here "$LABEL" "$DRIVER" || exit 1
command -v "$CC" >/dev/null 2>&1 || fail "missing C compiler: $CC"
command -v "$CLANG" >/dev/null 2>&1 || fail "missing LLVM compiler: $CLANG"

grep -Fq 'MirCapturedRequiredAbiLayoutRowAdmission(' "$ABI_OWNER" ||
    fail "Option<Bool> path does not consume the MIR ABI receipt"
grep -Fq 'DirectMirOptionBoolAbiFactReady(fact)' "$C_OWNER" ||
    fail "C target does not consume the admitted Option<Bool> fact"
grep -Fq 'DirectMirOptionBoolAbiFactReady(fact)' "$LLVM_OWNER" ||
    fail "LLVM target does not consume the admitted Option<Bool> fact"
grep -Fq 'persisted_type == "Option<Unknown>"' "$ABSENCE_OWNER" ||
    fail "persisted None leaf is not normalized by the absence owner"
for owner in "$C_OWNER" "$LLVM_OWNER"; do
    grep -Fq 'fact.some_tag' "$owner" || fail "target guessed Some tag"
    grep -Fq 'fact.none_tag' "$owner" || fail "target guessed None tag"
done

mkdir -p "$WORK_DIR"
rm -f "$WORK_DIR"/*
(cd "$ROOT_DIR" && "$DRIVER" --emit-mir-json-verified \
    "$SOURCE_REL" -o "$MIR_REL") >"$WORK_DIR/producer.out" \
    2>"$WORK_DIR/producer.err" || {
        cat "$WORK_DIR/producer.out" "$WORK_DIR/producer.err" >&2
        fail "MIR production failed"
    }
grep -Fq '"abi_type_name":"Option<Bool>"' "$MIR" ||
    fail "producer emitted no Option<Bool> ABI receipt"
grep -Fq '"expr0":"None"' "$MIR" ||
    fail "producer emitted no persisted None leaf"
grep -Fq '"abi_type_name":"Option<Unknown>"' "$MIR" ||
    fail "producer changed the generic None ABI spelling"

printf 'true\n' >"$WORK_DIR/expected.run"
for backend in c llvm; do
    artifact_rel="$WORK_REL/program.$backend"
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
        grep -Fq 'pgy_scalar_option_bool' "$artifact" ||
            fail "C artifact omitted Option<Bool> representation"
        grep -Fq '_Static_assert(sizeof(pgy_scalar_option_bool) == 8' \
            "$artifact" || fail "C artifact omitted Option<Bool> size proof"
        command=("$CC" -x c -std=c11 "$artifact")
        if pgy_selfhost_emitted_c_uses_runtime_headers "$artifact"; then
            command+=("-I$ROOT_DIR/src" "-I$ROOT_DIR/src/runtime" -pthread)
        fi
        command+=(-lm -o "$bin")
        "${command[@]}" >"$WORK_DIR/$backend.compile.out" \
            2>"$WORK_DIR/$backend.compile.err" || {
                cat "$WORK_DIR/$backend.compile.err" >&2
                fail "C artifact did not compile"
            }
    else
        grep -Fq '%pgy.scalar.option.bool = type { i32, i1 }' "$artifact" ||
            fail "LLVM artifact omitted Option<Bool> representation"
        "$CLANG" -x ir "$artifact" -o "$bin" \
            >"$WORK_DIR/$backend.compile.out" \
            2>"$WORK_DIR/$backend.compile.err" || {
                cat "$WORK_DIR/$backend.compile.err" >&2
                fail "LLVM artifact did not compile"
            }
    fi
    "$bin" | tr -d '\r' >"$WORK_DIR/$backend.run"
    cmp -s "$WORK_DIR/expected.run" "$WORK_DIR/$backend.run" ||
        fail "$backend runtime output drifted"
done

mutated_rel="$WORK_REL/option-bool-abi-layout.mir.json"
mutated="$ROOT_DIR/$mutated_rel"
python "$MUTATIONS" "$MIR" option-bool-abi-layout "$mutated"
[[ -s "$mutated" ]] || fail "could not create Option<Bool> ABI mutation"
for backend in c llvm; do
    output_rel="$WORK_REL/option-bool-abi-layout.$backend"
    output="$ROOT_DIR/$output_rel"
    rm -f "$output"
    if (cd "$ROOT_DIR" && "$DRIVER" "--mir-json-backend=$backend" \
        "$mutated_rel" -o "$output_rel") \
        >"$WORK_DIR/mutated.$backend.out" \
        2>"$WORK_DIR/mutated.$backend.err"; then
        fail "$backend accepted a mutated Option<Bool> ABI layout"
    fi
    [[ ! -e "$output" ]] ||
        fail "$backend published an artifact for mutated Option<Bool> ABI"
done

echo "[$LABEL] four-routine Option<Bool> C/LLVM parity + None leaf + ABI negative: PASS"
