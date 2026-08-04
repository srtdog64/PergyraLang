#!/usr/bin/env bash
# Empty Array<String> plus straight-line literal pushes owns C/LLVM mutation.
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
pgy_prepend_windows_runtime_paths
LABEL="self-host-one-mir-string-array-push"
DRIVER="${PGY_SELF_DRIVER_BIN:-$ROOT_DIR/bin/pgy-self-driver}"
CC="${CC:-gcc}"
CLANG="${PGY_SELFHOST_CLANG:-clang}"
PYTHON_BIN="${PYTHON_BIN:-$(command -v python3 || command -v python || true)}"
WORK_REL=".tmp/self_hosted/one_mir_string_array_push"
WORK_DIR="$ROOT_DIR/$WORK_REL"
SOURCE_REL="src/self_hosted/codegen/fixture/str_array_push.pgy"

fail() { echo "[$LABEL] $*" >&2; exit 1; }
require_text() { grep -Fq -- "$2" "$1" || fail "missing ${1#"$ROOT_DIR/"}: $2"; }
reject_text() { ! grep -Fq -- "$2" "$1" || fail "forbidden ${1#"$ROOT_DIR/"}: $2"; }
require_order() {
    local first second
    first="$(grep -Fn -- "$2" "$1" | head -1 | cut -d: -f1)"
    second="$(grep -Fn -- "$3" "$1" | head -1 | cut -d: -f1)"
    [[ -n "$first" && -n "$second" && "$first" -lt "$second" ]] || \
        fail "wrong order in ${1#"$ROOT_DIR/"}: $2 -> $3"
}

DRIVER="$(pgy_select_optional_exe_binary "$DRIVER")"
[[ -x "$DRIVER" ]] || fail "installed self-host driver is unavailable"
command -v "$CC" >/dev/null || fail "C compiler is unavailable"
command -v "$CLANG" >/dev/null || fail "clang is unavailable"
[[ -n "$PYTHON_BIN" ]] || fail "python is required for structured falsifiers"

MUTATION_LENGTH="$ROOT_DIR/src/self_hosted/compiler/direct_mir_scalar_cfg_string_array_mutation_length_owner.pgy"
DOMINANCE="$ROOT_DIR/src/self_hosted/compiler/direct_mir_scalar_cfg_string_array_push_dominance_owner.pgy"
C_STORAGE="$ROOT_DIR/src/self_hosted/compiler/direct_mir_scalar_cfg_string_array_c_storage_emission_owner.pgy"
C_MUTATION="$ROOT_DIR/src/self_hosted/compiler/direct_mir_scalar_cfg_string_array_c_mutation_emission_owner.pgy"
LLVM_STORAGE="$ROOT_DIR/src/self_hosted/compiler/direct_mir_scalar_cfg_string_array_llvm_storage_emission_owner.pgy"
LLVM_MUTATION="$ROOT_DIR/src/self_hosted/compiler/direct_mir_scalar_cfg_string_array_llvm_mutation_emission_owner.pgy"
require_text "$MUTATION_LENGTH" 'DirectMirScalarCfgStringArrayExpectedLengthBeforeNextMutation('
require_text "$DOMINANCE" 'position.local <= last_push.local'
require_text "$C_MUTATION" 'fact.expected_length_before + 1'
require_text "$LLVM_MUTATION" '.length.field, align 8'
for owner in "$C_STORAGE" "$C_MUTATION" "$LLVM_STORAGE" "$LLVM_MUTATION"; do
    reject_text "$owner" 'source_json'
    reject_text "$owner" 'JsonObjectFactTable'
    reject_text "$owner" 'MirExpressionGraphSequence'
    reject_text "$owner" 'str_array_push.pgy'
    reject_text "$owner" 'pgy_as_push'
    reject_text "$owner" 'pgy_array_push_String'
done

