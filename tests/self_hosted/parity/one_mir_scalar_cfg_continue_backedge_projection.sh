#!/usr/bin/env bash
# Producer-captured continue and fallthrough snapshots bind one range header phi.
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
pgy_prepend_windows_runtime_paths

LABEL="self-host-one-mir-scalar-cfg-continue-backedge"
DRIVER="${PGY_SELFHOST_ONE_MIR_DRIVER_BIN:-${PGY_SELF_DRIVER_BIN:-$ROOT_DIR/bin/pgy-self-driver}}"
CC="${CC:-gcc}"
CLANG="${PGY_SELFHOST_CLANG:-clang}"
PYTHON_BIN="${PYTHON_BIN:-$(command -v python3 || command -v python || true)}"
WORK_REL=".tmp/self_hosted/one_mir_scalar_cfg_continue_backedge"
WORK_DIR="$ROOT_DIR/$WORK_REL"
SOURCE_REL="examples/break_continue.pgy"
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

SNAPSHOT_OWNER="$ROOT_DIR/src/self_hosted/mir/routine_local_predecessor_snapshot_owner.pgy"
BINDING_OWNER="$ROOT_DIR/src/self_hosted/mir/routine_loop_header_backedge_binding_owner.pgy"
ADMISSION="$ROOT_DIR/src/self_hosted/compiler/direct_mir_scalar_cfg_graph_admission_owner.pgy"
require_text "$SNAPSHOT_OWNER" 'SelfMirLocalPredecessorSnapshotsAppend('
require_text "$BINDING_OWNER" 'SelfMirLocalPredecessorVersionAt('
require_text "$ADMISSION" 'source == "AST_CONTINUE"'
reject_text "$ADMISSION" 'break_continue.pgy'
for loop_owner in routine_for_owner.pgy routine_while_owner.pgy; do
    reject_text "$ROOT_DIR/src/self_hosted/mir/$loop_owner" \
        'body_block < SelfMirCfgBlockCount(build.cfg)'
done

mkdir -p "$WORK_DIR"; rm -f "$WORK_DIR"/*
(cd "$ROOT_DIR" && "$DRIVER" --emit-mir-json-verified \
    "$SOURCE_REL" -o "$MIR_REL") || fail "producer rejected continue source"
"$PYTHON_BIN" - "$MIR" "$WORK_DIR" <<'PY'
import copy, json, pathlib, sys
doc = json.loads(pathlib.Path(sys.argv[1]).read_text(encoding="utf-8"))
target = pathlib.Path(sys.argv[2]); blocks = doc["routines"][0]["blocks"]
assert len(blocks) == 8
phi = blocks[1]["instructions"][0]
assert phi["kind"] == "phi" and phi["result"] == "sum.3"
assert phi["uses"] == ["sum.1", "sum.3", "sum.9"]
assert blocks[5]["instructions"][0]["source_type"] == "AST_CONTINUE"
assert blocks[5]["succ_true"] == 1 and blocks[6]["succ_true"] == 1
assert blocks[3]["succ_true"] == 7
assert blocks[7]["instructions"][0]["uses"] == ["sum.3"]
def write(name, mutate):
    changed = copy.deepcopy(doc); mutate(changed["routines"][0]["blocks"])
    (target / f"{name}.json").write_text(
        json.dumps(changed, separators=(",", ":")), encoding="utf-8")
write("permuted", lambda b: b[1]["instructions"][0].__setitem__(
    "uses", ["sum.9", "sum.1", "sum.3"]))
write("missing-continue", lambda b: b[1]["instructions"][0].__setitem__(
    "uses", ["sum.1", "sum.9"]))
write("stale-continue", lambda b: b[1]["instructions"][0].__setitem__(
    "uses", ["sum.1", "sum.1", "sum.9"]))
write("wrong-continue-target", lambda b: b[5].__setitem__("succ_true", 7))
PY

project() {
    local input="$1" stem="$2" target="$3" suffix="$4"
    (cd "$ROOT_DIR" && "$DRIVER" "--mir-json-backend=$target" \
        "$WORK_REL/$input.json" -o "$WORK_REL/$stem.$suffix") ||
        fail "$target rejected $input"
}
for target in c llvm; do
    suffix=c; [[ "$target" == llvm ]] && suffix=ll
    project program base "$target" "$suffix"
    project permuted permuted "$target" "$suffix"
    cmp -s "$WORK_DIR/base.$suffix" "$WORK_DIR/permuted.$suffix" ||
        fail "$target incoming permutation changed artifact"
done
[[ "$(grep -Fc 'pgy_local_1 = pgy_local_1 + 1;' "$WORK_DIR/base.c")" == 2 ]] ||
    fail "C did not increment the range local on both backedges"
require_text "$WORK_DIR/base.ll" '%pgy.range.5.next'
require_text "$WORK_DIR/base.ll" '%pgy.range.6.next'
"$CC" -std=c11 -Wall -Wextra -Werror "$WORK_DIR/base.c" \
    -o "$WORK_DIR/c.exe" || fail "C artifact did not compile"
"$CLANG" -x ir "$WORK_DIR/base.ll" -o "$WORK_DIR/llvm.exe" \
    >/dev/null 2>&1 || fail "LLVM artifact did not compile"
"$WORK_DIR/c.exe" | tr -d '\r' >"$WORK_DIR/c.run"
"$WORK_DIR/llvm.exe" | tr -d '\r' >"$WORK_DIR/llvm.run"
printf '42\n' >"$WORK_DIR/expected.run"
cmp -s "$WORK_DIR/expected.run" "$WORK_DIR/c.run" &&
    cmp -s "$WORK_DIR/c.run" "$WORK_DIR/llvm.run" ||
    fail "C/LLVM execution did not produce exact 42"

for mutation in missing-continue stale-continue wrong-continue-target; do
    for target in c llvm; do
        artifact="$WORK_DIR/$mutation-$target.artifact"; rm -f "$artifact"
        if (cd "$ROOT_DIR" && "$DRIVER" "--mir-json-backend=$target" \
            "$WORK_REL/$mutation.json" -o "$WORK_REL/$mutation-$target.artifact") \
            >"$WORK_DIR/$mutation-$target.out" 2>"$WORK_DIR/$mutation-$target.err"; then
            fail "$target accepted $mutation"
        fi
        [[ ! -e "$artifact" ]] || fail "$target published before rejecting $mutation"
    done
done

echo "[$LABEL] continue and fallthrough snapshots drive exact 42 in C/LLVM"
