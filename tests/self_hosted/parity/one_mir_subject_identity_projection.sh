#!/usr/bin/env bash
# One source-produced subject MIR drives one stable identity in C and LLVM.

set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
pgy_prepend_windows_runtime_paths

LABEL="self-host-one-mir-subject-identity"
DRIVER_BIN="$(pgy_select_optional_exe_binary "${PGY_SELFHOST_SUBJECT_IDENTITY_DRIVER_BIN:-$ROOT_DIR/bin/pgy-self-driver}")"
PGY="$(pgy_select_optional_exe_binary "${PGY_BIN:-$ROOT_DIR/bin/pgy}")"
WORK_DIR="${PGY_SELFHOST_SUBJECT_IDENTITY_BUILD_DIR:-$ROOT_DIR/.tmp/self_hosted/driver/one_mir_subject_identity}"
SOURCE="$ROOT_DIR/src/self_hosted/mir_lower/fixture/nominal_subject.pgy"
MIR="$WORK_DIR/subject.one.mir.json"
NATIVE_MIR="$WORK_DIR/subject.native.mir.json"
MUTATIONS="$ROOT_DIR/tests/self_hosted/parity/one_mir_passive_nominal_literal_mutations.py"
PYTHON_BIN="${PYTHON_BIN:-$(command -v python3 || command -v python || true)}"
CC="${PGY_SELFHOST_CC:-gcc}"
CLANG="${PGY_SELFHOST_CLANG:-clang}"

