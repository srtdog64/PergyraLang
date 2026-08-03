#!/usr/bin/env bash
# One range receipt and producer-owned loop phis feed both general CFG backends.
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
pgy_prepend_windows_runtime_paths

LABEL="self-host-one-mir-scalar-cfg-for-break-exit"
DRIVER="${PGY_SELFHOST_ONE_MIR_DRIVER_BIN:-${PGY_SELF_DRIVER_BIN:-$ROOT_DIR/bin/pgy-self-driver}}"
CC="${CC:-gcc}"
CLANG="${PGY_SELFHOST_CLANG:-clang}"
PYTHON_BIN="${PYTHON_BIN:-$(command -v python3 || command -v python || true)}"
WORK_REL=".tmp/self_hosted/one_mir_scalar_cfg_for_break_exit"
WORK_DIR="$ROOT_DIR/$WORK_REL"
SOURCE_REL="src/self_hosted/mir_lower/fixture/for_break_exit.pgy"
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

FOR_OWNER="$ROOT_DIR/src/self_hosted/mir/routine_for_owner.pgy"
RANGE_OWNER="$ROOT_DIR/src/self_hosted/compiler/direct_mir_scalar_cfg_range_iteration_owner.pgy"
ADMISSION="$ROOT_DIR/src/self_hosted/compiler/direct_mir_scalar_cfg_graph_admission_owner.pgy"
INPUT_OWNER="$ROOT_DIR/src/self_hosted/compiler/direct_mir_scalar_cfg_graph_input_owner.pgy"
require_text "$FOR_OWNER" 'SelfMirLoopPrepareHeaderLocalVersions('
require_text "$FOR_OWNER" 'SelfMirLoopExitMergeLocalVersions('
require_text "$RANGE_OWNER" 'DirectMirScalarCfgRangeIterationFactsFromOwners('
require_text "$INPUT_OWNER" 'DirectMirScalarCfgRangeIterationFactsFromOwners('
reject_text "$ADMISSION" 'for_break_exit.pgy'
reject_text "$INPUT_OWNER" 'for_break_exit.pgy'
reject_text "$RANGE_OWNER" 'for_break_exit.pgy'

mkdir -p "$WORK_DIR"
rm -f "$WORK_DIR"/*
(cd "$ROOT_DIR" && "$DRIVER" --emit-mir-json-verified \
    "$SOURCE_REL" -o "$MIR_REL") || fail "installed producer rejected for-break source"

"$PYTHON_BIN" - "$MIR" "$WORK_DIR" <<'PY'
import copy, json, pathlib, sys
doc = json.loads(pathlib.Path(sys.argv[1]).read_text(encoding="utf-8"))
target = pathlib.Path(sys.argv[2])
blocks = doc["routines"][0]["blocks"]
assert len(blocks) == 6
assert any(row["kind"] == "loop-init" for row in blocks[0]["instructions"])
header, latch, exit_block = blocks[1], blocks[4], blocks[5]
assert header["instructions"][0]["kind"] == "phi"
assert header["instructions"][0]["result"] == "total.3"
assert header["instructions"][0]["uses"] == ["total.1", "total.5"]
assert header["succ_true"] == 2 and header["succ_false"] == 5
assert blocks[3]["succ_true"] == 5 and latch["succ_true"] == 1
assert exit_block["instructions"][0]["kind"] == "phi"
assert exit_block["instructions"][0]["result"] == "total.8"
assert exit_block["instructions"][0]["uses"] == ["total.3", "total.5"]
assert exit_block["instructions"][1]["uses"] == ["total.8"]
def write(name, mutate):
    changed = copy.deepcopy(doc); mutate(changed["routines"][0]["blocks"])
    (target / f"{name}.json").write_text(
        json.dumps(changed, separators=(",", ":")), encoding="utf-8")
write("header-permuted", lambda b: b[1]["instructions"][0].__setitem__(
    "uses", ["total.5", "total.1"]))
write("exit-permuted", lambda b: b[5]["instructions"][0].__setitem__(
    "uses", ["total.5", "total.3"]))
write("header-stale", lambda b: b[1]["instructions"][0].__setitem__(
    "uses", ["total.1", "total.1"]))
write("exit-stale", lambda b: b[5]["instructions"][0].__setitem__(
    "uses", ["total.3", "total.3"]))
write("log-stale", lambda b: b[5]["instructions"][1].__setitem__(
    "uses", ["total.3"]))
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
    project header-permuted header-permuted "$target" "$suffix"
    project exit-permuted exit-permuted "$target" "$suffix"
    cmp -s "$WORK_DIR/base.$suffix" "$WORK_DIR/header-permuted.$suffix" ||
        fail "$target header incoming permutation changed artifact"
    cmp -s "$WORK_DIR/base.$suffix" "$WORK_DIR/exit-permuted.$suffix" ||
        fail "$target exit incoming permutation changed artifact"
done

grep -Fq 'pgy_local_1 = pgy_local_1 + 1;' "$WORK_DIR/base.c" ||
    fail "C range latch was not emitted by the general graph plan"
grep -Fq '%pgy.range.4.next' "$WORK_DIR/base.ll" ||
    fail "LLVM range latch was not emitted by the general graph plan"
"$CC" -std=c11 -Wall -Wextra -Werror "$WORK_DIR/base.c" \
    -o "$WORK_DIR/c.exe" || fail "C artifact did not compile"
"$CLANG" -x ir "$WORK_DIR/base.ll" -o "$WORK_DIR/llvm.exe" \
    >/dev/null 2>&1 || fail "LLVM artifact did not compile"
"$WORK_DIR/c.exe" | tr -d '\r' >"$WORK_DIR/c.run"
"$WORK_DIR/llvm.exe" | tr -d '\r' >"$WORK_DIR/llvm.run"
printf '3\n' >"$WORK_DIR/expected.run"
cmp -s "$WORK_DIR/expected.run" "$WORK_DIR/c.run" &&
    cmp -s "$WORK_DIR/c.run" "$WORK_DIR/llvm.run" ||
    fail "C/LLVM execution did not produce exact 3"

for mutation in header-stale exit-stale log-stale; do
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

echo "[$LABEL] producer-owned for header/exit phis project through one C/LLVM graph plan"
