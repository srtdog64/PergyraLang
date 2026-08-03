#!/usr/bin/env bash
# Producer-owned loop-exit phi is consumed once by the general scalar CFG plan.
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
pgy_prepend_windows_runtime_paths

LABEL="self-host-one-mir-scalar-cfg-break-exit"
DRIVER="${PGY_SELFHOST_ONE_MIR_DRIVER_BIN:-${PGY_SELF_DRIVER_BIN:-$ROOT_DIR/bin/pgy-self-driver}}"
CC="${CC:-gcc}"
CLANG="${PGY_SELFHOST_CLANG:-clang}"
PYTHON_BIN="${PYTHON_BIN:-$(command -v python3 || command -v python || true)}"
WORK_REL=".tmp/self_hosted/one_mir_scalar_cfg_break_exit"
WORK_DIR="$ROOT_DIR/$WORK_REL"
SOURCE_REL="src/self_hosted/mir_lower/fixture/break_after_stmt.pgy"
MIR_REL="$WORK_REL/program.json"
MIR="$ROOT_DIR/$MIR_REL"

fail() { echo "[$LABEL] $*" >&2; exit 1; }
require_text() { grep -Fq -- "$2" "$1" || fail "missing ${1#"$ROOT_DIR/"}: $2"; }
reject_text() { ! grep -Fq -- "$2" "$1" || fail "forbidden ${1#"$ROOT_DIR/"}: $2"; }

DRIVER="$(pgy_select_optional_exe_binary "$DRIVER")"
[[ -x "$DRIVER" ]] || fail "installed self-host driver is unavailable"
command -v "$CC" >/dev/null || fail "C compiler is unavailable"
command -v "$CLANG" >/dev/null || fail "clang is unavailable"
[[ -n "$PYTHON_BIN" ]] || fail "python is required for structured falsifiers"

BREAK_OWNER="$ROOT_DIR/src/self_hosted/mir/routine_break_exit_fact_owner.pgy"
EXIT_OWNER="$ROOT_DIR/src/self_hosted/mir/routine_loop_exit_phi_owner.pgy"
ADMISSION="$ROOT_DIR/src/self_hosted/compiler/direct_mir_scalar_cfg_graph_admission_owner.pgy"
ROUTER="$ROOT_DIR/src/self_hosted/compiler/direct_mir_backend_projection_owner.pgy"
require_text "$BREAK_OWNER" 'SelfMirBreakExitFactsAppend('
require_text "$EXIT_OWNER" 'SelfMirLoopExitMergeLocalVersions('
require_text "$ROUTER" 'DirectMirScalarCfgGraphRouteClaimed(admitted)'
require_text "$ADMISSION" 'MirPhiPredecessorBindingFactFromOwners('
reject_text "$ADMISSION" 'break_after_stmt.pgy'
reject_text "$ADMISSION" 'DirectMirScalarCfgBreakExitMergeBridgeRequired('
[[ ! -e "$ROOT_DIR/src/self_hosted/compiler/direct_mir_scalar_cfg_break_exit_bridge_owner.pgy" ]] ||
    fail "retired break-exit bridge owner reappeared"

