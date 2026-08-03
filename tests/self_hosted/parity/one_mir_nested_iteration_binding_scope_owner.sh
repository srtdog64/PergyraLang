#!/usr/bin/env bash
# Canonical receipt-set identity preserves nested same-spelling range binders.
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
pgy_prepend_windows_runtime_paths
LABEL="self-host-one-mir-nested-iteration-binding-scope"
DRIVER="${PGY_SELF_DRIVER_BIN:-$ROOT_DIR/bin/pgy-self-driver}"
CC="${CC:-gcc}"; CLANG="${PGY_SELFHOST_CLANG:-clang}"
PYTHON_BIN="${PYTHON_BIN:-$(command -v python3 || command -v python || true)}"
WORK_REL=".tmp/self_hosted/one_mir_nested_iteration_binding_scope"
WORK_DIR="$ROOT_DIR/$WORK_REL"
SOURCE_REL="src/self_hosted/mir_lower/fixture/nested_iteration_binding_shadow.pgy"
fail() { echo "[$LABEL] $*" >&2; exit 1; }
require_text() { grep -Fq -- "$2" "$1" || fail "missing ${1#"$ROOT_DIR/"}: $2"; }
reject_text() { ! grep -Fq -- "$2" "$1" || fail "forbidden ${1#"$ROOT_DIR/"}: $2"; }
DRIVER="$(pgy_select_optional_exe_binary "$DRIVER")"
[[ -x "$DRIVER" && -n "$PYTHON_BIN" ]] || fail "driver/python unavailable"
command -v "$CC" >/dev/null && command -v "$CLANG" >/dev/null || fail "toolchain unavailable"

SET_OWNER="$ROOT_DIR/src/self_hosted/compiler/direct_mir_scalar_cfg_range_set_owner.pgy"
GRAPH_OWNER="$ROOT_DIR/src/self_hosted/compiler/direct_mir_scalar_cfg_graph_fact_owner.pgy"
LEAF_OWNER="$ROOT_DIR/src/self_hosted/compiler/direct_mir_scalar_cfg_leaf_operand_owner.pgy"
require_text "$SET_OWNER" 'struct DirectMirScalarCfgRangeFactSet {'
require_text "$SET_OWNER" 'DirectMirScalarCfgRangeFactAtLoopSyntaxId('
require_text "$GRAPH_OWNER" 'let range_iterations: DirectMirScalarCfgRangeFactSet;'
require_text "$LEAF_OWNER" 'DirectMirScalarCfgWireExprRefAt('
for owner in "$ROOT_DIR"/src/self_hosted/compiler/direct_mir_scalar_cfg_*.pgy; do
    reject_text "$owner" 'let range_iteration: DirectMirScalarCfgRangeIterationFact;'
    reject_text "$owner" 'range_local_row'
    reject_text "$owner" 'DirectMirScalarCfgRangeLocalName('
done

