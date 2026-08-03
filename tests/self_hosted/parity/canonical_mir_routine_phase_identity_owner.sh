#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
source "$ROOT_DIR/tests/self_hosted/parity/llvm_leg_helpers.sh"
pgy_prepend_windows_runtime_paths

LABEL="self-host-parity:canonical-mir-routine-phase-identity"
fail() { echo "[$LABEL] $*" >&2; exit 1; }

PGY="$(pgy_select_optional_exe_binary "${PGY_BIN:-$ROOT_DIR/bin/pgy}")"
DRIVER="$(pgy_select_optional_exe_binary "${PGY_SELF_DRIVER_BIN:-$ROOT_DIR/bin/pgy-self-driver}")"
PYTHON_BIN="${PYTHON_BIN:-python3}"
CC_BIN="${CC:-cc}"
pgy_require_runnable_binary_here "$LABEL:pgy" "$PGY" || fail "pgy is not runnable"
pgy_require_runnable_binary_here "$LABEL:driver" "$DRIVER" || fail "self driver is not runnable"
command -v "$PYTHON_BIN" >/dev/null 2>&1 || fail "python is required"
command -v "$CC_BIN" >/dev/null 2>&1 || fail "C compiler is required"

SOURCE="tests/self_hosted/parity/fixture/intent_typed_outcome_execution.pgy"
WORK_DIR="${PGY_SELFHOST_BUILD_DIR:-$ROOT_DIR/.tmp/self_hosted/canonical_mir_routine_phase_identity}"
mkdir -p "$WORK_DIR"
BASE="$WORK_DIR/base.mir.json"
PERMUTED="$WORK_DIR/cross-kind-permuted.mir.json"
NONMONOTONIC="$WORK_DIR/function-phase-nonmonotonic.mir.json"
BASE_CANONICAL="$WORK_DIR/base.canonical.mir.json"
PERMUTED_CANONICAL="$WORK_DIR/permuted.canonical.mir.json"
BASE_AGAIN="$WORK_DIR/base.canonical-again.mir.json"
PERMUTED_AGAIN="$WORK_DIR/permuted.canonical-again.mir.json"

(cd "$ROOT_DIR" && "$DRIVER" --emit-mir-json-verified "$SOURCE") \
    | tr -d '\r' >"$BASE"
"$PYTHON_BIN" - "$BASE" "$PERMUTED" "$NONMONOTONIC" <<'PY'
import copy
import json
from pathlib import Path
import sys

base = json.loads(Path(sys.argv[1]).read_text(encoding="utf-8"))
routines = base["routines"]
main = next(i for i, row in enumerate(routines)
            if row["kind"] == "function" and row["name"] == "Main")
intent = next(i for i, row in enumerate(routines)
              if row["kind"] == "intent" and row["name"] == "RunIntent")
assert main < intent, (main, intent)
permuted = copy.deepcopy(base)
rows = permuted["routines"]
rows[main], rows[intent] = rows[intent], rows[main]
assert next(i for i, row in enumerate(rows) if row["name"] == "RunIntent") < \
       next(i for i, row in enumerate(rows) if row["name"] == "Main")
Path(sys.argv[2]).write_text(
    json.dumps(permuted, separators=(",", ":")), encoding="utf-8"
)

nonmonotonic = copy.deepcopy(base)
rows = nonmonotonic["routines"]
accepted = next(i for i, row in enumerate(rows)
                if row["kind"] == "function" and
                row["name"] == "IntentRunAccepted")
rows[accepted], rows[main] = rows[main], rows[accepted]
assert rows[accepted]["source_syntax_id"] > rows[main]["source_syntax_id"]
Path(sys.argv[3]).write_text(
    json.dumps(nonmonotonic, separators=(",", ":")), encoding="utf-8"
)
PY

canonicalize() {
    local input="$1" output="$2"
    local input_arg
    input_arg="$(pgy_selfhost_path_relative_to_root "$input")"
    (cd "$ROOT_DIR" && "$PGY" --self-driver --canonicalize-mir-json "$input_arg") \
        | tr -d '\r' >"$output"
}

