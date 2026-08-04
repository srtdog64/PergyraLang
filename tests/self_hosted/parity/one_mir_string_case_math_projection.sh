#!/usr/bin/env bash
# Ordered three-argument call plus StringReplace/Int math in one GraphPlan.
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
pgy_prepend_windows_runtime_paths
LABEL="self-host-one-mir-string-case-math"
DRIVER="$(pgy_select_optional_exe_binary "${PGY_SELF_DRIVER_BIN:-$ROOT_DIR/bin/pgy-self-driver}")"
CC="${CC:-gcc}"
CLANG="${PGY_SELFHOST_CLANG:-clang}"
PYTHON_BIN="${PYTHON_BIN:-$(command -v python3 || command -v python || true)}"
WORK_REL=".tmp/self_hosted/one_mir_string_case_math"
WORK_DIR="$ROOT_DIR/$WORK_REL"
SOURCE_REL="src/self_hosted/codegen/fixture/str_case_math.pgy"

fail() { echo "[$LABEL] $*" >&2; exit 1; }
require_text() { grep -Fq -- "$2" "$1" || fail "missing ${1#"$ROOT_DIR/"}: $2"; }
reject_text() { ! grep -Fq -- "$2" "$1" || fail "forbidden ${1#"$ROOT_DIR/"}: $2"; }

[[ -x "$DRIVER" ]] || fail "current-source self-host driver is unavailable"
[[ -n "$PYTHON_BIN" ]] || fail "python is required"
command -v "$CC" >/dev/null || fail "C compiler is unavailable"
command -v "$CLANG" >/dev/null || fail "clang is unavailable"

