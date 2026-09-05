#!/usr/bin/env bash
#
# sea_execution_lane_golden_smoke.sh — golden test for the SEA ExecutionLane
# fact, end to end through real compilation.
#
# The policy unit test (execution_lane_policy_smoke.sh) proves the decision
# table in isolation. This proves the fact actually FLOWS: a real program is
# compiled, AIR is synthesised, each concurrency boundary is classified in the
# finalization pass, and the lane is emitted in `--air-json`. It also guards the
# regression where boundaries were left at the fail-closed zero value (Reject)
# because the classifier never ran.

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
pgy_prepend_windows_runtime_paths

PGY="${PGY_BIN:-$ROOT_DIR/bin/pgy}"
PGY="$(pgy_select_optional_exe_binary "$PGY")"

fail() { echo "[sea-lane-golden] FAIL: $*" >&2; exit 1; }

if [ ! -x "$PGY" ] && [ ! -f "$PGY" ]; then
    echo "[sea-lane-golden] SKIP: no compiler binary at $PGY"
    exit 0
fi
if ! pgy_binary_is_runnable_here "$PGY"; then
    echo "[sea-lane-golden] SKIP: compiler binary not runnable on this host"
    exit 0
fi

FIXTURE="$ROOT_DIR/tests/cases/backend_compare/intent_cross_world_transfer/main.pgy"
GOLDEN="$ROOT_DIR/tests/cases/sea_execution_lanes/expected_lanes.txt"
[ -f "$FIXTURE" ] || fail "missing fixture: $FIXTURE"
[ -f "$GOLDEN" ]  || fail "missing golden: $GOLDEN"

WORK="$(mktemp -d)"
JSON="$WORK/air.json"
ROUTINE_FIXTURE="$WORK/routine_boundary_air.pgy"
ROUTINE_JSON="$WORK/routine_air.json"
GOT="$WORK/got.txt"
PY_BIN=""
if command -v python3 >/dev/null 2>&1; then
    PY_BIN="$(command -v python3)"
elif command -v python >/dev/null 2>&1; then
    PY_BIN="$(command -v python)"
fi

cat > "$ROUTINE_FIXTURE" <<'EOF'
async func Inc(x: Int) -> Int {
    return x + 1;
}

// The structured spawn lifecycle (cf66092b) rejects a live handle at scope
// exit, so the routine is async and joins both handles; the two await
// boundaries that adds are part of the golden.
async func RoutineBoundaryAir() -> Void {
    let ch: Channel<Int> = Channel(1);
    ch <- 7;
    let v: Int = <-ch;
    let task: Future<Int> = spawn Inc(v);
    let blocked: Future<Int> = spawn blocking Inc(v);
    let a: Int = await task;
    let b: Int = await blocked;
}
EOF

"$PGY" --native-pipeline --air-json "$(pgy_path_for_compiler "$PGY" "$FIXTURE")" > "$JSON" 2>"$WORK/err" \
    || { cat "$WORK/err" >&2; fail "pgy --air-json failed"; }
"$PGY" --native-pipeline --air-json "$(pgy_path_for_compiler "$PGY" "$ROUTINE_FIXTURE")" > "$ROUTINE_JSON" 2>"$WORK/routine.err" \
    || { cat "$WORK/routine.err" >&2; fail "pgy --air-json routine-boundary fixture failed"; }

grep -Fq '"schema":"pgy.air.graph.v1"' "$JSON" \
    || fail "AIR JSON golden must consume the public graph schema"
grep -Fq '"hir_input":true' "$JSON" \
    || fail "AIR JSON golden must flow through HIR evidence"
grep -Fq '"rir_input":true' "$JSON" \
    || fail "AIR JSON golden must flow through RIR evidence"
grep -Fq '"mir_input":true' "$JSON" \
    || fail "AIR JSON golden must flow through MIR evidence"
grep -Fq '"location":{"line":' "$JSON" \
    || fail "AIR JSON golden must be source-gated, not synthetic AIR state"

if [ -n "$PY_BIN" ]; then
    "$PY_BIN" - "$JSON" "$ROUTINE_JSON" > "$GOT" <<'PY'
import json
import sys

