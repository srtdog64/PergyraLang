#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
PGY="${PGY_BIN:-$ROOT_DIR/bin/pgy}"
if [[ "$PGY" != *.exe && -x "${PGY}.exe" ]]; then
    PGY="${PGY}.exe"
fi
if [[ ! -x "$PGY" ]]; then
    echo "[air-json-schema] missing compiler binary: $PGY" >&2
    exit 1
fi

TMP_BASE="${TMPDIR:-${TEMP:-/tmp}}"
WORK_DIR="$(mktemp -d "${TMP_BASE%/}/pgy_air_json_schema.XXXXXX")"
trap 'rm -rf "$WORK_DIR"' EXIT

SOURCE="$ROOT_DIR/tests/cases/backend_compare/intent_zone_binding/main.pgy"
OUT="$WORK_DIR/air.json"
ERR="$WORK_DIR/air.err"

"$PGY" --air-json "$SOURCE" --backend=c > "$OUT" 2> "$ERR"

require_text() {
    local text="$1"
    if ! grep -Fq "$text" "$OUT"; then
        echo "[air-json-schema] missing JSON term: $text" >&2
        echo "---- air json ----" >&2
        sed -n '1,80p' "$OUT" >&2
        exit 1
    fi
}

for required in \
    '"schema":"pgy.air.graph.v1"' \
    '"summary"' \
    '"strict_evidence":true' \
    '"intent_count":1' \
    '"boundary_count"' \
    '"evidence_count"' \
    '"drift_count":0' \
    '"observability"' \
    '"abi_schema":"pgy.intent.observability.v1"' \
    '"trace_schema":"pgy.intent.trace.v1"' \
    '"surfaces":["last","history","active","recent"]' \
    '"intents"' \
    '"boundaries"' \
    '"evidence"' \
    '"drifts":[]' \
    '"evidence_flags"' \
    '"rir_boundary":true' \
    '"kind":"zone"' \
    '"kind":"rir_boundary"' \
    '"kind":"hir_cfg"' \
    '"kind":"dag_generic"' \
    '"kind":"dag_ability"' \
    '"kind":"mir_cleanup"' \
    '"location":{"line":' \
    '"authority_names"'; do
    require_text "$required"
done

PY_BIN=""
if command -v python3 >/dev/null 2>&1; then
    PY_BIN="$(command -v python3)"
elif command -v python >/dev/null 2>&1; then
    PY_BIN="$(command -v python)"
fi

if [[ -n "$PY_BIN" ]]; then
    "$PY_BIN" - "$OUT" <<'PY'
import json
import sys

with open(sys.argv[1], "r", encoding="utf-8") as fh:
    data = json.load(fh)

assert data["schema"] == "pgy.air.graph.v1"
summary = data["summary"]
assert summary["strict_evidence"] is True
assert summary["intent_count"] == len(data["intents"])
assert summary["boundary_count"] == len(data["boundaries"])
assert summary["evidence_count"] == len(data["evidence"])
assert summary["drift_count"] == len(data["drifts"]) == 0
assert data["observability"]["abi_schema"] == "pgy.intent.observability.v1"
assert data["observability"]["trace_schema"] == "pgy.intent.trace.v1"
assert data["observability"]["surfaces"] == ["last", "history", "active", "recent"]
assert any(b["kind"] == "zone" and b["evidence_flags"]["rir_boundary"] for b in data["boundaries"])
assert any(e["kind"] == "rir_boundary" for e in data["evidence"])
assert any(e["kind"] == "hir_cfg" for e in data["evidence"])
assert any(e["kind"] == "dag_generic" for e in data["evidence"])
assert any(e["kind"] == "dag_ability" for e in data["evidence"])
assert any(e["kind"] == "mir_cleanup" for e in data["evidence"])
assert all("location" in b and b["location"]["line"] > 0 for b in data["boundaries"])
print("[air-json-schema] parsed schema ok")
PY
else
    echo "[air-json-schema] python not found; grep contract ok"
fi
