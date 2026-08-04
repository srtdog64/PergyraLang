#!/usr/bin/env bash
# Typed Bool/logical/call extension through the sole scalar CFG GraphPlan.
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
pgy_prepend_windows_runtime_paths
LABEL="self-host-one-mir-bool-logic"
DRIVER="$(pgy_select_optional_exe_binary "${PGY_SELF_DRIVER_BIN:-$ROOT_DIR/bin/pgy-self-driver}")"
CC="${CC:-gcc}"
CLANG="${PGY_SELFHOST_CLANG:-clang}"
PYTHON_BIN="${PYTHON_BIN:-$(command -v python3 || command -v python || true)}"
WORK_REL=".tmp/self_hosted/one_mir_bool_logic"
WORK_DIR="$ROOT_DIR/$WORK_REL"
SOURCE_REL="src/self_hosted/codegen/fixture/bool_logic.pgy"

fail() { echo "[$LABEL] $*" >&2; exit 1; }
require_text() { grep -Fq -- "$2" "$1" || fail "missing ${1#"$ROOT_DIR/"}: $2"; }
reject_text() { ! grep -Fq -- "$2" "$1" || fail "forbidden ${1#"$ROOT_DIR/"}: $2"; }

[[ -x "$DRIVER" ]] || fail "current-source self-host driver is unavailable"
[[ -n "$PYTHON_BIN" ]] || fail "python is required"
command -v "$CC" >/dev/null || fail "C compiler is unavailable"
command -v "$CLANG" >/dev/null || fail "clang is unavailable"

while IFS='|' read -r owner cap; do
    [[ -f "$ROOT_DIR/$owner" ]] || fail "missing owner: $owner"
    lines="$(wc -l <"$ROOT_DIR/$owner")"
    [[ "$lines" -le "$cap" ]] || fail "owner hard cap exceeded: $owner=$lines/$cap"
done <<'EOF'
src/self_hosted/compiler/direct_mir_returned_array_program_route_owner.pgy|100
src/self_hosted/compiler/direct_mir_scalar_program_route_fact_owner.pgy|110
src/self_hosted/compiler/direct_mir_scalar_program_expression_fact_owner.pgy|175
src/self_hosted/compiler/direct_mir_scalar_program_expression_admission_owner.pgy|330
src/self_hosted/compiler/direct_mir_scalar_program_expression_readiness_owner.pgy|135
src/self_hosted/compiler/direct_mir_scalar_program_callable_admission_owner.pgy|125
src/self_hosted/compiler/direct_mir_scalar_cfg_program_extension_fact_owner.pgy|110
src/self_hosted/compiler/direct_mir_scalar_cfg_program_extension_readiness_owner.pgy|115
src/self_hosted/compiler/direct_mir_scalar_cfg_program_arithmetic_admission_owner.pgy|75
src/self_hosted/compiler/direct_mir_scalar_cfg_program_admission_owner.pgy|320
src/self_hosted/compiler/direct_mir_scalar_program_c_expression_owner.pgy|95
src/self_hosted/compiler/direct_mir_scalar_cfg_program_c_emission_owner.pgy|85
src/self_hosted/compiler/direct_mir_scalar_program_llvm_expression_owner.pgy|160
src/self_hosted/compiler/direct_mir_scalar_cfg_program_llvm_emission_owner.pgy|125
src/self_hosted/compiler/direct_mir_scalar_program_projection_owner.pgy|50
EOF

ROUTE="$ROOT_DIR/src/self_hosted/compiler/direct_mir_multi_routine_projection_owner.pgy"
C_EMIT="$ROOT_DIR/src/self_hosted/compiler/direct_mir_scalar_cfg_program_c_emission_owner.pgy"
LLVM_EMIT="$ROOT_DIR/src/self_hosted/compiler/direct_mir_scalar_cfg_program_llvm_emission_owner.pgy"
require_text "$ROUTE" 'DirectMirScalarProgramRouteFactFromAdmitted'
require_text "$ROUTE" 'CompileAdmittedDirectMirScalarProgramForTarget'
for owner in "$C_EMIT" "$LLVM_EMIT"; do
    for term in admitted source_json JsonObjectFactTable BuildMir FromAdmitted \
        bool_logic.pgy flag-on other-off grouped; do
        reject_text "$owner" "$term"
    done
done

mkdir -p "$WORK_DIR"
(cd "$ROOT_DIR" && "$DRIVER" --emit-mir-json-verified "$SOURCE_REL" \
    -o "$WORK_REL/program.json") >"$WORK_DIR/producer.out" \
    2>"$WORK_DIR/producer.err" || fail "current producer rejected source"
mir_sha="$(sha256sum "$WORK_DIR/program.json" | cut -d' ' -f1 | tr '[:lower:]' '[:upper:]')"
[[ "$mir_sha" == "35F8954AE3C72CD8BB74BABD947C7E298B3538CFD39A69199B705EE1A7E5C962" ]] || \
    fail "source MIR identity changed: $mir_sha"