mkdir -p "$WORK_DIR"
rm -f "$WORK_DIR"/*
(cd "$ROOT_DIR" && "$DRIVER" --emit-mir-json-verified \
    "$SOURCE_REL" -o "$MIR_REL") || fail "installed producer rejected break source"

"$PYTHON_BIN" - "$MIR" <<'PY'
import json, pathlib, sys
r = json.loads(pathlib.Path(sys.argv[1]).read_text(encoding="utf-8"))["routines"][0]
blocks = r["blocks"]
assert len(blocks) == 6
header = blocks[1]
exit_block = blocks[5]
assert header["succ_false"] == 5
assert blocks[3]["succ_true"] == 5
assert header["instructions"][0]["kind"] == "phi"
exit_phi = exit_block["instructions"][0]
exit_log = exit_block["instructions"][1]
assert exit_phi["kind"] == "phi"
assert exit_phi["result"] == "i.8"
assert set(exit_phi["uses"]) == {"i.2", "i.4"}
assert exit_log["uses"] == ["i.8"]
PY

project() {
    local target="$1" suffix="$2"
    (cd "$ROOT_DIR" && "$DRIVER" "--mir-json-backend=$target" \
        "$MIR_REL" -o "$WORK_REL/program.$suffix") ||
        fail "$target rejected producer-owned exit phi"
}
project c c
project llvm ll
require_text "$WORK_DIR/program.c" 'goto pgy_block_5;'
require_text "$WORK_DIR/program.ll" '@.pgy.scalar.cfg.int.format'
reject_text "$WORK_DIR/program.ll" 'backend-only exit value selection'
"$CC" -std=c11 -Wall -Wextra -Werror "$WORK_DIR/program.c" \
    -o "$WORK_DIR/program-c.exe" || fail "C projection did not compile"
"$CLANG" -x ir "$WORK_DIR/program.ll" -o "$WORK_DIR/program-llvm.exe" \
    >/dev/null 2>&1 || fail "LLVM projection did not compile"
"$WORK_DIR/program-c.exe" | tr -d '\r' >"$WORK_DIR/c.run"
"$WORK_DIR/program-llvm.exe" | tr -d '\r' >"$WORK_DIR/llvm.run"
printf '3\n3\n' >"$WORK_DIR/expected.run"
cmp -s "$WORK_DIR/expected.run" "$WORK_DIR/c.run" || fail "C output drifted"
cmp -s "$WORK_DIR/expected.run" "$WORK_DIR/llvm.run" || fail "LLVM output drifted"

"$PYTHON_BIN" - "$MIR" "$WORK_DIR" <<'PY'
import copy, json, pathlib, sys
doc = json.loads(pathlib.Path(sys.argv[1]).read_text(encoding="utf-8"))
target = pathlib.Path(sys.argv[2])
def write(name, mutate):
    changed = copy.deepcopy(doc)
    mutate(changed["routines"][0]["blocks"][5]["instructions"])
    (target / f"{name}.json").write_text(json.dumps(changed, separators=(",", ":")), encoding="utf-8")
write("missing-exit-phi", lambda rows: rows.pop(0))
write("stale-exit-use", lambda rows: rows[1].__setitem__("uses", ["i.4"]))
write("unknown-exit-incoming", lambda rows: rows[0].__setitem__("uses", ["i.2", "i.99"]))
write("duplicate-exit-incoming", lambda rows: rows[0].__setitem__("uses", ["i.2", "i.2"]))
PY

expect_rejected() {
    local name="$1" target artifact
    for target in c llvm; do
        artifact="$WORK_DIR/$name-$target.artifact"
        rm -f "$artifact"
        if (cd "$ROOT_DIR" && "$DRIVER" "--mir-json-backend=$target" \
            "$WORK_REL/$name.json" -o "$WORK_REL/$name-$target.artifact") \
            >"$WORK_DIR/$name-$target.out" 2>"$WORK_DIR/$name-$target.err"; then
            fail "$target accepted $name"
        fi
        [[ ! -e "$artifact" ]] || fail "$target published before rejecting $name"
    done
}
for mutation in missing-exit-phi stale-exit-use unknown-exit-incoming \
    duplicate-exit-incoming; do
    expect_rejected "$mutation"
done

MULTI_SOURCE_REL="src/self_hosted/mir_lower/fixture/multiple_break_exit.pgy"
MULTI_MIR_REL="$WORK_REL/multiple.json"
MULTI_MIR="$ROOT_DIR/$MULTI_MIR_REL"
(cd "$ROOT_DIR" && "$DRIVER" --emit-mir-json-verified \
    "$MULTI_SOURCE_REL" -o "$MULTI_MIR_REL") ||
    fail "installed producer rejected repeated break source"
"$PYTHON_BIN" - "$MULTI_MIR" "$WORK_DIR" <<'PY'
import copy, json, pathlib, sys
path, target = pathlib.Path(sys.argv[1]), pathlib.Path(sys.argv[2])
doc = json.loads(path.read_text(encoding="utf-8"))
rows = doc["routines"][0]["blocks"][7]["instructions"]
assert rows[0]["kind"] == "phi" and rows[0]["result"] == "i.9"
assert rows[0]["uses"] == ["i.2", "i.4", "i.4"]
assert rows[1]["uses"] == ["i.9"]
changed = copy.deepcopy(doc)
changed["routines"][0]["blocks"][7]["instructions"][0]["uses"] = ["i.4", "i.2", "i.4"]
(target / "multiple-permuted.json").write_text(
    json.dumps(changed, separators=(",", ":")), encoding="utf-8"
)
PY
MIR_REL="$MULTI_MIR_REL"; project c multiple.c; project llvm multiple.ll
"$CC" -std=c11 -Wall -Wextra -Werror "$WORK_DIR/program.multiple.c" \
    -o "$WORK_DIR/multiple-c.exe" || fail "multiple-break C did not compile"
"$CLANG" -x ir "$WORK_DIR/program.multiple.ll" -o "$WORK_DIR/multiple-llvm.exe" \
    >/dev/null 2>&1 || fail "multiple-break LLVM did not compile"
"$WORK_DIR/multiple-c.exe" | tr -d '\r' >"$WORK_DIR/multiple-c.run"
"$WORK_DIR/multiple-llvm.exe" | tr -d '\r' >"$WORK_DIR/multiple-llvm.run"
printf '2\n' >"$WORK_DIR/multiple-expected.run"
cmp -s "$WORK_DIR/multiple-expected.run" "$WORK_DIR/multiple-c.run" &&
    cmp -s "$WORK_DIR/multiple-c.run" "$WORK_DIR/multiple-llvm.run" ||
    fail "repeated break exit did not execute exact 2"
MIR_REL="$WORK_REL/multiple-permuted.json"
project c multiple-permuted.c; project llvm multiple-permuted.ll
cmp -s "$WORK_DIR/program.multiple.c" "$WORK_DIR/program.multiple-permuted.c" &&
    cmp -s "$WORK_DIR/program.multiple.ll" "$WORK_DIR/program.multiple-permuted.ll" ||
    fail "equal incoming slot permutation changed target artifacts"

echo "[$LABEL] producer exit phi and repeated-slot general C/LLVM CFG route ok"
