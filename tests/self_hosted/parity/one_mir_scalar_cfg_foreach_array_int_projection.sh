#!/usr/bin/env bash
# One collection receipt drives both scalar CFG backends without source replay.
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
pgy_prepend_windows_runtime_paths
LABEL="self-host-one-mir-scalar-cfg-foreach-array-int"
DRIVER="${PGY_SELF_DRIVER_BIN:-$ROOT_DIR/bin/pgy-self-driver}"
CC="${CC:-gcc}"
CLANG="${PGY_SELFHOST_CLANG:-clang}"
PYTHON_BIN="${PYTHON_BIN:-$(command -v python3 || command -v python || true)}"
WORK_REL=".tmp/self_hosted/one_mir_scalar_cfg_foreach_array_int"
WORK_DIR="$ROOT_DIR/$WORK_REL"
SOURCE_REL="src/self_hosted/mir_lower/fixture/foreach_array_int_sum.pgy"
MIR="$WORK_DIR/program.json"
fail() { echo "[$LABEL] $*" >&2; exit 1; }
require_text() { grep -Fq -- "$2" "$1" || fail "missing ${1#"$ROOT_DIR/"}: $2"; }
reject_text() { ! grep -Fq -- "$2" "$1" || fail "forbidden ${1#"$ROOT_DIR/"}: $2"; }

DRIVER="$(pgy_select_optional_exe_binary "$DRIVER")"
[[ -x "$DRIVER" ]] || fail "installed self-host driver is unavailable"
command -v "$CC" >/dev/null || fail "C compiler is unavailable"
command -v "$CLANG" >/dev/null || fail "clang is unavailable"
[[ -n "$PYTHON_BIN" ]] || fail "python is required for structured falsifiers"
ADMISSION="$ROOT_DIR/src/self_hosted/compiler/direct_mir_scalar_cfg_branch_admission_owner.pgy"
INPUT_OWNER="$ROOT_DIR/src/self_hosted/compiler/direct_mir_scalar_cfg_graph_input_owner.pgy"
FACT="$ROOT_DIR/src/self_hosted/compiler/direct_mir_scalar_cfg_foreach_fact_owner.pgy"
C_OWNER="$ROOT_DIR/src/self_hosted/compiler/direct_mir_scalar_cfg_foreach_typed_c_emission_owner.pgy"
LLVM_OWNER="$ROOT_DIR/src/self_hosted/compiler/direct_mir_scalar_cfg_foreach_typed_llvm_emission_owner.pgy"
require_text "$INPUT_OWNER" 'DirectMirScalarCfgForEachFactsFromOwners('
require_text "$ADMISSION" 'DirectMirScalarCfgConditionForEach()'
require_text "$FACT" 'collection_value_id'
require_text "$FACT" 'length_offset'
require_text "$C_OWNER" 'abi.c_length_type'
require_text "$LLVM_OWNER" 'abi.length_index'
for owner in "$ADMISSION" "$INPUT_OWNER" "$C_OWNER" "$LLVM_OWNER"; do
    reject_text "$owner" 'foreach_array_int_sum.pgy'
    reject_text "$owner" 'block_count == 4'
    reject_text "$owner" '"expr0"'
done

