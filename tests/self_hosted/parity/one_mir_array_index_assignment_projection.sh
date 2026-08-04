#!/usr/bin/env bash
# Bounded CollectionValue/Operation plan closes indexed Array assignment.
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
source "$ROOT_DIR/tests/self_hosted/parity/emitted_c_runtime_header_owner.sh"
pgy_prepend_windows_runtime_paths
LABEL="self-host-one-mir-array-index-assignment"
DRIVER="${PGY_SELF_DRIVER_BIN:-$ROOT_DIR/bin/pgy-self-driver}"
CC="${CC:-gcc}"
CLANG="${PGY_SELFHOST_CLANG:-clang}"
PYTHON_BIN="${PYTHON_BIN:-$(command -v python3 || command -v python || true)}"
WORK_REL=".tmp/self_hosted/one_mir_array_index_assignment"
WORK_DIR="$ROOT_DIR/$WORK_REL"
SOURCE_REL="src/self_hosted/codegen/fixture/array_index_assign.pgy"

fail() { echo "[$LABEL] $*" >&2; exit 1; }
require_text() { grep -Fq -- "$2" "$1" || fail "missing ${1#"$ROOT_DIR/"}: $2"; }
reject_text() { ! grep -Fq -- "$2" "$1" || fail "forbidden ${1#"$ROOT_DIR/"}: $2"; }

DRIVER="$(pgy_select_optional_exe_binary "$DRIVER")"
[[ -x "$DRIVER" ]] || fail "current-source self-host driver is unavailable"
command -v "$CC" >/dev/null || fail "C compiler is unavailable"
command -v "$CLANG" >/dev/null || fail "clang is unavailable"
[[ -n "$PYTHON_BIN" ]] || fail "python is required"

for owner in \
    "$ROOT_DIR/src/self_hosted/compiler/direct_mir_scalar_cfg_collection_plan_c_storage_owner.pgy" \
    "$ROOT_DIR/src/self_hosted/compiler/direct_mir_scalar_cfg_collection_plan_c_operation_owner.pgy" \
    "$ROOT_DIR/src/self_hosted/compiler/direct_mir_scalar_cfg_collection_plan_llvm_storage_owner.pgy" \
    "$ROOT_DIR/src/self_hosted/compiler/direct_mir_scalar_cfg_collection_plan_llvm_operation_owner.pgy"; do
    reject_text "$owner" 'source_json'
    reject_text "$owner" 'JsonObjectFactTable'
    reject_text "$owner" 'array_index_assign.pgy'
    reject_text "$owner" 'pgy_ai_set'
    reject_text "$owner" 'pgy_as_set'
done

mkdir -p "$WORK_DIR"
(cd "$ROOT_DIR" && "$DRIVER" --emit-mir-json-verified "$SOURCE_REL" \
    -o "$WORK_REL/program.json") >"$WORK_DIR/producer.out" \
    2>"$WORK_DIR/producer.err" || fail "current producer rejected source"
"$PYTHON_BIN" \
    "$ROOT_DIR/tests/self_hosted/parity/one_mir_array_index_assignment_mutations.py" \
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

goods=(program display-only renumbered alternate alternate-indices overflow-sum \
    overwritten-only groups-reordered)
bads=(bad-int-cross-receiver bad-string-cross-receiver bad-int-target-name \
    bad-stale-int-observation bad-stale-string-observation \
    bad-result-collision bad-spurious-use bad-int-index-negative \
    bad-int-index-oob bad-string-index-oob bad-int-log-index-oob \
    bad-int-log-root bad-string-log-root bad-int-log-before-set \
    bad-missing-int-set bad-missing-string-log bad-extra-assignment \
    bad-string-layout bad-int-value-type bad-multiple-blocks \
    bad-target-graph-missing)
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
    for changed in alternate alternate-indices overflow-sum overwritten-only \
        groups-reordered; do
        ! cmp -s "$WORK_DIR/base.$suffix" "$WORK_DIR/$changed.$suffix" || \
            fail "$target ignored $changed semantic input"
    done
    for bad in "${bads[@]}"; do
        if project "$bad" "$bad" "$target" "$suffix"; then
            fail "$target accepted $bad"
        fi
        [[ ! -e "$WORK_DIR/$bad.$suffix" ]] || fail "$target published $bad"
        grep -Eq '(direct MIR|MIR-LOWER ERROR)' "$WORK_DIR/$bad.$target.out" \
            "$WORK_DIR/$bad.$target.err" || fail "$target lost $bad diagnostic"
        ! grep -Eq 'unsupported block-count CFG|direct MIR scalar local type inventory is missing or invalid' \
            "$WORK_DIR/$bad.$target.out" "$WORK_DIR/$bad.$target.err" || \
            fail "$target retried $bad through a legacy route"
    done
done

source "$ROOT_DIR/tests/self_hosted/parity/one_mir_array_index_assignment_artifact_contract.sh"

compile_run() {
    local stem="$1" expected="$2"
    "$CC" -std=c11 -Wall -Wextra -Werror \
        "${PGY_SELFHOST_EMITTED_C_COMPILE_FLAGS[@]}" \
        "$WORK_DIR/$stem.c" \
        -o "$WORK_DIR/$stem-c.exe" >"$WORK_DIR/$stem-c.compile" 2>&1 || \
        fail "$stem C did not compile"
    "$CLANG" -x ir "$WORK_DIR/$stem.ll" -o "$WORK_DIR/$stem-llvm.exe" \
        >"$WORK_DIR/$stem-llvm.compile" 2>&1 || \
        fail "$stem LLVM did not compile"
    "$WORK_DIR/$stem-c.exe" | tr -d '\r' >"$WORK_DIR/$stem-c.run" || \
        fail "$stem C failed"
    "$WORK_DIR/$stem-llvm.exe" | tr -d '\r' >"$WORK_DIR/$stem-llvm.run" || \
        fail "$stem LLVM failed"
    cmp -s "$WORK_DIR/$stem-c.run" "$WORK_DIR/$stem-llvm.run" || \
        fail "$stem C/LLVM stdout differs"
    printf '%b' "$expected" >"$WORK_DIR/$stem.expected"
    cmp -s "$WORK_DIR/$stem-c.run" "$WORK_DIR/$stem.expected" || \
        fail "$stem stdout differs from semantic expectation"
}

compile_run base '13\nzb\n'
compile_run alternate '18\nqn\n'
compile_run alternate-indices '14\naz\n'
compile_run overflow-sum '-2147483648\nzb\n'
compile_run overwritten-only '13\nzb\n'
compile_run groups-reordered 'zb\n13\n'

echo "[$LABEL] bounded collection operation projection ok"
