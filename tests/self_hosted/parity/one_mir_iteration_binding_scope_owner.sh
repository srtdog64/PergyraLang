#!/usr/bin/env bash
# Explicit LocalRef identity keeps a range binder distinct from an outer local.
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
pgy_prepend_windows_runtime_paths

LABEL="self-host-one-mir-iteration-binding-scope"
DRIVER="${PGY_SELF_DRIVER_BIN:-$ROOT_DIR/bin/pgy-self-driver}"
CC="${CC:-gcc}"
CLANG="${PGY_SELFHOST_CLANG:-clang}"
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
command -v "$CC" >/dev/null || fail "C compiler is unavailable"
command -v "$CLANG" >/dev/null || fail "clang is unavailable"
[[ -n "$PYTHON_BIN" ]] || fail "python is required for structured falsifiers"

FOR_OWNER="$ROOT_DIR/src/self_hosted/mir/routine_for_owner.pgy"
WIRE_OWNER="$ROOT_DIR/src/self_hosted/compiler/direct_mir_scalar_cfg_wire_local_ref_owner.pgy"
SCOPE_OWNER="$ROOT_DIR/src/self_hosted/compiler/direct_mir_scalar_cfg_wire_range_scope_admission_owner.pgy"
PLAN_OWNER="$ROOT_DIR/src/self_hosted/compiler/direct_mir_scalar_cfg_local_ref_plan_owner.pgy"
require_text "$FOR_OWNER" 'SelfMirRoutinePushLocal('
require_text "$SCOPE_OWNER" 'DirectMirScalarCfgWireRangeScopesReady('
require_text "$PLAN_OWNER" 'DirectMirScalarCfgAppendIterationLocals('
reject_text "$FOR_OWNER" 'SelfMirRoutineAddLocal('
reject_text "$PLAN_OWNER" 'DirectMirScalarCfgLocalRow('
reject_text "$PLAN_OWNER" 'DirectMirScalarCfgRangeLocalRow('
for owner in "$WIRE_OWNER" "$SCOPE_OWNER" "$PLAN_OWNER"; do
    reject_text "$owner" 'iteration_binding_shadow.pgy'
done

