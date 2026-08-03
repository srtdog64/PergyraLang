#!/usr/bin/env bash
# One ArrayLength-bounded parts[i] receipt owns both C and LLVM execution.
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
pgy_prepend_windows_runtime_paths
LABEL="self-host-one-mir-indexed-string-array"
DRIVER="${PGY_SELF_DRIVER_BIN:-$ROOT_DIR/bin/pgy-self-driver}"
CC="${CC:-gcc}"
CLANG="${PGY_SELFHOST_CLANG:-clang}"
PYTHON_BIN="${PYTHON_BIN:-$(command -v python3 || command -v python || true)}"
WORK_REL=".tmp/self_hosted/one_mir_indexed_string_array"
WORK_DIR="$ROOT_DIR/$WORK_REL"
SOURCE_REL="src/self_hosted/codegen/fixture/str_array_concat.pgy"

fail() { echo "[$LABEL] $*" >&2; exit 1; }
require_text() { grep -Fq -- "$2" "$1" || fail "missing ${1#"$ROOT_DIR/"}: $2"; }
reject_text() { ! grep -Fq -- "$2" "$1" || fail "forbidden ${1#"$ROOT_DIR/"}: $2"; }

DRIVER="$(pgy_select_optional_exe_binary "$DRIVER")"
[[ -x "$DRIVER" ]] || fail "installed self-host driver is unavailable"
command -v "$CC" >/dev/null || fail "C compiler is unavailable"
command -v "$CLANG" >/dev/null || fail "clang is unavailable"
[[ -n "$PYTHON_BIN" ]] || fail "python is required for structured falsifiers"

ADMISSION="$ROOT_DIR/src/self_hosted/compiler/direct_mir_scalar_cfg_indexed_string_array_admission_owner.pgy"
RANGE_OWNER="$ROOT_DIR/src/self_hosted/compiler/direct_mir_scalar_cfg_range_bound_owner.pgy"
C_OWNER="$ROOT_DIR/src/self_hosted/compiler/direct_mir_scalar_cfg_indexed_string_c_emission_owner.pgy"
LLVM_OWNER="$ROOT_DIR/src/self_hosted/compiler/direct_mir_scalar_cfg_indexed_string_llvm_emission_owner.pgy"
PRODUCER_NATIVE="$ROOT_DIR/src/compiler/mir_ssa_use_edges.c"
PRODUCER_SELF="$ROOT_DIR/src/self_hosted/mir/routine_for_owner.pgy"
require_text "$RANGE_OWNER" 'sequence.arena.call_target_names[1] != "ArrayLength"'
require_text "$ADMISSION" 'DirectMirScalarCfgIndexedStringConcatGraphReady('
require_text "$C_OWNER" 'pgy_indexed_collection.length'
require_text "$C_OWNER" 'pgy_indexed_collection.data[pgy_local_'
require_text "$LLVM_OWNER" 'icmp ult i64'
require_text "$PRODUCER_NATIVE" 'MIR_BRANCH_FOR_RANGE'
require_text "$PRODUCER_NATIVE" '? inst->expr1'
require_text "$PRODUCER_SELF" 'node_id, AstExpressionLaneAuxiliary()'
for owner in "$C_OWNER" "$LLVM_OWNER"; do
    reject_text "$owner" 'source_json'
    reject_text "$owner" 'JsonObjectFactTable'
    reject_text "$owner" 'MirExpressionGraphSequence'
    reject_text "$owner" '"expr0"'
    reject_text "$owner" 'str_array_concat.pgy'
done
reject_text "$C_OWNER" 'pgy_indexed_collection.capacity)'
reject_text "$LLVM_OWNER" '.capacity = extractvalue'

