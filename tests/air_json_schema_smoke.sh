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
    echo "[air-json-schema] SKIP executable probe; missing compiler binary: $PGY"
    exit 0
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
SELECT_SOURCE="$WORK_DIR/air_select_receive.pgy"
SELECT_OUT="$WORK_DIR/air_select_receive.json"
SELECT_ERR="$WORK_DIR/air_select_receive.err"

cat > "$SELECT_SOURCE" <<'EOF'
func SelectReceiveAir(ch: Channel<Int>) -> Int {
    ch <- 7;
    select {
        case v = <-ch:
            return v;
        default:
            return 0;
    }
    return 0;
}
EOF

"$PGY" --air-json "$SOURCE" --backend=c > "$OUT" 2> "$ERR"
"$PGY" --air-json "$SELECT_SOURCE" --backend=c > "$SELECT_OUT" 2> "$SELECT_ERR"

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
    '"runtime_frontier_policy"' \
    '"schema":"pgy.runtime.frontier-policy.v1"' \
    '"subject":"bounded-frontier-pass-limit"' \
    '"fact_count":9' \
    '"intents"' \
    '"who_from_intent_default"' \
    '"who_from_on_receiver"' \
    '"who_from_single_participant"' \
    '"requires_from_action"' \
    '"causes_from_action"' \
    '"boundaries"' \
    '"source_from_intent_default"' \
    '"source_from_action"' \
    '"source_from_transfer"' \
    '"authority_from_zone"' \
    '"authority_from_action"' \
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
    '"boundary_kind":' \
    '"boundary_owner":' \
    '"boundary_source":' \
    '"fact_count":' \
    '"fallback_count":0' \
    '"mir_cleanup_evidence_count"' \
    '"mir_pin_cleanup_evidence_count"' \
    '"mir_terminator_evidence_count"' \
    '"mir_select_receive_evidence_count"' \
    '"dag_metadata_evidence_count"' \
    '"dag_generic_evidence_count"' \
    '"dag_ability_evidence_count"' \
    '"rir_effect_propagation_required_count"' \
    '"rir_effect_propagation_evidence_count"' \
    '"rir_relation_propagation_required_count"' \
    '"rir_relation_propagation_evidence_count"' \
    '"observability_schema_evidence_count"' \
    '"runtime_frontier_policy_evidence_count"' \
    '"kind":"observability_schema"' \
    '"kind":"runtime_frontier_policy"' \
    '"provider":"runtime-observability-schema"' \
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
    "$PY_BIN" - "$OUT" "$SELECT_OUT" <<'PY'
import json
import sys

with open(sys.argv[1], "r", encoding="utf-8") as fh:
    data = json.load(fh)
with open(sys.argv[2], "r", encoding="utf-8") as fh:
    select_data = json.load(fh)

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
assert data["runtime_frontier_policy"]["schema"] == "pgy.runtime.frontier-policy.v1"
assert data["runtime_frontier_policy"]["subject"] == "bounded-frontier-pass-limit"
assert data["runtime_frontier_policy"]["fact_count"] == 9
assert all("who_from_intent_default" in intent for intent in data["intents"])
assert all("who_from_on_receiver" in intent for intent in data["intents"])
assert all("who_from_single_participant" in intent for intent in data["intents"])
assert all("requires_from_action" in intent for intent in data["intents"])
assert all("causes_from_action" in intent for intent in data["intents"])
assert all("source_from_intent_default" in boundary for boundary in data["boundaries"])
assert all("source_from_action" in boundary for boundary in data["boundaries"])
assert all("source_from_transfer" in boundary for boundary in data["boundaries"])
assert all("authority_from_zone" in boundary for boundary in data["boundaries"])
assert all("authority_from_action" in boundary for boundary in data["boundaries"])
assert any(b["kind"] == "zone" and b["evidence_flags"]["rir_boundary"] for b in data["boundaries"])
assert any(e["kind"] == "rir_boundary" for e in data["evidence"])
assert any(e["kind"] == "hir_cfg" for e in data["evidence"])
assert all("provider" in e and "subject" in e for e in data["evidence"])
assert all("boundary_kind" in e for e in data["evidence"])
assert all("boundary_owner" in e for e in data["evidence"])
assert all("boundary_source" in e for e in data["evidence"])
assert all(e["boundary_kind"] for e in data["evidence"] if e["boundary"] is not None)
assert all(e["boundary_source"] for e in data["evidence"] if e["boundary"] is not None)
assert all("fact_count" in e and "fallback_count" in e for e in data["evidence"])
assert all(e["fallback_count"] == 0 for e in data["evidence"])
if summary["dag_metadata_evidence_count"] > 0:
    assert any(e["kind"] == "dag_metadata" for e in data["evidence"])
if summary["dag_generic_evidence_count"] > 0:
    assert any(e["kind"] == "dag_generic" for e in data["evidence"])
if summary["dag_ability_evidence_count"] > 0:
    assert any(e["kind"] == "dag_ability" for e in data["evidence"])
assert any(e["kind"] == "mir_cleanup" for e in data["evidence"])
assert summary["mir_cleanup_evidence_count"] >= 1
assert summary["mir_pin_cleanup_evidence_count"] >= 0
assert summary["mir_terminator_evidence_count"] >= 0
assert summary["mir_select_receive_evidence_count"] >= 0
if summary["mir_select_receive_evidence_count"] > 0:
    assert any(e["kind"] == "mir_select_receive" for e in data["evidence"])
select_summary = select_data["summary"]
assert select_data["schema"] == "pgy.air.graph.v1"
assert select_summary["mir_select_receive_evidence_count"] >= 1
assert any(
    e["kind"] == "mir_select_receive"
    and e["subject"] == "select-receive"
    and e["fact_count"] >= 1
    and e["fallback_count"] == 0
    for e in select_data["evidence"]
)
assert summary["rir_effect_propagation_evidence_count"] <= summary["rir_effect_propagation_required_count"]
assert summary["rir_relation_propagation_evidence_count"] <= summary["rir_relation_propagation_required_count"]
assert summary["observability_schema_evidence_count"] == 1
assert any(e["kind"] == "observability_schema" and e["provider"] == "runtime-observability-schema" for e in data["evidence"])
assert summary["runtime_frontier_policy_evidence_count"] == 1
assert any(e["kind"] == "runtime_frontier_policy" and e["provider"] == "pgy.runtime.frontier-policy.v1" for e in data["evidence"])
assert all("location" in b and b["location"]["line"] > 0 for b in data["boundaries"])
print("[air-json-schema] parsed schema ok")
PY
else
    require_select_text() {
        local text="$1"
        if ! grep -Fq "$text" "$SELECT_OUT"; then
            echo "[air-json-schema] missing select JSON term: $text" >&2
            echo "---- select air json ----" >&2
            sed -n '1,80p' "$SELECT_OUT" >&2
            exit 1
        fi
    }
    require_select_text '"kind":"mir_select_receive"'
    require_select_text '"subject":"select-receive"'
    echo "[air-json-schema] python not found; grep contract ok"
fi
