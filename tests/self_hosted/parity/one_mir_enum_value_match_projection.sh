#!/usr/bin/env bash
# One payload-free enum value and real match CFG feed both selected backends.
set -euo pipefail
ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
source "$ROOT_DIR/tests/self_hosted/parity/llvm_leg_helpers.sh"
pgy_prepend_windows_runtime_paths

LABEL="self-host-one-mir-enum-value-match"
DRIVER="${PGY_SELFHOST_DRIVER_BIN:-$ROOT_DIR/bin/pgy-self-driver.exe}"
FIXTURE="$ROOT_DIR/src/self_hosted/mir_lower/fixture/enum_simple.pgy"
WORK="${PGY_SELFHOST_ENUM_VALUE_MATCH_BUILD_DIR:-$ROOT_DIR/.tmp/self_hosted/driver/enum_value_match}"
MIR="$WORK/enum.one.mir.json"
MUTATOR="$ROOT_DIR/tests/self_hosted/parity/one_mir_enum_value_match_mutations.py"
CC="${PGY_SELFHOST_CC:-gcc}"
CLANG="${PGY_SELFHOST_CLANG:-clang}"
fail() { echo "[$LABEL] $*" >&2; exit 1; }
rel() { pgy_selfhost_path_relative_to_root "$1"; }
require() { [[ -f "$1" ]] || fail "missing file: ${1#"$ROOT_DIR"/}"; }
lines_at_most() { [[ "$(wc -l <"$ROOT_DIR/$1" | tr -d ' ')" -le "$2" ]] || fail "$1 exceeds $2 lines"; }
hash_file() { sha256sum "$1" | awk '{print $1}'; }

project() {
    local target="$1" input="$2" output="$3"
    rm -f "$output" "$output.out" "$output.err"
    (cd "$ROOT_DIR" && "$DRIVER" "--mir-json-backend=$target" \
        "$(rel "$input")" "$(rel "$output")" >"$output.out" 2>"$output.err") || {
        cat "$output.out" "$output.err" >&2 || true
        fail "$target rejected ${input##*/}"
    }
    [[ -s "$output" ]] || fail "$target emitted no artifact"
}

compile_run() {
    local target="$1" artifact="$2" expected="$3" stem="$4" bin actual
    bin="$WORK/$stem.exe"
    if [[ "$target" == c ]]; then
        "$CC" -x c -std=c11 "$artifact" -o "$bin" >"$bin.build" 2>&1 || fail "C compile failed: $stem"
    else
        "$CLANG" -x ir "$artifact" -o "$bin" >"$bin.build" 2>&1 || fail "LLVM compile failed: $stem"
    fi
    actual="$($bin | tr -d '\r')" || fail "execution failed: $stem"
    [[ "$actual" == "$expected" ]] || fail "$stem output '$actual', expected '$expected'"
}

reject_case() {
    local input="$1" target output
    for target in c llvm; do
        output="$WORK/rejected-$target-${input##*/}"
        rm -f "$output" "$output.out" "$output.err"
        if (cd "$ROOT_DIR" && "$DRIVER" "--mir-json-backend=$target" \
            "$(rel "$input")" "$(rel "$output")" >"$output.out" 2>"$output.err"); then
            fail "$target accepted negative ${input##*/}"
        fi
        [[ ! -e "$output" ]] || fail "$target published rejected artifact"
        grep -Eiq 'enum|identity-match|direct MIR CFG|MIR machine-layer' "$output.out" "$output.err" || {
            cat "$output.out" "$output.err" >&2 || true
            fail "$target rejection escaped the enum/CFG owner"
        }
        ! grep -Eiq 'compile-time declaration erasure|nominal literal|Option match|unsupported scalar' \
            "$output.out" "$output.err" || fail "$target retried a forbidden fallback"
    done
}

require "$DRIVER"; require "$FIXTURE"; require "$MUTATOR"
command -v "$CC" >/dev/null 2>&1 || fail "missing C compiler"
command -v "$CLANG" >/dev/null 2>&1 || fail "missing clang"
command -v python >/dev/null 2>&1 || fail "missing python"
mkdir -p "$WORK"

ROUTE="src/self_hosted/compiler/direct_mir_enum_value_match_route_owner.pgy"
ABI="src/self_hosted/compiler/direct_mir_payload_free_enum_abi_owner.pgy"
PLAN_FACT="src/self_hosted/compiler/direct_mir_enum_value_match_plan_fact_owner.pgy"
PLAN="src/self_hosted/compiler/direct_mir_enum_value_match_plan_owner.pgy"
PLAN_MUTATION="src/self_hosted/compiler/direct_mir_enum_value_match_plan_mutation_owner.pgy"
AIR="src/self_hosted/air/mir_identity_match_cfg_certificate_fact_owner.pgy"
AIR_MUTATION="src/self_hosted/air/mir_identity_match_cfg_certificate_mutation_owner.pgy"
ROOT_OWNER="src/self_hosted/compiler/direct_mir_backend_projection_owner.pgy"
EMITTER="src/self_hosted/compiler/direct_mir_backend_emission_owner.pgy"
INDEX="src/self_hosted/mir_lower/program_enum_variant_index_owner.pgy"
for file in "$ROUTE" "$ABI" "$PLAN_FACT" "$PLAN" "$PLAN_MUTATION" "$AIR" \
    "$AIR_MUTATION" "$ROOT_OWNER" "$EMITTER" "$INDEX"; do require "$ROOT_DIR/$file"; done
