#!/usr/bin/env bash
# One returned Array<Int> producer receipt feeds nested/sequential foreach CFG.
# Registry forbidden-fallback inventory exercised below: call_text_reparse,
# repeated_return_collection_materialization, hoisted_call_abi_ignored,
# hoisted_call_as_local_literal.
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
pgy_prepend_windows_runtime_paths
LABEL="self-host-one-mir-returned-array-foreach"
DRIVER="${PGY_SELF_DRIVER_BIN:-$ROOT_DIR/bin/pgy-self-driver}"
CC="${CC:-gcc}"
CLANG="${PGY_SELFHOST_CLANG:-clang}"
PYTHON_BIN="${PYTHON_BIN:-$(command -v python3 || command -v python || true)}"
WORK_REL=".tmp/self_hosted/one_mir_returned_array_foreach"
WORK_DIR="$ROOT_DIR/$WORK_REL"
SOURCE_REL="src/self_hosted/mir_lower/fixture/for_each_call.pgy"
fail() { echo "[$LABEL] $*" >&2; exit 1; }
require_text() { grep -Fq -- "$2" "$1" || fail "missing ${1#"$ROOT_DIR/"}: $2"; }
reject_text() { ! grep -Fq -- "$2" "$1" || fail "forbidden ${1#"$ROOT_DIR/"}: $2"; }

DRIVER="$(pgy_select_optional_exe_binary "$DRIVER")"
[[ -x "$DRIVER" ]] || fail "installed self-host driver is unavailable"
command -v "$CC" >/dev/null || fail "C compiler is unavailable"
command -v "$CLANG" >/dev/null || fail "clang is unavailable"
[[ -n "$PYTHON_BIN" ]] || fail "python is required for structured falsifiers"
PROGRAM_OWNER="$ROOT_DIR/src/self_hosted/compiler/direct_mir_returned_array_foreach_program_owner.pgy"
PRODUCER_OWNER="$ROOT_DIR/src/self_hosted/compiler/direct_mir_array_int_producer_fact_owner.pgy"
COLLECTION_OWNER="$ROOT_DIR/src/self_hosted/compiler/direct_mir_scalar_cfg_foreach_collection_owner.pgy"
SET_OWNER="$ROOT_DIR/src/self_hosted/compiler/direct_mir_scalar_cfg_foreach_set_owner.pgy"
ROUTER="$ROOT_DIR/src/self_hosted/compiler/direct_mir_multi_routine_projection_owner.pgy"
SHARED_ROUTE="$ROOT_DIR/src/self_hosted/compiler/direct_mir_returned_array_program_route_owner.pgy"
require_text "$ROUTER" 'DirectMirReturnedArrayForEachProgramCandidate(admitted)'
require_text "$PROGRAM_OWNER" 'DirectMirReturnedArrayForEachProgramFactFromAdmitted'
require_text "$PRODUCER_OWNER" 'DirectMirArrayIntProducerFactFromRoutine'
require_text "$COLLECTION_OWNER" 'DirectMirArrayReturnProducerName(graph)'
require_text "$SET_OWNER" 'DirectMirScalarCfgForEachStorageRow('
reject_text "$SHARED_ROUTE" 'entrypoint_block_count'
require_text "$PROGRAM_OWNER" \
    'DirectMirScalarCfgCollectionIterationCount(admitted, index) > 0'
for owner in "$PROGRAM_OWNER" "$PRODUCER_OWNER" "$COLLECTION_OWNER"; do
    reject_text "$owner" 'for_each_call.pgy'
    reject_text "$owner" '"MakeValues()"'
    reject_text "$owner" 'routine_block_counts[main_row] == 10'
done

mkdir -p "$WORK_DIR"
(cd "$ROOT_DIR" && "$DRIVER" --emit-mir-json-verified "$SOURCE_REL" \
    -o "$WORK_REL/program.json") || fail "installed producer rejected source"
[[ -s "$WORK_DIR/program.json" ]] || fail "installed producer emitted no MIR"

"$PYTHON_BIN" - "$WORK_DIR/program.json" "$WORK_DIR" <<'PY'
import copy, json, pathlib, sys
doc=json.loads(pathlib.Path(sys.argv[1]).read_text(encoding="utf-8")); out=pathlib.Path(sys.argv[2])
def rows(value):
    main=next(r for r in value["routines"] if r["name"]=="Main")
    producer=next(r for r in value["routines"] if r["name"]!="Main")
    return main, producer
def write(name, fn):
    value=copy.deepcopy(doc); fn(value); (out/f"{name}.json").write_text(json.dumps(value,separators=(",",":")),encoding="utf-8")
write("routine-permuted",lambda d:d["routines"].reverse())
def short(d):
    _,p=rows(d); x=p["blocks"][0]["instructions"][0]; g=x["expr0_graph"]
    g["root"]=4; g["nodes"]=[
      {"kind":"array_literal","text":"[]","call_target_kind":"none","call_target_name":"","left":None,"right":None},
      {"kind":"integer_literal","text":"4","call_target_kind":"none","call_target_name":"","left":None,"right":None},
      {"kind":"array_element","text":"[4]","call_target_kind":"none","call_target_name":"","left":0,"right":1},
      {"kind":"integer_literal","text":"5","call_target_kind":"none","call_target_name":"","left":None,"right":None},
      {"kind":"array_element","text":"[4, 5]","call_target_kind":"none","call_target_name":"","left":2,"right":3}]
