#!/usr/bin/env bash
# One plan owns ArrayLength, dynamic/literal reads, and bounded String ArraySet.
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
pgy_prepend_windows_runtime_paths
LABEL="self-host-one-mir-string-array-mutation"
DRIVER="${PGY_SELF_DRIVER_BIN:-$ROOT_DIR/bin/pgy-self-driver}"
CC="${CC:-gcc}"
CLANG="${PGY_SELFHOST_CLANG:-clang}"
PYTHON_BIN="${PYTHON_BIN:-$(command -v python3 || command -v python || true)}"
WORK_REL=".tmp/self_hosted/one_mir_string_array_mutation"
WORK_DIR="$ROOT_DIR/$WORK_REL"
SOURCE_REL="src/self_hosted/codegen/fixture/str_array.pgy"

fail() { echo "[$LABEL] $*" >&2; exit 1; }
require_text() { grep -Fq -- "$2" "$1" || fail "missing ${1#"$ROOT_DIR/"}: $2"; }
reject_text() { ! grep -Fq -- "$2" "$1" || fail "forbidden ${1#"$ROOT_DIR/"}: $2"; }

DRIVER="$(pgy_select_optional_exe_binary "$DRIVER")"
[[ -x "$DRIVER" ]] || fail "installed self-host driver is unavailable"
command -v "$CC" >/dev/null || fail "C compiler is unavailable"
command -v "$CLANG" >/dev/null || fail "clang is unavailable"
[[ -n "$PYTHON_BIN" ]] || fail "python is required for structured falsifiers"

PLAN="$ROOT_DIR/src/self_hosted/compiler/direct_mir_scalar_cfg_string_array_plan_fact_owner.pgy"
ADMISSION="$ROOT_DIR/src/self_hosted/compiler/direct_mir_scalar_cfg_string_array_plan_admission_owner.pgy"
DOMINANCE="$ROOT_DIR/src/self_hosted/compiler/direct_mir_scalar_cfg_string_array_plan_dominance_owner.pgy"
C_OWNER="$ROOT_DIR/src/self_hosted/compiler/direct_mir_scalar_cfg_string_array_c_emission_owner.pgy"
LLVM_OWNER="$ROOT_DIR/src/self_hosted/compiler/direct_mir_scalar_cfg_string_array_llvm_emission_owner.pgy"
require_text "$PLAN" 'DirectMirScalarCfgStringArrayAccessSetKind()'
require_text "$ADMISSION" 'DirectMirScalarCfgStringArrayWhileLengthGraphFactFrom(graph)'
require_text "$ADMISSION" 'DirectMirScalarCfgStringArrayAccessSetKind()'
require_text "$DOMINANCE" 'MirRoutineBlockDominates('
require_text "$DOMINANCE" 'predecessor_count == 1'
require_text "$ROOT_DIR/src/self_hosted/compiler/direct_mir_scalar_cfg_string_array_graph_readiness_owner.pgy" \
    'fact.index.value_row >= ArrayLength(plan.value_types)'
require_text "$C_OWNER" '.length'
require_text "$LLVM_OWNER" 'icmp ult i64'
for owner in "$C_OWNER" "$LLVM_OWNER"; do
    reject_text "$owner" 'source_json'
    reject_text "$owner" 'JsonObjectFactTable'
    reject_text "$owner" 'MirExpressionGraphSequence'
    reject_text "$owner" 'str_array.pgy'
done
reject_text "$C_OWNER" '.capacity)'
reject_text "$LLVM_OWNER" '.capacity = extractvalue'

