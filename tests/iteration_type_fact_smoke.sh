#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
pgy_prepend_windows_runtime_paths

PGY="${PGY_BIN:-$ROOT_DIR/bin/pgy.exe}"
if [[ "$PGY" != *.exe ]] && pgy_binary_expects_windows_paths "${PGY}.exe"; then
    PGY="${PGY}.exe"
fi
if [[ ! -x "$PGY" ]]; then
    echo "[iteration-type-fact] missing compiler binary: $PGY" >&2
    exit 1
fi

TMP_BASE="${TMPDIR:-${TEMP:-/tmp}}"
WORK_DIR="$(mktemp -d "${TMP_BASE%/}/pgy_iteration_fact.XXXXXX")"
trap 'rm -rf "$WORK_DIR"' EXIT
JSON="$WORK_DIR/loop.json"
(cd "$ROOT_DIR" && "$PGY" --test-native-mir-json-oracle \
    "$(pgy_path_for_compiler "$PGY" "$ROOT_DIR/examples/break_continue.pgy")" \
    >"$JSON")
grep -Fq '"iteration_type_fact_count":1' "$JSON"
grep -Fq '"iteration_type_facts":[{"function_syntax_id":' "$JSON"
grep -Fq '"binding_type":"Int"' "$JSON"
grep -Fq '"iterable_type":"Int"' "$JSON"

echo "[iteration-type-fact] for-loop type fact survives semantic, MIR, and JSON anchors"
