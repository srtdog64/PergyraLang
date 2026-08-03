#!/usr/bin/env bash
# Bounded dynamic Array<Int> push owns C/LLVM mutation, length, and read.
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
pgy_prepend_windows_runtime_paths
LABEL="self-host-one-mir-array-int-loop-push"
DRIVER="${PGY_SELF_DRIVER_BIN:-$ROOT_DIR/bin/pgy-self-driver}"
CC="${CC:-gcc}"
CLANG="${PGY_SELFHOST_CLANG:-clang}"
PYTHON_BIN="${PYTHON_BIN:-$(command -v python3 || command -v python || true)}"
WORK_REL=".tmp/self_hosted/one_mir_array_int_loop_push"
WORK_DIR="$ROOT_DIR/$WORK_REL"
SOURCE_REL="src/self_hosted/codegen/fixture/array_push.pgy"

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

FACT="$ROOT_DIR/src/self_hosted/compiler/direct_mir_scalar_cfg_array_int_loop_mutation_fact_owner.pgy"
READY="$ROOT_DIR/src/self_hosted/compiler/direct_mir_scalar_cfg_array_int_loop_mutation_readiness_owner.pgy"
C_OWNER="$ROOT_DIR/src/self_hosted/compiler/direct_mir_scalar_cfg_array_int_c_emission_owner.pgy"
LLVM_OWNER="$ROOT_DIR/src/self_hosted/compiler/direct_mir_scalar_cfg_array_int_llvm_emission_owner.pgy"
require_text "$FACT" 'DirectMirScalarCfgArrayIntLoopMutationFact'
require_text "$READY" 'DirectMirScalarCfgArrayIntProducerOrderReady'
for owner in "$C_OWNER" "$LLVM_OWNER"; do
    reject_text "$owner" 'source_json'
    reject_text "$owner" 'JsonObjectFactTable'
    reject_text "$owner" 'MirExpressionGraphSequence'
    reject_text "$owner" 'array_push.pgy'
    reject_text "$owner" 'pgy_ai_push'
    reject_text "$owner" 'pgy_array_push_Int'
done

mkdir -p "$WORK_DIR"
rm -f "$WORK_DIR"/*
(cd "$ROOT_DIR" && "$DRIVER" --emit-mir-json-verified "$SOURCE_REL" \
    -o "$WORK_REL/program.json") >"$WORK_DIR/producer.out" \
    2>"$WORK_DIR/producer.err" || fail "installed producer rejected source"
"$PYTHON_BIN" \
    "$ROOT_DIR/tests/self_hosted/parity/one_mir_array_int_loop_push_mutations.py" \
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

for target in c llvm; do
    suffix=c; [[ "$target" == llvm ]] && suffix=ll
    for good in program display-only value-add bound-three phi-permuted; do
        stem="$good"; [[ "$good" == program ]] && stem=base
        project "$good" "$stem" "$target" "$suffix" || \
            fail "$target rejected $good"
    done
    cmp -s "$WORK_DIR/base.$suffix" "$WORK_DIR/display-only.$suffix" || \
        fail "$target display text changed artifact"
    cmp -s "$WORK_DIR/base.$suffix" "$WORK_DIR/phi-permuted.$suffix" || \
        fail "$target phi order changed artifact"
    for bad in bad-push-receiver bad-push-value bad-push-use-missing \
        bad-push-use-duplicate bad-push-leaf bad-push-edge bad-push-route \
        bad-bound-zero bad-bound-overflow bad-producer-edges \
        bad-producer-backedge bad-step bad-duplicate-push bad-length-target \
        bad-length-use bad-read-collection bad-read-index bad-read-edge \
        bad-final-length-target bad-final-length-use bad-source-type \
        bad-duplicate-local bad-array-layout; do
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

source "$ROOT_DIR/tests/self_hosted/parity/one_mir_array_int_loop_push_artifact_contract.sh"

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
compile_run base '30\n5\n'
compile_run value-add '20\n5\n'
compile_run bound-three '5\n3\n'
echo "[$LABEL] bounded dynamic push executes exact C/LLVM parity"
