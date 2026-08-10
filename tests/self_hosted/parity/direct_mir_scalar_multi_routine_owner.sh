#!/usr/bin/env bash
# General scalar programs are admitted from callable identities, not N-routine shapes.
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
source "$ROOT_DIR/tests/self_hosted/parity/emitted_c_runtime_header_owner.sh"
pgy_prepend_windows_runtime_paths

LABEL="self-host-direct-mir-scalar-multi-routine"
DRIVER="$(pgy_select_optional_exe_binary "${PGY_SELF_DRIVER_BIN:-$ROOT_DIR/bin/pgy-self-driver}")"
CC="${PGY_SELFHOST_CC:-gcc}"
CLANG="${PGY_SELFHOST_CLANG:-clang}"
WORK_REL=".tmp/self_hosted/direct_mir_scalar_multi_routine"
WORK_DIR="$ROOT_DIR/$WORK_REL"
SOURCE_REL="tests/self_hosted/fixtures/direct_mir_four_routine_scalar.pgy"
MIR_REL="$WORK_REL/program.mir.json"
MIR="$ROOT_DIR/$MIR_REL"
MUTATIONS="$ROOT_DIR/tests/self_hosted/parity/direct_mir_multi_routine_mutations.py"

ROUTE_OWNER="$ROOT_DIR/src/self_hosted/compiler/direct_mir_scalar_program_route_fact_owner.pgy"
INVENTORY_OWNER="$ROOT_DIR/src/self_hosted/compiler/direct_mir_scalar_program_callable_inventory_owner.pgy"
GRAPH_OWNER="$ROOT_DIR/src/self_hosted/compiler/direct_mir_scalar_cfg_program_graph_admission_owner.pgy"
PARTITION_OWNER="$ROOT_DIR/src/self_hosted/compiler/direct_mir_scalar_cfg_program_routine_partition_owner.pgy"

fail() { echo "[$LABEL] $*" >&2; exit 1; }
require_file() { [[ -f "$1" ]] || fail "missing file: ${1#"$ROOT_DIR/"}"; }

for owner in "$ROUTE_OWNER" "$INVENTORY_OWNER" "$GRAPH_OWNER" \
    "$PARTITION_OWNER" "$MUTATIONS"; do
    require_file "$owner"
done
pgy_require_runnable_binary_here "$LABEL" "$DRIVER" || exit 1
command -v "$CC" >/dev/null 2>&1 || fail "missing C compiler: $CC"
command -v "$CLANG" >/dev/null 2>&1 || fail "missing LLVM compiler: $CLANG"

grep -Fq 'routine_rows: Array<Int>' "$ROUTE_OWNER" ||
    fail "route does not carry admitted routine rows"
grep -Fq 'DirectMirScalarCfgProgramCallableInventoryFromAdmitted(' \
    "$GRAPH_OWNER" || fail "GraphPlan does not build one callable inventory"
grep -Fq 'DirectMirScalarCfgProgramCallableInventoryOrdinalBySyntaxId(' \
    "$INVENTORY_OWNER" || fail "callable inventory has no syntax-id join"
grep -Fq 'while ordinal < ArrayLength(route.routine_rows)' "$GRAPH_OWNER" ||
    fail "GraphPlan does not admit every routed routine"
for owner in "$ROUTE_OWNER" "$GRAPH_OWNER" "$PARTITION_OWNER"; do
    if grep -Eq 'routine_count[[:space:]]*==[[:space:]]*(3|4)|routine_count[[:space:]]*<=[[:space:]]*2' "$owner"; then
        fail "exact routine-count classifier returned: ${owner#"$ROOT_DIR/"}"
    fi
done

mkdir -p "$WORK_DIR"
rm -f "$WORK_DIR"/*
(cd "$ROOT_DIR" && "$DRIVER" --emit-mir-json-verified "$SOURCE_REL" -o "$MIR_REL") \
    >"$WORK_DIR/producer.out" 2>"$WORK_DIR/producer.err" || {
        cat "$WORK_DIR/producer.out" "$WORK_DIR/producer.err" >&2
        fail "MIR production failed"
    }
grep -Fq '"schema":"pgy.mir.v1"' "$MIR" || fail "producer emitted no MIR"

for backend in c llvm; do
    artifact_rel="$WORK_REL/program.$backend"
    artifact="$ROOT_DIR/$artifact_rel"
    bin="$WORK_DIR/program-$backend.exe"
    (cd "$ROOT_DIR" && "$DRIVER" "--mir-json-backend=$backend" \
        "$MIR_REL" -o "$artifact_rel") \
        >"$WORK_DIR/$backend.project.out" 2>"$WORK_DIR/$backend.project.err" || {
            cat "$WORK_DIR/$backend.project.out" "$WORK_DIR/$backend.project.err" >&2
            fail "$backend projection failed"
        }
    [[ -s "$artifact" ]] || fail "$backend projection emitted no artifact"
    if [[ "$backend" == c ]]; then
        command=("$CC" -x c -std=c11 "$artifact")
        if pgy_selfhost_emitted_c_uses_runtime_headers "$artifact"; then
            command+=("-I$ROOT_DIR/src" "-I$ROOT_DIR/src/runtime" -pthread)
        fi
        command+=(-lm -o "$bin")
        "${command[@]}" >"$WORK_DIR/$backend.compile.out" \
            2>"$WORK_DIR/$backend.compile.err" || fail "C artifact did not compile"
    else
        "$CLANG" -x ir "$artifact" -o "$bin" \
            >"$WORK_DIR/$backend.compile.out" 2>"$WORK_DIR/$backend.compile.err" ||
            fail "LLVM artifact did not compile"
    fi
    "$bin" | tr -d '\r' >"$WORK_DIR/$backend.run"
    printf '10\n' >"$WORK_DIR/expected.run"
    cmp -s "$WORK_DIR/expected.run" "$WORK_DIR/$backend.run" ||
        fail "$backend runtime output drifted"
done

for mutation in missing-call-target duplicate-routine-identity; do
    mutated_rel="$WORK_REL/$mutation.mir.json"
    mutated="$ROOT_DIR/$mutated_rel"
    python "$MUTATIONS" "$MIR" "$mutation" "$mutated"
    [[ -s "$mutated" ]] || fail "could not create $mutation"
    for backend in c llvm; do
        output_rel="$WORK_REL/$mutation.$backend"
        output="$ROOT_DIR/$output_rel"
        rm -f "$output"
        if (cd "$ROOT_DIR" && "$DRIVER" "--mir-json-backend=$backend" \
            "$mutated_rel" -o "$output_rel") >"$WORK_DIR/$mutation.$backend.out" \
            2>"$WORK_DIR/$mutation.$backend.err"; then
            fail "$backend accepted $mutation"
        fi
        [[ ! -e "$output" ]] || fail "$backend published artifact for $mutation"
    done
done

echo "[$LABEL] four-routine C/LLVM parity + callable identity negatives: PASS"
