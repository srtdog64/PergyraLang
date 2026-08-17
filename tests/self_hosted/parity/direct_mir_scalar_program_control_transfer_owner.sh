#!/usr/bin/env bash
# One control-transfer fact owns break/continue edges in the program GraphPlan.
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
source "$ROOT_DIR/tests/self_hosted/parity/emitted_c_runtime_header_owner.sh"
pgy_prepend_windows_runtime_paths

LABEL="self-host-direct-mir-scalar-program-control-transfer"
DRIVER="${PGY_SELF_DRIVER_BIN:-$ROOT_DIR/bin/pgy-self-driver}"
CC="${CC:-gcc}"
CLANG="${PGY_SELFHOST_CLANG:-clang}"
PYTHON_BIN="${PYTHON_BIN:-$(command -v python3 || command -v python || true)}"
WORK_REL=".tmp/self_hosted/direct_mir_scalar_program_control_transfer"
WORK_DIR="$ROOT_DIR/$WORK_REL"
SOURCE_REL="tests/self_hosted/fixtures/direct_mir_program_break_continue.pgy"
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

TRANSFER="$ROOT_DIR/src/self_hosted/compiler/direct_mir_scalar_cfg_control_transfer_admission_owner.pgy"
BRANCH="$ROOT_DIR/src/self_hosted/compiler/direct_mir_scalar_cfg_branch_admission_owner.pgy"
PROGRAM="$ROOT_DIR/src/self_hosted/compiler/direct_mir_scalar_cfg_program_routine_admission_owner.pgy"
for owner in "$BRANCH" "$PROGRAM"; do
    require_text "$owner" 'DirectMirScalarCfgControlTransferFromOwners('
done
require_text "$TRANSFER" 'source == "AST_BREAK"'
require_text "$TRANSFER" 'source == "AST_CONTINUE"'
require_text "$TRANSFER" 'MirRoutineBlockDominates('
reject_text "$BRANCH" 'source == "AST_BREAK"'
reject_text "$PROGRAM" 'source == "AST_CONTINUE"'

mkdir -p "$WORK_DIR"; rm -f "$WORK_DIR"/*
(cd "$ROOT_DIR" && "$DRIVER" --emit-mir-json-verified \
    "$SOURCE_REL" -o "$MIR_REL") || fail "producer rejected control-transfer source"
"$PYTHON_BIN" - "$MIR" "$WORK_DIR" <<'PY'
import copy, json, pathlib, sys
doc = json.loads(pathlib.Path(sys.argv[1]).read_text(encoding="utf-8"))
target = pathlib.Path(sys.argv[2])
routine = next(r for r in doc["routines"] if r["name"] == "SumWithTransfers")
found = {}
for block in routine["blocks"]:
    for row in block["instructions"]:
        source = row.get("source_type")
        if source in ("AST_BREAK", "AST_CONTINUE"):
            assert source not in found and row.get("uses") == []
            assert block.get("succ_true") is not None and block.get("succ_false") is None
            found[source] = block
assert set(found) == {"AST_BREAK", "AST_CONTINUE"}
header = found["AST_CONTINUE"]["succ_true"]
exit_block = found["AST_BREAK"]["succ_true"]
assert header != exit_block
def write(name, source, successor):
    changed = copy.deepcopy(doc)
    routine = next(r for r in changed["routines"] if r["name"] == "SumWithTransfers")
    block = next(b for b in routine["blocks"] if any(
        row.get("source_type") == source for row in b["instructions"]))
    block["succ_true"] = successor
    (target / f"{name}.json").write_text(
        json.dumps(changed, separators=(",", ":")), encoding="utf-8")
write("wrong-continue-target", "AST_CONTINUE", exit_block)
write("wrong-break-target", "AST_BREAK", header)
PY

project() {
    local input="$1" stem="$2" target="$3" suffix="$4"
    (cd "$ROOT_DIR" && "$DRIVER" "--mir-json-backend=$target" \
        "$WORK_REL/$input.json" -o "$WORK_REL/$stem.$suffix") ||
        fail "$target rejected $input"
}
for target in c llvm; do
    suffix=c; [[ "$target" == llvm ]] && suffix=ll
    project program program "$target" "$suffix"
done
command=("$CC" -std=c11 -Wall -Wextra -Werror "$WORK_DIR/program.c")
if pgy_selfhost_emitted_c_uses_runtime_headers "$WORK_DIR/program.c"; then
    command+=("-I$ROOT_DIR/src" "-I$ROOT_DIR/src/runtime" -pthread)
fi
command+=(-o "$WORK_DIR/c.exe")
"${command[@]}" || fail "C artifact did not compile"
"$CLANG" -x ir "$WORK_DIR/program.ll" -o "$WORK_DIR/llvm.exe" \
    >/dev/null 2>&1 || fail "LLVM artifact did not compile"
"$WORK_DIR/c.exe" | tr -d '\r' >"$WORK_DIR/c.run"
"$WORK_DIR/llvm.exe" | tr -d '\r' >"$WORK_DIR/llvm.run"
printf '25\n' >"$WORK_DIR/expected.run"
cmp -s "$WORK_DIR/expected.run" "$WORK_DIR/c.run" &&
    cmp -s "$WORK_DIR/c.run" "$WORK_DIR/llvm.run" ||
    fail "C/LLVM control-transfer output drifted"

for mutation in wrong-continue-target wrong-break-target; do
    for target in c llvm; do
        artifact="$WORK_DIR/$mutation-$target.artifact"; rm -f "$artifact"
        if (cd "$ROOT_DIR" && "$DRIVER" "--mir-json-backend=$target" \
            "$WORK_REL/$mutation.json" -o "$WORK_REL/$mutation-$target.artifact") \
            >"$WORK_DIR/$mutation-$target.out" 2>"$WORK_DIR/$mutation-$target.err"; then
            fail "$target accepted $mutation"
        fi
        [[ ! -e "$artifact" ]] || fail "$target published $mutation"
    done
done

echo "[$LABEL] shared break/continue fact executes exact 25 in C/LLVM"
