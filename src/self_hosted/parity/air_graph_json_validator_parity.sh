#!/usr/bin/env bash
# Rung 2 parity for the AIR graph JSON validator (2026-05-27).
#
# Pergyra is the origin
# (src/self_hosted/tools/air_graph_json_validator/main.pgy).
# Shell grep is the parity backend. Asserts: clean exit, JSON byte-equal vs
# expected/clean.json, count parity vs shell grep on the fixture, and a
# synthetic missing-key fixture (strip "summary" key, expect rc=1).
#
# Also asserts that the committed fixture still matches the live `pgy --air-json`
# output for the canonical AIR-producing test source, so the validator does not
# drift away from the compiler's actual AIR shape.

set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
pgy_prepend_windows_runtime_paths

PGY="${PGY_BIN:-$ROOT_DIR/bin/pgy}"
if [[ "$PGY" != *.exe ]] && pgy_binary_expects_windows_paths "${PGY}.exe"; then
    PGY="${PGY}.exe"
fi
PGY_EXPLICIT=0
[[ -n "${PGY_BIN:-}" ]] && PGY_EXPLICIT=1

if [[ ! -x "$PGY" ]]; then
    if [[ "$PGY_EXPLICIT" -eq 0 ]]; then
        echo "[self-host-parity:air-graph-json] SKIP missing compiler binary: $PGY"
        exit 0
    fi
    echo "[self-host-parity:air-graph-json] missing compiler binary: $PGY" >&2
    exit 1
fi

PERGYRA_TOOL_SOURCE="$ROOT_DIR/src/self_hosted/tools/air_graph_json_validator/main.pgy"
PERGYRA_TOOL_BUILD_DIR="${PGY_SELFHOST_BUILD_DIR:-$ROOT_DIR/.tmp/self_hosted/air_graph_json_validator}"
PERGYRA_TOOL="$PERGYRA_TOOL_BUILD_DIR/main.pgy"
EXPECTED_JSON_FILE="$ROOT_DIR/src/self_hosted/tools/air_graph_json_validator/expected/clean.json"
FIXTURE_FILE="$ROOT_DIR/src/self_hosted/tools/air_graph_json_validator/fixture/sample.json"
AIR_SOURCE="$ROOT_DIR/tests/cases/backend_compare/intent_zone_binding/main.pgy"

for path in "$PERGYRA_TOOL_SOURCE" "$EXPECTED_JSON_FILE" "$FIXTURE_FILE" "$AIR_SOURCE"; do
    if [[ ! -f "$path" ]]; then
        echo "[self-host-parity:air-graph-json] missing input: $path" >&2
        exit 1
    fi
done

