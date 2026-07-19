#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
pgy_prepend_windows_runtime_paths
PGY="${PGY_BIN:-$ROOT_DIR/bin/pgy}"
SOURCE="$ROOT_DIR/tests/cases/mir_speculation_facts/main.pgy"
OUT="$ROOT_DIR/.tmp/mir-speculation-facts.json"
PYTHON_BIN="${PYTHON_BIN:-}"

if [[ -z "$PYTHON_BIN" ]]; then
    if command -v python3 >/dev/null 2>&1; then
        PYTHON_BIN="$(command -v python3)"
    elif command -v python >/dev/null 2>&1; then
        PYTHON_BIN="$(command -v python)"
    else
        echo "[mir-speculation-fact] Python is required for MIR JSON validation" >&2
        exit 1
    fi
fi

mkdir -p "$(dirname "$OUT")"
"$PGY" "$(pgy_path_for_compiler "$PGY" "$SOURCE")" --mir-json \
    > "$OUT" 2> "$OUT.stderr"

"$PYTHON_BIN" - "$OUT" <<'PY'
import json
import sys

with open(sys.argv[1], encoding="utf-8") as stream:
    document = json.load(stream)

instructions = (
    instruction
    for routine in document.get("routines", [])
    for block in routine.get("blocks", [])
    for instruction in block.get("instructions", [])
)
facts = {
    (instruction.get("expr0"), instruction.get("expr1")):
        instruction.get("speculation")
    for instruction in instructions
}
expected = {
    ("7", "chosen"): {"pure": True, "non_trapping": True},
    ("(100 / divisor)", "chosen"):
        {"pure": False, "non_trapping": False},
}
for key, fact in expected.items():
    if facts.get(key) != fact:
        raise SystemExit(
            f"missing speculation fact for expr0={key[0]!r}, expr1={key[1]!r}"
        )
PY
grep -Fq 'mir_capture_speculation_facts(&routine)' "$ROOT_DIR/src/compiler/mir.c"
grep -Fq 'mir_validate_speculation_facts(routine, error_message)' \
    "$ROOT_DIR/src/compiler/mir_fact_validate.c"
grep -Fq 'expression is missing speculation safety fact' \
    "$ROOT_DIR/src/compiler/mir_speculation_facts.c"

echo "[mir-speculation-fact] local/literal safety and composite fail-closed facts ok"