write("graph-short",short)
write("bad-producer-abi",lambda d:rows(d)[1]["blocks"][0]["instructions"][0]["abi_layout"]["fields"][1].__setitem__("offset",16))
write("bad-call-abi",lambda d:rows(d)[0]["blocks"][0]["instructions"][1]["abi_layout"]["fields"][1].__setitem__("offset",16))
write("bad-call-abi-id",lambda d:rows(d)[0]["blocks"][0]["instructions"][1].__setitem__("abi_layout_id",rows(d)[0]["blocks"][0]["instructions"][1]["abi_layout_id"]+1))
write("bad-call-target",lambda d:rows(d)[0]["blocks"][0]["instructions"][1]["expr0_graph"]["nodes"][1].__setitem__("call_target_name","MissingValues"))
write("bad-call-leaf",lambda d:rows(d)[0]["blocks"][0]["instructions"][1]["expr0_graph"]["nodes"][0].__setitem__("text","OtherValues"))
write("bad-call-local-ref",lambda d:rows(d)[0]["blocks"][0]["instructions"][1]["expr0_local_refs"].append({"node":0,"ref":"iteration:999:0"}))
write("bad-hoist",lambda d:rows(d)[0]["iteration_type_facts"][0].__setitem__("collection_hoisted",False))
write("bad-result-name",lambda d:rows(d)[0]["blocks"][2]["instructions"][0].__setitem__("arg0","wrong_collection"))
write("bad-local-ref",lambda d:rows(d)[0]["blocks"][4]["instructions"][0]["expr0_local_refs"][0].__setitem__("ref","iteration:999:0"))
PY

project() {
    local input="$1" stem="$2" target="$3" suffix="$4"
    (cd "$ROOT_DIR" && "$DRIVER" "--mir-json-backend=$target" \
        "$WORK_REL/$input.json" -o "$WORK_REL/$stem.$suffix") \
        >"$WORK_DIR/$stem.$target.out" 2>"$WORK_DIR/$stem.$target.err"
}
for target in c llvm; do
    suffix=c; [[ "$target" == llvm ]] && suffix=ll
    project program base "$target" "$suffix" || fail "$target rejected returned foreach"
    project routine-permuted routine-permuted "$target" "$suffix" || fail "$target rejected routine permutation"
    project graph-short graph-short "$target" "$suffix" || fail "$target rejected graph-owned return"
    cmp -s "$WORK_DIR/base.$suffix" "$WORK_DIR/routine-permuted.$suffix" || fail "$target routine order changed artifact"
    for bad in bad-producer-abi bad-call-abi bad-call-abi-id bad-call-target bad-call-leaf bad-call-local-ref bad-hoist bad-result-name bad-local-ref; do
        if project "$bad" "$bad" "$target" "$suffix"; then fail "$target accepted $bad"; fi
        [[ ! -s "$WORK_DIR/$bad.$suffix" ]] || fail "$target published $bad"
        grep -Eq 'direct MIR (returned Array<Int> foreach|scalar CFG|Array<Int> return)' "$WORK_DIR/$bad.$target.out" "$WORK_DIR/$bad.$target.err" || fail "$target lost owned diagnostic for $bad"
        ! grep -Fq 'direct MIR Array<Int> return program envelope is invalid' "$WORK_DIR/$bad.$target.out" "$WORK_DIR/$bad.$target.err" || fail "$target retried the legacy return-only route"
    done
done

[[ $(grep -Ec 'pgy_foreach_storage_[0-9]+\[' "$WORK_DIR/base.c") -eq 1 ]] || fail "C materialized the returned collection more than once"
[[ $(grep -Ec '%pgy\.array\.foreach\.[0-9]+\.storage = alloca' "$WORK_DIR/base.ll") -eq 1 ]] || fail "LLVM materialized the returned collection more than once"
"$CC" -std=c11 -Wall -Wextra -Werror "$WORK_DIR/base.c" -o "$WORK_DIR/base-c.exe" || fail "C artifact did not compile"
"$CLANG" -x ir "$WORK_DIR/base.ll" -o "$WORK_DIR/base-llvm.exe" || fail "LLVM artifact did not compile"
"$CC" -std=c11 -Wall -Wextra -Werror "$WORK_DIR/graph-short.c" -o "$WORK_DIR/short-c.exe" || fail "short C artifact did not compile"
"$CLANG" -x ir "$WORK_DIR/graph-short.ll" -o "$WORK_DIR/short-llvm.exe" || fail "short LLVM artifact did not compile"
for exe in base-c base-llvm; do "$WORK_DIR/$exe.exe" | tr -d '\r' >"$WORK_DIR/$exe.run"; grep -Fxq 30 "$WORK_DIR/$exe.run" || fail "$exe did not execute exact 30"; done
for exe in short-c short-llvm; do "$WORK_DIR/$exe.exe" | tr -d '\r' >"$WORK_DIR/$exe.run"; grep -Fxq 36 "$WORK_DIR/$exe.run" || fail "$exe ignored graph-owned [4,5]"; done
echo "[$LABEL] returned-array producer receipt owns nested/sequential C/LLVM foreach"
