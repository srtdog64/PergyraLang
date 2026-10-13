#!/usr/bin/env bash
# Subject and vessel literals share one stable mutable-identity owner family.

set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
pgy_prepend_windows_runtime_paths

LABEL="self-host-one-mir-mutable-nominal-identity"
DRIVER_BIN="$(pgy_select_optional_exe_binary "${PGY_SELFHOST_MUTABLE_NOMINAL_IDENTITY_DRIVER_BIN:-$ROOT_DIR/bin/pgy-self-driver}")"
PGY="$(pgy_select_optional_exe_binary "${PGY_BIN:-$ROOT_DIR/bin/pgy}")"
WORK_DIR="${PGY_SELFHOST_MUTABLE_NOMINAL_IDENTITY_BUILD_DIR:-$ROOT_DIR/.tmp/self_hosted/driver/one_mir_mutable_nominal_identity}"
SUBJECT_SOURCE="$ROOT_DIR/src/self_hosted/mir_lower/fixture/nominal_subject.pgy"
VESSEL_SOURCE="$ROOT_DIR/src/self_hosted/mir_lower/fixture/nominal_vessel.pgy"
SUBJECT_MIR="$WORK_DIR/subject.one.mir.json"
VESSEL_MIR="$WORK_DIR/vessel.one.mir.json"
SUBJECT_NATIVE_MIR="$WORK_DIR/subject.native.mir.json"
VESSEL_NATIVE_MIR="$WORK_DIR/vessel.native.mir.json"
MUTATIONS="$ROOT_DIR/tests/self_hosted/parity/one_mir_nominal_literal_mutations.py"
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
        lines="$(python3 "$ROOT_DIR/scripts/source_size_count.py" "$owner" | tr -d ' ')" || fail "source-size measurement failed"
        [[ "$lines" -le "$cap" ]] || fail "owner hard cap exceeded: ${owner#"$ROOT_DIR/"}=$lines/$cap"
        total=$((total + lines))
    done <<EOF
$ROOT_DIR/src/self_hosted/compiler/direct_mir_mutable_nominal_identity_plan_owner.pgy|180
$ROOT_DIR/src/self_hosted/compiler/direct_mir_mutable_nominal_identity_target_projection_owner.pgy|100
$ROOT_DIR/src/self_hosted/compiler/direct_mir_mutable_nominal_identity_c_emission_owner.pgy|90
$ROOT_DIR/src/self_hosted/compiler/direct_mir_mutable_nominal_identity_llvm_emission_owner.pgy|90
EOF
    [[ "$total" -le 384 ]] || fail "mutable nominal identity owner cap exceeded: $total/384"
}

project() {
    local input="$1" target="$2" output="$3"
    rm -f "$output" "$output.stdout" "$output.stderr"
    (cd "$ROOT_DIR" && "$DRIVER_BIN" "--mir-json-backend=$target" "$(root_relative "$input")" -o "$(root_relative "$output")" >"$output.stdout" 2>"$output.stderr") || {
        cat "$output.stdout" "$output.stderr" >&2 || true
        fail "$target rejected mutable nominal identity MIR"
    }
    [[ -s "$output" ]] || fail "$target emitted no mutable nominal identity artifact"
}

reject_mutation() {
    local name="$1" target="${2:-c}" output
    output="$WORK_DIR/$name.$target.artifact"
    rm -f "$output" "$output.stdout" "$output.stderr"
    if (cd "$ROOT_DIR" && "$DRIVER_BIN" "--mir-json-backend=$target" "$(root_relative "$WORK_DIR/$name.json")" -o "$(root_relative "$output")" >"$output.stdout" 2>"$output.stderr"); then
        fail "$target accepted mutable nominal identity mutation: $name"
    fi
    [[ ! -e "$output" ]] || fail "$target emitted before rejecting $name"
    grep -Eq 'nominal literal|mutable nominal identity|compile-time declaration erasure' "$output.stdout" "$output.stderr" || fail "$target rejection lost declaration/identity owner: $name"
    ! grep -Eq 'passive nominal|unsupported scalar facts' "$output.stdout" "$output.stderr" || fail "$target retried passive/scalar after mutable identity admission: $name"
}

