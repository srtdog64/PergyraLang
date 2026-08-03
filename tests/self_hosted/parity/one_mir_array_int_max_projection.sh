#!/usr/bin/env bash
# Read-only initialized Array<Int> range maximum shares the sealed GraphPlan.
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
pgy_prepend_windows_runtime_paths
LABEL="self-host-one-mir-array-int-max"
DRIVER="${PGY_SELF_DRIVER_BIN:-$ROOT_DIR/bin/pgy-self-driver}"
CC="${CC:-gcc}"
CLANG="${PGY_SELFHOST_CLANG:-clang}"
PYTHON_BIN="${PYTHON_BIN:-$(command -v python3 || command -v python || true)}"
WORK_REL=".tmp/self_hosted/one_mir_array_int_max"
WORK_DIR="$ROOT_DIR/$WORK_REL"
SOURCE_REL="src/self_hosted/codegen/fixture/array_max.pgy"

fail() { echo "[$LABEL] $*" >&2; exit 1; }
require_text() { grep -Fq -- "$2" "$1" || fail "missing ${1#"$ROOT_DIR/"}: $2"; }
reject_text() { ! grep -Fq -- "$2" "$1" || fail "forbidden ${1#"$ROOT_DIR/"}: $2"; }

DRIVER="$(pgy_select_optional_exe_binary "$DRIVER")"
[[ -x "$DRIVER" ]] || fail "installed self-host driver is unavailable"
command -v "$CC" >/dev/null || fail "C compiler is unavailable"
command -v "$CLANG" >/dev/null || fail "clang is unavailable"
[[ -n "$PYTHON_BIN" ]] || fail "python is required"

FACT="$ROOT_DIR/src/self_hosted/compiler/direct_mir_scalar_cfg_array_int_program_fact_owner.pgy"
READY="$ROOT_DIR/src/self_hosted/compiler/direct_mir_scalar_cfg_array_int_read_only_readiness_owner.pgy"
require_text "$FACT" 'DirectMirScalarCfgArrayIntReadOnlyInitializedMode'
require_text "$READY" 'DirectMirScalarCfgArrayIntReadOnlyOperationsReady'
for owner in \
    "$ROOT_DIR/src/self_hosted/compiler/direct_mir_scalar_cfg_array_int_read_only_c_emission_owner.pgy" \
    "$ROOT_DIR/src/self_hosted/compiler/direct_mir_scalar_cfg_array_int_read_only_llvm_emission_owner.pgy"; do
    reject_text "$owner" 'source_json'
    reject_text "$owner" 'JsonObjectFactTable'
    reject_text "$owner" 'MirExpressionGraphSequence'
    reject_text "$owner" 'array_max.pgy'
done

mkdir -p "$WORK_DIR"
rm -f "$WORK_DIR"/*
(cd "$ROOT_DIR" && "$DRIVER" --emit-mir-json-verified "$SOURCE_REL" \
    -o "$WORK_REL/program.json") >"$WORK_DIR/producer.out" \
    2>"$WORK_DIR/producer.err" || fail "installed producer rejected source"
"$PYTHON_BIN" \
    "$ROOT_DIR/tests/self_hosted/parity/one_mir_array_int_max_mutations.py" \
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

goods=(program display-only phi-permuted first-max last-max all-negative)
bads=(bad-array-root bad-array-overflow bad-source-type bad-array-layout \
    bad-duplicate-definition bad-initial-index bad-initial-receiver \
    bad-range-start bad-range-capacity bad-range-use bad-range-edges \
    bad-compare-kind bad-compare-stale bad-compare-edges bad-update-index \
    bad-update-target bad-update-edge bad-header-phi bad-join-phi \
    bad-log-stale bad-log-target bad-extra-collection-use bad-duplicate-log)
for target in c llvm; do
    suffix=c; [[ "$target" == llvm ]] && suffix=ll
    for good in "${goods[@]}"; do
        stem="$good"; [[ "$good" == program ]] && stem=base
        project "$good" "$stem" "$target" "$suffix" || \
            fail "$target rejected $good"
    done
    cmp -s "$WORK_DIR/base.$suffix" "$WORK_DIR/display-only.$suffix" || \
        fail "$target display text changed artifact"
    cmp -s "$WORK_DIR/base.$suffix" "$WORK_DIR/phi-permuted.$suffix" || \
        fail "$target phi order changed artifact"
    for good in first-max last-max all-negative; do
        ! cmp -s "$WORK_DIR/base.$suffix" "$WORK_DIR/$good.$suffix" || \
            fail "$target ignored $good values"
    done
    for bad in "${bads[@]}"; do
        if project "$bad" "$bad" "$target" "$suffix"; then
            fail "$target accepted $bad"
        fi
        [[ ! -e "$WORK_DIR/$bad.$suffix" ]] || fail "$target published $bad"
        grep -Eq '(direct MIR|MIR-LOWER ERROR)' "$WORK_DIR/$bad.$target.out" \
            "$WORK_DIR/$bad.$target.err" || fail "$target lost $bad diagnostic"
        ! grep -Eq 'direct MIR Option match|unsupported block-count CFG' \
            "$WORK_DIR/$bad.$target.out" "$WORK_DIR/$bad.$target.err" || \
            fail "$target retried $bad through a legacy route"
    done
done

source "$ROOT_DIR/tests/self_hosted/parity/one_mir_array_int_max_artifact_contract.sh"

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
compile_run base '9\n'
compile_run first-max '12\n'
compile_run last-max '14\n'
compile_run all-negative '-2\n'
echo "[$LABEL] read-only range maximum executes exact C/LLVM parity"
