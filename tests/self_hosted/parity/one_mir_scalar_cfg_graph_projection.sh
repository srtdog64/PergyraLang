#!/usr/bin/env bash
# One admitted scalar CFG graph drives both backends without a topology case table.
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
pgy_prepend_windows_runtime_paths

LABEL="self-host-one-mir-scalar-cfg-graph"
DRIVER="${PGY_SELF_DRIVER_BIN:-$ROOT_DIR/bin/pgy-self-driver}"
CC="${CC:-gcc}"
CLANG="${PGY_SELFHOST_CLANG:-clang}"
PYTHON_BIN="${PYTHON_BIN:-}"
WORK_REL=".tmp/self_hosted/one_mir_scalar_cfg_graph"
WORK_DIR="$ROOT_DIR/$WORK_REL"
SOURCE_REL="src/self_hosted/mir_lower/fixture/nested_if_in_loop.pgy"
MIR_REL="$WORK_REL/program.json"
MIR="$ROOT_DIR/$MIR_REL"

fail() { echo "[$LABEL] $*" >&2; exit 1; }
require_text() { grep -Fq -- "$2" "$1" || fail "missing ${1#"$ROOT_DIR/"}: $2"; }
reject_text() { ! grep -Fq -- "$2" "$1" || fail "forbidden ${1#"$ROOT_DIR/"}: $2"; }

DRIVER="$(pgy_select_optional_exe_binary "$DRIVER")"
[[ -x "$DRIVER" ]] || fail "installed self-host driver is unavailable"
command -v "$CC" >/dev/null || fail "C compiler is unavailable"
command -v "$CLANG" >/dev/null || fail "clang is unavailable"
if [[ -z "$PYTHON_BIN" ]]; then
    PYTHON_BIN="$(command -v python3 || command -v python || true)"
fi
[[ -n "$PYTHON_BIN" ]] || fail "python is required for structured falsifiers"

ADMISSION="$ROOT_DIR/src/self_hosted/compiler/direct_mir_scalar_cfg_graph_admission_owner.pgy"
GRAPH_PHI="$ROOT_DIR/src/self_hosted/compiler/direct_mir_scalar_cfg_phi_operation_admission_owner.pgy"
BRANCH_ADMISSION="$ROOT_DIR/src/self_hosted/compiler/direct_mir_scalar_cfg_branch_admission_owner.pgy"
PHI_OWNER="$ROOT_DIR/src/self_hosted/mir_lower/phi_predecessor_binding_fact_owner.pgy"
C_OWNER="$ROOT_DIR/src/self_hosted/compiler/direct_mir_scalar_cfg_c_emission_owner.pgy"
LLVM_OWNER="$ROOT_DIR/src/self_hosted/compiler/direct_mir_scalar_cfg_llvm_emission_owner.pgy"
ROUTER="$ROOT_DIR/src/self_hosted/compiler/direct_mir_backend_projection_owner.pgy"
require_text "$ROUTER" 'DirectMirScalarCfgGraphRouteClaimed(admitted)'
require_text "$ADMISSION" 'DirectMirScalarCfgPhiOperationFromOwners('
require_text "$GRAPH_PHI" 'MirPhiPredecessorBindingFactFromOwners('
require_text "$BRANCH_ADMISSION" 'MirRoutineBlockDominates('
require_text "$PHI_OWNER" 'predecessor_blocks'
require_text "$C_OWNER" 'DirectMirScalarCfgGraphPlanReady(plan)'
require_text "$LLVM_OWNER" 'DirectMirScalarCfgGraphPlanReady(plan)'
for owner in "$ADMISSION" "$GRAPH_PHI" "$PHI_OWNER" "$C_OWNER" "$LLVM_OWNER"; do
    reject_text "$owner" 'nested_if_in_loop.pgy'
    reject_text "$owner" 'block_count == 8'
done
reject_text "$C_OWNER" '"expr0"'
reject_text "$LLVM_OWNER" '"expr0"'
reject_text "$PHI_OWNER" 'uses[predecessor'

