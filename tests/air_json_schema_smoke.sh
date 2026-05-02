#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
PGY="${PGY_BIN:-$ROOT_DIR/bin/pgy}"
PGY_EXPLICIT=0
if [[ -n "${PGY_BIN:-}" ]]; then
    PGY_EXPLICIT=1
fi
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

if ! "$PGY" --help >"$WORK_DIR/pgy-help.out" 2>"$WORK_DIR/pgy-help.err"; then
    if [[ "$PGY_EXPLICIT" -eq 0 ]]; then
        echo "[air-json-schema] SKIP default compiler executable probe; schema contract remains gated when PGY_BIN is provided"
        exit 0
    fi
    echo "[air-json-schema] compiler binary is not runnable: $PGY" >&2
    cat "$WORK_DIR/pgy-help.err" >&2
    exit 1
fi

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
    '"rir_input":true' \
    '"intent_count":1' \
    '"boundary_count"' \
    '"evidence_count"' \
    '"drift_count":0' \
    '"observability"' \
    '"abi_schema":"pgy.intent.observability.v1"' \
    '"trace_schema":"pgy.intent.trace.v1"' \
    '"surfaces":["last","history","active","recent"]' \
    '"intents"' \
    '"who_from_intent_default"' \
    '"boundaries"' \
    '"source_from_intent_default"' \
    '"source_from_transfer"' \
    '"evidence"' \
    '"drifts":[]' \
    '"evidence_flags"' \
    '"rir_boundary":true' \
    '"kind":"zone"' \
    '"kind":"rir_boundary"' \
    '"kind":"hir_cfg"' \
    '"kind":"mir_cleanup"' \
    '"provider":' \
    '"subject":' \
    '"fact_count":' \
    '"fallback_count":0' \
    '"mir_cleanup_evidence_count"' \
    '"mir_pin_cleanup_evidence_count"' \
    '"dag_generic_evidence_count"' \
    '"dag_ability_evidence_count"' \
    '"rir_effect_propagation_required_count"' \
    '"rir_effect_propagation_evidence_count"' \
    '"rir_relation_propagation_required_count"' \
    '"rir_relation_propagation_evidence_count"' \
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
assert summary["rir_input"] is True
assert summary["intent_count"] == len(data["intents"])
assert summary["boundary_count"] == len(data["boundaries"])
assert summary["evidence_count"] == len(data["evidence"])
assert summary["drift_count"] == len(data["drifts"]) == 0
assert data["observability"]["abi_schema"] == "pgy.intent.observability.v1"
assert data["observability"]["trace_schema"] == "pgy.intent.trace.v1"
assert data["observability"]["surfaces"] == ["last", "history", "active", "recent"]
assert all("who_from_intent_default" in intent for intent in data["intents"])
assert all("source_from_intent_default" in boundary for boundary in data["boundaries"])
assert all("source_from_transfer" in boundary for boundary in data["boundaries"])
assert any(b["kind"] == "zone" and b["evidence_flags"]["rir_boundary"] for b in data["boundaries"])
assert any(e["kind"] == "rir_boundary" for e in data["evidence"])
assert any(e["kind"] == "hir_cfg" for e in data["evidence"])
assert all("provider" in e and "subject" in e for e in data["evidence"])
assert all("fact_count" in e and "fallback_count" in e for e in data["evidence"])
assert all(e["fallback_count"] == 0 for e in data["evidence"])
if summary["dag_generic_evidence_count"] > 0:
    assert any(e["kind"] == "dag_generic" for e in data["evidence"])
if summary["dag_ability_evidence_count"] > 0:
    assert any(e["kind"] == "dag_ability" for e in data["evidence"])
assert any(e["kind"] == "mir_cleanup" for e in data["evidence"])
assert summary["mir_cleanup_evidence_count"] >= 1
assert summary["mir_pin_cleanup_evidence_count"] >= 0
assert summary["rir_effect_propagation_evidence_count"] <= summary["rir_effect_propagation_required_count"]
assert summary["rir_relation_propagation_evidence_count"] <= summary["rir_relation_propagation_required_count"]
assert all("location" in b and b["location"]["line"] > 0 for b in data["boundaries"])
print("[air-json-schema] parsed schema ok")
PY
else
    echo "[air-json-schema] python not found; grep contract ok"
fi
