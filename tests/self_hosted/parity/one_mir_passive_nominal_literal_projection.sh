#!/usr/bin/env bash
# One source-produced passive nominal MIR drives real C/LLVM construction/read.

set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
pgy_prepend_windows_runtime_paths

LABEL="self-host-one-mir-passive-nominal-literal"
DRIVER_BIN="$(pgy_select_optional_exe_binary "${PGY_SELFHOST_PASSIVE_NOMINAL_DRIVER_BIN:-$ROOT_DIR/bin/pgy-self-driver}")"
PGY="$(pgy_select_optional_exe_binary "${PGY_BIN:-$ROOT_DIR/bin/pgy}")"
WORK_DIR="${PGY_SELFHOST_PASSIVE_NOMINAL_BUILD_DIR:-$ROOT_DIR/.tmp/self_hosted/driver/one_mir_passive_nominal_literal}"
SOURCE="$ROOT_DIR/src/self_hosted/mir_lower/fixture/nominal_tobject.pgy"
MIR="$WORK_DIR/tobject.one.mir.json"
NATIVE_MIR="$WORK_DIR/tobject.native.mir.json"
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
$ROOT_DIR/src/self_hosted/compiler/direct_mir_passive_nominal_declaration_fact_owner.pgy|180
$ROOT_DIR/src/self_hosted/compiler/direct_mir_passive_nominal_literal_graph_fact_owner.pgy|150
$ROOT_DIR/src/self_hosted/compiler/direct_mir_passive_nominal_literal_program_identity_owner.pgy|200
$ROOT_DIR/src/self_hosted/compiler/direct_mir_passive_nominal_literal_route_fact_owner.pgy|90
$ROOT_DIR/src/self_hosted/compiler/direct_mir_passive_nominal_literal_instruction_envelope_owner.pgy|100
$ROOT_DIR/src/self_hosted/compiler/direct_mir_passive_nominal_literal_abi_absence_owner.pgy|120
$ROOT_DIR/src/self_hosted/compiler/direct_mir_passive_nominal_literal_program_admission_owner.pgy|220
$ROOT_DIR/src/self_hosted/compiler/direct_mir_passive_nominal_literal_plan_owner.pgy|140
$ROOT_DIR/src/self_hosted/compiler/direct_mir_passive_nominal_literal_target_projection_owner.pgy|60
$ROOT_DIR/src/self_hosted/compiler/direct_mir_passive_nominal_literal_c_emission_owner.pgy|90
$ROOT_DIR/src/self_hosted/compiler/direct_mir_passive_nominal_literal_llvm_emission_owner.pgy|90
$ROOT_DIR/src/self_hosted/compiler/direct_mir_passive_nominal_literal_projection_owner.pgy|50
EOF
    [[ "$total" -le 1024 ]] || fail "passive nominal owner family cap exceeded: $total/1024"
    [[ "$(wc -l < "$ROOT_DIR/src/self_hosted/compiler/direct_mir_exact_json_array_cardinality_owner.pgy" | tr -d ' ')" -le 90 ]] || fail "exact JSON-array cardinality owner hard cap exceeded"
    [[ ! -e "$ROOT_DIR/src/self_hosted/compiler/direct_mir_inferred_generic_member_array_shape_owner.pgy" ]] || fail "retired inferred-family array-shape owner reappeared"
    ! grep -R -Eq 'DirectMirInferredGenericMemberExact(Object|String)ArrayCount' "$ROOT_DIR/src/self_hosted/compiler" || fail "exact JSON-array cardinality retained a family-local owner"
    [[ "$(grep -Fc 'DirectMirPassiveNominalLiteralRouteFactFromAdmitted(admitted)' "$ROOT_DIR/src/self_hosted/compiler/direct_mir_backend_projection_owner.pgy")" -eq 1 ]] || fail "passive nominal classification is not single-shot"
    ! grep -R -Fq 'DirectMirPassiveNominalLiteralProgramCandidate' "$ROOT_DIR/src/self_hosted/compiler" || fail "passive nominal route was re-evaluated in admission"
    grep -Fq 'CompileAdmittedDirectMirPassiveNominalLiteral(' "$ROOT_DIR/src/self_hosted/compiler/direct_mir_backend_projection_owner.pgy" || fail "passive nominal projection is not routed"
    [[ "$(grep -Ec 'PlanFromAdmitted\(' "$ROOT_DIR/src/self_hosted/compiler/direct_mir_passive_nominal_literal_projection_owner.pgy")" -eq 1 ]] || fail "passive nominal projection retried a planner"
    grep -Fq 'typed_nominal_physical_abi_absent' "$ROOT_DIR/src/self_hosted/compiler/direct_mir_passive_nominal_literal_plan_owner.pgy" || fail "typed ABI absence is not explicit"
    grep -Fq 'typed_nominal_abi_absence.digest' "$ROOT_DIR/src/self_hosted/compiler/direct_mir_passive_nominal_literal_plan_owner.pgy" || fail "plan is not sealed to captured ABI absence"
    grep -Fq 'DirectMirExactObjectArrayCount(methods, 0)' "$ROOT_DIR/src/self_hosted/compiler/direct_mir_passive_nominal_declaration_fact_owner.pgy" || fail "non-object declaration tail is not rejected"
    ! grep -Eq 'construction_shape|field_read_shape' "$ROOT_DIR/src/self_hosted/compiler/direct_mir_passive_nominal_literal_target_projection_owner.pgy" || fail "target projection duplicated emitter syntax authority"
    ! grep -Fq 'direct_mir_nominal_declaration_abi_fact_owner.pgy' "$ROOT_DIR/src/self_hosted/compiler/direct_mir_passive_nominal_literal"* || fail "passive nominal plan imported physical struct ABI"
    local emitter
    for emitter in direct_mir_passive_nominal_literal_c_emission_owner.pgy direct_mir_passive_nominal_literal_llvm_emission_owner.pgy; do
        ! grep -Eq 'MirMachineLayerAdmittedJsonInput|MirExpressionGraphSequence|JsonObjectFact' "$ROOT_DIR/src/self_hosted/compiler/$emitter" || fail "$emitter reopened MIR"
        ! grep -Eq '"(class|object|tobject|PlayerDto)"|fixture' "$ROOT_DIR/src/self_hosted/compiler/$emitter" || fail "$emitter dispatched on a nominal or fixture spelling"
    done
}