canonicalize "$BASE" "$BASE_CANONICAL"
canonicalize "$PERMUTED" "$PERMUTED_CANONICAL"
canonicalize "$BASE_CANONICAL" "$BASE_AGAIN"
canonicalize "$PERMUTED_CANONICAL" "$PERMUTED_AGAIN"
cmp -s "$BASE_CANONICAL" "$PERMUTED_CANONICAL" \
    || fail "cross-kind routine permutation changed canonical MIR"
cmp -s "$BASE_CANONICAL" "$BASE_AGAIN" \
    || fail "base canonical MIR is not a fixpoint"
cmp -s "$PERMUTED_CANONICAL" "$PERMUTED_AGAIN" \
    || fail "permuted canonical MIR is not a fixpoint"

NONMONOTONIC_ARG="$(pgy_selfhost_path_relative_to_root "$NONMONOTONIC")"
set +e
(cd "$ROOT_DIR" && "$PGY" --self-driver \
    --canonicalize-mir-json "$NONMONOTONIC_ARG") \
    >"$WORK_DIR/nonmonotonic.out" 2>"$WORK_DIR/nonmonotonic.err"
nonmonotonic_rc=$?
set -e
[[ "$nonmonotonic_rc" -ne 0 ]] \
    || fail "nonmonotonic function source epoch was accepted"
if grep -Fq '"schema":"pgy.mir.v1"' "$WORK_DIR/nonmonotonic.out" "$WORK_DIR/nonmonotonic.err"; then
    fail "nonmonotonic function source epoch emitted a partial artifact"
fi
grep -Fq 'MIR top-level routine source order is invalid' "$WORK_DIR/nonmonotonic.out" "$WORK_DIR/nonmonotonic.err" \
    || fail "nonmonotonic function source epoch diagnostic drifted"

"$PYTHON_BIN" - "$BASE" "$BASE_CANONICAL" <<'PY'
import json
from pathlib import Path
import sys

base = json.loads(Path(sys.argv[1]).read_text(encoding="utf-8"))
doc = json.loads(Path(sys.argv[2]).read_text(encoding="utf-8"))
def top_level_source_order(value):
    rows = sorted(value["routines"], key=lambda row: row["source_syntax_id"])
    return [(row["kind"], row.get("owner", ""), row["name"])
            for row in rows if row["kind"] in ("function", "intent")]
assert top_level_source_order(base) == top_level_source_order(doc)
specializations = base["generic_method_specializations"]
assert len(specializations) == 1
assert specializations[0]["callable"] == "Identity"
assert specializations[0]["source_owner_syntax_id"] > 0
ids = {(row["kind"], row.get("owner", ""), row["name"]):
       row["source_syntax_id"] for row in doc["routines"]}
assert len(ids) == len(doc["routines"])
assert all(isinstance(value, int) and value > 0 for value in ids.values())
assert len(set(ids.values())) == len(ids)
assert ids[("function", "", "Main")] != ids[("intent", "", "RunIntent")]
PY

CANONICAL_ARG="$(pgy_selfhost_path_relative_to_root "$BASE_CANONICAL")"
(cd "$ROOT_DIR" && "$DRIVER" --mir-json "$CANONICAL_ARG") \
    >"$WORK_DIR/program.c"
"$CC_BIN" -x c -std=c11 -fwrapv -fno-strict-aliasing \
    -I"$ROOT_DIR/src" -I"$ROOT_DIR/src/runtime" -pthread \
    "$WORK_DIR/program.c" -o "$WORK_DIR/program.exe"
"$WORK_DIR/program.exe" | tr -d '\r' >"$WORK_DIR/program.run"
printf '%s\n' 'accepted=true' 'calls=1' 'rejected=false' 'calls=2' \
    >"$WORK_DIR/expected.run"
cmp -s "$WORK_DIR/expected.run" "$WORK_DIR/program.run" \
    || fail "canonical cross-kind program execution drifted"

echo "[$LABEL] PASS"
