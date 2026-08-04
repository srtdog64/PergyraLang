#!/usr/bin/env bash
# Multi-routine String equality must execute through one sealed scalar GraphPlan.
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
pgy_prepend_windows_runtime_paths
LABEL="self-host-one-mir-string-equality"
DRIVER="$(pgy_select_optional_exe_binary "${PGY_SELF_DRIVER_BIN:-$ROOT_DIR/bin/pgy-self-driver}")"
CC="${CC:-gcc}"
CLANG="${PGY_SELFHOST_CLANG:-clang}"
PYTHON_BIN="${PYTHON_BIN:-$(command -v python3 || command -v python || true)}"
WORK_REL=".tmp/self_hosted/one_mir_string_equality"
WORK_DIR="$ROOT_DIR/$WORK_REL"
SOURCE_REL="src/self_hosted/codegen/fixture/string_equality.pgy"

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

GENERIC="$ROOT_DIR/src/self_hosted/compiler/direct_mir_scalar_cfg_graph_admission_owner.pgy"
PROGRAM="$ROOT_DIR/src/self_hosted/compiler/direct_mir_scalar_cfg_program_graph_admission_owner.pgy"
ROUTINE="$ROOT_DIR/src/self_hosted/compiler/direct_mir_scalar_cfg_program_routine_admission_owner.pgy"
C_EMIT="$ROOT_DIR/src/self_hosted/compiler/direct_mir_scalar_cfg_program_c_emission_owner.pgy"
LLVM_EMIT="$ROOT_DIR/src/self_hosted/compiler/direct_mir_scalar_cfg_program_llvm_emission_owner.pgy"
ABI="$ROOT_DIR/src/self_hosted/compiler/runtime_call_abi_structured_fact_owner.pgy"
CALL="$ROOT_DIR/src/self_hosted/compiler/direct_mir_scalar_program_call_expression_admission_owner.pgy"
for term in DirectMirScalarCfgProgramAdmissionState program.active; do
    reject_text "$GENERIC" "$term"
done
[[ "$(grep -Fc 'DirectMirScalarCfgProgramAppendRoutine(' "$PROGRAM")" -eq 1 ]] ||
    fail "one loop does not own every scalar-program routine admission"
require_text "$PROGRAM" 'while ordinal < ArrayLength(routine_rows)'
[[ "$(grep -Fc 'DirectMirScalarCfgSealGraphPlan(' "$PROGRAM")" -eq 1 ]] ||
    fail "program graph must seal exactly once"
require_text "$ROUTINE" 'func DirectMirScalarCfgProgramAppendRoutine('
for owner in "$C_EMIT" "$LLVM_EMIT"; do
    require_text "$owner" 'while routine < ArrayLength(plan.routines.roles)'
    require_text "$owner" 'plan.routines.block_starts[routine]'
    for term in admitted source_json JsonObjectFactTable BuildMir FromAdmitted \
        string_equality.pgy; do reject_text "$owner" "$term"; done
done
require_text "$ABI" 'CompilerRuntimeCallAbiStringCompareFact('
require_text "$CALL" 'call_target_syntax_ids[chain.call_node]'
if grep -RFq -- 'pgy_scalar_callable_0' "$ROOT_DIR/src/self_hosted/compiler" ||
   grep -RFq -- 'pgy.scalar.callable.0' "$ROOT_DIR/src/self_hosted/compiler"; then
    fail "retired callable-specific backend symbol returned"
fi

mkdir -p "$WORK_DIR"
(cd "$ROOT_DIR" && "$DRIVER" --emit-mir-json-verified "$SOURCE_REL" \
    -o "$WORK_REL/program.json") >"$WORK_DIR/producer.out" \
    2>"$WORK_DIR/producer.err" || fail "current producer rejected source"
mir_sha="$(sha256sum "$WORK_DIR/program.json" | cut -d' ' -f1 | tr '[:lower:]' '[:upper:]')"
[[ "$mir_sha" == "1A9D856F377CCF27424E72F19B535EE8431B737D1ED61FF868E3CB3DC6638228" ]] ||
    fail "source MIR identity changed: $mir_sha"
"$PYTHON_BIN" "$ROOT_DIR/tests/self_hosted/parity/one_mir_string_equality_mutations.py" \
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

goods=(program display-only routine-order)
bads=(bad-call-target bad-parameter-identity bad-string-comparison-kind \
    bad-return-type bad-callable-edge missing-terminal-return)
for target in c llvm; do
    suffix=c; [[ "$target" == llvm ]] && suffix=ll
    for good in "${goods[@]}"; do
        stem="$good"; [[ "$good" == program ]] && stem=base
        project "$good" "$stem" "$target" "$suffix" || fail "$target rejected $good"
    done
    cmp -s "$WORK_DIR/base.$suffix" "$WORK_DIR/display-only.$suffix" ||
        fail "$target display text changed the artifact"
    cmp -s "$WORK_DIR/base.$suffix" "$WORK_DIR/routine-order.$suffix" ||
        fail "$target routine order changed the artifact"
    for bad in "${bads[@]}"; do
        if project "$bad" "$bad" "$target" "$suffix"; then
            fail "$target accepted $bad"
        fi
        [[ ! -e "$WORK_DIR/$bad.$suffix" ]] || fail "$target published $bad"
        grep -Fq -- 'direct MIR scalar' "$WORK_DIR/$bad.$target.out" \
            "$WORK_DIR/$bad.$target.err" || fail "$target $bad escaped scalar diagnostic"
        ! grep -Fq -- 'terminal multi-routine graph is unsupported' \
            "$WORK_DIR/$bad.$target.out" "$WORK_DIR/$bad.$target.err" ||
            fail "$target $bad retried the retired terminal route"
    done
done

require_text "$WORK_DIR/base.c" 'strcmp('
require_text "$WORK_DIR/base.c" 'pgy_scalar_routine_1'
require_text "$WORK_DIR/base.ll" 'declare i32 @strcmp(ptr, ptr)'
require_text "$WORK_DIR/base.ll" '@pgy.scalar.routine.1'
expected=$'I\nS\nS\n?\neq'
for stem in base display-only routine-order; do
    "$CC" -std=c11 "$WORK_DIR/$stem.c" -o "$WORK_DIR/$stem.c.exe" || fail "C compile failed: $stem"
    "$CLANG" "$WORK_DIR/$stem.ll" -o "$WORK_DIR/$stem.llvm.exe" || fail "LLVM compile failed: $stem"
    c_out="$("$WORK_DIR/$stem.c.exe" | tr -d '\r')" || fail "C execution failed: $stem"
    llvm_out="$("$WORK_DIR/$stem.llvm.exe" | tr -d '\r')" || fail "LLVM execution failed: $stem"
    [[ "$c_out" == "$expected" ]] || fail "C stdout changed: $stem"
    [[ "$llvm_out" == "$expected" ]] || fail "LLVM stdout changed: $stem"
done
final_sha="$(sha256sum "$WORK_DIR/program.json" | cut -d' ' -f1 | tr '[:lower:]' '[:upper:]')"
[[ "$final_sha" == "$mir_sha" ]] || fail "projection mutated admitted MIR"
echo "[$LABEL] ok: one layered GraphPlan executes routine-partitioned String equality in C and LLVM"
