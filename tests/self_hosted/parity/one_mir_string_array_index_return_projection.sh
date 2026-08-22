#!/usr/bin/env bash
# One GraphPlan carriage of a borrowed-static Array<String> through a call.
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
source "$ROOT_DIR/tests/self_hosted/parity/emitted_c_runtime_header_owner.sh"
pgy_prepend_windows_runtime_paths
LABEL="self-host-one-mir-string-array-index-return"
DRIVER="$(pgy_select_optional_exe_binary "${PGY_SELF_DRIVER_BIN:-$ROOT_DIR/bin/pgy-self-driver}")"
CC="${CC:-gcc}"
CLANG="${PGY_SELFHOST_CLANG:-clang}"
PYTHON_BIN="${PYTHON_BIN:-$(command -v python3 || command -v python || true)}"
WORK_REL=".tmp/self_hosted/one_mir_string_array_index_return"
WORK_DIR="$ROOT_DIR/$WORK_REL"
SOURCE_REL="src/self_hosted/codegen/fixture/string_array_index_return.pgy"

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
src/self_hosted/compiler/direct_mir_array_string_literal_fact_owner.pgy|40
src/self_hosted/compiler/direct_mir_bounded_literal_index_owner.pgy|15
src/self_hosted/compiler/direct_mir_scalar_program_array_string_literal_admission_owner.pgy|120
src/self_hosted/compiler/direct_mir_scalar_program_array_string_literal_operand_admission_owner.pgy|120
src/self_hosted/compiler/direct_mir_scalar_program_array_string_literal_readiness_owner.pgy|110
src/self_hosted/compiler/direct_mir_scalar_program_array_string_cleanup_policy_owner.pgy|90
src/self_hosted/compiler/direct_mir_scalar_program_array_string_expression_kind_owner.pgy|10
src/self_hosted/compiler/direct_mir_scalar_program_array_string_callable_abi_owner.pgy|25
src/self_hosted/compiler/direct_mir_scalar_cfg_program_extension_fact_readiness_owner.pgy|100
src/self_hosted/compiler/direct_mir_scalar_program_extension_abi_seal_owner.pgy|120
src/self_hosted/compiler/direct_mir_scalar_program_array_string_boundary_fact_owner.pgy|120
src/self_hosted/compiler/direct_mir_scalar_program_array_string_boundary_admission_owner.pgy|150
src/self_hosted/compiler/direct_mir_scalar_program_array_string_boundary_plan_readiness_owner.pgy|35
src/self_hosted/compiler/direct_mir_scalar_program_c_array_string_literal_expression_owner.pgy|45
src/self_hosted/compiler/direct_mir_scalar_program_llvm_array_string_literal_expression_owner.pgy|90
src/self_hosted/compiler/direct_mir_scalar_program_c_array_string_cleanup_owner.pgy|30
src/self_hosted/compiler/direct_mir_scalar_program_llvm_array_string_cleanup_owner.pgy|30
src/self_hosted/compiler/direct_mir_scalar_cfg_llvm_foreign_declaration_owner.pgy|110
src/self_hosted/compiler/direct_mir_scalar_program_callable_signature_owner.pgy|140
src/self_hosted/compiler/direct_mir_scalar_program_callable_parameter_policy_owner.pgy|240
src/self_hosted/compiler/direct_mir_scalar_program_callable_signature_empty_owner.pgy|40
src/self_hosted/compiler/direct_mir_scalar_program_callable_route_envelope_owner.pgy|120
src/self_hosted/compiler/direct_mir_scalar_program_direct_call_readiness_owner.pgy|50
EOF

BOUNDARY="$ROOT_DIR/src/self_hosted/compiler/direct_mir_scalar_program_array_string_boundary_admission_owner.pgy"
LITERAL="$ROOT_DIR/src/self_hosted/compiler/direct_mir_scalar_program_array_string_literal_admission_owner.pgy"
C_SIGNATURE="$ROOT_DIR/src/self_hosted/compiler/direct_mir_scalar_cfg_program_c_signature_owner.pgy"
require_text "$BOUNDARY" 'DirectMirBoundedLiteralIndexReady'
require_text "$BOUNDARY" 'DirectMirScalarProgramArrayStringAbiMatchesCallable'
require_text "$C_SIGNATURE" 'CompilerAbiLayoutArrayStringCValueType()'
for owner in "$BOUNDARY" "$LITERAL"; do
    for term in source_json expr0 string_array_index_return Pick native_retry \
        DirectMirScalarCfgStringArrayPlan; do
        reject_text "$owner" "$term"
    done
