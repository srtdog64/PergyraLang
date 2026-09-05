#!/usr/bin/env bash
# One admitted MIR proves compile-time declaration erasure plus general literal Log.
# Forbidden fallback: direct_mir_literal_log_uppercase_only_read.
set -euo pipefail
if ! command -v dirname >/dev/null 2>&1; then PATH="/usr/bin:/bin:$PATH"; export PATH; fi
ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
source "$ROOT_DIR/tests/self_hosted/parity/llvm_leg_helpers.sh"
pgy_prepend_windows_runtime_paths

LABEL="self-host-one-mir-compile-time-declaration-literal"
PGY="${PGY_BIN:-$ROOT_DIR/bin/pgy}"
PGY="$(pgy_select_optional_exe_binary "$PGY")"
DRIVER="${PGY_SELFHOST_DRIVER_BIN:-$ROOT_DIR/bin/pgy-self-driver.exe}"
FIXTURE="$ROOT_DIR/src/self_hosted/mir_lower/fixture/ability_decl.pgy"
WORK="${PGY_SELFHOST_ABILITY_LITERAL_BUILD_DIR:-$ROOT_DIR/.tmp/self_hosted/driver/ability_literal_log}"
MIR="$WORK/ability.one.mir.json"
MUTATOR="$ROOT_DIR/tests/self_hosted/parity/one_mir_compile_time_declaration_literal_mutations.py"
CC="${PGY_SELFHOST_CC:-gcc}"
CLANG="${PGY_SELFHOST_CLANG:-clang}"
fail() { echo "[$LABEL] $*" >&2; exit 1; }
rel() { pgy_selfhost_path_relative_to_root "$1"; }
require() { [[ -f "$1" ]] || fail "missing file: ${1#"$ROOT_DIR/"}"; }

project() {
    local target="$1" input="$2" output="$3"
    rm -f "$output" "$output.out" "$output.err"
    (cd "$ROOT_DIR" && "$DRIVER" "--mir-json-backend=$target" \
        "$(rel "$input")" -o "$(rel "$output")" >"$output.out" 2>"$output.err") || {
        cat "$output.out" "$output.err" >&2 || true
        fail "$target rejected admitted positive MIR: ${input##*/}"
    }
    [[ -s "$output" ]] || fail "$target emitted no artifact: ${input##*/}"
}

compile_run() {
    local target="$1" artifact="$2" expected="$3" stem="$4" bin
    bin="$WORK/$stem.exe"
    if [[ "$target" == c ]]; then
        "$CC" -x c -std=c11 "$artifact" -o "$bin" >"$bin.build" 2>&1 || fail "C compile failed: $stem"
    else
        "$CLANG" -x ir "$artifact" -o "$bin" >"$bin.build" 2>&1 || fail "LLVM compile failed: $stem"
    fi
    local actual
    actual="$($bin)" || fail "execution failed: $stem"
    [[ "$actual" == "$expected" ]] || fail "$stem output '$actual', expected '$expected'"
}

reject_case() {
    local input="$1" target output
    for target in c llvm; do
        output="$WORK/rejected-$target-${input##*/}"
        rm -f "$output" "$output.out" "$output.err"
        if (cd "$ROOT_DIR" && "$DRIVER" "--mir-json-backend=$target" \
            "$(rel "$input")" -o "$(rel "$output")" >"$output.out" 2>"$output.err"); then
            fail "$target accepted negative MIR: ${input##*/}"
        fi
        [[ ! -e "$output" ]] || fail "$target published a rejected artifact: ${input##*/}"
        grep -Eiq 'compile-time declaration erasure|literal Log|MIR machine-layer' "$output.out" "$output.err" || {
            cat "$output.out" "$output.err" >&2 || true
            fail "$target negative escaped the declaration/literal owner: ${input##*/}"
        }
    done
}

require "$PGY"; require "$DRIVER"; require "$FIXTURE"; require "$MUTATOR"
command -v "$CC" >/dev/null 2>&1 || fail "missing C compiler"
command -v "$CLANG" >/dev/null 2>&1 || fail "missing clang"
command -v python >/dev/null 2>&1 || fail "missing python"
mkdir -p "$WORK"