mkdir -p "$WORK_DIR"
rm -f "$WORK_DIR"/*
(cd "$ROOT_DIR" && "$DRIVER" --emit-mir-json-verified \
    "$SOURCE_REL" -o "$MIR_REL") >"$WORK_DIR/producer.out" \
    2>"$WORK_DIR/producer.err" || {
        cat "$WORK_DIR/producer.out" "$WORK_DIR/producer.err" >&2 || true
        fail "installed producer rejected the active source"
    }
grep -Fq '"schema":"pgy.mir.v1"' "$MIR" || fail "producer emitted no MIR"
grep -Fq '"id":1,"reachable":true' "$MIR" || fail "active CFG header is absent"
grep -Fq '"succ_true":2}' "$MIR" || fail "constant-true loop exit was not pruned"
! grep -Fq '"succ_true":2,"succ_false":7' "$MIR" ||
    fail "infeasible loop exit reintroduced a non-dominating use path"

project() {
    local target="$1" suffix="$2" artifact
    artifact="$WORK_DIR/program.$suffix"
    (cd "$ROOT_DIR" && "$DRIVER" "--mir-json-backend=$target" \
        "$MIR_REL" -o "$WORK_REL/program.$suffix") \
        >"$WORK_DIR/$target.out" 2>"$WORK_DIR/$target.err" || {
            cat "$WORK_DIR/$target.out" "$WORK_DIR/$target.err" >&2 || true
            fail "$target rejected the admitted scalar CFG"
        }
    [[ -s "$artifact" ]] || fail "$target emitted no artifact"
}
project c c
project llvm ll

"$CC" -std=c11 -Wall -Wextra -Werror "$WORK_DIR/program.c" \
    -o "$WORK_DIR/program-c.exe" >"$WORK_DIR/c.compile" 2>&1 || {
        cat "$WORK_DIR/c.compile" >&2; fail "C projection did not compile";
    }
"$CLANG" -x ir "$WORK_DIR/program.ll" -o "$WORK_DIR/program-llvm.exe" \
    >"$WORK_DIR/llvm.compile" 2>&1 || {
        cat "$WORK_DIR/llvm.compile" >&2; fail "LLVM projection did not compile";
    }
"$WORK_DIR/program-c.exe" | tr -d '\r' >"$WORK_DIR/c.run"
"$WORK_DIR/program-llvm.exe" | tr -d '\r' >"$WORK_DIR/llvm.run"
printf '1\n1\n' >"$WORK_DIR/expected.run"
cmp -s "$WORK_DIR/expected.run" "$WORK_DIR/c.run" ||
    fail "C projection did not execute exact 1/1"
cmp -s "$WORK_DIR/expected.run" "$WORK_DIR/llvm.run" ||
    fail "LLVM projection did not execute exact 1/1"
cmp -s "$WORK_DIR/c.run" "$WORK_DIR/llvm.run" ||
    fail "C and LLVM projections diverged"

"$PYTHON_BIN" - "$MIR" "$WORK_DIR" <<'PY'
import copy
import json
import pathlib
import sys

source = pathlib.Path(sys.argv[1])
target = pathlib.Path(sys.argv[2])
document = json.loads(source.read_text(encoding="utf-8"))

def write(name, mutate):
    changed = copy.deepcopy(document)
    mutate(changed["routines"][0]["blocks"])
    (target / f"{name}.json").write_text(
        json.dumps(changed, separators=(",", ":")), encoding="utf-8"
    )

write("bad-dominance", lambda blocks: blocks[1].__setitem__("succ_false", 7))
write("bad-phi", lambda blocks: blocks[4]["instructions"][0].__setitem__(
    "uses", ["largest.7", "largest.7"]
))
write("bad-break", lambda blocks: blocks[5].__setitem__("succ_true", 1))
write("stale-use", lambda blocks: blocks[7]["instructions"][0].__setitem__(
    "uses", ["largest.1"]
))
PY

expect_rejected() {
    local name="$1" diagnostic="$2" target artifact
    for target in c llvm; do
        artifact="$WORK_DIR/$name-$target.artifact"
        rm -f "$artifact"
        if (cd "$ROOT_DIR" && "$DRIVER" "--mir-json-backend=$target" \
            "$WORK_REL/$name.json" -o "$WORK_REL/$name-$target.artifact") \
            >"$WORK_DIR/$name-$target.out" 2>"$WORK_DIR/$name-$target.err"; then
            fail "$target accepted $name"
        fi
        [[ ! -e "$artifact" ]] || fail "$target published before rejecting $name"
        grep -Fq "$diagnostic" "$WORK_DIR/$name-$target.out" \
            "$WORK_DIR/$name-$target.err" || {
                cat "$WORK_DIR/$name-$target.out" \
                    "$WORK_DIR/$name-$target.err" >&2 || true
                fail "$target lost the $name diagnostic"
            }
    done
}
expect_rejected bad-dominance 'direct MIR scalar CFG Log fact is invalid'
expect_rejected bad-phi 'direct MIR scalar CFG predecessor/phi binding is invalid'
expect_rejected bad-break 'direct MIR scalar CFG break edge is invalid'
expect_rejected stale-use 'direct MIR scalar CFG Log fact is invalid'

echo "[$LABEL] one general scalar CFG plan owns exact C/LLVM execution and negatives"