lines_at_most "$ROUTE" 90; lines_at_most "$ABI" 100; lines_at_most "$PLAN_FACT" 120
lines_at_most "$PLAN" 420; lines_at_most "$PLAN_MUTATION" 70; lines_at_most "$AIR" 220
lines_at_most "$AIR_MUTATION" 50; lines_at_most "$INDEX" 240; lines_at_most "$EMITTER" 500
family_lines=0
for file in "$ROUTE" "$ABI" "$PLAN_FACT" "$PLAN" "$PLAN_MUTATION"; do
    family_lines=$((family_lines + $(wc -l <"$ROOT_DIR/$file")))
done
[[ "$family_lines" -le 770 ]] || fail "enum semantic owner family exceeds 770 lines"
[[ "$(grep -R -F --include='*.pgy' 'DirectMirEnumValueMatchRouteFactFromAdmitted(' "$ROOT_DIR/src/self_hosted" | wc -l | tr -d ' ')" == 2 ]] || fail "enum route must have one definition and one issuer"
[[ "$(grep -R -F --include='*.pgy' 'DirectMirCfgPlanFromAdmitted(' "$ROOT_DIR/src/self_hosted" | wc -l | tr -d ' ')" == 2 ]] || fail "CFG plan must remain the only plan issuer"
python - "$ROOT_DIR/$ROOT_OWNER" <<'PY'
import pathlib, sys
text = pathlib.Path(sys.argv[1]).read_text(encoding="utf-8")
assert text.index("DirectMirEnumValueMatchRouteFactFromAdmitted(admitted)") < text.index("DirectMirNominalLiteralRouteFactFromAdmitted(admitted)")
PY
! grep -R -Eq --include='*.pgy' 'DirectMirEnum[A-Za-z0-9_]*Emit(C|Llvm|LLVM)' "$ROOT_DIR/src/self_hosted" || fail "target-specific enum emitter returned"
! grep -Eiq 'Direction|South|enum_simple|expected.?300' "$ROOT_DIR/$ROUTE" "$ROOT_DIR/$ABI" "$ROOT_DIR/$PLAN_FACT" "$ROOT_DIR/$PLAN" "$ROOT_DIR/$AIR" || fail "fixture identity leaked into owners"
grep -Fq 'CallableReceiverNominalKindOwnsCompileTimeContractOnly' "$ROOT_DIR/src/self_hosted/compiler/direct_mir_compile_time_declaration_erasure_owner.pgy" || fail "erasure route is not narrowed"

rm -f "$MIR"
(cd "$ROOT_DIR" && "$DRIVER" --emit-mir-json-verified "$(rel "$FIXTURE")" "$(rel "$MIR")") \
    >"$WORK/produce.out" 2>"$WORK/produce.err" || fail "source-to-MIR failed"
[[ -s "$MIR" ]] || fail "source-to-MIR emitted no artifact"
mir_hash="$(hash_file "$MIR")"
python "$MUTATOR" "$MIR" "$WORK"

project c "$MIR" "$WORK/enum.c"; project llvm "$MIR" "$WORK/enum.ll"
[[ "$(hash_file "$MIR")" == "$mir_hash" ]] || fail "projection mutated the admitted MIR"
compile_run c "$WORK/enum.c" $'2\n300' enum-c
compile_run llvm "$WORK/enum.ll" $'2\n300' enum-llvm
[[ "$(grep -Fc 'printf(' "$WORK/enum.c")" == 3 ]] || fail "C did not preserve three Log calls"
grep -Fq 'if (pgy_enum_value == 2)' "$WORK/enum.c" || fail "C flattened the match CFG"
[[ "$(grep -Fc 'call i32 (ptr, ...) @printf' "$WORK/enum.ll")" == 3 ]] || fail "LLVM did not preserve three Log calls"
grep -Fq 'icmp eq i64 %pgy.enum.value, 2' "$WORK/enum.ll" || fail "LLVM flattened the match CFG"
! grep -Eiq 'Direction|South|alloca|insertvalue|extractvalue|pgy_runtime|@pgy_' "$WORK/enum.c" "$WORK/enum.ll" || fail "enum gained names, aggregate storage, or runtime helpers"

for variant in coherent-rename display-drift; do
    project c "$WORK/$variant.positive.json" "$WORK/$variant.c"
    project llvm "$WORK/$variant.positive.json" "$WORK/$variant.ll"
    cmp -s "$WORK/enum.c" "$WORK/$variant.c" || fail "$variant changed C artifact"
    cmp -s "$WORK/enum.ll" "$WORK/$variant.ll" || fail "$variant changed LLVM artifact"
done
for row in 'case-one:2\n0' 'selected-ordinal-one:1\n0' 'arm-literals:2\n333'; do
    variant="${row%%:*}"; expected="${row#*:}"; expected="$(printf '%b' "$expected")"
    project c "$WORK/$variant.positive.json" "$WORK/$variant.c"
    project llvm "$WORK/$variant.positive.json" "$WORK/$variant.ll"
    compile_run c "$WORK/$variant.c" "$expected" "$variant-c"
    compile_run llvm "$WORK/$variant.ll" "$expected" "$variant-llvm"
done

negative_count=0
for input in "$WORK"/*.negative.json; do reject_case "$input"; negative_count=$((negative_count + 1)); done
[[ "$negative_count" -eq 34 ]] || fail "expected 34 negative cases, got $negative_count"
echo "[$LABEL] exact C/LLVM 2+300, 5 metamorphic cases, and $negative_count negatives ok"