mkdir -p "$WORK_DIR"
rm -f "$WORK_DIR"/*
(cd "$ROOT_DIR" && "$DRIVER" --emit-mir-json-verified "$SOURCE_REL" \
    -o "$WORK_REL/program.json") >"$WORK_DIR/producer.out" \
    2>"$WORK_DIR/producer.err" || {
        cat "$WORK_DIR/producer.out" "$WORK_DIR/producer.err" >&2 || true
        fail "installed producer rejected the active source"
    }
grep -Fq '"uses":["i.3","names.1"]' "$WORK_DIR/program.json" || fail "while use identity is absent"
grep -Fq '"arg0":"ArraySet"' "$WORK_DIR/program.json" || fail "ArraySet is absent"
"$PYTHON_BIN" "$ROOT_DIR/tests/self_hosted/parity/one_mir_string_array_mutations.py" \
    "$WORK_DIR/program.json" "$WORK_DIR"

project() {
    local input="$1" stem="$2" target="$3" suffix="$4"
    (cd "$ROOT_DIR" && "$DRIVER" "--mir-json-backend=$target" \
        "$WORK_REL/$input.json" -o "$WORK_REL/$stem.$suffix") \
        >"$WORK_DIR/$stem.$target.out" 2>"$WORK_DIR/$stem.$target.err"
}

for target in c llvm; do
    suffix=c; [[ "$target" == llvm ]] && suffix=ll
    project program base "$target" "$suffix" || fail "$target rejected String-array plan"
    project display-only display-only "$target" "$suffix" || fail "$target read display text"
    project graph-values graph-values "$target" "$suffix" || fail "$target rejected graph values"
    project empty-set-value empty-set-value "$target" "$suffix" || fail "$target lost empty String value"
    project graph-set-value graph-set-value "$target" "$suffix" || fail "$target rejected graph set value"
    project set-log-reordered set-log-reordered "$target" "$suffix" || fail "$target lost operation order"
    cmp -s "$WORK_DIR/base.$suffix" "$WORK_DIR/display-only.$suffix" || fail "$target display text changed artifact"
    for bad in bad-branch-index-use bad-branch-collection-use bad-length-target \
        bad-length-edge bad-log-index-use bad-log-index-edge bad-while-init \
        bad-while-step bad-set-receiver \
        bad-set-index-oob bad-set-index-negative bad-set-value-kind \
        bad-post-read-oob bad-guard-edge bad-guard-bypass bad-array-layout \
        stale-collection; do
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

require_text "$WORK_DIR/base.c" 'pgy_indexed_collection_0.length'
require_text "$WORK_DIR/base.c" 'pgy_indexed_collection_0.data[1] = "BOB"'
require_text "$WORK_DIR/base.ll" 'icmp ult i64'
require_text "$WORK_DIR/base.ll" 'store ptr @.pgy.scalar.cfg.indexed.set.1'

compile_and_run() {
    local stem="$1" expected="$2"
    "$CC" -std=c11 -Wall -Wextra -Werror "$WORK_DIR/$stem.c" \
        -o "$WORK_DIR/$stem-c.exe" >"$WORK_DIR/$stem-c.compile" 2>&1 || fail "$stem C did not compile"
    "$CLANG" -x ir "$WORK_DIR/$stem.ll" -o "$WORK_DIR/$stem-llvm.exe" \
        >"$WORK_DIR/$stem-llvm.compile" 2>&1 || fail "$stem LLVM did not compile"
    printf '%b' "$expected" >"$WORK_DIR/$stem.expected"
    for backend in c llvm; do
        "$WORK_DIR/$stem-$backend.exe" | tr -d '\r' >"$WORK_DIR/$stem-$backend.run"
        cmp -s "$WORK_DIR/$stem.expected" "$WORK_DIR/$stem-$backend.run" || fail "$stem $backend output drift"
    done
}
compile_and_run base 'alice\nbob\ncarol\nBOB\n'
compile_and_run graph-values 'ant\nbee\ncat\nBOB\n'
compile_and_run empty-set-value 'alice\nbob\ncarol\n\n'
compile_and_run graph-set-value 'alice\nbob\ncarol\nBobby\n'
compile_and_run set-log-reordered 'alice\nbob\ncarol\nbob\n'

echo "[$LABEL] one receipt executes exact C/LLVM while-read-static-set parity and all stale paths fail closed"
