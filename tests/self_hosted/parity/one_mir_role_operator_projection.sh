#!/usr/bin/env bash
# One producer-resolved role operator call feeds both selected direct backends.
set -euo pipefail
ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
source "$ROOT_DIR/tests/self_hosted/parity/llvm_leg_helpers.sh"
pgy_prepend_windows_runtime_paths

LABEL="self-host-one-mir-role-operator"
DRIVER="${PGY_SELFHOST_DRIVER_BIN:-$ROOT_DIR/bin/pgy-self-driver.exe}"
FIXTURE="$ROOT_DIR/src/self_hosted/mir_lower/fixture/role_operator_dispatch.pgy"
WORK="${PGY_SELFHOST_ROLE_OPERATOR_BUILD_DIR:-$ROOT_DIR/.tmp/self_hosted/driver/role_operator}"
MIR="$WORK/role-operator.one.mir.json"
MUTATOR="$ROOT_DIR/tests/self_hosted/parity/one_mir_role_operator_mutations.py"
CC="${PGY_SELFHOST_CC:-gcc}"
CLANG="${PGY_SELFHOST_CLANG:-clang}"
fail() { echo "[$LABEL] $*" >&2; exit 1; }
rel() { pgy_selfhost_path_relative_to_root "$1"; }
require() { [[ -f "$1" ]] || fail "missing file: ${1#"$ROOT_DIR"/}"; }
lines_at_most() {
    [[ "$(wc -l <"$ROOT_DIR/$1" | tr -d ' ')" -le "$2" ]] ||
        fail "$1 exceeds $2 lines"
}
hash_file() { sha256sum "$1" | awk '{print $1}'; }

project() {
    local target="$1" input="$2" output="$3"
    rm -f "$output" "$output.out" "$output.err"
    (cd "$ROOT_DIR" && "$DRIVER" "--mir-json-backend=$target" \
        "$(rel "$input")" -o "$(rel "$output")" \
        >"$output.out" 2>"$output.err") || {
        cat "$output.out" "$output.err" >&2 || true
        fail "$target rejected ${input##*/}"
    }
    [[ -s "$output" ]] || fail "$target emitted no artifact"
}

compile_run() {
    local target="$1" artifact="$2" expected="$3" stem="$4" bin actual
    bin="$WORK/$stem.exe"
    if [[ "$target" == c ]]; then
        "$CC" -x c -std=c11 "$artifact" -o "$bin" \
            >"$bin.build" 2>&1 || fail "C compile failed: $stem"
    else
        "$CLANG" -x ir "$artifact" -o "$bin" \
            >"$bin.build" 2>&1 || fail "LLVM compile failed: $stem"
    fi
    actual="$($bin | tr -d '\r')" || fail "execution failed: $stem"
    [[ "$actual" == "$expected" ]] ||
        fail "$stem output '$actual', expected '$expected'"
}

reject_case() {
    local input="$1" target output
    for target in c llvm; do
        output="$WORK/rejected-$target-${input##*/}"
        rm -f "$output" "$output.out" "$output.err"
        if (cd "$ROOT_DIR" && "$DRIVER" "--mir-json-backend=$target" \
            "$(rel "$input")" -o "$(rel "$output")" \
            >"$output.out" 2>"$output.err"); then
            fail "$target accepted negative ${input##*/}"
        fi
        [[ ! -e "$output" ]] || fail "$target published rejected artifact"
        grep -Eiq 'role operator|multi-routine|MIR machine-layer|direct MIR scalar local' \
            "$output.out" "$output.err" || {
            cat "$output.out" "$output.err" >&2 || true
            fail "$target rejection escaped the role/multi-routine owner"
        }
    done
}

require "$DRIVER"; require "$FIXTURE"; require "$MUTATOR"
command -v "$CC" >/dev/null 2>&1 || fail "missing C compiler"
command -v "$CLANG" >/dev/null 2>&1 || fail "missing clang"
command -v python >/dev/null 2>&1 || fail "missing python"
mkdir -p "$WORK"

VOCAB="src/self_hosted/semantic/role_operator_vocabulary_owner.pgy"
RESOLUTION="src/self_hosted/semantic/role_operator_resolution_owner.pgy"
DECL="src/self_hosted/compiler/direct_mir_role_operator_declaration_fact_owner.pgy"
PLAN="src/self_hosted/compiler/direct_mir_role_operator_plan_owner.pgy"
ABI="src/self_hosted/compiler/direct_mir_role_operator_abi_projection_owner.pgy"
EMITTER="src/self_hosted/compiler/direct_mir_role_operator_emission_owner.pgy"
ROUTER="src/self_hosted/compiler/direct_mir_multi_routine_projection_owner.pgy"
GRAPH="src/self_hosted/semantic/ast_expression_graph_fact_owner.pgy"
ROLE_C="src/self_hosted/codegen/emission/role_dispatch_emit_owner.pgy"
for file in "$VOCAB" "$RESOLUTION" "$DECL" "$PLAN" "$ABI" "$EMITTER" \
    "$ROUTER" "$GRAPH" "$ROLE_C"; do require "$ROOT_DIR/$file"; done
lines_at_most "$VOCAB" 220; lines_at_most "$RESOLUTION" 340
lines_at_most "$DECL" 310; lines_at_most "$PLAN" 560
lines_at_most "$ABI" 140; lines_at_most "$EMITTER" 150
family_lines=0
for file in "$DECL" "$PLAN" "$ABI" "$EMITTER"; do
    family_lines=$((family_lines + $(wc -l <"$ROOT_DIR/$file")))