mkdir -p "$WORK_DIR"
rm -f "$WORK_DIR"/*
(cd "$ROOT_DIR" && "$DRIVER" --emit-mir-json-verified "$SOURCE_REL" \
    -o "$WORK_REL/program.json") || fail "producer rejected nested source"
"$PYTHON_BIN" - "$WORK_DIR/program.json" "$WORK_DIR" <<'PY'
import copy, json, pathlib, sys
doc = json.loads(pathlib.Path(sys.argv[1]).read_text(encoding="utf-8"))
out = pathlib.Path(sys.argv[2]); r = doc["routines"][0]
assert r["source_locals"] == [{"name":"i","type":"Int"}]*3
assert [x["iteration_syntax_id"] for x in r["iteration_type_facts"]] == [7,9]
assert [x["loop_syntax_id"] for x in r["loop_flow_summaries"]] == [7,9]
rows = [x for b in r["blocks"] for x in b["instructions"]]
assert [(b.get("succ_true"), b.get("succ_false")) for b in r["blocks"]] == [
    (1,None),(2,6),(3,None),(4,5),(3,None),(1,None),(None,None)]
assert [x["kind"] for x in rows] == ["def","loop-init","branch","loop-init","branch","stmt","stmt","stmt"]
assert rows[0]["local_ref"] == "declaration:6:0"
assert [rows[x]["local_ref"] for x in (1,2,3,4)] == ["iteration:7:0","iteration:7:0","iteration:9:0","iteration:9:0"]
assert rows[5]["expr0_local_refs"] == [{"node":0,"ref":"iteration:9:0"}]
assert rows[6]["expr0_local_refs"] == [{"node":0,"ref":"iteration:7:0"}]
assert rows[7]["uses"] == ["i.1"] and rows[7]["expr0_local_refs"] == []
def emit(name, fn):
    x=copy.deepcopy(doc); rr=x["routines"][0]; ins=[y for b in rr["blocks"] for y in b["instructions"]]
    fn(rr,ins); (out/f"{name}.json").write_text(json.dumps(x,separators=(",",":")),encoding="utf-8")
emit("type-order",lambda r,x:r["iteration_type_facts"].reverse())
emit("flow-order",lambda r,x:r["loop_flow_summaries"].reverse())
emit("inner-init-ref",lambda r,x:x[3].__setitem__("local_ref","iteration:7:0"))
emit("inner-branch-ref",lambda r,x:x[4].__setitem__("local_ref","iteration:7:0"))
emit("inner-body-ref",lambda r,x:x[5]["expr0_local_refs"][0].__setitem__("ref","iteration:7:0"))
emit("escaped-inner-ref",lambda r,x:x[6]["expr0_local_refs"][0].__setitem__("ref","iteration:9:0"))
def final_direct(r,x):
    x[7]["uses"]=[]; x[7]["expr0_local_refs"]=[{"node":0,"ref":"iteration:7:0"}]
emit("final-direct-ref",final_direct)
def missing_type(r,x):
    r["iteration_type_facts"].pop(); r["iteration_type_fact_count"]=1
emit("missing-type",missing_type)
emit("inner-latch-target",lambda r,x:r["blocks"][4].__setitem__("succ_true",1))
emit("outer-latch-target",lambda r,x:r["blocks"][5].__setitem__("succ_true",3))
PY

project() { (cd "$ROOT_DIR" && "$DRIVER" "--mir-json-backend=$2" \
    "$WORK_REL/$1.json" -o "$WORK_REL/$1.$3"); }
project program c c; project program llvm ll
for p in type-order flow-order; do
    project "$p" c c; project "$p" llvm ll
    cmp -s "$WORK_DIR/program.c" "$WORK_DIR/$p.c" || fail "$p changed C"
    cmp -s "$WORK_DIR/program.ll" "$WORK_DIR/$p.ll" || fail "$p changed LLVM"
done
"$CC" -std=c11 -Wall -Wextra -Werror "$WORK_DIR/program.c" -o "$WORK_DIR/c.exe"
"$CLANG" -x ir "$WORK_DIR/program.ll" -o "$WORK_DIR/llvm.exe" >/dev/null 2>&1
"$WORK_DIR/c.exe" | tr -d '\r' >"$WORK_DIR/c.run"
"$WORK_DIR/llvm.exe" | tr -d '\r' >"$WORK_DIR/llvm.run"
printf '0\n1\n0\n0\n1\n1\n40\n' >"$WORK_DIR/expected.run"
cmp -s "$WORK_DIR/expected.run" "$WORK_DIR/c.run" && cmp -s "$WORK_DIR/c.run" "$WORK_DIR/llvm.run" || fail "execution mismatch"

for mutation in inner-init-ref inner-branch-ref missing-type inner-latch-target outer-latch-target; do
    diagnostic='direct MIR scalar CFG range iteration facts are invalid'
    if [[ "$mutation" == missing-type ]]; then
        diagnostic='direct MIR scalar CFG range inventory is invalid'
    elif [[ "$mutation" == inner-latch-target ||
            "$mutation" == outer-latch-target ]]; then
        diagnostic='direct MIR scalar CFG range instruction/CFG join is invalid'
    fi
    for target in c llvm; do
        artifact="$WORK_DIR/$mutation-$target.artifact"
        if (cd "$ROOT_DIR" && "$DRIVER" "--mir-json-backend=$target" \
            "$WORK_REL/$mutation.json" -o "$WORK_REL/$mutation-$target.artifact") \
            >"$WORK_DIR/x.out" 2>"$WORK_DIR/x.err"; then fail "$target accepted $mutation"; fi
        [[ ! -e "$artifact" ]] || fail "$target published $mutation"
        grep -Fq "$diagnostic" "$WORK_DIR/x.out" "$WORK_DIR/x.err" || fail "$mutation lost diagnostic"
    done
done
for mutation in inner-body-ref escaped-inner-ref final-direct-ref; do
    for target in c llvm; do
        artifact="$WORK_DIR/$mutation-$target.artifact"
        if (cd "$ROOT_DIR" && "$DRIVER" "--mir-json-backend=$target" \
            "$WORK_REL/$mutation.json" -o "$WORK_REL/$mutation-$target.artifact") \
            >"$WORK_DIR/x.out" 2>"$WORK_DIR/x.err"; then fail "$target accepted $mutation"; fi
        [[ ! -e "$artifact" ]] || fail "$target published $mutation"
        grep -Fq 'direct MIR scalar CFG range LocalRef binding is invalid' "$WORK_DIR/x.out" "$WORK_DIR/x.err" || fail "$mutation lost diagnostic"
    done
done
echo "[$LABEL] canonical receipt set preserves nested lexical range identity"