pgy_selfhost_tmp_root() {
    local tmp_root="${TMPDIR:-/tmp}"
    case "$tmp_root" in
        /*|[A-Za-z]:/*) ;;
        *) tmp_root="$ROOT_DIR/$tmp_root" ;;
    esac
    mkdir -p "$tmp_root"
    printf '%s\n' "$tmp_root"
}

mkdir -p "$PERGYRA_TOOL_BUILD_DIR"
cp "$PERGYRA_TOOL_SOURCE" "$PERGYRA_TOOL"

set +e
PERGYRA_OUT="$(cd "$ROOT_DIR" && "$PGY" "$PERGYRA_TOOL" --run 2>/dev/null)"
P_RC=$?
set -e

if [[ "$P_RC" -ne 0 ]]; then
    echo "[self-host-parity:air-graph-json] clean exit-code FAIL (pergyra=$P_RC)" >&2
    printf '%s\n' "$PERGYRA_OUT" >&2
    exit 1
fi
if ! grep -Fq 'pgy.selfhost.air-graph-validator.v1' <<<"$PERGYRA_OUT"; then
    echo "[self-host-parity:air-graph-json] schema header missing" >&2
    exit 1
fi

# Shell drift detector - extract counts from fixture.
extract_int() {
    local field="$1"
    grep -oE "\"${field}\":[0-9]+" "$FIXTURE_FILE" | head -n 1 | grep -oE '[0-9]+' || true
}
SHELL_INTENTS="$(extract_int intent_count)"
SHELL_BOUNDARIES="$(extract_int boundary_count)"
SHELL_EVIDENCE="$(extract_int evidence_count)"
SHELL_DRIFTS="$(extract_int drift_count)"

if [[ -z "$SHELL_INTENTS" || -z "$SHELL_BOUNDARIES" || -z "$SHELL_EVIDENCE" || -z "$SHELL_DRIFTS" ]]; then
    echo "[self-host-parity:air-graph-json] shell grep ground truth missing from fixture" >&2
    exit 1
fi

if ! grep -Fq "\"intents\":${SHELL_INTENTS}," <<<"$PERGYRA_OUT"; then
    echo "[self-host-parity:air-graph-json] counts.intents parity FAIL (shell=${SHELL_INTENTS})" >&2
    printf '%s\n' "$PERGYRA_OUT" >&2
    exit 1
fi
if ! grep -Fq "\"boundaries\":${SHELL_BOUNDARIES}," <<<"$PERGYRA_OUT"; then
    echo "[self-host-parity:air-graph-json] counts.boundaries parity FAIL (shell=${SHELL_BOUNDARIES})" >&2
    exit 1
fi
if ! grep -Fq "\"evidence\":${SHELL_EVIDENCE}," <<<"$PERGYRA_OUT"; then
    echo "[self-host-parity:air-graph-json] counts.evidence parity FAIL (shell=${SHELL_EVIDENCE})" >&2
    exit 1
fi
if ! grep -Fq "\"drifts\":${SHELL_DRIFTS}," <<<"$PERGYRA_OUT"; then
    echo "[self-host-parity:air-graph-json] counts.drifts parity FAIL (shell=${SHELL_DRIFTS})" >&2
    exit 1
fi

# Clean JSON shape parity.
PERGYRA_JSON="$(printf '%s\n' "$PERGYRA_OUT" \
    | grep -F 'pgy.selfhost.air-graph-validator.v1' \
    | tail -n 1)"
EXPECTED_JSON="$(cat "$EXPECTED_JSON_FILE")"
if [[ "$PERGYRA_JSON" != "$EXPECTED_JSON" ]]; then
    echo "[self-host-parity:air-graph-json] clean JSON parity FAIL" >&2
    echo "expected: $EXPECTED_JSON" >&2
    echo "actual:   $PERGYRA_JSON" >&2
    exit 1
fi

# Drift guard - committed fixture must still match live pgy --air-json output.
# Some sandboxed shells (MSYS / Git-Bash) cannot launch the pgy subprocess
# directly; treat that as a SKIP for the drift rung rather than a hard fail.
SELFHOST_TMP_ROOT="$(pgy_selfhost_tmp_root)"
LIVE_AIR_JSON_DIR="$(mktemp -d "$SELFHOST_TMP_ROOT/pgy-selfhost-air-live.XXXXXX")"
cleanup_live_air() {
    rm -rf "$LIVE_AIR_JSON_DIR"
}
trap cleanup_live_air EXIT
LIVE_AIR_JSON="$LIVE_AIR_JSON_DIR/live.json"
set +e
(cd "$ROOT_DIR" && "$PGY" --air-json "$AIR_SOURCE" 2>/dev/null \
    | grep '^{' | head -n 1 > "$LIVE_AIR_JSON")
LIVE_RC=$?
set -e
DRIFT_GUARD="skipped"
if [[ "$LIVE_RC" -eq 0 && -s "$LIVE_AIR_JSON" ]]; then
    # Normalize trailing CR/LF before diff -- Windows pgy emits CRLF,
    # the committed fixture has no trailing newline. Both are byte-equivalent
    # JSON; we only care that the JSON content matches.
    LIVE_NORM="$LIVE_AIR_JSON.norm"
    FIXTURE_NORM="$LIVE_AIR_JSON_DIR/fixture.norm"
    tr -d '\r\n' < "$LIVE_AIR_JSON" > "$LIVE_NORM"
    tr -d '\r\n' < "$FIXTURE_FILE" > "$FIXTURE_NORM"
    if ! diff -q "$LIVE_NORM" "$FIXTURE_NORM" >/dev/null 2>&1; then
        # PGY_AIR_GRAPH_JSON_SKIP_DRIFT escape hatch: the committed
        # fixture is pinned against an LLVM-enabled build. Builds that
        # toggle structural compile flags (e.g. LLVM_ENABLED=0 on the
        # Windows + macOS-c-only CI lanes) can legitimately reshape
        # formatting around the same underlying AIR. The summary/count
        # parity above already cross-checks the semantic shape; the
        # byte-equal drift detector is the stricter add-on, so let
        # those lanes opt out instead of forcing dual fixtures.
        if [[ "${PGY_AIR_GRAPH_JSON_SKIP_DRIFT:-0}" == "1" ]]; then
            DRIFT_GUARD="skipped-by-env"
        else
            echo "[self-host-parity:air-graph-json] committed fixture drifted from live pgy --air-json output" >&2
            echo "regenerate via: pgy --air-json $AIR_SOURCE > $FIXTURE_FILE" >&2
            exit 1
        fi
    else
        DRIFT_GUARD="ok"
    fi
fi

# Negative fixture - synthetic missing-key (strip top-level "summary":{...}).
NEG_ROOT="$(mktemp -d "$SELFHOST_TMP_ROOT/pgy-selfhost-air-neg.XXXXXX")"
cleanup_neg_root() {
    rm -rf "$NEG_ROOT"
    cleanup_live_air
}
trap cleanup_neg_root EXIT
mkdir -p "$NEG_ROOT/src/self_hosted/tools/air_graph_json_validator/fixture"
mkdir -p "$NEG_ROOT/.tmp"
# Strip the "summary":{...}, segment - simple sed that matches the live shape.
sed -E 's/"summary":\{[^}]*\},//' "$FIXTURE_FILE" \
    > "$NEG_ROOT/src/self_hosted/tools/air_graph_json_validator/fixture/sample.json"

set +e
NEG_OUT="$(cd "$NEG_ROOT" && "$PGY" "$PERGYRA_TOOL" --run 2>&1)"
NEG_RC=$?
set -e
if [[ "$NEG_RC" -ne 1 ]]; then
    echo "[self-host-parity:air-graph-json] missing-key fixture expected rc=1, got rc=$NEG_RC" >&2
    printf '%s\n' "$NEG_OUT" >&2
    exit 1
fi
if ! grep -Fq '"ok":false' <<<"$NEG_OUT"; then
    echo "[self-host-parity:air-graph-json] missing-key fixture expected ok:false" >&2
    printf '%s\n' "$NEG_OUT" >&2
    exit 1
fi
if ! grep -Fq '"missing_keys":1' <<<"$NEG_OUT"; then
    echo "[self-host-parity:air-graph-json] missing-key fixture expected missing_keys:1" >&2
    printf '%s\n' "$NEG_OUT" >&2
    exit 1
fi

echo "[self-host-parity:air-graph-json] rung-2 parity ok (intents=$SHELL_INTENTS boundaries=$SHELL_BOUNDARIES evidence=$SHELL_EVIDENCE drifts=$SHELL_DRIFTS; missing-key rc=1; live-drift=$DRIFT_GUARD)"