done
[[ "$family_lines" -le 1200 ]] || fail "role direct owner family exceeds 1200 lines"
grep -Fq 'SemanticRoleOperatorKindSupported(kind)' "$ROOT_DIR/$GRAPH" ||
    fail "expression graph duplicates the supported role-operator set"
grep -Fq 'SemanticRoleOperatorKindForMethodName(mname)' "$ROOT_DIR/$ROLE_C" ||
    fail "legacy C role dispatch does not consume the role vocabulary"
! grep -Eq 'AstExpressionNodeAdd\(\)|mname == "Add"' "$ROOT_DIR/$ROLE_C" ||
    fail "legacy C role dispatch restored a private Add selector"
python - "$ROOT_DIR/$ROUTER" <<'PY'
import pathlib, sys
text = pathlib.Path(sys.argv[1]).read_text(encoding="utf-8")
assert text.index("DirectMirRoleOperatorProgramCandidate(admitted)") < text.index("DirectMirArrayReturnProgramCandidate(admitted)")
PY
! grep -Eiq 'IntMath|Arithmetic|Hero|role_operator_dispatch|expected.?123|"123"' \
    "$ROOT_DIR/$DECL" "$ROOT_DIR/$PLAN" "$ROOT_DIR/$ABI" \
    "$ROOT_DIR/$EMITTER" || fail "fixture identity leaked into role owners"
! grep -Eq 'int32_t|i32 %rhs|define internal i32' "$ROOT_DIR/$EMITTER" ||
    fail "role emitter restored a private Int ABI"

rm -f "$MIR"
(cd "$ROOT_DIR" && "$DRIVER" --emit-mir-json-verified \
    "$(rel "$FIXTURE")" -o "$(rel "$MIR")") \
    >"$WORK/produce.out" 2>"$WORK/produce.err" || fail "source-to-MIR failed"
[[ -s "$MIR" ]] || fail "source-to-MIR emitted no artifact"
mir_hash="$(hash_file "$MIR")"
grep -Fq '"call_target_kind":"role_operator"' "$MIR" ||
    fail "producer did not persist the resolved role target kind"
grep -Eq '"call_target_name":"[^."]+\.[^."]+\.[^."]+"' "$MIR" ||
    fail "producer did not persist role/ability/method identity"
python "$MUTATOR" "$MIR" "$WORK"

project c "$MIR" "$WORK/role-operator.c"
project llvm "$MIR" "$WORK/role-operator.ll"
[[ "$(hash_file "$MIR")" == "$mir_hash" ]] ||
    fail "projection mutated the admitted MIR"
compile_run c "$WORK/role-operator.c" 123 role-operator-c
compile_run llvm "$WORK/role-operator.ll" 123 role-operator-llvm
grep -Fq 'static int32_t pgy_direct_role_operator_method(int32_t *self, int32_t rhs)' \
    "$WORK/role-operator.c" || fail "C role method call ABI drifted"
grep -Fq 'pgy_direct_role_operator_method(&pgy_receiver, pgy_rhs)' \
    "$WORK/role-operator.c" || fail "C flattened the role method call"
! grep -Fq 'pgy_lhs + pgy_rhs' "$WORK/role-operator.c" ||
    fail "C retried primitive addition"
grep -Fq 'define internal i64 @pgy.direct.role.operator.method(ptr %self, i64 %rhs)' \
    "$WORK/role-operator.ll" || fail "LLVM role method call ABI drifted"
grep -Fq 'call i64 @pgy.direct.role.operator.method(ptr %pgy.receiver, i64' \
    "$WORK/role-operator.ll" || fail "LLVM flattened the role method call"
! grep -Fq '= add i64' "$WORK/role-operator.ll" ||
    fail "LLVM retried primitive addition"

for variant in declaration-order routine-order display-drift coherent-rename; do
    project c "$WORK/$variant.positive.json" "$WORK/$variant.c"
    project llvm "$WORK/$variant.positive.json" "$WORK/$variant.ll"
    cmp -s "$WORK/role-operator.c" "$WORK/$variant.c" ||
        fail "$variant changed C artifact"
    cmp -s "$WORK/role-operator.ll" "$WORK/$variant.ll" ||
        fail "$variant changed LLVM artifact"
done
for row in 'method-value:321' 'operand-values:123'; do
    variant="${row%%:*}"; expected="${row#*:}"
    project c "$WORK/$variant.positive.json" "$WORK/$variant.c"
    project llvm "$WORK/$variant.positive.json" "$WORK/$variant.ll"
    compile_run c "$WORK/$variant.c" "$expected" "$variant-c"
    compile_run llvm "$WORK/$variant.ll" "$expected" "$variant-llvm"
done

negative_count=0
for input in "$WORK"/*.negative.json; do
    reject_case "$input"
    negative_count=$((negative_count + 1))
done
[[ "$negative_count" -eq 30 ]] ||
    fail "expected 30 negative cases, got $negative_count"
PGY_SELF_DRIVER_BIN="$DRIVER" bash \
    "$ROOT_DIR/tests/self_hosted/parity/role_receiver_canonicalization_owner.sh"
echo "[$LABEL] exact C/LLVM 123, 6 metamorphic cases, and $negative_count negatives ok"