"$PYTHON_BIN" "$ROOT_DIR/tests/self_hosted/parity/one_mir_bool_logic_mutations.py" \
    "$WORK_DIR/program.json" "$WORK_DIR"

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

expected_base=$'flag-on\nother-off\nand\nlogic\ngrouped\n0\n2\n4'
expected_other=$'flag-on\nlogic\n0\n2\n4'
expected_and=$'flag-on\nother-off\nand\ngrouped\n0\n2\n4'
expected_modulo=$'flag-on\nother-off\nand\nlogic\ngrouped\n0\n3'
goods=(program other-true logical-and modulo-three display-only routine-order)
bads=(bad-use-identity bad-logical-kind bad-backedge bad-call-target \
    bad-call-argument-type bad-short-circuit-rhs bad-phi-incoming-identity)
bads+=(bad-modulo-zero bad-modulo-minus-one bad-add-unbounded)

for target in c llvm; do
    suffix=c; [[ "$target" == llvm ]] && suffix=ll
    for good in "${goods[@]}"; do
        stem="$good"; [[ "$good" == program ]] && stem=base
        project "$good" "$stem" "$target" "$suffix" || fail "$target rejected $good"
    done
    cmp -s "$WORK_DIR/base.$suffix" "$WORK_DIR/display-only.$suffix" || \
        fail "$target display-only text changed the artifact"
    cmp -s "$WORK_DIR/base.$suffix" "$WORK_DIR/routine-order.$suffix" || \
        fail "$target routine order changed the artifact"
    for bad in "${bads[@]}"; do
        artifact="$WORK_DIR/$bad.$suffix"
        if project "$bad" "$bad" "$target" "$suffix"; then
            fail "$target accepted $bad"
        fi
        [[ ! -e "$artifact" ]] || fail "$target published artifact for $bad"
        grep -Fq -- 'direct MIR scalar' \
            "$WORK_DIR/$bad.$target.out" "$WORK_DIR/$bad.$target.err" || \
            fail "$target $bad escaped the scalar-program diagnostic"
        for legacy in 'returned Array<Int> foreach' 'Array<Int> return' \
            'collection program' 'Option match' \
            'terminal multi-routine graph is unsupported'; do
            ! grep -Fq -- "$legacy" "$WORK_DIR/$bad.$target.out" \
                "$WORK_DIR/$bad.$target.err" || \
                fail "$target $bad escaped through legacy route: $legacy"
        done
    done
done

for target in c llvm; do
    suffix=c; [[ "$target" == llvm ]] && suffix=ll
    if project non-scalar-callable-signature non-scalar-callable-signature \
            "$target" "$suffix"; then
        fail "$target scalar route claimed a non-(Int)-to-Bool callable"
    fi
    [[ ! -e "$WORK_DIR/non-scalar-callable-signature.$suffix" ]] ||
        fail "$target published the non-scalar route artifact"
    ! grep -Fq -- 'direct MIR scalar CFG program route is invalid' \
        "$WORK_DIR/non-scalar-callable-signature.$target.out" \
        "$WORK_DIR/non-scalar-callable-signature.$target.err" ||
        fail "$target overclaimed the non-scalar callable signature"
done

compile_run() {
    local stem="$1" expected="$2"
    "$CC" -std=c11 "$WORK_DIR/$stem.c" -o "$WORK_DIR/$stem.c.exe" || \
        fail "C host compile failed: $stem"
    "$CLANG" "$WORK_DIR/$stem.ll" -o "$WORK_DIR/$stem.llvm.exe" || \
        fail "LLVM host compile failed: $stem"
    local c_out llvm_out
    c_out="$("$WORK_DIR/$stem.c.exe" | tr -d '\r')" || \
        fail "C execution failed: $stem"
    llvm_out="$("$WORK_DIR/$stem.llvm.exe" | tr -d '\r')" || \
        fail "LLVM execution failed: $stem"
    [[ "$c_out" == "$expected" ]] || fail "C stdout changed: $stem"
    [[ "$llvm_out" == "$expected" ]] || fail "LLVM stdout changed: $stem"
}

compile_run base "$expected_base"
compile_run other-true "$expected_other"
compile_run logical-and "$expected_and"
compile_run modulo-three "$expected_modulo"
compile_run display-only "$expected_base"
compile_run routine-order "$expected_base"

final_sha="$(sha256sum "$WORK_DIR/program.json" | cut -d' ' -f1 | tr '[:lower:]' '[:upper:]')"
[[ "$final_sha" == "$mir_sha" ]] || fail "projection mutated admitted MIR"
echo "[$LABEL] ok: GraphPlan solely owns CFG/SSA/phi; typed Bool/call rows extend both backends"