mkdir -p "$WORK_DIR"
rm -f "$WORK_DIR"/*
(cd "$ROOT_DIR" && "$DRIVER" --emit-mir-json-verified "$SOURCE_REL" \
    -o "$WORK_REL/program.json") >"$WORK_DIR/producer.out" \
    2>"$WORK_DIR/producer.err" || {
        cat "$WORK_DIR/producer.out" "$WORK_DIR/producer.err" >&2 || true
        fail "installed producer rejected the active source"
    }
grep -Fq '"kind":"branch"' "$WORK_DIR/program.json" || fail "range branch is absent"
grep -Fq '"uses":["parts.1"]' "$WORK_DIR/program.json" || fail "range bound ValueId was not persisted"
"$PYTHON_BIN" \
    "$ROOT_DIR/tests/self_hosted/parity/one_mir_indexed_string_array_mutations.py" \
    "$WORK_DIR/program.json" "$WORK_DIR"

project() {
    local input="$1" stem="$2" target="$3" suffix="$4"
    rm -f "$WORK_DIR/$stem.$suffix"
    (cd "$ROOT_DIR" && "$DRIVER" "--mir-json-backend=$target" \
        "$WORK_REL/$input.json" -o "$WORK_REL/$stem.$suffix") \
        >"$WORK_DIR/$stem.$target.out" 2>"$WORK_DIR/$stem.$target.err"
}

for target in c llvm; do
    suffix=c; [[ "$target" == llvm ]] && suffix=ll
    project program base "$target" "$suffix" || fail "$target rejected indexed String array"
    project graph-values graph-values "$target" "$suffix" || fail "$target rejected graph values"
    project display-only display-only "$target" "$suffix" || fail "$target read display text as authority"
    cmp -s "$WORK_DIR/base.$suffix" "$WORK_DIR/display-only.$suffix" || fail "$target display text changed artifact"
    for bad in bad-branch-use bad-bound-target bad-bound-edge bad-body-use \
        bad-collection-leaf bad-index-local bad-index-edge bad-concat-target \
        bad-literal-spine stale-collection bad-capacity-layout \
        bad-iteration-binding; do
        if project "$bad" "$bad" "$target" "$suffix"; then
            fail "$target accepted $bad"
        fi
        [[ ! -s "$WORK_DIR/$bad.$suffix" ]] || fail "$target published $bad"
        grep -Eq '(direct MIR|MIR-LOWER ERROR)' \
            "$WORK_DIR/$bad.$target.out" "$WORK_DIR/$bad.$target.err" || {
                cat "$WORK_DIR/$bad.$target.out" "$WORK_DIR/$bad.$target.err" >&2 || true
                fail "$target lost owned diagnostic for $bad"
            }
        ! grep -Fq 'direct MIR Option match' \
            "$WORK_DIR/$bad.$target.out" "$WORK_DIR/$bad.$target.err" || \
            fail "$target retried $bad as Option"
        ! grep -Fq 'direct MIR CFG single-node literal graph is invalid' \
            "$WORK_DIR/$bad.$target.out" "$WORK_DIR/$bad.$target.err" || \
            fail "$target retried $bad through the retired literal-bound path"
    done
done

require_text "$WORK_DIR/base.c" '((size_t)pgy_local_1) < pgy_indexed_collection.length'
require_text "$WORK_DIR/base.c" 'pgy_indexed_collection.data[pgy_local_1]'
require_text "$WORK_DIR/base.ll" 'icmp ult i64'
require_text "$WORK_DIR/base.ll" 'extractvalue { ptr, i64, i64, ptr } %pgy.array.indexed.3, 1'
require_text "$WORK_DIR/base.ll" 'getelementptr inbounds ptr'
reject_text "$WORK_DIR/base.ll" '%pgy.cond.1.capacity'

compile_and_run() {
    local stem="$1" expected="$2"
    "$CC" -std=c11 -Wall -Wextra -Werror "$WORK_DIR/$stem.c" \
        -o "$WORK_DIR/$stem-c.exe" >"$WORK_DIR/$stem-c.compile" 2>&1 || {
            cat "$WORK_DIR/$stem-c.compile" >&2; fail "$stem C did not compile";
        }
    "$CLANG" -x ir "$WORK_DIR/$stem.ll" -o "$WORK_DIR/$stem-llvm.exe" \
        >"$WORK_DIR/$stem-llvm.compile" 2>&1 || {
            cat "$WORK_DIR/$stem-llvm.compile" >&2; fail "$stem LLVM did not compile";
        }
    printf '%s\n' "$expected" >"$WORK_DIR/$stem.expected"
    for backend in c llvm; do
        "$WORK_DIR/$stem-$backend.exe" | tr -d '\r' >"$WORK_DIR/$stem-$backend.run"
        cmp -s "$WORK_DIR/$stem.expected" "$WORK_DIR/$stem-$backend.run" || \
            fail "$stem $backend output drift"
    done
}
compile_and_run base xyz
compile_and_run graph-values abbccc

echo "[$LABEL] graph-owned ArrayLength/parts[i] executes exact C/LLVM parity and all stale/text/capacity paths fail closed"