project() {
    local input="$1" target="$2" output="$3"
    rm -f "$output" "$output.stdout" "$output.stderr"
    (cd "$ROOT_DIR" && "$DRIVER_BIN" "--mir-json-backend=$target" "$(root_relative "$input")" "$(root_relative "$output")" >"$output.stdout" 2>"$output.stderr") || {
        cat "$output.stdout" "$output.stderr" >&2 || true
        fail "$target rejected passive nominal MIR"
    }
    [[ -s "$output" ]] || fail "$target emitted no passive nominal artifact"
}

reject_mutation() {
    local name="$1" target="${2:-c}" output
    output="$WORK_DIR/$name.$target.artifact"
    rm -f "$output" "$output.stdout" "$output.stderr"
    if (cd "$ROOT_DIR" && "$DRIVER_BIN" "--mir-json-backend=$target" "$(root_relative "$WORK_DIR/$name.json")" "$(root_relative "$output")" >"$output.stdout" 2>"$output.stderr"); then
        fail "$target accepted passive nominal mutation: $name"
    fi
    [[ ! -e "$output" ]] || fail "$target emitted before rejecting $name"
    grep -Fq 'passive nominal' "$output.stdout" "$output.stderr" || fail "$target rejection lost passive nominal owner: $name"
    ! grep -Fq 'unsupported scalar facts' "$output.stdout" "$output.stderr" || fail "$target retried scalar after passive nominal admission: $name"
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

for path in "$SOURCE" "$DRIVER_BIN" "$PGY"; do require_file "$path"; done
[[ -n "$PYTHON_BIN" ]] || fail "python is required"
command -v "$CC" >/dev/null || fail "missing C compiler"
command -v "$CLANG" >/dev/null || fail "missing LLVM compiler"
assert_owner_ratchet
mkdir -p "$WORK_DIR"
rm -f "$MIR" "$NATIVE_MIR"
(cd "$ROOT_DIR" && "$DRIVER_BIN" --emit-mir-json-verified "$(root_relative "$SOURCE")" "$(root_relative "$MIR")") || fail "source-to-MIR rejected tobject fixture"
mir_digest="$(hash_file "$MIR")"
(cd "$ROOT_DIR" && "$PGY" --mir-json "$(pgy_path_for_compiler "$PGY" "$SOURCE")" >"$NATIVE_MIR") || fail "native MIR oracle rejected tobject fixture"
"$PYTHON_BIN" "$ROOT_DIR/tests/self_hosted/parity/one_mir_passive_nominal_literal_mutations.py" "$MIR" "$NATIVE_MIR" compare || fail "native/self passive nominal semantics drifted"
"$PYTHON_BIN" "$ROOT_DIR/tests/self_hosted/parity/one_mir_passive_nominal_literal_mutations.py" "$MIR" "$WORK_DIR"

project "$MIR" c "$WORK_DIR/baseline.c"
project "$MIR" llvm "$WORK_DIR/baseline.ll"
[[ "$(hash_file "$MIR")" == "$mir_digest" ]] || fail "projection mutated MIR"
grep -Fq 'PlayerDto _pgy_nominal_0 = { .score = 12 };' "$WORK_DIR/baseline.c" || fail "C lost real nominal construction"
grep -Fq 'int32_t _pgy_member_0 = _pgy_nominal_0.score;' "$WORK_DIR/baseline.c" || fail "C lost real field read"
grep -Fq '%pgy.nominal.0 = insertvalue %PlayerDto poison, i32 12, 0' "$WORK_DIR/baseline.ll" || fail "LLVM lost nominal construction"
grep -Fq '%pgy.member.0 = extractvalue %PlayerDto %pgy.nominal.0, 0' "$WORK_DIR/baseline.ll" || fail "LLVM lost field read"
! grep -Fq 'add i64 0, 12' "$WORK_DIR/baseline.ll" || fail "LLVM folded nominal flow into scalar output"
compile_and_expect baseline 12

for variant in semantic-rename literal-seventy-three; do
    project "$WORK_DIR/$variant.json" c "$WORK_DIR/$variant.c"
    project "$WORK_DIR/$variant.json" llvm "$WORK_DIR/$variant.ll"
done
compile_and_expect semantic-rename 12
compile_and_expect literal-seventy-three 73
for host in host-object host-class; do
    project "$WORK_DIR/$host.json" c "$WORK_DIR/$host.c"
    project "$WORK_DIR/$host.json" llvm "$WORK_DIR/$host.ll"
    cmp -s "$WORK_DIR/baseline.c" "$WORK_DIR/$host.c" || fail "C representation drifted for $host"
    cmp -s "$WORK_DIR/baseline.ll" "$WORK_DIR/$host.ll" || fail "LLVM representation drifted for $host"
done

for mutation in kind-drift host-subject declaration-name-drift declaration-id-zero field-name-drift field-type-drift field-kind-drift field-id-collision method-tail entrypoint-drift return-drift source-local-type-drift constructor-type-drift constructor-field-drift constructor-edge-drift constructor-noncanonical-int definition-result-drift definition-local-drift definition-arg-type-drift definition-expr-type-drift definition-abi-forged instruction-tail missing-use stale-use member-receiver-drift member-name-drift member-edge-drift unreachable; do reject_mutation "$mutation"; done
for mutation in kind-drift host-subject field-type-drift constructor-edge-drift definition-expr-type-drift definition-abi-forged instruction-tail stale-use member-name-drift; do reject_mutation "$mutation" llvm; done

echo "[$LABEL] PASS: tobject exact 12, real C/LLVM construction+read, four variants, 28 C negatives, 9 LLVM sentinels (sha256=$mir_digest)"
