#!/usr/bin/env bash
# Nested range continue/fallthrough transfers stay with the innermost receipt.
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
pgy_prepend_windows_runtime_paths
LABEL="self-host-one-mir-nested-iteration-continue-scope"
DRIVER="${PGY_SELF_DRIVER_BIN:-$ROOT_DIR/bin/pgy-self-driver}"
CC="${CC:-gcc}"; CLANG="${PGY_SELFHOST_CLANG:-clang}"
PYTHON_BIN="${PYTHON_BIN:-$(command -v python3 || command -v python || true)}"
WORK_REL=".tmp/self_hosted/one_mir_nested_iteration_continue_scope"
WORK_DIR="$ROOT_DIR/$WORK_REL"
SOURCE_REL="src/self_hosted/mir_lower/fixture/nested_iteration_continue_shadow.pgy"
fail() { echo "[$LABEL] $*" >&2; exit 1; }
require_text() { grep -Fq -- "$2" "$1" || fail "missing ${1#"$ROOT_DIR/"}: $2"; }
DRIVER="$(pgy_select_optional_exe_binary "$DRIVER")"
[[ -x "$DRIVER" && -n "$PYTHON_BIN" ]] || fail "driver/python unavailable"
command -v "$CC" >/dev/null && command -v "$CLANG" >/dev/null || fail "toolchain unavailable"

TRANSFER_OWNER="$ROOT_DIR/src/self_hosted/compiler/direct_mir_scalar_cfg_range_transfer_admission_owner.pgy"
SCOPE_OWNER="$ROOT_DIR/src/self_hosted/compiler/direct_mir_scalar_cfg_wire_range_scope_admission_owner.pgy"
require_text "$TRANSFER_OWNER" 'DirectMirScalarCfgRangeTransfersReady('
require_text "$TRANSFER_OWNER" 'DirectMirScalarCfgRangeActiveAtBlock('
require_text "$SCOPE_OWNER" 'DirectMirScalarCfgRangeOwnsDirectUseBlock('

