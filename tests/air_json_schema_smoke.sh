#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
pgy_prepend_windows_runtime_paths
PGY="${PGY_BIN:-$ROOT_DIR/bin/pgy}"
PGY_EXPLICIT=0
if [[ -n "${PGY_BIN:-}" ]]; then
    PGY_EXPLICIT=1
fi

PGY="$(pgy_select_optional_exe_binary "$PGY")"
if [[ ! -x "$PGY" ]]; then
    if [[ "$PGY_EXPLICIT" -eq 0 ]]; then
        echo "[air-json-schema] SKIP executable probe; missing compiler binary: $PGY"
        exit 0
    fi
    echo "[air-json-schema] missing compiler binary: $PGY" >&2
    exit 1
fi
if ! pgy_binary_is_runnable_here "$PGY"; then
    if [[ "$PGY_EXPLICIT" -eq 0 ]]; then
        echo "[air-json-schema] SKIP default compiler executable probe; binary is not runnable on this host: $PGY"
        exit 0
    fi
    pgy_require_runnable_binary_here "air-json-schema" "$PGY"
fi

TMP_BASE="${TMPDIR:-${TEMP:-/tmp}}"
if pgy_binary_expects_windows_paths "$PGY"; then
    TMP_BASE="$ROOT_DIR/.tmp"
    mkdir -p "$TMP_BASE"
fi
WORK_DIR="$(mktemp -d "${TMP_BASE%/}/pgy_air_json_schema.XXXXXX")"
trap 'rm -rf "$WORK_DIR"' EXIT

to_native_path_for_pgy() {
    pgy_path_for_compiler "$PGY" "$1"
}

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
BOILERPLATE_SOURCE="$ROOT_DIR/tests/cases/backend_compare/boilerplate_reduction/main.pgy"
BOILERPLATE_OUT="$WORK_DIR/air_boilerplate.json"
BOILERPLATE_ERR="$WORK_DIR/air_boilerplate.err"
WORLD_SOURCE="$ROOT_DIR/tests/cases/backend_compare/intent_cross_world_transfer/main.pgy"
WORLD_OUT="$WORK_DIR/air_world_transfer.json"
WORLD_ERR="$WORK_DIR/air_world_transfer.err"
LIFECYCLE_SOURCE="$ROOT_DIR/tests/air_erasure/fixtures/06_lifecycle_branch.pgy"
LIFECYCLE_OUT="$WORK_DIR/air_lifecycle.json"
LIFECYCLE_ERR="$WORK_DIR/air_lifecycle.err"
SELECT_SOURCE="$WORK_DIR/air_select_receive.pgy"
SELECT_OUT="$WORK_DIR/air_select_receive.json"
SELECT_ERR="$WORK_DIR/air_select_receive.err"