mkdir -p "$WORK_DIR"
rm -f "$WORK_DIR"/*
(cd "$ROOT_DIR" && "$DRIVER" --emit-mir-json-verified \
    "$SOURCE_REL" -o "$MIR_REL") || fail "producer rejected shadow source"
"$PYTHON_BIN" - "$MIR" "$WORK_DIR" <<'PY'
import copy, json, pathlib, sys
doc = json.loads(pathlib.Path(sys.argv[1]).read_text(encoding="utf-8"))
target = pathlib.Path(sys.argv[2]); routine = doc["routines"][0]
assert routine["source_locals"] == [
    {"name": "i", "type": "Int"}, {"name": "i", "type": "Int"}]
ins = [row for block in routine["blocks"] for row in block["instructions"]]
assert [row["kind"] for row in ins] == [
    "def", "loop-init", "branch", "stmt", "stmt"]
assert ins[0]["result"] == "i.1" and ins[0]["local_ref"] == "declaration:6:0"
assert ins[1]["local_ref"] == ins[2]["local_ref"] == "iteration:7:0"
assert ins[3]["uses"] == [] and ins[3]["expr0_local_refs"] == [
    {"node": 0, "ref": "iteration:7:0"}]
assert ins[4]["uses"] == ["i.1"] and ins[4]["expr0_local_refs"] == []
assert all(row["kind"] != "phi" for row in ins)
def write(name, mutate):
    changed = copy.deepcopy(doc); rows = [row for block in
        changed["routines"][0]["blocks"] for row in block["instructions"]]
    mutate(changed["routines"][0], rows)
    (target / f"{name}.json").write_text(
        json.dumps(changed, separators=(",", ":")), encoding="utf-8")
write("missing-def-ref", lambda r, x: x[0].pop("local_ref"))
write("forged-def-ref", lambda r, x: x[0].__setitem__("local_ref", "iteration:7:0"))
write("forged-init-ref", lambda r, x: x[1].__setitem__("local_ref", "declaration:6:0"))
write("missing-body-ref", lambda r, x: x[3].__setitem__("expr0_local_refs", []))
write("forged-body-ref", lambda r, x: x[3]["expr0_local_refs"][0].__setitem__(
    "ref", "declaration:6:0"))
write("body-outer-use", lambda r, x: x[3].__setitem__("uses", ["i.1"]))
write("missing-outer-use", lambda r, x: x[4].__setitem__("uses", []))
write("orphan-outer-ref", lambda r, x: x[4].__setitem__("expr0_local_refs", [
    {"node": 99, "ref": "iteration:7:0"}]))
write("duplicate-local", lambda r, x: r["source_locals"].append(
    copy.deepcopy(r["source_locals"][0])))
PY

project() {
    local input="$1" target="$2" suffix="$3"
    (cd "$ROOT_DIR" && "$DRIVER" "--mir-json-backend=$target" \
        "$WORK_REL/$input.json" -o "$WORK_REL/$input.$suffix") ||
        fail "$target rejected $input"
}
project program c c; project program llvm ll
[[ "$(sha256sum "$MIR" | awk '{print $1}')" == \
    fe64a5314b7a1146bbcffa826752f2c9aeeea6f2c8fe603d0955b1c76dfff006 ]] ||
    fail "single-range MIR identity drifted"
[[ "$(sha256sum "$WORK_DIR/program.c" | awk '{print $1}')" == \
    e2724f4f1b6972be932c30e4545ae89eb749e7f8883b322e2df23eb9c25fdf31 ]] ||
    fail "single-range C artifact drifted"
[[ "$(sha256sum "$WORK_DIR/program.ll" | awk '{print $1}')" == \
    9cedd99d55b94b8473ceb9741b954617d7d349257e527d374a9d708920c0c449 ]] ||
    fail "single-range LLVM artifact drifted"
"$CC" -std=c11 -Wall -Wextra -Werror "$WORK_DIR/program.c" \
    -o "$WORK_DIR/c.exe" || fail "C artifact did not compile"
"$CLANG" -x ir "$WORK_DIR/program.ll" -o "$WORK_DIR/llvm.exe" \
    >/dev/null 2>&1 || fail "LLVM artifact did not compile"
"$WORK_DIR/c.exe" | tr -d '\r' >"$WORK_DIR/c.run"
"$WORK_DIR/llvm.exe" | tr -d '\r' >"$WORK_DIR/llvm.run"
printf '0\n1\n2\n40\n' >"$WORK_DIR/expected.run"
cmp -s "$WORK_DIR/expected.run" "$WORK_DIR/c.run" &&
    cmp -s "$WORK_DIR/c.run" "$WORK_DIR/llvm.run" ||
    fail "C/LLVM execution did not preserve lexical binding identity"

for mutation in missing-def-ref forged-def-ref forged-init-ref \
    missing-body-ref forged-body-ref body-outer-use missing-outer-use \
    orphan-outer-ref duplicate-local; do
    case "$mutation" in
        missing-def-ref)
            diagnostic='direct MIR scalar CFG LocalRef plan is invalid' ;;
        duplicate-local)
            diagnostic='direct MIR scalar CFG local inventory is invalid: source_syntax_id=' ;;
        *)
            diagnostic='direct MIR scalar CFG range LocalRef binding is invalid' ;;
    esac
    for target in c llvm; do
        artifact="$WORK_DIR/$mutation-$target.artifact"
        if (cd "$ROOT_DIR" && "$DRIVER" "--mir-json-backend=$target" \
            "$WORK_REL/$mutation.json" -o "$WORK_REL/$mutation-$target.artifact") \
            >"$WORK_DIR/$mutation-$target.out" 2>"$WORK_DIR/$mutation-$target.err"; then
            fail "$target accepted $mutation"
        fi
        [[ ! -e "$artifact" ]] || fail "$target published before rejecting $mutation"
        grep -Fq "$diagnostic" \
            "$WORK_DIR/$mutation-$target.out" "$WORK_DIR/$mutation-$target.err" ||
            fail "$target lost the LocalRef boundary diagnostic for $mutation"
    done
done

echo "[$LABEL] one LocalRef plan preserves lexical scope in exact C/LLVM execution"
