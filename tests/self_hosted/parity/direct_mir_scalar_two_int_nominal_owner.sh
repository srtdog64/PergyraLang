#!/usr/bin/env bash
# A MIR-owned two-Int nominal ABI participates in the arbitrary routine GraphPlan.
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
source "$ROOT_DIR/tests/self_hosted/parity/emitted_c_runtime_header_owner.sh"
pgy_prepend_windows_runtime_paths

LABEL="self-host-direct-mir-scalar-two-int-nominal"
DRIVER="$(pgy_select_optional_exe_binary "${PGY_SELF_DRIVER_BIN:-$ROOT_DIR/bin/pgy-self-driver}")"
CC="${PGY_SELFHOST_CC:-gcc}"
CLANG="${PGY_SELFHOST_CLANG:-clang}"
WORK_REL=".tmp/self_hosted/direct_mir_scalar_two_int_nominal"
WORK_DIR="$ROOT_DIR/$WORK_REL"
SOURCE_REL="tests/self_hosted/fixtures/direct_mir_four_routine_two_int_nominal.pgy"
UNSUPPORTED_REL="tests/self_hosted/fixtures/direct_mir_five_routine_unsupported_nominal.pgy"
MIR_REL="$WORK_REL/program.mir.json"
MIR="$ROOT_DIR/$MIR_REL"
MUTATIONS="$ROOT_DIR/tests/self_hosted/parity/direct_mir_multi_routine_mutations.py"

ABI_OWNER="$ROOT_DIR/src/self_hosted/compiler/direct_mir_scalar_program_two_int_nominal_abi_fact_owner.pgy"
TARGET_OWNER="$ROOT_DIR/src/self_hosted/compiler/direct_mir_scalar_program_two_int_nominal_target_owner.pgy"
C_OWNER="$ROOT_DIR/src/self_hosted/compiler/direct_mir_scalar_program_c_two_int_nominal_owner.pgy"
LLVM_OWNER="$ROOT_DIR/src/self_hosted/compiler/direct_mir_scalar_program_llvm_two_int_nominal_owner.pgy"
ROUTE_OWNER="$ROOT_DIR/src/self_hosted/compiler/direct_mir_scalar_program_route_fact_owner.pgy"

fail() { echo "[$LABEL] $*" >&2; exit 1; }
for owner in "$ABI_OWNER" "$TARGET_OWNER" "$C_OWNER" "$LLVM_OWNER" \
    "$ROUTE_OWNER" "$MUTATIONS"; do
    [[ -f "$owner" ]] || fail "missing owner: ${owner#"$ROOT_DIR/"}"
done
pgy_require_runnable_binary_here "$LABEL" "$DRIVER" || exit 1
command -v "$CC" >/dev/null 2>&1 || fail "missing C compiler: $CC"
command -v "$CLANG" >/dev/null 2>&1 || fail "missing LLVM compiler: $CLANG"

grep -Fq 'DirectMirNominalDeclarationAbiFactFromDocument(' "$ABI_OWNER" ||
    fail "program nominal fact does not consume declaration ABI identity"
grep -Fq 'MirCapturedRequiredAbiLayoutRowAdmission(' "$ABI_OWNER" ||
    fail "program nominal fact does not cross-seal instruction ABI rows"
grep -Fq 'DirectMirTwoIntNominalPhysicalRowReady(' "$ABI_OWNER" ||
    fail "program nominal fact lost its physical-shape proof"
grep -Fq 'while row < count' "$ABI_OWNER" ||
    fail "program nominal fact does not scan the admitted declaration index"
if grep -Eq 'count[[:space:]]*==[[:space:]]*1' "$ABI_OWNER"; then
    fail "program nominal fact regressed to a one-declaration classifier"
fi
grep -Fq 'DirectMirScalarProgramTwoIntNominalTargetReadyFor(' "$TARGET_OWNER" ||
    fail "nominal target projection has no capability receipt gate"
grep -Fq 'CompilerTargetCapabilityFingerprint()' "$TARGET_OWNER" ||
    fail "nominal target projection lost its capability fingerprint"
grep -Fq 'nominal: DirectMirScalarProgramTwoIntNominalAbiFact' "$ROUTE_OWNER" ||
    fail "route does not carry the nominal fact"
for owner in "$C_OWNER" "$LLVM_OWNER"; do
    grep -Fq 'DirectMirScalarProgramTwoIntNominalTargetFromFact(' "$owner" ||
        fail "target emission bypasses the nominal projection"
done
if grep -Eq 'routine_count[[:space:]]*==[[:space:]]*4' "$ROUTE_OWNER"; then
    fail "nominal support regressed to a fixture-count route"
fi