cat > "$SELECT_SOURCE" <<'EOF'
func SelectReceiveAir() -> Int {
    let ch: Channel<Int> = Channel(1);
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

"$PGY" --air-json "$(to_native_path_for_pgy "$SOURCE")" --backend=c > "$OUT" 2> "$ERR"
"$PGY" --air-json "$(to_native_path_for_pgy "$BOILERPLATE_SOURCE")" --backend=c > "$BOILERPLATE_OUT" 2> "$BOILERPLATE_ERR"
"$PGY" --air-json "$(to_native_path_for_pgy "$WORLD_SOURCE")" --backend=c > "$WORLD_OUT" 2> "$WORLD_ERR"
"$PGY" --air-json "$(to_native_path_for_pgy "$LIFECYCLE_SOURCE")" --backend=c > "$LIFECYCLE_OUT" 2> "$LIFECYCLE_ERR"
"$PGY" --air-json "$(to_native_path_for_pgy "$SELECT_SOURCE")" --backend=c > "$SELECT_OUT" 2> "$SELECT_ERR"

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
    '"pass_limit_fact_count":10' \
    '"overflow_reason_fact_count":5' \
    '"fact_count":15' \
    '"intents"' \
    '"compression_budget"' \
    '"compression_reason"' \
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
    '"kind":"mir_terminator"' \
    '"provider_kind":' \
    '"subject_kind":' \
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
    '"unproven_retain_count"' \
    '"inherent_concurrency_count"' \
    '"slot_capability_retain_count"' \
    '"lifecycle_state_space_count"' \
    '"lifecycle_state_spaces"' \
    '"kind":"observability_schema"' \
    '"kind":"runtime_frontier_policy"' \
    '"provider":"runtime-observability-schema"' \
    '"location":{"line":' \
    '"boundary_capture"' \
    '"captures_pin"' \
    '"captures_live_view"' \
    '"captures_raw_slot"' \
    '"captures_raw_channel"' \
    '"captures_value_only"' \
    '"crosses_authority_boundary"' \
    '"requires_movability"' \
    '"has_io_or_ffi_effect"' \
    '"is_await_heavy_local"' \
    '"is_deterministic_fork_join"' \
    '"is_concurrent_site"' \
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
    "$PY_BIN" - "$OUT" "$SELECT_OUT" "$BOILERPLATE_OUT" "$WORLD_OUT" "$LIFECYCLE_OUT" <<'PY'
import json
import sys

with open(sys.argv[1], "r", encoding="utf-8") as fh:
    data = json.load(fh)
with open(sys.argv[2], "r", encoding="utf-8") as fh:
    select_data = json.load(fh)
with open(sys.argv[3], "r", encoding="utf-8") as fh:
    boilerplate_data = json.load(fh)
with open(sys.argv[4], "r", encoding="utf-8") as fh:
    world_data = json.load(fh)
with open(sys.argv[5], "r", encoding="utf-8") as fh:
    lifecycle_data = json.load(fh)

assert data["schema"] == "pgy.air.graph.v1"
summary = data["summary"]
assert summary["strict_evidence"] is True
assert summary["rir_input"] is True
assert summary["intent_count"] == len(data["intents"])
assert summary["boundary_count"] == len(data["boundaries"])
assert summary["evidence_count"] == len(data["evidence"])
assert summary["drift_count"] == len(data["drifts"]) == 0
assert summary["lifecycle_state_space_count"] == len(data["lifecycle_state_spaces"]) == 0
assert data["observability"]["abi_schema"] == "pgy.intent.observability.v1"
assert data["observability"]["trace_schema"] == "pgy.intent.trace.v1"
assert data["observability"]["surfaces"] == ["last", "history", "active", "recent"]
assert data["runtime_frontier_policy"]["schema"] == "pgy.runtime.frontier-policy.v1"
assert data["runtime_frontier_policy"]["subject"] == "bounded-frontier-pass-limit"
assert data["runtime_frontier_policy"]["pass_limit_fact_count"] == 10
assert data["runtime_frontier_policy"]["overflow_reason_fact_count"] == 5
assert data["runtime_frontier_policy"]["fact_count"] == 15
assert all("who_from_intent_default" in intent for intent in data["intents"])
assert all("who_from_on_receiver" in intent for intent in data["intents"])
assert all("who_from_single_participant" in intent for intent in data["intents"])
assert all("requires_from_action" in intent for intent in data["intents"])
assert all("causes_from_action" in intent for intent in data["intents"])
assert all("compression_budget" in intent for intent in data["intents"])
assert all("compression_reason" in intent for intent in data["intents"])
assert all(
    intent["compression_budget"] in {"retain", "summarize", "erase", "forbid"}
    for intent in data["intents"]
)
assert all("source_from_intent_default" in boundary for boundary in data["boundaries"])
assert all("source_from_action" in boundary for boundary in data["boundaries"])
assert all("source_from_transfer" in boundary for boundary in data["boundaries"])
assert all("authority_from_zone" in boundary for boundary in data["boundaries"])
assert all("authority_from_action" in boundary for boundary in data["boundaries"])
assert all("boundary_capture" in boundary for boundary in data["boundaries"])
capture_keys = {
    "captures_pin",
    "captures_live_view",
    "captures_raw_slot",
    "captures_raw_channel",
    "captures_value_only",
    "crosses_authority_boundary",
    "requires_movability",
    "has_io_or_ffi_effect",
    "is_await_heavy_local",
    "is_deterministic_fork_join",
    "is_concurrent_site",
}
assert all(
    capture_keys <= set(boundary["boundary_capture"].keys())
    for boundary in data["boundaries"]
)
assert any(
    b["kind"] == "zone"
    and b["execution_lane"] == "PinnedZone"
    and b["boundary_capture"]["captures_pin"] is True
    for b in data["boundaries"]
)
assert all("compression_budget" in boundary for boundary in data["boundaries"])
assert all("compression_reason" in boundary for boundary in data["boundaries"])
assert all(
    boundary["compression_budget"] in {"retain", "summarize", "erase", "forbid"}
    for boundary in data["boundaries"]
)
assert any(
    b["kind"] == "zone" and b["compression_budget"] in {"retain", "summarize"}
    for b in data["boundaries"]
)
assert any(
    b["kind"] == "zone" and b["compression_budget"] == "retain"
    for b in data["boundaries"]
)
assert any(
    b["kind"] == "zone" and b["compression_budget"] == "summarize"
    for b in boilerplate_data["boundaries"]
)
assert any(
    intent["compression_budget"] == "summarize"
    for intent in boilerplate_data["intents"]
)
assert any(
    b["kind"] == "world" and b["compression_budget"] == "retain"
    for b in world_data["boundaries"]
)
assert any(
    b["kind"] == "world"
    and b["source_from_transfer"] is True
    and "runtime-visible coordination" in b["compression_reason"]
    for b in world_data["boundaries"]
)
assert any(b["kind"] == "zone" and b["evidence_flags"]["rir_boundary"] for b in data["boundaries"])
assert any(e["kind"] == "rir_boundary" for e in data["evidence"])
assert any(e["kind"] == "hir_cfg" for e in data["evidence"])
assert all("provider_kind" in e and "subject_kind" in e for e in data["evidence"])
assert all("provider" in e and "subject" in e for e in data["evidence"])
assert any(e["provider_kind"] == "rir" and e["subject_kind"] == "boundary" for e in data["evidence"])
assert any(e["provider_kind"] == "hir" and e["subject_kind"] == "cfg" for e in data["evidence"])
assert any(e["provider_kind"] == "mir" and e["subject_kind"] == "cleanup" for e in data["evidence"])
assert all("boundary_kind" in e for e in data["evidence"])
assert all("boundary_owner" in e for e in data["evidence"])
assert all("boundary_source" in e for e in data["evidence"])
assert all(e["boundary_kind"] for e in data["evidence"] if e["boundary"] is not None)
assert all(e["boundary_source"] for e in data["evidence"] if e["boundary"] is not None)
assert all("fact_count" in e and "fallback_count" in e for e in data["evidence"])
assert all(e["fallback_count"] == 0 for e in data["evidence"])
def count_kind(graph, kind):
    return sum(1 for e in graph["evidence"] if e["kind"] == kind)

if summary["dag_metadata_evidence_count"] > 0:
    assert any(e["kind"] == "dag_metadata" for e in data["evidence"])
if summary["dag_generic_evidence_count"] > 0:
    assert any(e["kind"] == "dag_generic" for e in data["evidence"])
if summary["dag_ability_evidence_count"] > 0:
    assert any(e["kind"] == "dag_ability" for e in data["evidence"])
assert summary["dag_metadata_evidence_count"] == count_kind(data, "dag_metadata")
assert summary["dag_generic_evidence_count"] == count_kind(data, "dag_generic")
assert summary["dag_ability_evidence_count"] == count_kind(data, "dag_ability")
assert any(e["kind"] == "mir_cleanup" for e in data["evidence"])
assert summary["mir_cleanup_evidence_count"] >= 1
assert summary["mir_cleanup_evidence_count"] == count_kind(data, "mir_cleanup")
assert summary["mir_pin_cleanup_evidence_count"] >= 0
assert summary["mir_terminator_evidence_count"] >= 1
assert summary["mir_terminator_evidence_count"] == count_kind(data, "mir_terminator")
assert any(e["kind"] == "mir_terminator" for e in data["evidence"])
assert summary["mir_select_receive_evidence_count"] >= 0
assert summary["mir_select_receive_evidence_count"] == count_kind(data, "mir_select_receive")
if summary["mir_select_receive_evidence_count"] > 0:
    assert any(e["kind"] == "mir_select_receive" for e in data["evidence"])
select_summary = select_data["summary"]
assert select_data["schema"] == "pgy.air.graph.v1"
assert select_summary["mir_select_receive_evidence_count"] >= 1
assert select_summary["mir_select_receive_evidence_count"] == count_kind(select_data, "mir_select_receive")
assert any(
    e["kind"] == "mir_select_receive"
    and e["provider_kind"] == "mir"
    and e["subject_kind"] == "select_receive"
    and e["subject"] == "select-receive"
    and e["fact_count"] >= 1
    and e["fallback_count"] == 0
    for e in select_data["evidence"]
)
assert summary["rir_effect_propagation_evidence_count"] <= summary["rir_effect_propagation_required_count"]
assert summary["rir_relation_propagation_evidence_count"] <= summary["rir_relation_propagation_required_count"]
assert summary["observability_schema_evidence_count"] == 1
assert summary["observability_schema_evidence_count"] == count_kind(data, "observability_schema")
assert any(e["kind"] == "observability_schema" and e["provider"] == "runtime-observability-schema" for e in data["evidence"])
assert summary["runtime_frontier_policy_evidence_count"] == 1
assert summary["runtime_frontier_policy_evidence_count"] == count_kind(data, "runtime_frontier_policy")
assert any(e["kind"] == "runtime_frontier_policy" and e["provider"] == "pgy.runtime.frontier-policy.v1" for e in data["evidence"])
assert summary["unproven_retain_count"] >= 0
assert summary["inherent_concurrency_count"] >= 0
assert summary["slot_capability_retain_count"] >= 0
assert all("location" in b and b["location"]["line"] > 0 for b in data["boundaries"])
lifecycle_summary = lifecycle_data["summary"]
assert lifecycle_summary["lifecycle_state_space_count"] == len(lifecycle_data["lifecycle_state_spaces"])
assert lifecycle_summary["lifecycle_state_space_count"] >= 1
payment = next(
    space for space in lifecycle_data["lifecycle_state_spaces"]
    if space["subject"] == "Payment"
)
assert {"Pending", "Authorized", "Captured"} <= set(payment["states"])
assert any(
    op["name"] == "Authorize" and op["valid_from_mask"] == "0x1"
    for op in payment["ops"]
)
assert any(
    op["name"] == "Capture" and op["valid_from_mask"] == "0x2"
    for op in payment["ops"]
)
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
    require_select_text '"provider_kind":"mir"'
    require_select_text '"subject_kind":"select_receive"'
    require_select_text '"subject":"select-receive"'
    if ! grep -Fq '"subject":"Payment"' "$LIFECYCLE_OUT"; then
        echo "[air-json-schema] missing lifecycle state-space subject" >&2
        sed -n '1,80p' "$LIFECYCLE_OUT" >&2
        exit 1
    fi
    if ! grep -Fq '"valid_from_mask":"0x2"' "$LIFECYCLE_OUT"; then
        echo "[air-json-schema] missing lifecycle valid-from mask" >&2
        sed -n '1,80p' "$LIFECYCLE_OUT" >&2
        exit 1
    fi
    echo "[air-json-schema] python not found; grep contract ok"
fi