mkdir -p "$WORK_DIR"
rm -f "$WORK_DIR"/*
(cd "$ROOT_DIR" && "$DRIVER" --emit-mir-json-verified "$SOURCE_REL" \
    -o "$WORK_REL/program.json") >"$WORK_DIR/producer.out" \
    2>"$WORK_DIR/producer.err" || {
        cat "$WORK_DIR/producer.out" "$WORK_DIR/producer.err" >&2 || true
        fail "installed producer rejected the active source"
    }
grep -Fq '"expr0":"[]"' "$WORK_DIR/program.json" || fail "empty array is absent"
[[ "$(grep -Fo '"arg0":"ArrayPush"' "$WORK_DIR/program.json" | wc -l)" -eq 3 ]] || \
    fail "exact push count is absent"
"$PYTHON_BIN" \
    "$ROOT_DIR/tests/self_hosted/parity/one_mir_string_array_push_mutations.py" \
    "$WORK_DIR/program.json" "$WORK_DIR"

project() {
    local input="$1" stem="$2" target="$3" suffix="$4"
    local code=0
    rm -f "$WORK_DIR/$stem.$suffix"
    (cd "$ROOT_DIR" && "$DRIVER" "--mir-json-backend=$target" \
        "$WORK_REL/$input.json" -o "$WORK_REL/$stem.$suffix") \
        >"$WORK_DIR/$stem.$target.out" 2>"$WORK_DIR/$stem.$target.err" || code=$?
    [[ "$code" -ne 0 ]] && return "$code"
    ! grep -Fq 'CODEGEN ERROR' "$WORK_DIR/$stem.$target.out" "$WORK_DIR/$stem.$target.err"
}

for target in c llvm; do
    suffix=c; [[ "$target" == llvm ]] && suffix=ll
    project program base "$target" "$suffix" || fail "$target rejected String push"
    project display-only display-only "$target" "$suffix" || fail "$target read display text"
    project graph-value graph-value "$target" "$suffix" || fail "$target rejected graph value"
    project push-order push-order "$target" "$suffix" || fail "$target rejected source order"
    cmp -s "$WORK_DIR/base.$suffix" "$WORK_DIR/display-only.$suffix" || \
        fail "$target display text changed artifact"
    for bad in bad-empty-graph bad-empty-kind bad-push-receiver \
        bad-push-use-missing bad-push-use-duplicate bad-push-value-kind \
        bad-push-route bad-push-before-definition bad-entry-backedge \
        bad-intermediate-length \
        bad-loop-push bad-branch-push bad-branch-length-target bad-branch-length-edge \
        bad-final-length-target bad-final-length-edge bad-final-length-use \
        bad-array-layout stale-collection; do
        if project "$bad" "$bad" "$target" "$suffix"; then
            fail "$target accepted $bad"
        fi
        [[ ! -s "$WORK_DIR/$bad.$suffix" ]] || fail "$target published $bad"
        grep -Eq '(direct MIR|MIR-LOWER ERROR)' \
            "$WORK_DIR/$bad.$target.out" "$WORK_DIR/$bad.$target.err" || \
            fail "$target lost owned diagnostic for $bad"
        ! grep -Fq 'direct MIR Option match' \
            "$WORK_DIR/$bad.$target.out" "$WORK_DIR/$bad.$target.err" || \
            fail "$target retried $bad as Option"
    done
done

source "$ROOT_DIR/tests/self_hosted/parity/one_mir_string_array_push_artifact_contract.sh"

compile_and_run() {
    local stem="$1" expected="$2"
    "$CC" -std=c11 -Wall -Wextra -Werror "$WORK_DIR/$stem.c" \
        -o "$WORK_DIR/$stem-c.exe" >"$WORK_DIR/$stem-c.compile" 2>&1 || \
        fail "$stem C did not compile"
    "$CLANG" -x ir "$WORK_DIR/$stem.ll" -o "$WORK_DIR/$stem-llvm.exe" \
        >"$WORK_DIR/$stem-llvm.compile" 2>&1 || fail "$stem LLVM did not compile"
    printf '%b' "$expected" >"$WORK_DIR/$stem.expected"
    for backend in c llvm; do
        "$WORK_DIR/$stem-$backend.exe" | tr -d '\r' >"$WORK_DIR/$stem-$backend.run"
        cmp -s "$WORK_DIR/$stem.expected" "$WORK_DIR/$stem-$backend.run" || \
            fail "$stem $backend output drift"
    done
}
compile_and_run base 'abbccc\n3\n'
compile_and_run graph-value 'aBccc\n3\n'
compile_and_run push-order 'cccbba\n3\n'

echo "[$LABEL] one empty-array receipt executes exact ordered C/LLVM push and length parity"
