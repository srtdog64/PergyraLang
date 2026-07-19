#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
pgy_prepend_windows_runtime_paths
PGY="${PGY_BIN:-$ROOT_DIR/bin/pgy}"
SOURCE="$ROOT_DIR/tests/air_erasure/fixtures/03_secure_slot.pgy"
OUT="$ROOT_DIR/.tmp/destructure-type-facts.json"
PYTHON_BIN="${PYTHON_BIN:-}"

if [[ -z "$PYTHON_BIN" ]]; then
    if command -v python3 >/dev/null 2>&1; then
        PYTHON_BIN="$(command -v python3)"
    elif command -v python >/dev/null 2>&1; then
        PYTHON_BIN="$(command -v python)"
    else
        echo "[destructure-type-fact] Python is required" >&2
        exit 1
    fi
fi

mkdir -p "$(dirname "$OUT")"
"$PGY" "$(pgy_path_for_compiler "$PGY" "$SOURCE")" --mir-json >"$OUT"

"$PYTHON_BIN" - "$ROOT_DIR" "$OUT" <<'PY'
import json
import pathlib
import sys

root = pathlib.Path(sys.argv[1])
with open(sys.argv[2], encoding="utf-8") as stream:
    document = json.load(stream)

main = next(
    (routine for routine in document.get("routines", [])
     if routine.get("name") == "Main"),
    None,
)
expected = [
    (0, 2, "SecureSlot<Int>"),
    (1, 2, "Token<Int>"),
]
actual = [] if main is None else [
    (row.get("binding_index"), row.get("binding_count"),
     row.get("binding_type"))
    for row in main.get("destructure_type_facts", [])
]
if main is None or main.get("destructure_type_fact_count") != 2 \
        or actual != expected:
    raise SystemExit(
        f"secure destructure type facts drifted: count="
        f"{None if main is None else main.get('destructure_type_fact_count')} "
        f"rows={actual!r}"
    )

source = (root / "src/compiler/mir_source_local_types.c").read_text(
    encoding="utf-8"
)
case = source.split("case AST_LET_DESTRUCTURE:", 1)[1].split(
    "case AST_BLOCK:", 1
)[0]
for forbidden in (
    "mir_source_local_expr_type_name",
    "mir_source_local_unwrap_array_or_slice_type",
    "mir_source_local_tuple_element_type",
):
    if forbidden in case:
        raise SystemExit(
            f"destructure source-local typing reopened fallback: {forbidden}"
        )

test = (root / "src/tests/mir/test_mir_lowering_part_i.cases.h").read_text(
    encoding="utf-8"
)
for required in (
    "MIR destructure local typing fails when semantic fact is missing",
    "missing or invalid source-local type fact",
):
    if required not in test:
        raise SystemExit(f"missing destructure negative gate term: {required}")
PY

# Closure claim: a missing semantic destructure fact must fail MIR lowering.
echo "[destructure-type-fact] semantic owner, MIR consumer, and missing-fact gate ok"