mkdir -p "$WORK_DIR"
rm -f "$WORK_DIR"/*
(cd "$ROOT_DIR" && "$DRIVER" --emit-mir-json-verified "$SOURCE_REL" \
    -o "$WORK_REL/program.json") || fail "installed producer rejected foreach source"
[[ -s "$MIR" ]] || fail "installed producer emitted no MIR"

"$PYTHON_BIN" - "$MIR" "$WORK_DIR" <<'PY'
import copy, json, pathlib, sys
doc=json.loads(pathlib.Path(sys.argv[1]).read_text(encoding="utf-8")); out=pathlib.Path(sys.argv[2])
def write(name, fn):
    value=copy.deepcopy(doc); fn(value["routines"][0]); (out/f"{name}.json").write_text(json.dumps(value,separators=(",",":")),encoding="utf-8")
write("phi-permuted",lambda r:r["blocks"][1]["instructions"][0].__setitem__("uses",["total.6","total.1"]))
def short(r):
    graph=r["blocks"][0]["instructions"][0]["expr0_graph"]
    graph["root"]=4; graph["nodes"]=[
      {"kind":"array_literal","text":"[]","call_target_kind":"none","call_target_name":"","left":None,"right":None},
      {"kind":"integer_literal","text":"4","call_target_kind":"none","call_target_name":"","left":None,"right":None},
      {"kind":"array_element","text":"[4]","call_target_kind":"none","call_target_name":"","left":0,"right":1},
      {"kind":"integer_literal","text":"5","call_target_kind":"none","call_target_name":"","left":None,"right":None},
      {"kind":"array_element","text":"[4, 5]","call_target_kind":"none","call_target_name":"","left":2,"right":3}]
write("graph-short",short)
write("bad-abi",lambda r:r["blocks"][0]["instructions"][0]["abi_layout"]["fields"][1].__setitem__("offset",16))
write("missing-iteration",lambda r:(r.__setitem__("iteration_type_fact_count",0),r.__setitem__("iteration_type_facts",[])))
write("wrong-iterable",lambda r:r["iteration_type_facts"][0].__setitem__("iterable_type","Int"))
write("hoisted",lambda r:r["iteration_type_facts"][0].__setitem__("collection_hoisted",True))
write("branch-use",lambda r:r["blocks"][1]["instructions"][1].__setitem__("uses",["total.1"]))
write("branch-graph",lambda r:r["blocks"][1]["instructions"][1]["expr0_graph"]["nodes"][0].__setitem__("text","total"))
write("literal-spine",lambda r:r["blocks"][0]["instructions"][0]["expr0_graph"]["nodes"][4].__setitem__("left",0))
write("overflow-element",lambda r:r["blocks"][0]["instructions"][0]["expr0_graph"]["nodes"][1].__setitem__("text","2147483648"))
write("body-leaf",lambda r:r["blocks"][2]["instructions"][0]["expr0_graph"]["nodes"][1].__setitem__("text","values"))
write("duplicate-phi",lambda r:r["blocks"][1]["instructions"][0].__setitem__("uses",["total.1","total.1"]))
write("backedge-exit",lambda r:r["blocks"][2].__setitem__("succ_true",3))
write("header-swap",lambda r:(r["blocks"][1].__setitem__("succ_true",3),r["blocks"][1].__setitem__("succ_false",2)))
PY

project() {
    local input="$1" stem="$2" target="$3" suffix="$4"
    (cd "$ROOT_DIR" && "$DRIVER" "--mir-json-backend=$target" \
        "$WORK_REL/$input.json" -o "$WORK_REL/$stem.$suffix") \
        >"$WORK_DIR/$stem.$target.out" 2>"$WORK_DIR/$stem.$target.err"
}
for target in c llvm; do
    suffix=c; [[ "$target" == llvm ]] && suffix=ll
    project program base "$target" "$suffix" || fail "$target rejected the admitted foreach"
    project phi-permuted phi-permuted "$target" "$suffix" || fail "$target rejected reordered phi inputs"
    project graph-short graph-short "$target" "$suffix" || fail "$target rejected graph-owned short array"
    cmp -s "$WORK_DIR/base.$suffix" "$WORK_DIR/phi-permuted.$suffix" || fail "$target phi order changed the artifact"
    for bad in bad-abi missing-iteration wrong-iterable hoisted branch-use branch-graph literal-spine overflow-element body-leaf duplicate-phi backedge-exit header-swap; do
        if project "$bad" "$bad" "$target" "$suffix"; then fail "$target accepted $bad"; fi
        [[ ! -s "$WORK_DIR/$bad.$suffix" ]] || fail "$target published $bad"
        grep -Eq 'direct MIR (scalar CFG|Array<Int>)' "$WORK_DIR/$bad.$target.out" "$WORK_DIR/$bad.$target.err" || fail "$target lost owned foreach diagnostic for $bad"
        ! grep -Fq 'direct MIR range CFG block inventory' "$WORK_DIR/$bad.$target.out" "$WORK_DIR/$bad.$target.err" || fail "$target retried $bad through legacy range CFG"
    done
done

"$CC" -std=c11 -Wall -Wextra -Werror "$WORK_DIR/base.c" -o "$WORK_DIR/base-c.exe" || fail "C artifact did not compile"
"$CLANG" -x ir "$WORK_DIR/base.ll" -o "$WORK_DIR/base-llvm.exe" || fail "LLVM artifact did not compile"
"$CC" -std=c11 -Wall -Wextra -Werror "$WORK_DIR/graph-short.c" -o "$WORK_DIR/short-c.exe" || fail "short C artifact did not compile"
"$CLANG" -x ir "$WORK_DIR/graph-short.ll" -o "$WORK_DIR/short-llvm.exe" || fail "short LLVM artifact did not compile"
for exe in base-c base-llvm; do "$WORK_DIR/$exe.exe" | tr -d '\r' >"$WORK_DIR/$exe.run"; grep -Fxq 6 "$WORK_DIR/$exe.run" || fail "$exe did not execute exact 6"; done
for exe in short-c short-llvm; do "$WORK_DIR/$exe.exe" | tr -d '\r' >"$WORK_DIR/$exe.run"; grep -Fxq 9 "$WORK_DIR/$exe.run" || fail "$exe ignored graph-owned [4,5]"; done
echo "[$LABEL] Array<Int> foreach receipt owns C/LLVM execution and rejects fallback"