mkdir -p "$WORK_DIR"
rm -f "$WORK_DIR"/*
(cd "$ROOT_DIR" && "$DRIVER" --emit-mir-json-verified "$SOURCE_REL" \
    -o "$WORK_REL/program.json") || fail "producer rejected nested continue source"
"$PYTHON_BIN" - "$WORK_DIR/program.json" "$WORK_DIR" <<'PY'
import copy, json, pathlib, sys
doc=json.loads(pathlib.Path(sys.argv[1]).read_text(encoding="utf-8")); out=pathlib.Path(sys.argv[2])
r=doc["routines"][0]; rows=[x for b in r["blocks"] for x in b["instructions"]]
assert r["source_locals"] == [{"name":"i","type":"Int"}]*3
assert [x["iteration_syntax_id"] for x in r["iteration_type_facts"]] == [7,9]
assert [x["loop_syntax_id"] for x in r["loop_flow_summaries"]] == [7,9]
assert [(b.get("succ_true"),b.get("succ_false")) for b in r["blocks"]] == [
    (1,None),(2,8),(3,None),(4,7),(5,6),(3,None),(3,None),(1,None),(None,None)]
assert [x["kind"] for x in rows] == [
    "def","loop-init","branch","loop-init","branch","branch","branch","stmt","stmt","stmt"]
assert rows[0]["local_ref"] == "declaration:6:0"
assert [rows[x]["local_ref"] for x in (1,2)] == ["iteration:7:0"]*2
assert [rows[x]["local_ref"] for x in (3,4)] == ["iteration:9:0"]*2
assert rows[5]["expr0_local_refs"] == [{"node":0,"ref":"iteration:9:0"}]
assert rows[6]["source_type"] == "AST_CONTINUE" and rows[6]["uses"] == []
assert rows[7]["expr0_local_refs"] == [{"node":0,"ref":"iteration:9:0"}]
assert rows[8]["expr0_local_refs"] == [{"node":0,"ref":"iteration:7:0"}]
assert rows[9]["uses"] == ["i.1"] and rows[9]["expr0_local_refs"] == []
assert all(x["kind"] != "phi" for x in rows)
def emit(name,fn):
    x=copy.deepcopy(doc); rr=x["routines"][0]; ins=[y for b in rr["blocks"] for y in b["instructions"]]
    fn(rr,ins); (out/f"{name}.json").write_text(json.dumps(x,separators=(",",":")),encoding="utf-8")
emit("type-order",lambda r,x:r["iteration_type_facts"].reverse())
emit("flow-order",lambda r,x:r["loop_flow_summaries"].reverse())
emit("condition-outer-ref",lambda r,x:x[5]["expr0_local_refs"][0].__setitem__("ref","iteration:7:0"))
def outer_ssa(r,x): x[5]["expr0_local_refs"]=[]; x[5]["uses"]=["i.1"]
emit("condition-outer-ssa",outer_ssa)
emit("continue-outer-header",lambda r,x:r["blocks"][5].__setitem__("succ_true",1))
emit("fallthrough-outer-header",lambda r,x:r["blocks"][6].__setitem__("succ_true",1))
PY

project() { (cd "$ROOT_DIR" && "$DRIVER" "--mir-json-backend=$2" \
    "$WORK_REL/$1.json" -o "$WORK_REL/$1.$3"); }
project program c c; project program llvm ll
for p in type-order flow-order; do
    project "$p" c c; project "$p" llvm ll
    cmp -s "$WORK_DIR/program.c" "$WORK_DIR/$p.c" || fail "$p changed C"
    cmp -s "$WORK_DIR/program.ll" "$WORK_DIR/$p.ll" || fail "$p changed LLVM"
done
[[ "$(grep -Fc 'pgy_local_2 = pgy_local_2 + 1;' "$WORK_DIR/program.c")" == 2 ]] ||
    fail "inner continue/fallthrough increments drifted"
[[ "$(grep -Fc 'pgy_local_1 = pgy_local_1 + 1;' "$WORK_DIR/program.c")" == 1 ]] ||
    fail "outer range increment drifted"
"$CC" -std=c11 -Wall -Wextra -Werror "$WORK_DIR/program.c" -o "$WORK_DIR/c.exe"
"$CLANG" -x ir "$WORK_DIR/program.ll" -o "$WORK_DIR/llvm.exe" >/dev/null 2>&1
"$WORK_DIR/c.exe" | tr -d '\r' >"$WORK_DIR/c.run"
"$WORK_DIR/llvm.exe" | tr -d '\r' >"$WORK_DIR/llvm.run"
printf '0\n2\n0\n0\n2\n1\n40\n' >"$WORK_DIR/expected.run"
cmp -s "$WORK_DIR/expected.run" "$WORK_DIR/c.run" &&
    cmp -s "$WORK_DIR/c.run" "$WORK_DIR/llvm.run" || fail "execution mismatch"

for mutation in condition-outer-ref condition-outer-ssa; do
    for target in c llvm; do
        artifact="$WORK_DIR/$mutation-$target.artifact"
        if (cd "$ROOT_DIR" && "$DRIVER" "--mir-json-backend=$target" \
            "$WORK_REL/$mutation.json" -o "$WORK_REL/$mutation-$target.artifact") \
            >"$WORK_DIR/x.out" 2>"$WORK_DIR/x.err"; then fail "$target accepted $mutation"; fi
        [[ ! -e "$artifact" ]] || fail "$target published $mutation"
        grep -Fq 'direct MIR scalar CFG range LocalRef binding is invalid' \
            "$WORK_DIR/x.out" "$WORK_DIR/x.err" || fail "$mutation lost diagnostic"
    done
done
for mutation in continue-outer-header fallthrough-outer-header; do
    for target in c llvm; do
        artifact="$WORK_DIR/$mutation-$target.artifact"
        if (cd "$ROOT_DIR" && "$DRIVER" "--mir-json-backend=$target" \
            "$WORK_REL/$mutation.json" -o "$WORK_REL/$mutation-$target.artifact") \
            >"$WORK_DIR/x.out" 2>"$WORK_DIR/x.err"; then fail "$target accepted $mutation"; fi
        [[ ! -e "$artifact" ]] || fail "$target published $mutation"
        grep -Fq 'direct MIR scalar CFG range transfer is invalid' \
            "$WORK_DIR/x.out" "$WORK_DIR/x.err" || fail "$mutation lost diagnostic"
    done
done
echo "[$LABEL] innermost range owns continue and fallthrough transfers"
