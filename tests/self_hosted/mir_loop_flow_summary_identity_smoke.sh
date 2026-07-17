#!/usr/bin/env bash
# Native LoopFlowSummary/state rows must reach self-hosted mir_lower.
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
INDEX="$ROOT_DIR/src/self_hosted/mir_lower/routine_fact_index_owner.pgy"
LOWER="$ROOT_DIR/src/self_hosted/mir_lower/routine_lower.pgy"
DUMP="$ROOT_DIR/src/compiler/mir_json_dump.c"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
pgy_prepend_windows_runtime_paths
export PATH
PGY="${PGY_BIN:-$ROOT_DIR/bin/pgy.exe}"
if [[ "$PGY" != *.exe && -x "${PGY}.exe" ]]; then PGY="${PGY}.exe"; fi
[[ -x "$PGY" ]] || { echo "missing compiler binary: $PGY" >&2; exit 1; }

BUILD_DIR="${PGY_SELFHOST_BUILD_DIR:-$ROOT_DIR/.tmp/self_hosted/mir_loop_flow}"
mkdir -p "$BUILD_DIR"
LOWER_BIN="$BUILD_DIR/mir_lower.exe"
MIR_JSON="$BUILD_DIR/loop_flow.mirjson"
VALID_OUT="$BUILD_DIR/loop_flow.out"
BAD_COUNT_JSON="$BUILD_DIR/loop_flow.bad_count.mirjson"
BAD_COUNT_OUT="$BUILD_DIR/loop_flow.bad_count.out"
BAD_RANGE_JSON="$BUILD_DIR/loop_flow.bad_range.mirjson"
BAD_RANGE_OUT="$BUILD_DIR/loop_flow.bad_range.out"
BAD_KIND_JSON="$BUILD_DIR/loop_flow.bad_kind.mirjson"
BAD_KIND_OUT="$BUILD_DIR/loop_flow.bad_kind.out"
BAD_STATE_INDEX_JSON="$BUILD_DIR/loop_flow.bad_state_index.mirjson"
BAD_STATE_INDEX_OUT="$BUILD_DIR/loop_flow.bad_state_index.out"
LOWER_LOG="$BUILD_DIR/mir_lower.compile.log"

compile_rc=0
(cd "$ROOT_DIR" && "$PGY" \
    "$(pgy_path_for_compiler "$PGY" "src/self_hosted/mir_lower/main.pgy")" \
    --backend=c -o "$(pgy_path_for_compiler "$PGY" "$LOWER_BIN")" \
    >"$LOWER_LOG" 2>&1) || compile_rc=$?
if ((compile_rc != 0)); then
    echo "mir_lower rebuild failed" >&2
    cat "$LOWER_LOG" >&2
    exit 1
fi

CASE="tests/cases/semantic_loop_flow/summary_hit.pgy"
(cd "$ROOT_DIR" && "$PGY" --mir-json \
    "$(pgy_path_for_compiler "$PGY" "$CASE")" >"$MIR_JSON")
grep -Eq -- '"loop_flow_summary_count":[1-9][0-9]*' "$MIR_JSON"
grep -Fq -- '"loop_flow_summaries"' "$MIR_JSON"
grep -Eq -- '"loop_flow_state_count":[1-9][0-9]*' "$MIR_JSON"
grep -Fq -- '"loop_flow_states"' "$MIR_JSON"
grep -Fq -- '"kind":"while"' "$MIR_JSON"
grep -Fq -- '"stable_index"' "$MIR_JSON"
grep -Fq -- '"is_consumed"' "$MIR_JSON"
grep -Fq -- '"entry_state_start"' "$MIR_JSON"
grep -Fq -- '"exit_state_count"' "$MIR_JSON"

MIR_REL="${MIR_JSON#$ROOT_DIR/}"
if ! (cd "$ROOT_DIR" && "$LOWER_BIN" "$MIR_REL" >"$VALID_OUT" 2>&1); then
    echo "valid LoopFlowSummary MIR was rejected" >&2
    cat "$VALID_OUT" >&2
    exit 1
fi
grep -Fq -- "Function: Main" "$VALID_OUT"

sed '0,/"loop_flow_summary_count":[1-9][0-9]*/s//"loop_flow_summary_count":0/' \
    "$MIR_JSON" >"$BAD_COUNT_JSON"
BAD_COUNT_REL="${BAD_COUNT_JSON#$ROOT_DIR/}"
if (cd "$ROOT_DIR" && "$LOWER_BIN" "$BAD_COUNT_REL" >"$BAD_COUNT_OUT" 2>&1); then
    echo "inconsistent summary count was accepted" >&2
    exit 1
fi
grep -Eq -- "routine LoopFlowSummary facts are incomplete|routine MIR fact index is incomplete" "$BAD_COUNT_OUT"

sed '0,/"entry_state_count":[0-9][0-9]*/s//"entry_state_count":999999/' \
    "$MIR_JSON" >"$BAD_RANGE_JSON"
BAD_RANGE_REL="${BAD_RANGE_JSON#$ROOT_DIR/}"
if (cd "$ROOT_DIR" && "$LOWER_BIN" "$BAD_RANGE_REL" >"$BAD_RANGE_OUT" 2>&1); then
    echo "out-of-range state span was accepted" >&2
    exit 1
fi
grep -Eq -- "routine LoopFlowSummary facts are incomplete|routine MIR fact index is incomplete" "$BAD_RANGE_OUT"

sed '0,/"kind":"while"/s//"kind":"for"/' \
    "$MIR_JSON" >"$BAD_KIND_JSON"
BAD_KIND_REL="${BAD_KIND_JSON#$ROOT_DIR/}"
if (cd "$ROOT_DIR" && "$LOWER_BIN" "$BAD_KIND_REL" >"$BAD_KIND_OUT" 2>&1); then
    echo "CFG/kind-mismatched summary was accepted" >&2
    exit 1
fi
grep -Fq -- "routine LoopFlowSummary facts do not match CFG loop projection" "$BAD_KIND_OUT"

sed 's/"loop_flow_states":\[{"stable_index":[0-9][0-9]*/"loop_flow_states":[{"stable_index":999999/' \
    "$MIR_JSON" >"$BAD_STATE_INDEX_JSON"
BAD_STATE_INDEX_REL="${BAD_STATE_INDEX_JSON#$ROOT_DIR/}"
if (cd "$ROOT_DIR" && "$LOWER_BIN" "$BAD_STATE_INDEX_REL" >"$BAD_STATE_INDEX_OUT" 2>&1); then
    echo "state stable index without a ResourceFlow owner was accepted" >&2
    exit 1
fi
grep -Eq -- "routine LoopFlowSummary facts are incomplete|routine MIR fact index is incomplete" "$BAD_STATE_INDEX_OUT"

grep -Fq -- 'loop_flow_summary_count' "$DUMP"
grep -Fq -- 'loop_flow_summaries' "$DUMP"
grep -Fq -- 'loop_flow_state_count' "$DUMP"
grep -Fq -- 'loop_flow_states' "$DUMP"
grep -Fq -- 'MirRoutineFactIndexLoopFlowFactsValid' "$INDEX"
grep -Fq -- 'loop_flow_summary_entry_starts' "$INDEX"
grep -Fq -- 'routine LoopFlowSummary facts are incomplete' "$LOWER"
grep -Fq -- 'LoopFlowSummaryProjectionReady' "$LOWER"
grep -Fq -- 'MirRoutineFactIndexResourceFlowIndexKnown' "$INDEX"
echo "[self-host-mir-loop-flow] LoopFlowSummary identity and fail-closed gates passed"