done

mkdir -p "$WORK_DIR"
(cd "$ROOT_DIR" && "$DRIVER" --emit-mir-json-verified "$SOURCE_REL" \
    -o "$WORK_REL/program.json") >"$WORK_DIR/producer.out" \
    2>"$WORK_DIR/producer.err" || fail "current producer rejected source"
mir_sha="$(sha256sum "$WORK_DIR/program.json" | cut -d' ' -f1 | tr '[:lower:]' '[:upper:]')"
[[ "$mir_sha" == "6CE3CB8D614BEF3A40DAA39468128B00BB390B9B0ED260DA61EC1E68A2F72D9A" ]] || \
    fail "source MIR identity changed: $mir_sha"
"$PYTHON_BIN" "$ROOT_DIR/tests/self_hosted/parity/one_mir_string_array_index_return_mutations.py" \
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

goods=(program display-only routine-order semantic)
bads=(bad-param-type bad-param-carriage bad-param-pass bad-param-abi-required \
    bad-param-layout bad-local-layout bad-call-target bad-call-argument \
    bad-return-type bad-index-negative bad-index-upper bad-index-topology \
    bad-literal-spine)
for target in c llvm; do
    suffix=c; [[ "$target" == llvm ]] && suffix=ll
    for good in "${goods[@]}"; do
        stem="$good"; [[ "$good" == program ]] && stem=base
        project "$good" "$stem" "$target" "$suffix" || fail "$target rejected $good"
    done
    cmp -s "$WORK_DIR/base.$suffix" "$WORK_DIR/display-only.$suffix" || \
        fail "$target display-only mutation changed the artifact"
    cmp -s "$WORK_DIR/base.$suffix" "$WORK_DIR/routine-order.$suffix" || \
        fail "$target routine order changed the artifact"
    ! cmp -s "$WORK_DIR/base.$suffix" "$WORK_DIR/semantic.$suffix" || \
        fail "$target semantic mutation did not change the artifact"
    for bad in "${bads[@]}"; do
        if project "$bad" "$bad" "$target" "$suffix"; then
            fail "$target accepted $bad"
        fi
        [[ ! -e "$WORK_DIR/$bad.$suffix" ]] || fail "$target published $bad"
    done
done

compile_run() {
    local stem="$1" expected="$2"
    local command=("$CC" -std=c11 "$WORK_DIR/$stem.c")
    if pgy_selfhost_emitted_c_uses_runtime_headers "$WORK_DIR/$stem.c"; then
        command+=("-I$ROOT_DIR/src" "-I$ROOT_DIR/src/runtime" -pthread)
    fi
    command+=(-o "$WORK_DIR/$stem-c.exe")
    "${command[@]}" >"$WORK_DIR/$stem-c.compile" 2>&1 ||
        fail "$stem C did not compile"
    "$CLANG" -x ir "$WORK_DIR/$stem.ll" -o "$WORK_DIR/$stem-llvm.exe" \
        >"$WORK_DIR/$stem-llvm.compile" 2>&1 || fail "$stem LLVM did not compile"
    "$WORK_DIR/$stem-c.exe" | tr -d '\r' >"$WORK_DIR/$stem-c.run"
    "$WORK_DIR/$stem-llvm.exe" | tr -d '\r' >"$WORK_DIR/$stem-llvm.run"
    cmp -s "$WORK_DIR/$stem-c.run" "$WORK_DIR/$stem-llvm.run" || \
        fail "$stem C/LLVM stdout differs"
    printf '%b' "$expected" >"$WORK_DIR/$stem.expected"
    cmp -s "$WORK_DIR/$stem-c.run" "$WORK_DIR/$stem.expected" || \
        fail "$stem stdout differs from expectation"
}

compile_run base 'one\n'
compile_run semantic 'blue\n'
! grep -Fq 'pgy_as_drop_owned(&pgy_local_' "$WORK_DIR/base.c" || \
    fail "C deep-dropped borrowed literal elements"
! grep -Fq 'call void @pgy_as_drop_owned(ptr %pgy.local.' "$WORK_DIR/base.ll" || \
    fail "LLVM deep-dropped borrowed literal elements"
grep -Fq 'pgy_scalar_routine_1(pgy_local_0)' "$WORK_DIR/base.c" || \
    fail "C lost aggregate-by-value call carriage"
grep -Fq '@pgy.scalar.routine.1(%pgy.array.string' "$WORK_DIR/base.ll" || \
    fail "LLVM lost aggregate-by-value call carriage"

echo "[$LABEL] Array<String> call/index/borrowed-result projection ok"