mkdir -p "$WORK_DIR"
rm -f "$WORK_DIR"/*
(cd "$ROOT_DIR" && "$DRIVER" --emit-mir-json-verified \
    "$SOURCE_REL" -o "$MIR_REL") >"$WORK_DIR/producer.out" \
    2>"$WORK_DIR/producer.err" || {
        cat "$WORK_DIR/producer.out" "$WORK_DIR/producer.err" >&2
        fail "MIR production failed"
    }
grep -Fq '"name":"Pair"' "$MIR" || fail "producer emitted no Pair declaration"
grep -Fq '"name":"Metadata"' "$MIR" ||
    fail "producer omitted the unrelated declaration falsifier"
grep -Fq '"abi_type_name":"Pair"' "$MIR" ||
    fail "producer emitted no Pair ABI receipt"

printf '11\n' >"$WORK_DIR/expected.run"
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
        grep -Fq 'pgy_scalar_nominal_value' "$artifact" ||
            fail "C artifact omitted the nominal representation"
        grep -Fq 'pgy_scalar_nominal_value pgy_param_0' "$artifact" ||
            fail "C artifact omitted the nominal routine parameter"
        grep -Fq '_Static_assert(_Alignof(pgy_scalar_nominal_value) == 4' \
            "$artifact" || fail "C artifact omitted the alignment receipt"
        command=("$CC" -x c -std=c11 "$artifact")
        if pgy_selfhost_emitted_c_uses_runtime_headers "$artifact"; then
            command+=("-I$ROOT_DIR/src" "-I$ROOT_DIR/src/runtime" -pthread)
        fi
        command+=(-lm -o "$bin")
        "${command[@]}" >"$WORK_DIR/$backend.compile.out" \
            2>"$WORK_DIR/$backend.compile.err" || fail "C artifact did not compile"
    else
        grep -Fq '%pgy.scalar.nominal.value = type { i32, i32 }' "$artifact" ||
            fail "LLVM artifact omitted the nominal representation"
        grep -Fq '%pgy.scalar.nominal.value %pgy.param.0' "$artifact" ||
            fail "LLVM artifact omitted the nominal routine parameter"
        "$CLANG" -x ir "$artifact" -o "$bin" \
            >"$WORK_DIR/$backend.compile.out" 2>"$WORK_DIR/$backend.compile.err" ||
            fail "LLVM artifact did not compile"
    fi
    "$bin" | tr -d '\r' >"$WORK_DIR/$backend.run"
    cmp -s "$WORK_DIR/expected.run" "$WORK_DIR/$backend.run" ||
        fail "$backend runtime output drifted"
done

unsupported_mir_rel="$WORK_REL/unsupported-nominal.mir.json"
(cd "$ROOT_DIR" && "$DRIVER" --emit-mir-json-verified \
    "$UNSUPPORTED_REL" -o "$unsupported_mir_rel") \
    >"$WORK_DIR/unsupported.producer.out" \
    2>"$WORK_DIR/unsupported.producer.err" || {
        cat "$WORK_DIR/unsupported.producer.out" \
            "$WORK_DIR/unsupported.producer.err" >&2
        fail "unsupported nominal MIR production failed"
    }
for backend in c llvm; do
    output_rel="$WORK_REL/unsupported-nominal.$backend"
    output="$ROOT_DIR/$output_rel"
    rm -f "$output"
    if (cd "$ROOT_DIR" && "$DRIVER" "--mir-json-backend=$backend" \
        "$unsupported_mir_rel" -o "$output_rel") \
        >"$WORK_DIR/unsupported.$backend.out" \
        2>"$WORK_DIR/unsupported.$backend.err"; then
        fail "$backend accepted a referenced unsupported nominal"
    fi
    [[ ! -e "$output" ]] ||
        fail "$backend published a referenced unsupported nominal"
done

mutated_rel="$WORK_REL/two-int-nominal-abi-layout.mir.json"
python "$MUTATIONS" "$MIR" two-int-nominal-abi-layout \
    "$ROOT_DIR/$mutated_rel"
for backend in c llvm; do
    output_rel="$WORK_REL/two-int-nominal-abi-layout.$backend"
    output="$ROOT_DIR/$output_rel"
    rm -f "$output"
    if (cd "$ROOT_DIR" && "$DRIVER" "--mir-json-backend=$backend" \
        "$mutated_rel" -o "$output_rel") >"$WORK_DIR/mutated.$backend.out" \
        2>"$WORK_DIR/mutated.$backend.err"; then
        fail "$backend accepted a mutated nominal ABI layout"
    fi
    [[ ! -e "$output" ]] ||
        fail "$backend published an artifact for a mutated nominal ABI"
done

echo "[$LABEL] declaration isolation + C/LLVM nominal parity/negatives: PASS"