compile_and_expect() {
    local stem="$1" expected="$2"
    "$CC" -std=c11 -Wall -Wextra -Werror -I"$ROOT_DIR/src" -I"$ROOT_DIR/src/runtime" "$WORK_DIR/$stem.c" -o "$WORK_DIR/$stem-c.exe"
    "$CLANG" "$WORK_DIR/$stem.ll" -o "$WORK_DIR/$stem-llvm.exe" 2>"$WORK_DIR/$stem.llvm.log"
    printf '%s\n' "$expected" >"$WORK_DIR/$stem.expected"
    "$WORK_DIR/$stem-c.exe" | tr -d '\r' >"$WORK_DIR/$stem-c.out"
    "$WORK_DIR/$stem-llvm.exe" | tr -d '\r' >"$WORK_DIR/$stem-llvm.out"
    cmp -s "$WORK_DIR/$stem.expected" "$WORK_DIR/$stem-c.out" || fail "C output is not exact $expected"
    cmp -s "$WORK_DIR/$stem.expected" "$WORK_DIR/$stem-llvm.out" || fail "LLVM output is not exact $expected"
}

assert_identity_shape() {
    local stem="$1" type_name="$2" field_name="$3" literal="$4"
    grep -Fq "$type_name _pgy_identity_storage_0;" "$WORK_DIR/$stem.c" || fail "C lost one $stem storage"
    grep -Fq "$type_name *const _pgy_identity_0 = &_pgy_identity_storage_0;" "$WORK_DIR/$stem.c" || fail "C lost the stable $stem pointer"
    grep -Fq "_pgy_identity_0->$field_name = $literal;" "$WORK_DIR/$stem.c" || fail "C initialization bypassed $stem identity"
    grep -Fq "int32_t _pgy_member_0 = _pgy_identity_0->$field_name;" "$WORK_DIR/$stem.c" || fail "C read bypassed $stem identity"
    ! grep -Eq '_pgy_identity_storage_0\.|_pgy_nominal_0' "$WORK_DIR/$stem.c" || fail "C copied $stem as an aggregate value"
    for sentinel in "%pgy.identity.0 = alloca %$type_name" "%pgy.identity.field.0 = getelementptr inbounds %$type_name, ptr %pgy.identity.0, i32 0, i32 0" "store i32 $literal, ptr %pgy.identity.field.0" "%pgy.member.0 = load i32, ptr %pgy.identity.field.0" 'sext i32 %pgy.member.0 to i64'; do
        grep -Fq "$sentinel" "$WORK_DIR/$stem.ll" || fail "LLVM lost $stem identity sentinel: $sentinel"
    done
    local opcode
    for opcode in alloca getelementptr store load; do
        [[ "$(grep -Ec "^[[:space:]]+([^=]+ = )?$opcode " "$WORK_DIR/$stem.ll")" -eq 1 ]] || fail "LLVM $stem $opcode count is not exactly one"
    done
    ! grep -Eq 'insertvalue|extractvalue' "$WORK_DIR/$stem.ll" || fail "LLVM copied $stem through aggregate SSA"
}

for path in "$SUBJECT_SOURCE" "$VESSEL_SOURCE" "$DRIVER_BIN" "$PGY" "$MUTATIONS"; do require_file "$path"; done
[[ -n "$PYTHON_BIN" ]] || fail "python is required"
command -v "$CC" >/dev/null || fail "missing C compiler"
command -v "$CLANG" >/dev/null || fail "missing LLVM compiler"
assert_owner_ratchet
mkdir -p "$WORK_DIR"
rm -f "$SUBJECT_MIR" "$VESSEL_MIR" "$SUBJECT_NATIVE_MIR" "$VESSEL_NATIVE_MIR"
(cd "$ROOT_DIR" && "$DRIVER_BIN" --emit-mir-json-verified "$(root_relative "$SUBJECT_SOURCE")" -o "$(root_relative "$SUBJECT_MIR")") || fail "source-to-MIR rejected subject fixture"
(cd "$ROOT_DIR" && "$DRIVER_BIN" --emit-mir-json-verified "$(root_relative "$VESSEL_SOURCE")" -o "$(root_relative "$VESSEL_MIR")") || fail "source-to-MIR rejected vessel fixture"
subject_digest="$(hash_file "$SUBJECT_MIR")"
vessel_digest="$(hash_file "$VESSEL_MIR")"
(cd "$ROOT_DIR" && "$PGY" --test-native-mir-json-oracle "$(pgy_path_for_compiler "$PGY" "$SUBJECT_SOURCE")" >"$SUBJECT_NATIVE_MIR") || fail "native MIR oracle rejected subject fixture"
(cd "$ROOT_DIR" && "$PGY" --test-native-mir-json-oracle "$(pgy_path_for_compiler "$PGY" "$VESSEL_SOURCE")" >"$VESSEL_NATIVE_MIR") || fail "native MIR oracle rejected vessel fixture"
"$PYTHON_BIN" "$MUTATIONS" "$SUBJECT_MIR" "$SUBJECT_NATIVE_MIR" compare || fail "native/self subject semantics drifted"
"$PYTHON_BIN" "$MUTATIONS" "$VESSEL_MIR" "$VESSEL_NATIVE_MIR" compare || fail "native/self vessel semantics drifted"
"$PYTHON_BIN" "$MUTATIONS" "$VESSEL_MIR" "$WORK_DIR"