ERASURE="$ROOT_DIR/src/self_hosted/compiler/direct_mir_compile_time_declaration_erasure_owner.pgy"
PLAN="$ROOT_DIR/src/self_hosted/compiler/direct_mir_literal_log_plan_owner.pgy"
EMISSION="$ROOT_DIR/src/self_hosted/compiler/direct_mir_literal_log_emission_owner.pgy"
ROOT_OWNER="$ROOT_DIR/src/self_hosted/compiler/direct_mir_backend_projection_owner.pgy"
for file in "$ERASURE" "$PLAN" "$EMISSION" "$ROOT_OWNER"; do require "$file"; done
for term in runtime_materialization_count runtime_layout_count runtime_symbol_count storage_count; do
    grep -Fq "$term" "$ERASURE" || fail "erasure receipt misses $term"
done
for term in Arithmetic Add ability_decl; do
    ! grep -Fq "$term" "$ERASURE" "$PLAN" "$EMISSION" || fail "fixture dispatch leaked: $term"
done
! grep -R -Fq --include='*.pgy' 'DirectMirHelloProjectionFromAdmitted' "$ROOT_DIR/src/self_hosted/compiler" || fail "old hello admission returned"
! grep -R -Eq --include='*.pgy' 'DirectMirHelloEmit(C|Llvm)' "$ROOT_DIR/src/self_hosted/compiler" || fail "old hello emitter returned"
grep -Fq 'DirectMirCompileTimeDeclarationErasureRouteClaimed(' "$ROOT_OWNER" || fail "declaration-bearing malformed input can retry another route"
route_body="$(sed -n '/^func DirectMirCompileTimeDeclarationErasureRouteClaimed(/,/^}/p' "$ERASURE")"
! grep -Eq 'declarations\.(kinds|nominal_kinds)' <<<"$route_body" || fail "route claim reintroduced semantic kind scanning"
grep -Fq 'DirectMirLiteralLogPlanFromAdmitted(admitted)' "$ROOT_OWNER" || fail "literal plan is not the production owner"
! grep -Eq 'Json|MirMachine|source_json|Arithmetic|Add|ability' "$EMISSION" || fail "emitter reopened MIR or declaration policy"
grep -Fq 'StringRuntimeCInt32LineFormat()' "$PLAN" || fail "literal Int log lost its canonical 32-bit format"
! grep -Fq 'StringRuntimeCIntLineFormat()' "$PLAN" || fail "literal Int log returned to the Long format"
grep -Fq '#include <stdint.h>' "$EMISSION" || fail "literal Int C emission omitted the exact-width header"
grep -Fq 'output = Concat(output, "i32 ");' "$EMISSION" || fail "literal Int LLVM emission is not i32"

rm -f "$MIR"
(cd "$ROOT_DIR" && "$DRIVER" --emit-mir-json-verified "$(rel "$FIXTURE")" -o "$(rel "$MIR")") \
    >"$WORK/produce.out" 2>"$WORK/produce.err" || fail "source-to-MIR failed"
[[ -s "$MIR" ]] || fail "source-to-MIR emitted no artifact"
python "$MUTATOR" "$MIR" "$WORK"

for target in c llvm; do project "$target" "$MIR" "$WORK/ability.$target"; done
compile_run c "$WORK/ability.c" 7 ability-c
compile_run llvm "$WORK/ability.llvm" 7 ability-llvm
for artifact in "$WORK/ability.c" "$WORK/ability.llvm"; do
    ! grep -Eq 'Arithmetic|Add|rhs|ability|pgy_runtime|@pgy_' "$artifact" || fail "declaration materialized: ${artifact##*/}"
done
! grep -Eq 'alloca|getelementptr|load|store|insertvalue|extractvalue' "$WORK/ability.llvm" || fail "LLVM materialized erased storage"

for variant in zero-declaration coherent-rename display-expr0-drift; do
    for target in c llvm; do
        project "$target" "$WORK/$variant.positive.json" "$WORK/$variant.$target"
        cmp -s "$WORK/ability.$target" "$WORK/$variant.$target" || fail "$variant changed $target runtime artifact"
    done
done
for literal in 73 ready; do
    for target in c llvm; do
        project "$target" "$WORK/literal-$literal.positive.json" "$WORK/literal-$literal.$target"
        compile_run "$target" "$WORK/literal-$literal.$target" "$literal" "literal-$literal-$target"
    done
done

negative_count=0
for input in "$WORK"/*.negative.json; do reject_case "$input"; negative_count=$((negative_count + 1)); done
[[ "$negative_count" -eq 28 ]] || fail "expected 28 negative cases, got $negative_count"
echo "[$LABEL] ability exact 7, zero-decl/rename/display equality, literal 73, and $negative_count negatives ok; string literal ready also C/LLVM exact"
