#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
PGY="${PGY_BIN:-$ROOT_DIR/bin/pgy}"
SOURCE="$ROOT_DIR/tests/cases/mir_speculation_facts/main.pgy"
OUT="$ROOT_DIR/.tmp/mir-speculation-facts.json"

mkdir -p "$(dirname "$OUT")"
"$PGY" "$SOURCE" --mir-json > "$OUT" 2> "$OUT.stderr"

grep -Fq '"expr0":"7","expr1":"chosen","speculation":{"pure":true,"non_trapping":true}' "$OUT"
grep -Fq '"speculation":{"pure":true,"non_trapping":true}' "$OUT"
grep -Fq '"expr0":"(100 / divisor)"' "$OUT"
grep -Fq '"expr0":"(100 / divisor)","expr1":"chosen","speculation":{"pure":false,"non_trapping":false}' "$OUT"
grep -Fq 'mir_capture_speculation_facts(&routine)' "$ROOT_DIR/src/compiler/mir.c"
grep -Fq 'mir_validate_speculation_facts(routine, error_message)' \
    "$ROOT_DIR/src/compiler/mir_fact_validate.c"
grep -Fq 'expression is missing speculation safety fact' \
    "$ROOT_DIR/src/compiler/mir_speculation_facts.c"

echo "[mir-speculation-fact] local/literal safety and composite fail-closed facts ok"
