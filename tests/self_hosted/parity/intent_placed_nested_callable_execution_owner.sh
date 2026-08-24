#!/usr/bin/env bash
set -euo pipefail

# A direct intent target with an outer zone placement remains an action-shaped
# step boundary. Only placement-free direct intent calls use the nested lane.

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
pgy_prepend_windows_runtime_paths
export PATH

fail() {
    echo "[self-host-placed-nested-intent] $*" >&2
    exit 1
}

PGY="$(pgy_select_optional_exe_binary "${PGY_BIN:-$ROOT_DIR/bin/pgy}")"
DRIVER="$(pgy_select_optional_exe_binary "${PGY_SELFHOST_PREBUILT_DRIVER:-}")"
PYTHON_BIN="${PYTHON_BIN:-python3}"
BUILD_DIR="${PGY_SELFHOST_PLACED_NESTED_BUILD_DIR:-$ROOT_DIR/.tmp/self_hosted/intent_placed_nested_callable_execution}"
SOURCE_REL="tests/self_hosted/parity/fixture/intent_placed_nested_execution.pgy"

pgy_require_runnable_binary_here "self-host-placed-nested-intent" "$PGY" ||
    fail "PGY_BIN is not runnable"
pgy_require_runnable_binary_here "self-host-placed-nested-intent" "$DRIVER" ||
    fail "prebuilt production driver is required"
command -v "$PYTHON_BIN" >/dev/null 2>&1 || fail "python is required"
mkdir -p "$BUILD_DIR"

self_c="$BUILD_DIR/placed.self.c"
(cd "$ROOT_DIR" && "$DRIVER" "$SOURCE_REL" --emit-c-verified \
    >"$self_c" 2>"$BUILD_DIR/placed.self.err") ||
    { cat "$BUILD_DIR/placed.self.err" >&2; fail "self C emission failed"; }

"$PYTHON_BIN" - "$self_c" <<'PY'
from pathlib import Path
import re
import sys

text = Path(sys.argv[1]).read_text(encoding="utf-8")
match = re.search(r"bool FrontendPipeline\([^;]+\)\n\{", text)
if not match:
    raise SystemExit("missing FrontendPipeline definition")
start = match.end() - 1
depth = 0
block = ""
for index in range(start, len(text)):
    depth += text[index] == "{"
    depth -= text[index] == "}"
    if depth == 0:
        block = text[start:index + 1]
        break
for anchor in (
    "(*intake).source = (*source);",
    "SourceIntakeZone_sync(intake);",
    "IntakeSource(&(*intake), &(*intake).source, paths);",
    "(*source) = (*intake).source;",
):
    if anchor not in block:
        raise SystemExit(f"placed nested call lost outer boundary: {anchor}")
PY

suffix=""
[[ "$PGY" == *.exe ]] && suffix=".exe"
driver_for_pgy="$(pgy_path_for_compiler "$PGY" "$DRIVER")"
public_exe="$BUILD_DIR/placed.public$suffix"
native_exe="$BUILD_DIR/placed.native$suffix"
(cd "$ROOT_DIR" && env PGY_SELF_DRIVER_BIN="$driver_for_pgy" "$PGY" \
    "$SOURCE_REL" --backend=c \
    -o "$(pgy_path_for_compiler "$PGY" "$public_exe")" \
    >"$BUILD_DIR/placed.public.compile.out" \
    2>"$BUILD_DIR/placed.public.compile.err") ||
    { cat "$BUILD_DIR/placed.public.compile.err" >&2; fail "public compile failed"; }
(cd "$ROOT_DIR" && "$PGY" "$SOURCE_REL" --native-pipeline --backend=c \
    -o "$(pgy_path_for_compiler "$PGY" "$native_exe")" \
    >"$BUILD_DIR/placed.native.compile.out" \
    2>"$BUILD_DIR/placed.native.compile.err") ||
    { cat "$BUILD_DIR/placed.native.compile.err" >&2; fail "native compile failed"; }
"$public_exe" | tr -d '\r' >"$BUILD_DIR/placed.public.run"
"$native_exe" | tr -d '\r' >"$BUILD_DIR/placed.native.run"
cmp -s "$BUILD_DIR/placed.public.run" "$BUILD_DIR/placed.native.run" ||
    fail "public/native output differs"
grep -Fxq '[Intent] FrontendPipeline=true' "$BUILD_DIR/placed.public.run" ||
    fail "placed nested intent boundary did not execute"

echo "[self-host-placed-nested-intent] outer placement execution parity: PASS"
