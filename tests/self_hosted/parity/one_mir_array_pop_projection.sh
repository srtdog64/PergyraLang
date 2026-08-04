#!/usr/bin/env bash
# Two collection identities and three pop effects share one sealed GraphPlan.
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
pgy_prepend_windows_runtime_paths
LABEL="self-host-one-mir-array-pop"
DRIVER="${PGY_SELF_DRIVER_BIN:-$ROOT_DIR/bin/pgy-self-driver}"
CC="${CC:-gcc}"
CLANG="${PGY_SELFHOST_CLANG:-clang}"
PYTHON_BIN="${PYTHON_BIN:-$(command -v python3 || command -v python || true)}"
WORK_REL=".tmp/self_hosted/one_mir_array_pop"
WORK_DIR="$ROOT_DIR/$WORK_REL"
SOURCE_REL="src/self_hosted/codegen/fixture/array_pop.pgy"

fail() { echo "[$LABEL] $*" >&2; exit 1; }
require_text() { grep -Fq -- "$2" "$1" || fail "missing ${1#"$ROOT_DIR/"}: $2"; }
reject_text() { ! grep -Fq -- "$2" "$1" || fail "forbidden ${1#"$ROOT_DIR/"}: $2"; }

DRIVER="$(pgy_select_optional_exe_binary "$DRIVER")"
[[ -x "$DRIVER" ]] || fail "current-source self-host driver is unavailable"
command -v "$CC" >/dev/null || fail "C compiler is unavailable"
command -v "$CLANG" >/dev/null || fail "clang is unavailable"
[[ -n "$PYTHON_BIN" ]] || fail "python is required"

for owner in \
    "$ROOT_DIR/src/self_hosted/compiler/direct_mir_scalar_cfg_array_int_pop_c_operation_owner.pgy" \
    "$ROOT_DIR/src/self_hosted/compiler/direct_mir_scalar_cfg_array_int_pop_llvm_operation_owner.pgy" \
    "$ROOT_DIR/src/self_hosted/compiler/direct_mir_scalar_cfg_string_array_c_mutation_emission_owner.pgy" \
    "$ROOT_DIR/src/self_hosted/compiler/direct_mir_scalar_cfg_string_array_llvm_mutation_emission_owner.pgy"; do
    reject_text "$owner" 'source_json'
    reject_text "$owner" 'JsonObjectFactTable'
    reject_text "$owner" 'array_pop.pgy'
    reject_text "$owner" 'pgy_ai_pop'
    reject_text "$owner" 'pgy_as_pop'
done

mkdir -p "$WORK_DIR"
rm -f "$WORK_DIR"/*
(cd "$ROOT_DIR" && "$DRIVER" --emit-mir-json-verified "$SOURCE_REL" \
    -o "$WORK_REL/program.json") >"$WORK_DIR/producer.out" \
    2>"$WORK_DIR/producer.err" || fail "current producer rejected source"
"$PYTHON_BIN" \
    "$ROOT_DIR/tests/self_hosted/parity/one_mir_array_pop_mutations.py" \
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

goods=(program display-only renumbered alternate popped-only)
bads=(bad-first-int-receiver bad-second-int-receiver \
    bad-string-cross-receiver bad-pop-result bad-missing-int-pop \
    bad-missing-string-pop bad-extra-string-pop bad-pop-after-observation \
    bad-length-target bad-string-index-oob bad-string-layout \
    bad-loop-successor)
for target in c llvm; do
    suffix=c; [[ "$target" == llvm ]] && suffix=ll
    for good in "${goods[@]}"; do
        stem="$good"; [[ "$good" == program ]] && stem=base
        project "$good" "$stem" "$target" "$suffix" || \
            fail "$target rejected $good"
    done
    cmp -s "$WORK_DIR/base.$suffix" "$WORK_DIR/display-only.$suffix" || \
        fail "$target display text changed artifact"
    cmp -s "$WORK_DIR/base.$suffix" "$WORK_DIR/renumbered.$suffix" || \
        fail "$target coherent ValueId renumber changed artifact"
    ! cmp -s "$WORK_DIR/base.$suffix" "$WORK_DIR/alternate.$suffix" || \
        fail "$target ignored alternate source values"
    ! cmp -s "$WORK_DIR/base.$suffix" "$WORK_DIR/popped-only.$suffix" || \
        fail "$target discarded physically retained popped slots"
    for bad in "${bads[@]}"; do
        if project "$bad" "$bad" "$target" "$suffix"; then
            fail "$target accepted $bad"
        fi
        [[ ! -e "$WORK_DIR/$bad.$suffix" ]] || fail "$target published $bad"
        grep -Eq '(direct MIR|MIR-LOWER ERROR)' "$WORK_DIR/$bad.$target.out" \
            "$WORK_DIR/$bad.$target.err" || fail "$target lost $bad diagnostic"
        ! grep -Eq 'direct MIR Option match|unsupported block-count CFG|direct MIR scalar local type inventory is missing or invalid' \
            "$WORK_DIR/$bad.$target.out" "$WORK_DIR/$bad.$target.err" || \
            fail "$target retried $bad through a legacy route"
    done
done

source "$ROOT_DIR/tests/self_hosted/parity/one_mir_array_pop_artifact_contract.sh"

compile_run() {
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
compile_run base '30\n2\n2\na\n'
compile_run alternate '-1\n2\n2\nx\n'
compile_run popped-only '30\n2\n2\na\n'
echo "[$LABEL] bounded ArrayPop executes exact C/LLVM parity"