fail() { echo "[$LABEL] $*" >&2; exit 1; }
root_relative() { case "$1" in "$ROOT_DIR"/*) printf '%s\n' "${1#"$ROOT_DIR"/}";; *) printf '%s\n' "$1";; esac; }
hash_file() { sha256sum "$1" | cut -d' ' -f1; }
require_file() { [[ -f "$1" ]] || fail "missing $1"; }

assert_owner_ratchet() {
    local total=0 owner cap lines
    while IFS='|' read -r owner cap; do
        lines="$(wc -l < "$owner" | tr -d ' ')"
        [[ "$lines" -le "$cap" ]] || fail "owner hard cap exceeded: ${owner#"$ROOT_DIR/"}=$lines/$cap"
        total=$((total + lines))
    done <<EOF
$ROOT_DIR/src/self_hosted/compiler/direct_mir_subject_identity_plan_owner.pgy|180
$ROOT_DIR/src/self_hosted/compiler/direct_mir_subject_identity_target_projection_owner.pgy|100
$ROOT_DIR/src/self_hosted/compiler/direct_mir_subject_identity_c_emission_owner.pgy|90
$ROOT_DIR/src/self_hosted/compiler/direct_mir_subject_identity_llvm_emission_owner.pgy|90
EOF
    local subject_inventory=("$ROOT_DIR"/src/self_hosted/compiler/direct_mir_subject_identity_*.pgy)
    [[ "${#subject_inventory[@]}" -eq 4 ]] || fail "subject identity owner inventory drifted"
    [[ "$total" -le 384 ]] || fail "subject identity owner cap exceeded: $total/384"
    [[ "$(grep -R -F 'DirectMirNominalLiteralRouteFactFromAdmitted(' "$ROOT_DIR/src/self_hosted/compiler" | wc -l | tr -d ' ')" -eq 2 ]] || fail "nominal route escaped one definition and one call"
    [[ "$(grep -R -F 'DirectMirNominalLiteralProgramAdmissionFromAdmitted(' "$ROOT_DIR/src/self_hosted/compiler" | wc -l | tr -d ' ')" -eq 2 ]] || fail "nominal admission escaped one definition and one call"
    [[ "$(grep -Fc 'DirectMirNominalLiteralProgramAdmissionFromAdmitted(admitted, route)' "$ROOT_DIR/src/self_hosted/compiler/direct_mir_nominal_literal_projection_owner.pgy")" -eq 1 ]] || fail "common nominal admission is not single-shot"
    [[ "$(grep -Ec 'SubjectIdentityPlanFromAdmission\(' "$ROOT_DIR/src/self_hosted/compiler/direct_mir_nominal_literal_projection_owner.pgy")" -eq 1 ]] || fail "subject planner was retried"
    ! grep -R -Eq 'SubjectIdentityProgramCandidate|SubjectIdentity.*FromAdmitted' "$ROOT_DIR/src/self_hosted/compiler" || fail "subject family re-read admitted MIR"
    ! grep -R -Fq 'direct_mir_passive_nominal_literal_plan_owner.pgy' "$ROOT_DIR/src/self_hosted/compiler/direct_mir_subject_identity"* || fail "subject imported the passive value plan"
    grep -Fq 'storage_count == 1' "$ROOT_DIR/src/self_hosted/compiler/direct_mir_subject_identity_plan_owner.pgy" || fail "subject plan lost exact-one storage"
    grep -Fq 'stable_pointer_count == 1' "$ROOT_DIR/src/self_hosted/compiler/direct_mir_subject_identity_plan_owner.pgy" || fail "subject plan lost exact-one pointer"
    grep -Fq 'result_capture_digest' "$ROOT_DIR/src/self_hosted/compiler/direct_mir_subject_identity_plan_owner.pgy" || fail "subject allocation identity is not definition-sealed"
    ! grep -Fq 'aggregate_value' "$ROOT_DIR/src/self_hosted/compiler/direct_mir_subject_identity_plan_owner.pgy" || fail "subject plan widened to passive value carriage"
    ! grep -Eq 'alloca|getelementptr|insertvalue|extractvalue|\*const|_pgy_subject' "$ROOT_DIR/src/self_hosted/compiler/direct_mir_subject_identity_target_projection_owner.pgy" || fail "subject target projection owns emitter syntax"
    local emitter
    for emitter in direct_mir_subject_identity_c_emission_owner.pgy direct_mir_subject_identity_llvm_emission_owner.pgy; do
        ! grep -Eq 'MirMachineLayerAdmittedJsonInput|MirExpressionGraphSequence|JsonObjectFact' "$ROOT_DIR/src/self_hosted/compiler/$emitter" || fail "$emitter reopened MIR"
        ! grep -Eq '"(Hero|subject)"|nominal_subject|fixture' "$ROOT_DIR/src/self_hosted/compiler/$emitter" || fail "$emitter dispatched on a subject spelling"
    done
}

project() {
    local input="$1" target="$2" output="$3"
    rm -f "$output" "$output.stdout" "$output.stderr"
    (cd "$ROOT_DIR" && "$DRIVER_BIN" "--mir-json-backend=$target" "$(root_relative "$input")" "$(root_relative "$output")" >"$output.stdout" 2>"$output.stderr") || {
        cat "$output.stdout" "$output.stderr" >&2 || true
        fail "$target rejected subject identity MIR"
    }
    [[ -s "$output" ]] || fail "$target emitted no subject identity artifact"
}

reject_mutation() {
    local name="$1" target="${2:-c}" output
    output="$WORK_DIR/$name.$target.artifact"
    rm -f "$output" "$output.stdout" "$output.stderr"
    if (cd "$ROOT_DIR" && "$DRIVER_BIN" "--mir-json-backend=$target" "$(root_relative "$WORK_DIR/$name.json")" "$(root_relative "$output")" >"$output.stdout" 2>"$output.stderr"); then
        fail "$target accepted subject identity mutation: $name"
    fi
    [[ ! -e "$output" ]] || fail "$target emitted before rejecting $name"
    grep -Eq 'nominal literal|subject identity' "$output.stdout" "$output.stderr" || fail "$target rejection lost subject identity owner: $name"
    ! grep -Eq 'passive nominal|unsupported scalar facts' "$output.stdout" "$output.stderr" || fail "$target retried passive/scalar after subject admission: $name"
}

compile_and_expect() {
    local stem="$1" expected="$2"
    "$CC" -std=c11 -Wall -Wextra -Werror "$WORK_DIR/$stem.c" -o "$WORK_DIR/$stem-c.exe"
    "$CLANG" "$WORK_DIR/$stem.ll" -o "$WORK_DIR/$stem-llvm.exe" 2>"$WORK_DIR/$stem.llvm.log"
    printf '%s\n' "$expected" >"$WORK_DIR/$stem.expected"
    "$WORK_DIR/$stem-c.exe" | tr -d '\r' >"$WORK_DIR/$stem-c.out"
    "$WORK_DIR/$stem-llvm.exe" | tr -d '\r' >"$WORK_DIR/$stem-llvm.out"
    cmp -s "$WORK_DIR/$stem.expected" "$WORK_DIR/$stem-c.out" || fail "C output is not exact $expected"
    cmp -s "$WORK_DIR/$stem.expected" "$WORK_DIR/$stem-llvm.out" || fail "LLVM output is not exact $expected"
}

for path in "$SOURCE" "$DRIVER_BIN" "$PGY" "$MUTATIONS"; do require_file "$path"; done
[[ -n "$PYTHON_BIN" ]] || fail "python is required"
command -v "$CC" >/dev/null || fail "missing C compiler"
command -v "$CLANG" >/dev/null || fail "missing LLVM compiler"
assert_owner_ratchet
mkdir -p "$WORK_DIR"
rm -f "$MIR" "$NATIVE_MIR"
(cd "$ROOT_DIR" && "$DRIVER_BIN" --emit-mir-json-verified "$(root_relative "$SOURCE")" "$(root_relative "$MIR")") || fail "source-to-MIR rejected subject fixture"
mir_digest="$(hash_file "$MIR")"
(cd "$ROOT_DIR" && "$PGY" --mir-json "$(pgy_path_for_compiler "$PGY" "$SOURCE")" >"$NATIVE_MIR") || fail "native MIR oracle rejected subject fixture"
"$PYTHON_BIN" "$MUTATIONS" "$MIR" "$NATIVE_MIR" compare || fail "native/self subject semantics drifted"
"$PYTHON_BIN" "$MUTATIONS" "$MIR" "$WORK_DIR"

project "$MIR" c "$WORK_DIR/baseline.c"
project "$MIR" llvm "$WORK_DIR/baseline.ll"
[[ "$(hash_file "$MIR")" == "$mir_digest" ]] || fail "projection mutated MIR"
grep -Fq 'Hero _pgy_subject_storage_0;' "$WORK_DIR/baseline.c" || fail "C lost one subject storage"
grep -Fq 'Hero *const _pgy_subject_0 = &_pgy_subject_storage_0;' "$WORK_DIR/baseline.c" || fail "C lost the stable subject pointer"
grep -Fq '_pgy_subject_0->hp = 7;' "$WORK_DIR/baseline.c" || fail "C initialization bypassed subject identity"
grep -Fq 'int32_t _pgy_member_0 = _pgy_subject_0->hp;' "$WORK_DIR/baseline.c" || fail "C read bypassed subject identity"
! grep -Eq '_pgy_subject_storage_0\.|_pgy_nominal_0|\{ \.hp = 7 \}' "$WORK_DIR/baseline.c" || fail "C copied subject as an aggregate value"
for sentinel in '%pgy.subject.0 = alloca %Hero' '%pgy.subject.field.0 = getelementptr inbounds %Hero, ptr %pgy.subject.0, i32 0, i32 0' 'store i32 7, ptr %pgy.subject.field.0' '%pgy.member.0 = load i32, ptr %pgy.subject.field.0' 'sext i32 %pgy.member.0 to i64'; do grep -Fq "$sentinel" "$WORK_DIR/baseline.ll" || fail "LLVM lost stable identity sentinel: $sentinel"; done
for opcode in alloca getelementptr store load; do [[ "$(grep -Ec "^[[:space:]]+([^=]+ = )?$opcode " "$WORK_DIR/baseline.ll")" -eq 1 ]] || fail "LLVM $opcode count is not exactly one"; done
! grep -Eq 'insertvalue|extractvalue' "$WORK_DIR/baseline.ll" || fail "LLVM copied subject through aggregate SSA"
compile_and_expect baseline 7

for variant in semantic-rename literal-seventy-three; do project "$WORK_DIR/$variant.json" c "$WORK_DIR/$variant.c"; project "$WORK_DIR/$variant.json" llvm "$WORK_DIR/$variant.ll"; done
compile_and_expect semantic-rename 7
compile_and_expect literal-seventy-three 73
project "$WORK_DIR/host-tobject.json" c "$WORK_DIR/host-tobject.c"
project "$WORK_DIR/host-tobject.json" llvm "$WORK_DIR/host-tobject.ll"
grep -Fq 'Hero _pgy_nominal_0 = { .hp = 7 };' "$WORK_DIR/host-tobject.c" || fail "value host did not select passive C representation"
grep -Fq '%pgy.nominal.0 = insertvalue %Hero poison, i32 7, 0' "$WORK_DIR/host-tobject.ll" || fail "value host did not select passive LLVM representation"
compile_and_expect host-tobject 7

for mutation in kind-drift nominal-kind-drift host-vessel declaration-name-drift declaration-id-zero field-name-drift field-type-drift field-kind-drift field-id-collision method-tail entrypoint-drift return-drift source-local-name-drift source-local-type-drift constructor-type-drift constructor-field-drift constructor-edge-drift constructor-noncanonical-int definition-result-drift definition-local-drift definition-arg-type-drift definition-expr-type-drift definition-abi-type-drift definition-abi-forged instruction-tail duplicate-identity-definition second-source-local missing-use duplicate-use stale-use member-receiver-drift member-name-drift member-edge-drift unreachable; do reject_mutation "$mutation"; done
for mutation in kind-drift nominal-kind-drift host-vessel field-type-drift source-local-name-drift constructor-edge-drift definition-expr-type-drift definition-abi-type-drift definition-abi-forged instruction-tail duplicate-identity-definition duplicate-use stale-use member-name-drift; do reject_mutation "$mutation" llvm; done

echo "[$LABEL] PASS: subject exact 7, stable C/LLVM identity, 3 value variants, 34 C negatives, 14 LLVM sentinels (sha256=$mir_digest)"
