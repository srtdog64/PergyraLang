#!/usr/bin/env bash
set -euo pipefail

# Canonical nested intents must remain placement-free direct calls through the
# public self-host C route. Native C is the independent execution oracle.

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
pgy_prepend_windows_runtime_paths
export PATH

fail() {
    echo "[self-host-nested-intent-execution] $*" >&2
    exit 1
}

PGY="${PGY_BIN:-$ROOT_DIR/bin/pgy}"
DRIVER="${PGY_SELFHOST_PREBUILT_DRIVER:-}"
PYTHON_BIN="${PYTHON_BIN:-python3}"
BUILD_DIR="${PGY_SELFHOST_NESTED_INTENT_BUILD_DIR:-$ROOT_DIR/.tmp/self_hosted/intent_nested_callable_execution}"
SOURCE_REL="examples/composite_intent_orchestration/main.pgy"
SOURCE="$ROOT_DIR/$SOURCE_REL"

[[ -n "$DRIVER" ]] || fail "prebuilt production driver is required"
PGY="$(pgy_select_optional_exe_binary "$PGY")"
DRIVER="$(pgy_select_optional_exe_binary "$DRIVER")"
pgy_require_runnable_binary_here "self-host-nested-intent-execution" "$PGY" ||
    fail "PGY_BIN is not runnable"
pgy_require_runnable_binary_here "self-host-nested-intent-execution" "$DRIVER" ||
    fail "prebuilt driver is not runnable"
command -v "$PYTHON_BIN" >/dev/null 2>&1 || fail "python is required"
mkdir -p "$BUILD_DIR"

"$PYTHON_BIN" - "$SOURCE" "$BUILD_DIR/failure.pgy" <<'PY'
from pathlib import Path
import sys

text = Path(sys.argv[1]).read_text(encoding="utf-8")
needle = "reserved = reserved + 1;"
if text.count(needle) != 1:
    raise SystemExit("canonical reserve mutation anchor drifted")
Path(sys.argv[2]).write_text(
    text.replace(needle, "reserved = reserved + 0;"),
    encoding="utf-8", newline="\n",
)
PY

success_c="$BUILD_DIR/success.self.c"
(cd "$ROOT_DIR" && "$DRIVER" \
    "$SOURCE_REL" --emit-c-verified \
    >"$success_c" 2>"$BUILD_DIR/success.self.err") ||
    { cat "$BUILD_DIR/success.self.err" >&2; fail "canonical self C emission failed"; }

"$PYTHON_BIN" - "$success_c" <<'PY'
from pathlib import Path
import re
import sys

text = Path(sys.argv[1]).read_text(encoding="utf-8")
def body(name):
    match = re.search(rf"bool {name}\([^;]+\)\n\{{", text)
    if not match:
        raise SystemExit(f"missing {name} definition")
    start = match.end() - 1
    depth = 0
    for index in range(start, len(text)):
        depth += text[index] == "{"
        depth -= text[index] == "}"
        if depth == 0:
            return text[start:index + 1]
    raise SystemExit(f"unterminated {name} definition")

fulfill = body("FulfillOrder")
process = body("ProcessOrder")
for anchor in ("ReserveStock(", "ChargeBuyer(", "ShipParcel("):
    if anchor not in fulfill:
        raise SystemExit(f"FulfillOrder lost direct nested call {anchor}")
if "FulfillOrder(" not in process:
    raise SystemExit("ProcessOrder lost direct nested call")
for name, block in (("FulfillOrder", fulfill), ("ProcessOrder", process)):
    if "_sync(" in block or "trace_materialize" in block:
        raise SystemExit(f"{name} regained fake placement materialization")
PY

suffix=""
[[ "$PGY" == *.exe ]] && suffix=".exe"
driver_for_pgy="$(pgy_path_for_compiler "$PGY" "$DRIVER")"
for case_name in success failure; do
    source_rel="$SOURCE_REL"
    [[ "$case_name" == failure ]] &&
        source_rel="${BUILD_DIR#"$ROOT_DIR/"}/failure.pgy"
    public_exe="$BUILD_DIR/$case_name.public$suffix"
    native_exe="$BUILD_DIR/$case_name.native$suffix"
    public_arg="$(pgy_path_for_compiler "$PGY" "$public_exe")"
    native_arg="$(pgy_path_for_compiler "$PGY" "$native_exe")"
    (cd "$ROOT_DIR" && env PGY_SELF_DRIVER_BIN="$driver_for_pgy" \
        PGY_DEBUG_PIPELINE_TIMING=1 "$PGY" "$source_rel" --backend=c \
        -o "$public_arg" >"$BUILD_DIR/$case_name.public.compile.out" \
        2>"$BUILD_DIR/$case_name.public.compile.err") ||
        { cat "$BUILD_DIR/$case_name.public.compile.err" >&2; fail "$case_name public C compile failed"; }
    ! grep -Fq '[pipeline timing]' "$BUILD_DIR/$case_name.public.compile.err" ||
        fail "$case_name public route re-entered the native compiler"
    (cd "$ROOT_DIR" && "$PGY" "$source_rel" --native-pipeline --backend=c \
        -o "$native_arg" >"$BUILD_DIR/$case_name.native.compile.out" \
        2>"$BUILD_DIR/$case_name.native.compile.err") ||
        { cat "$BUILD_DIR/$case_name.native.compile.err" >&2; fail "$case_name native C compile failed"; }
    "$public_exe" | tr -d '\r' >"$BUILD_DIR/$case_name.public.run"
    "$native_exe" | tr -d '\r' >"$BUILD_DIR/$case_name.native.run"
    cmp -s "$BUILD_DIR/$case_name.public.run" "$BUILD_DIR/$case_name.native.run" ||
        fail "$case_name public/native output differs"
done

grep -Fxq '[Intent] ProcessOrder=true' "$BUILD_DIR/success.public.run" ||
    fail "canonical nested success did not propagate"
grep -Fxq '[Intent] ProcessOrder=false' "$BUILD_DIR/failure.public.run" ||
    fail "nested intent failure did not propagate"

echo "[self-host-nested-intent-execution] public C direct-call success/failure parity: PASS"
