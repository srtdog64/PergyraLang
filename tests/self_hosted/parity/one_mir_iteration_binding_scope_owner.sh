#!/usr/bin/env bash
# The self-host MIR producer keeps an iteration binding lexical and consumers fail closed.
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
pgy_prepend_windows_runtime_paths

LABEL="self-host-one-mir-iteration-binding-scope"
DRIVER="${PGY_SELF_DRIVER_BIN:-$ROOT_DIR/bin/pgy-self-driver}"
PYTHON_BIN="${PYTHON_BIN:-$(command -v python3 || command -v python || true)}"
WORK_REL=".tmp/self_hosted/one_mir_iteration_binding_scope"
WORK_DIR="$ROOT_DIR/$WORK_REL"
SOURCE_REL="src/self_hosted/mir_lower/fixture/iteration_binding_shadow.pgy"
MIR_REL="$WORK_REL/program.json"
MIR="$ROOT_DIR/$MIR_REL"

fail() { echo "[$LABEL] $*" >&2; exit 1; }
require_text() { grep -Fq -- "$2" "$1" || fail "missing ${1#"$ROOT_DIR/"}: $2"; }
reject_text() { ! grep -Fq -- "$2" "$1" || fail "forbidden ${1#"$ROOT_DIR/"}: $2"; }
DRIVER="$(pgy_select_optional_exe_binary "$DRIVER")"
[[ -x "$DRIVER" ]] || fail "installed self-host driver is unavailable"
[[ -n "$PYTHON_BIN" ]] || fail "python is required for structured assertions"

FOR_OWNER="$ROOT_DIR/src/self_hosted/mir/routine_for_owner.pgy"
require_text "$FOR_OWNER" 'SelfMirRoutinePushLocal('
require_text "$FOR_OWNER" 'SelfMirRoutineAtLocalCount('
reject_text "$FOR_OWNER" 'SelfMirRoutineAddLocal('
reject_text "$FOR_OWNER" 'iteration_binding_shadow.pgy'

mkdir -p "$WORK_DIR"
(cd "$ROOT_DIR" && "$DRIVER" --emit-mir-json-verified \
    "$SOURCE_REL" -o "$MIR_REL") || fail "producer rejected shadow source"
"$PYTHON_BIN" - "$MIR" <<'PY'
import json, pathlib, sys
routine = json.loads(pathlib.Path(sys.argv[1]).read_text(encoding="utf-8"))["routines"][0]
assert routine["source_locals"] == [
    {"name": "i", "type": "Int"}, {"name": "i", "type": "Int"}]
blocks = routine["blocks"]
assert len(blocks) == 4
assert [ins["kind"] for block in blocks for ins in block["instructions"]] == [
    "def", "loop-init", "branch", "stmt", "stmt"]
assert blocks[0]["instructions"][0]["result"] == "i.1"
assert blocks[2]["instructions"][0]["expr0"] == "Log(i)"
assert blocks[2]["instructions"][0]["uses"] == []
assert blocks[3]["instructions"][0]["uses"] == ["i.1"]
assert all(ins["kind"] != "phi" for block in blocks for ins in block["instructions"])
PY

for target in c llvm; do
    artifact="$WORK_DIR/$target.artifact"
    if (cd "$ROOT_DIR" && "$DRIVER" "--mir-json-backend=$target" \
        "$MIR_REL" -o "$WORK_REL/$target.artifact") \
        >"$WORK_DIR/$target.out" 2>"$WORK_DIR/$target.err"; then
        fail "$target guessed through a duplicated local spelling"
    fi
    [[ ! -s "$artifact" ]] || fail "$target published before local identity admission"
    grep -Fq 'range iteration facts are invalid' "$WORK_DIR/$target.out" ||
        fail "$target rejection did not name the range identity boundary"
done

echo "[$LABEL] producer restores outer binding and C/LLVM refuse name-only recovery"