while IFS='|' read -r owner cap; do
    [[ -z "$owner" || "$owner" == \#* ]] && continue
    lines="$(wc -l <"$ROOT_DIR/$owner")"
    [[ "$lines" -le "$cap" ]] || fail "owner hard cap exceeded: $owner=$lines/$cap"
done <"$ROOT_DIR/tests/self_hosted/parity/scalar_program_owner_caps.tsv"

GRAPH="$ROOT_DIR/src/self_hosted/compiler/direct_mir_scalar_cfg_graph_fact_owner.pgy"
PARAMS="$ROOT_DIR/src/self_hosted/compiler/direct_mir_routine_parameter_set_fact_owner.pgy"
PARAM_ADMISSION="$ROOT_DIR/src/self_hosted/compiler/direct_mir_routine_parameter_set_admission_owner.pgy"
DIRECT="$ROOT_DIR/src/self_hosted/compiler/direct_mir_scalar_program_direct_call_readiness_owner.pgy"
IDENTITY="$ROOT_DIR/src/self_hosted/compiler/direct_mir_scalar_cfg_program_expression_identity_readiness_owner.pgy"
RUNTIME="$ROOT_DIR/src/self_hosted/compiler/direct_mir_scalar_program_case_math_runtime_requirement_owner.pgy"
MAGNITUDE="$ROOT_DIR/src/self_hosted/compiler/direct_mir_scalar_cfg_program_arithmetic_magnitude_owner.pgy"
require_text "$GRAPH" 'pgy.selfhost.direct-mir-scalar-cfg-graph-plan.v21'
require_text "$PARAMS" 'let type_names: Array<String>'
require_text "$PARAMS" 'let param_digests: Array<Int>'
reject_text "$PARAMS" 'Array<DirectMirRoutineParamFact>'
require_text "$PARAM_ADMISSION" 'cursor = bounds[1]'
require_text "$DIRECT" 'DirectMirScalarProgramNaryOperandRows'
require_text "$IDENTITY" 'ArrayLength(arguments) != plan.routines.parameter_counts[target]'
require_text "$RUNTIME" 'DirectMirScalarProgramCaseMathRuntimeIdsDigest'
require_text "$MAGNITUDE" 'DirectMirScalarCfgProgramAddBoundedByMagnitude'
for owner in "$PARAMS" "$PARAM_ADMISSION" "$DIRECT" "$IDENTITY" "$RUNTIME" "$MAGNITUDE" \
    "$ROOT_DIR/src/self_hosted/compiler/direct_mir_scalar_program_c_case_math_expression_owner.pgy" \
    "$ROOT_DIR/src/self_hosted/compiler/direct_mir_scalar_program_llvm_case_math_expression_owner.pgy"; do
    reject_text "$owner" 'str_case_math.pgy'
    reject_text "$owner" 'HELLO, WORLD!'
    reject_text "$owner" 'a+b+a+b'
done

mkdir -p "$WORK_DIR"
(cd "$ROOT_DIR" && "$DRIVER" --emit-mir-json-verified "$SOURCE_REL" \
    -o "$WORK_REL/producer.json") >"$WORK_DIR/producer.out" \
    2>"$WORK_DIR/producer.err" || fail "current producer rejected source"
mir_sha="$(sha256sum "$WORK_DIR/producer.json" | cut -d' ' -f1 | tr '[:lower:]' '[:upper:]')"
[[ "$mir_sha" == "D0E8EDFAF1B91AED04D5ED99BBDDCD3BB7B250DB673810DD5CCB224E29CDA7AF" ]] ||
    fail "source MIR identity changed: $mir_sha"
"$PYTHON_BIN" "$ROOT_DIR/tests/self_hosted/parity/one_mir_string_case_math_mutations.py" \
    "$WORK_DIR/producer.json" "$WORK_DIR"

project() {
    local input="$1" stem="$2" target="$3" suffix="$4" code=0
    rm -f "$WORK_DIR/$stem.$suffix"
    (cd "$ROOT_DIR" && "$DRIVER" "--mir-json-backend=$target" \
        "$WORK_REL/$input.json" -o "$WORK_REL/$stem.$suffix") \
        >"$WORK_DIR/$stem.$target.out" 2>"$WORK_DIR/$stem.$target.err" || code=$?
    [[ "$code" -ne 0 ]] && return "$code"
    ! grep -Fq 'CODEGEN ERROR' "$WORK_DIR/$stem.$target.out" \
        "$WORK_DIR/$stem.$target.err"
}

goods=(program display-only semantic-change routine-order)
bads=(bad-parameter-ordinal bad-parameter-type bad-direct-call-chain \
    bad-direct-target-syntax bad-return-type bad-min-argument-type \
    bad-builtin-target bad-add-magnitude)
for target in c llvm; do
    suffix=c; [[ "$target" == llvm ]] && suffix=ll
    for good in "${goods[@]}"; do
        stem="$good"; [[ "$good" == program ]] && stem=base
        project "$good" "$stem" "$target" "$suffix" || fail "$target rejected $good"
    done
    cmp -s "$WORK_DIR/base.$suffix" "$WORK_DIR/display-only.$suffix" ||
        fail "$target display text changed the artifact"
    cmp -s "$WORK_DIR/base.$suffix" "$WORK_DIR/routine-order.$suffix" ||
        fail "$target routine row order changed the artifact"
    ! cmp -s "$WORK_DIR/base.$suffix" "$WORK_DIR/semantic-change.$suffix" ||
        fail "$target semantic mutation did not change the artifact"
    for bad in "${bads[@]}"; do
        if project "$bad" "$bad" "$target" "$suffix"; then
            fail "$target accepted $bad"
        fi
        [[ ! -e "$WORK_DIR/$bad.$suffix" ]] || fail "$target published $bad"
        reject_text "$WORK_DIR/$bad.$target.out" 'terminal multi-routine graph is unsupported'
        reject_text "$WORK_DIR/$bad.$target.err" 'terminal multi-routine graph is unsupported'
        reject_text "$WORK_DIR/$bad.$target.out" 'direct MIR CFG merge phi'
        reject_text "$WORK_DIR/$bad.$target.err" 'direct MIR CFG merge phi'
    done
done

for symbol in pgy_strreplace pgy_abs pgy_min pgy_max; do
    require_text "$WORK_DIR/base.c" "$symbol"
    require_text "$WORK_DIR/base.ll" "$symbol"
done
require_text "$WORK_DIR/base.c" 'pgy_scalar_routine_1(long long pgy_param_0, long long pgy_param_1, long long pgy_param_2)'
require_text "$WORK_DIR/base.ll" 'define internal i64 @pgy.scalar.routine.1(i64 %pgy.param.0, i64 %pgy.param.1, i64 %pgy.param.2)'
expected_base=$'HELLO, WORLD!\nhello, world!\nHello, Pergyra!\na+b+a+b\n42\n3\n7\n50\n7'
expected_semantic=$'HELLO, CODEX!\nhello, codex!\nHello, Codex!\na+b+a+b\n42\n3\n7\n50\n7'
for stem in base display-only semantic-change routine-order; do
    "$CC" -std=c11 "$WORK_DIR/$stem.c" -o "$WORK_DIR/$stem.c.exe" ||
        fail "C compile failed: $stem"
    "$CLANG" "$WORK_DIR/$stem.ll" -o "$WORK_DIR/$stem.llvm.exe" ||
        fail "LLVM compile failed: $stem"
    expected="$expected_base"; [[ "$stem" == semantic-change ]] && expected="$expected_semantic"
    c_out="$("$WORK_DIR/$stem.c.exe" | sed 's/\r$//')" || fail "C execution failed: $stem"
    llvm_out="$("$WORK_DIR/$stem.llvm.exe" | sed 's/\r$//')" || fail "LLVM execution failed: $stem"
    [[ "$c_out" == "$expected" ]] || fail "C stdout changed: $stem=$c_out"
    [[ "$llvm_out" == "$expected" ]] || fail "LLVM stdout changed: $stem=$llvm_out"
done
final_sha="$(sha256sum "$WORK_DIR/producer.json" | cut -d' ' -f1 | tr '[:lower:]' '[:upper:]')"
[[ "$final_sha" == "$mir_sha" ]] || fail "projection mutated admitted MIR"
echo "[$LABEL] ok: ordered call and StringReplace/Int math execute from one typed GraphPlan in C and LLVM"