project "$SUBJECT_MIR" c "$WORK_DIR/subject.c"
project "$SUBJECT_MIR" llvm "$WORK_DIR/subject.ll"
project "$VESSEL_MIR" c "$WORK_DIR/vessel.c"
project "$VESSEL_MIR" llvm "$WORK_DIR/vessel.ll"
[[ "$(hash_file "$SUBJECT_MIR")" == "$subject_digest" ]] || fail "projection mutated subject MIR"
[[ "$(hash_file "$VESSEL_MIR")" == "$vessel_digest" ]] || fail "projection mutated vessel MIR"
assert_identity_shape subject Hero hp 7
assert_identity_shape vessel HP value 13
compile_and_expect subject 7
compile_and_expect vessel 13

for variant in semantic-rename literal-seventy-three host-subject; do
    project "$WORK_DIR/$variant.json" c "$WORK_DIR/$variant.c"
    project "$WORK_DIR/$variant.json" llvm "$WORK_DIR/$variant.ll"
done
assert_identity_shape semantic-rename Packet value 13
assert_identity_shape literal-seventy-three HP value 73
assert_identity_shape host-subject HP value 13
compile_and_expect semantic-rename 13
compile_and_expect literal-seventy-three 73
compile_and_expect host-subject 13
project "$WORK_DIR/host-tobject.json" c "$WORK_DIR/host-tobject.c"
project "$WORK_DIR/host-tobject.json" llvm "$WORK_DIR/host-tobject.ll"
grep -Fq 'pgy_local_0 = ({ pgy_scalar_logical_record_value_0 pgy_record_value_' "$WORK_DIR/host-tobject.c" || fail "value host did not select sequenced passive C representation"
grep -Fq '.field_0 = (13LL);' "$WORK_DIR/host-tobject.c" || fail "value host lost its admitted field value"
grep -Fq 'insertvalue %pgy.scalar.logical.record.value.0 poison, i64 13, 0' "$WORK_DIR/host-tobject.ll" || fail "value host did not select passive LLVM representation"
compile_and_expect host-tobject 13

for mutation in kind-drift nominal-kind-drift declaration-name-drift declaration-id-zero field-name-drift field-type-drift field-kind-drift field-id-collision method-tail entrypoint-drift return-drift source-local-name-drift source-local-type-drift constructor-type-drift constructor-field-drift constructor-edge-drift constructor-noncanonical-int definition-result-drift definition-local-drift definition-arg-type-drift definition-expr-type-drift definition-abi-type-drift definition-abi-forged instruction-tail duplicate-identity-definition second-source-local missing-use duplicate-use stale-use member-receiver-drift member-name-drift member-edge-drift unreachable; do reject_mutation "$mutation"; done
for mutation in kind-drift nominal-kind-drift field-type-drift source-local-name-drift source-local-type-drift constructor-edge-drift definition-expr-type-drift definition-abi-type-drift definition-abi-forged instruction-tail duplicate-identity-definition duplicate-use stale-use member-name-drift; do reject_mutation "$mutation" llvm; done

echo "[$LABEL] PASS: subject exact 7 + vessel exact 13, one mutable owner, 6 positive pairs, 33 C negatives, 14 LLVM sentinels (subject_sha256=$subject_digest vessel_sha256=$vessel_digest)"