fields = [
    ("pin", "captures_pin"),
    ("live_view", "captures_live_view"),
    ("raw_slot", "captures_raw_slot"),
    ("raw_channel", "captures_raw_channel"),
    ("value_only", "captures_value_only"),
    ("authority", "crosses_authority_boundary"),
    ("movable", "requires_movability"),
    ("io", "has_io_or_ffi_effect"),
    ("await_local", "is_await_heavy_local"),
    ("fork_join", "is_deterministic_fork_join"),
    ("concurrent", "is_concurrent_site"),
]

def bool_text(value):
    return "true" if value is True else "false"

for path in sys.argv[1:]:
    with open(path, "r", encoding="utf-8") as fh:
        graph = json.load(fh)

    if graph.get("schema") != "pgy.air.graph.v1":
        raise SystemExit("unexpected AIR graph schema")
    summary = graph.get("summary", {})
    for key in ("hir_input", "rir_input", "mir_input"):
        if summary.get(key) is not True:
            raise SystemExit(f"missing required source pipeline evidence: {key}")

    boundaries = graph.get("boundaries", [])
    if not boundaries:
        raise SystemExit("expected at least one real source boundary")

    for boundary in boundaries:
        location = boundary.get("location", {})
        if not isinstance(location, dict) or location.get("line", 0) <= 0:
            raise SystemExit("boundary lacks source location; refusing synthetic AIR row")
        capture = boundary.get("boundary_capture", {})
        parts = [
            boundary.get("kind", ""),
            boundary.get("execution_lane", ""),
            "source=" + str(boundary.get("source", "")),
            "sync=" + str(boundary.get("sync", "")),
        ]
        for label, key in fields:
            parts.append(f"{label}={bool_text(capture.get(key))}")
        print(" ".join(parts))
PY
else
    # Fallback for Windows Git Bash dev loops without Python. `pgy --air-json`
    # emits compact single-line JSON; this fixture has no quotes inside source
    # names, so the stable boundary/capture shape can still be extracted.
    { grep -oE '"kind":"[^"]+"[^}]*"boundary_capture":\{[^}]*\}' "$JSON";
      grep -oE '"kind":"[^"]+"[^}]*"boundary_capture":\{[^}]*\}' "$ROUTINE_JSON"; } \
        | awk '
function jval(line, key, marker, pos, rest, end) {
    marker = "\"" key "\":";
    pos = index(line, marker);
    if (pos == 0) return "";
    rest = substr(line, pos + length(marker));
    if (substr(rest, 1, 1) == "\"") {
        rest = substr(rest, 2);
        end = index(rest, "\"");
        return end > 0 ? substr(rest, 1, end - 1) : "";
    }
    if (substr(rest, 1, 4) == "true") return "true";
    if (substr(rest, 1, 5) == "false") return "false";
    return "";
}
{
    print jval($0, "kind") " " jval($0, "execution_lane") \
        " source=" jval($0, "source") \
        " sync=" jval($0, "sync") \
        " pin=" jval($0, "captures_pin") \
        " live_view=" jval($0, "captures_live_view") \
        " raw_slot=" jval($0, "captures_raw_slot") \
        " raw_channel=" jval($0, "captures_raw_channel") \
        " value_only=" jval($0, "captures_value_only") \
        " authority=" jval($0, "crosses_authority_boundary") \
        " movable=" jval($0, "requires_movability") \
        " io=" jval($0, "has_io_or_ffi_effect") \
        " await_local=" jval($0, "is_await_heavy_local") \
        " fork_join=" jval($0, "is_deterministic_fork_join") \
        " concurrent=" jval($0, "is_concurrent_site");
}' \
        > "$GOT" || true
fi

tr -d '\r' < "$GOT" > "$GOT.norm"
mv "$GOT.norm" "$GOT"

# Regression guard: a classified boundary must never be the fail-closed zero
# value. If every lane is Reject the finalization pass did not run.
if [ -s "$GOT" ] && ! grep -qvE '^[^[:space:]]+[[:space:]]+Reject([[:space:]]|$)' "$GOT"; then
    fail "every boundary is Reject — the SEA finalization classifier did not run"
fi

if ! diff -u "$GOLDEN" "$GOT"; then
    fail "ExecutionLane golden drift (see diff above). If the fixture or the lane policy changed on purpose, update $GOLDEN."
fi

echo "[sea-lane-golden] PASS ($(wc -l < "$GOT" | tr -d ' ') boundaries)"
